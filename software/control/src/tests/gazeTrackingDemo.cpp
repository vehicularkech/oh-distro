#include <drake/systems/plants/RigidBodyIK.h>
#include <drake/systems/plants/RigidBodyTree.h>
#include <drake/systems/plants/constraint/RigidBodyConstraint.h>

#include <drake/systems/plants/IKoptions.h>
#include <iostream>
#include <cstdlib>
#include <limits>
#include <ConciseArgs>

using namespace Eigen;

#include "lcmtypes/bot_core/robot_state_t.hpp"
#include "lcmtypes/bot_core/pose_t.hpp"
#include <lcm/lcm-cpp.hpp>

#include <bot_frames/bot_frames.h>
#include <bot_param/param_client.h>

struct CommandLineConfig {
  std::string urdf_filename;
  Eigen::Vector3d gazeGoal;
};

enum TrackingControlMode {
  DESIRED_HEAD_ORIENTATION,
  JOINT_POSITION_GOAL
};

inline double toRad(double deg) {
  return (deg * M_PI / 180);
}

inline double toDeg(double rad) {
  return (rad * 180 / M_PI);
}

inline double clamp(double x, double lower, double upper) {
  return x < lower ? lower : (x > upper ? upper : x);
}

class App{
  public:
    App(std::shared_ptr<lcm::LCM> &lcm_, const CommandLineConfig& cl_cfg_, TrackingControlMode mode_);
    
    ~App(){
    }

    void getRobotState(bot_core::robot_state_t& robot_state_msg, int64_t utime_in, Eigen::VectorXd q, std::vector<std::string> jointNames);

    int getConstraints(Eigen::VectorXd q_star, Eigen::VectorXd &q_sol);

    void solveGazeProblem();

    void robotStateHandler(const lcm::ReceiveBuffer* rbuf, const std::string& channel, const  bot_core::robot_state_t* msg);

    void gazeGoalHandler(const lcm::ReceiveBuffer* rbuf, const std::string& channel, const  bot_core::vector_3d_t* msg);

    void aprilTagTransformHandler(const lcm::ReceiveBuffer* rbuf, const std::string& channel, const  bot_core::rigid_transform_t* msg);

    int get_trans_with_utime(std::string from_frame, std::string to_frame, int64_t utime, Eigen::Isometry3d& mat);

  private:
    std::shared_ptr<lcm::LCM> lcm_;
    CommandLineConfig cl_cfg_;
    RigidBodyTree model_;
    TrackingControlMode mode_;

    bot_core::robot_state_t rstate_;
    std::map<std::string, int> dofMap_;

    BotParam* botparam_;
    BotFrames* botframes_;
};

App::App(std::shared_ptr<lcm::LCM> &lcm_, const CommandLineConfig& cl_cfg_, TrackingControlMode mode_):
                       lcm_(lcm_), cl_cfg_(cl_cfg_), mode_(mode_){

  model_.addRobotFromURDF(cl_cfg_.urdf_filename);
  model_.compile();
  dofMap_ = model_.computePositionNameToIndexMap();

  botparam_ = bot_param_new_from_server(lcm_->getUnderlyingLCM(), 0);
  botframes_= bot_frames_get_global(lcm_->getUnderlyingLCM(), botparam_);

  lcm_->subscribe("EST_ROBOT_STATE",&App::robotStateHandler,this);

  lcm_->subscribe("GAZE_GOAL", &App::gazeGoalHandler, this);

  lcm_->subscribe("APRIL_TAG_TO_CAMERA_LEFT", &App::aprilTagTransformHandler, this);
}

int App::get_trans_with_utime(std::string from_frame, std::string to_frame,
                              int64_t utime, Eigen::Isometry3d& mat) {
  int status;
  double matx[16];
  status = bot_frames_get_trans_mat_4x4_with_utime(
      botframes_, from_frame.c_str(), to_frame.c_str(), utime, matx);
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      mat(i, j) = matx[i * 4 + j];
    }
  }
  return status;
}

// Find the joint position indices corresponding to 'name'
std::vector<int> getJointPositionVectorIndices(const RigidBodyTree &model, const std::string &name) {
  std::shared_ptr<RigidBody> joint_parent_body = model.findJoint(name);
  int num_positions = joint_parent_body->getJoint().getNumPositions();
  std::vector<int> ret(static_cast<size_t>(num_positions));

  // fill with sequentially increasing values, starting at joint_parent_body->position_num_start:
  iota(ret.begin(), ret.end(), joint_parent_body->position_num_start);
  return ret;
}

void findJointAndInsert(const RigidBodyTree &model, const std::string &name, std::vector<int> &position_list) {
  auto position_indices = getJointPositionVectorIndices(model, name);

  position_list.insert(position_list.end(), position_indices.begin(), position_indices.end());
}

Eigen::Quaterniond euler_to_quat(double roll, double pitch, double yaw) {
  
  // This conversion function introduces a NaN in Eigen Rotations when:
  // roll == pi , pitch,yaw =0    ... or other combinations.
  // cos(pi) ~=0 but not exactly 0 
  // Post DRC Trails: replace these with Eigen's own conversions
  if ( ((roll==M_PI) && (pitch ==0)) && (yaw ==0)){
    return  Eigen::Quaterniond(0,1,0,0);
  }else if( ((pitch==M_PI) && (roll ==0)) && (yaw ==0)){
    return  Eigen::Quaterniond(0,0,1,0);
  }else if( ((yaw==M_PI) && (roll ==0)) && (pitch ==0)){
    return  Eigen::Quaterniond(0,0,0,1);
  }
  
  double sy = sin(yaw*0.5);
  double cy = cos(yaw*0.5);
  double sp = sin(pitch*0.5);
  double cp = cos(pitch*0.5);
  double sr = sin(roll*0.5);
  double cr = cos(roll*0.5);
  double w = cr*cp*cy + sr*sp*sy;
  double x = sr*cp*cy - cr*sp*sy;
  double y = cr*sp*cy + sr*cp*sy;
  double z = cr*cp*sy - sr*sp*cy;
  return Eigen::Quaterniond(w,x,y,z);
}

void quat_to_euler(Eigen::Quaterniond q, double& roll, double& pitch, double& yaw) {
  const double q0 = q.w();
  const double q1 = q.x();
  const double q2 = q.y();
  const double q3 = q.z();
  roll = atan2(2*(q0*q1+q2*q3), 1-2*(q1*q1+q2*q2));
  pitch = asin(2*(q0*q2-q3*q1));
  yaw = atan2(2*(q0*q3+q1*q2), 1-2*(q2*q2+q3*q3));
}

VectorXd robotStateToDrakePosition(const bot_core::robot_state_t& rstate,
                                   const std::map<std::string, int>& dofMap,
                                   int num_positions) {
  VectorXd q = VectorXd::Zero(num_positions, 1);
  for (int i=0; i < rstate.num_joints; ++i) {
    auto iter = dofMap.find(rstate.joint_name.at(i));
    if (iter != dofMap.end()) {
      int index = iter->second;
      q(index) = rstate.joint_position[i];
    }
  }

  std::map<std::string,int>::const_iterator iter;
  iter = dofMap.find("base_x");
  if (iter!=dofMap.end()) {
    int index = iter->second;
    q[index] = rstate.pose.translation.x;
  }
  iter = dofMap.find("base_y");
  if (iter!=dofMap.end()) {
    int index = iter->second;
    q[index] = rstate.pose.translation.y;
  }
  iter = dofMap.find("base_z");
  if (iter!=dofMap.end()) {
    int index = iter->second;
    q[index] = rstate.pose.translation.z;
  }

  Vector4d quat;
  quat[0] = rstate.pose.rotation.w;
  quat[1] = rstate.pose.rotation.x;
  quat[2] = rstate.pose.rotation.y;
  quat[3] = rstate.pose.rotation.z;
  Vector3d rpy = quat2rpy(quat);

  iter = dofMap.find("base_roll");
  if (iter!=dofMap.end()) {
    int index = iter->second;
    q[index] = rpy[0];
  }

  iter = dofMap.find("base_pitch");
  if (iter!=dofMap.end()) {
    int index = iter->second; 
    q[index] = rpy[1];
  }

  iter = dofMap.find("base_yaw");
  if (iter!=dofMap.end()) {
    int index = iter->second; 
    q[index] = rpy[2];
  }

  return q;
}


/////////////////////////////////////////////////
void App::getRobotState(bot_core::robot_state_t& robot_state_msg, int64_t utime_in, Eigen::VectorXd q, std::vector<std::string> jointNames){
  robot_state_msg.utime = utime_in;

  // Pelvis Pose:
  robot_state_msg.pose.translation.x =q(0);
  robot_state_msg.pose.translation.y =q(1);
  robot_state_msg.pose.translation.z =q(2);

  Eigen::Quaterniond quat = euler_to_quat( q(3), q(4), q(5));

  robot_state_msg.pose.rotation.w = quat.w();
  robot_state_msg.pose.rotation.x = quat.x();
  robot_state_msg.pose.rotation.y = quat.y();
  robot_state_msg.pose.rotation.z = quat.z();

  robot_state_msg.twist.linear_velocity.x  = 0;
  robot_state_msg.twist.linear_velocity.y  = 0;
  robot_state_msg.twist.linear_velocity.z  = 0;
  robot_state_msg.twist.angular_velocity.x = 0;
  robot_state_msg.twist.angular_velocity.y = 0;
  robot_state_msg.twist.angular_velocity.z = 0;

  // Joint States:
  for (size_t i = 0; i < jointNames.size(); i++)  {
    robot_state_msg.joint_name.push_back( jointNames[i] );
    robot_state_msg.joint_position.push_back( q(i) );
    robot_state_msg.joint_velocity.push_back( 0);
    robot_state_msg.joint_effort.push_back( 0 );
  }
  robot_state_msg.num_joints = robot_state_msg.joint_position.size();
}


int App::getConstraints(Eigen::VectorXd q_star, Eigen::VectorXd &q_sol){
  Vector2d tspan;
  tspan << 0, 1;

  // 0 Pelvis Position and Orientation Constraints
  int pelvis_link = model_.findLinkId("pelvis");
  Vector3d pelvis_pt = Vector3d::Zero();
  Vector3d pelvis_pos0;
  pelvis_pos0(0) = rstate_.pose.translation.x;
  pelvis_pos0(1) = rstate_.pose.translation.y;
  pelvis_pos0(2) = rstate_.pose.translation.z;
  Vector3d pelvis_pos_lb = pelvis_pos0;
  //pelvis_pos_lb(0) += 0.001;
  //pelvis_pos_lb(1) += 0.001;
  //pelvis_pos_lb(2) += 0.001;
  Vector3d pelvis_pos_ub = pelvis_pos_lb;
  //pelvis_pos_ub(2) += 0.001;
  WorldPositionConstraint kc_pelvis_pos(&model_, pelvis_link, pelvis_pt, pelvis_pos_lb, pelvis_pos_ub, tspan);
  Eigen::Vector4d pelvis_quat_des(rstate_.pose.rotation.w , rstate_.pose.rotation.x, rstate_.pose.rotation.y, rstate_.pose.rotation.z);
  double pelvis_tol = 0;//0.0017453292519943296;
  WorldQuatConstraint kc_pelvis_quat(&model_, pelvis_link, pelvis_quat_des, pelvis_tol, tspan);

  // 1 Back Posture Constraint
  PostureConstraint kc_posture_back(&model_, tspan);
  std::vector<int> back_idx;
  findJointAndInsert(model_, "torsoYaw", back_idx);
  findJointAndInsert(model_, "torsoPitch", back_idx);
  findJointAndInsert(model_, "torsoRoll", back_idx);
  VectorXd back_lb = VectorXd::Zero(3);
  VectorXd back_ub = VectorXd::Zero(3);
  kc_posture_back.setJointLimits(3, back_idx.data(), back_lb, back_ub);

  // 2 Neck Safe Joint Limit Constraints
  PostureConstraint kc_posture_neck(&model_, tspan);
  std::vector<int> neck_idx;
  findJointAndInsert(model_, "lowerNeckPitch", neck_idx);
  findJointAndInsert(model_, "neckYaw", neck_idx);
  findJointAndInsert(model_, "upperNeckPitch", neck_idx);
  VectorXd neck_lb = VectorXd::Zero(3);
  neck_lb(0) = toRad(0.0);
  neck_lb(1) = toRad(-15.0);
  neck_lb(2) = toRad(-50.0);
  VectorXd neck_ub = VectorXd::Zero(3);
  neck_ub(0) = toRad(45.0);
  neck_ub(1) = toRad(15.0);
  neck_ub(2) = toRad(0.0);
  kc_posture_neck.setJointLimits(1, neck_idx.data(), neck_lb, neck_ub);

  // 3 Look At constraint:
  int head_link = model_.findLinkId("head");
  Eigen::Vector3d gaze_axis = Eigen::Vector3d(1,0,0);
  Eigen::Vector3d target = cl_cfg_.gazeGoal;
  Eigen::Vector3d gaze_origin = Eigen::Vector3d(0,0, 0);//0.45);// inserting this offset almost achieves the required look-at
  double conethreshold = 0;
  WorldGazeTargetConstraint kc_gaze(&model_, head_link, gaze_axis, target, gaze_origin, conethreshold, tspan);

  // Assemble Constraint Set
  std::vector<RigidBodyConstraint *> constraint_array;
  constraint_array.push_back(&kc_pelvis_pos);
  constraint_array.push_back(&kc_pelvis_quat);
  constraint_array.push_back(&kc_gaze);
  constraint_array.push_back(&kc_posture_back); // leave this out to also use the back
  constraint_array.push_back(&kc_posture_neck); // safe neck joint limits - does not adhere to yaw?

  // Solve
  IKoptions ikoptions(&model_);
  int info;
  std::vector<std::string> infeasible_constraint;
  inverseKin(&model_, q_star, q_star, constraint_array.size(), constraint_array.data(), q_sol, info, infeasible_constraint, ikoptions);
  printf("INFO = %d\n", info);
  if (info != 1)
    for (auto it = infeasible_constraint.begin(); it != infeasible_constraint.end(); it++)
      std::cout << *it << std::endl;

  return info;
}

static inline bot_core::pose_t getPoseAsBotPose(Eigen::Isometry3d pose, int64_t utime){
  bot_core::pose_t pose_msg;
  pose_msg.utime =   utime;
  pose_msg.pos[0] = pose.translation().x();
  pose_msg.pos[1] = pose.translation().y();
  pose_msg.pos[2] = pose.translation().z();  
  Eigen::Quaterniond r_x(pose.rotation());
  pose_msg.orientation[0] =  r_x.w();  
  pose_msg.orientation[1] =  r_x.x();  
  pose_msg.orientation[2] =  r_x.y();  
  pose_msg.orientation[3] =  r_x.z();  
  return pose_msg;
}


void App::solveGazeProblem(){
  // Publish the query for visualisation in Director
  bot_core::pose_t goalMsg;
  goalMsg.utime = rstate_.utime;  
  goalMsg.pos[0] = cl_cfg_.gazeGoal(0);
  goalMsg.pos[1] = cl_cfg_.gazeGoal(1);
  goalMsg.pos[2] = cl_cfg_.gazeGoal(2);
  goalMsg.orientation[0] = 1;
  goalMsg.orientation[1] = 0;
  goalMsg.orientation[2] = 0;
  goalMsg.orientation[3] = 0;
  lcm_->publish("POSE_BODY_ALT", &goalMsg);

  // Solve the IK problem for the neck:
  VectorXd q_star(robotStateToDrakePosition(rstate_, dofMap_, model_.num_positions));
  VectorXd q_sol(model_.num_positions);
  int info = getConstraints(q_star, q_sol);
  if (info != 1) {
    std::cout << "Problem not solved\n";
    return;
  }

  if (mode_ == TrackingControlMode::DESIRED_HEAD_ORIENTATION) { // publish utorso-to-head as orientation, not properly tracking but works with different orientations
    // Get the utorso to head frame:
    int head_link = model_.findLinkId("head");
    int utorso_link = model_.findLinkId("torso");
    KinematicsCache<double> cache = model_.doKinematics(q_sol);
    Eigen::Isometry3d world_to_head = model_.relativeTransform(cache, head_link, 0);
    Eigen::Isometry3d utorso_to_head = model_.relativeTransform(cache, head_link, utorso_link);

    // Apply 180 roll as head orientation control seems to be in z-up frame
    Eigen::Isometry3d rotation_frame;
    rotation_frame.setIdentity();
    Eigen::Quaterniond quat = euler_to_quat( M_PI, 0, 0);
    rotation_frame.rotate(quat); 
    utorso_to_head = utorso_to_head*rotation_frame;

    bot_core::pose_t world_to_head_frame_pose_msg =  getPoseAsBotPose(world_to_head, rstate_.utime);
    lcm_->publish("POSE_VICON",&world_to_head_frame_pose_msg);// for debug
    bot_core::pose_t utorso_to_head_frame_pose_msg =  getPoseAsBotPose(utorso_to_head, rstate_.utime);
    lcm_->publish("DESIRED_HEAD_ORIENTATION",&utorso_to_head_frame_pose_msg);// temp
  } else if (mode_ == TrackingControlMode::JOINT_POSITION_GOAL) { // publish neck pitch and yaw joints as orientation. this works ok when robot is facing 1,0,0,0
    // Fish out the two neck joints (in simulation) and send as a command:
    std::vector<std::string> jointNames;
    for (int i=0 ; i <model_.num_positions ; i++){
      // std::cout << model.getPositionName(i) << " " << i << "\n";
      jointNames.push_back( model_.getPositionName(i) ) ;
    }
    bot_core::robot_state_t robot_state_msg;
    getRobotState(robot_state_msg, 0*1E6, q_sol , jointNames);
    lcm_->publish("CANDIDATE_ROBOT_ENDPOSE",&robot_state_msg);

    std::vector<std::string>::iterator it1 = std::find(jointNames.begin(),
        jointNames.end(), "lowerNeckPitch");
    int lowerNeckPitchIndex = std::distance(jointNames.begin(), it1);
    float lowerNeckPitchAngle = q_sol[lowerNeckPitchIndex];

    std::vector<std::string>::iterator it2 = std::find(jointNames.begin(),
        jointNames.end(), "neckYaw");
    int neckYawIndex = std::distance(jointNames.begin(), it2);
    float neckYawAngle = q_sol[neckYawIndex];

    std::cout << lowerNeckPitchAngle << " (" << lowerNeckPitchAngle*180.0/M_PI << ") is lowerNeckPitchAngle\n";
    std::cout << neckYawAngle << " (" << neckYawAngle*180.0/M_PI << ") is neckYawAngle\n";

    bot_core::pose_t headOrientationMsg;
    headOrientationMsg.utime = rstate_.utime;
    headOrientationMsg.pos[0] = 0;
    headOrientationMsg.pos[1] = 0;
    headOrientationMsg.pos[2] = 0;
    Eigen::Quaterniond quat = euler_to_quat(0, lowerNeckPitchAngle, neckYawAngle);
    headOrientationMsg.orientation[0] = quat.w();
    headOrientationMsg.orientation[1] = quat.x();
    headOrientationMsg.orientation[2] = quat.y();
    headOrientationMsg.orientation[3] = quat.z();
    lcm_->publish("DESIRED_HEAD_ORIENTATION",&headOrientationMsg);
    lcm_->publish("POSE_VICON",&headOrientationMsg); // for debug
  } else {
    std::cerr << "Mode not selected" << std::endl;
  }

  //std::cout << "Desired orientation sent, exiting\n";
  //exit(-1);
}

int robotStateCounter = 0;
void App::robotStateHandler(const lcm::ReceiveBuffer* rbuf,
                            const std::string& channel,
                            const bot_core::robot_state_t* msg) {
  rstate_ = *msg;
  if (robotStateCounter % 400 == 0) solveGazeProblem();

  robotStateCounter++;
}

void App::gazeGoalHandler(const lcm::ReceiveBuffer* rbuf,
                          const std::string& channel,
                          const bot_core::vector_3d_t* msg) {
  cl_cfg_.gazeGoal(0) = msg->x;
  cl_cfg_.gazeGoal(1) = msg->y;
  cl_cfg_.gazeGoal(2) = msg->z;
  std::cout << "Updated gaze goal to " << cl_cfg_.gazeGoal << std::endl;
}

int aprilTagCounter = 0;
void App::aprilTagTransformHandler(const lcm::ReceiveBuffer* rbuf,
                                   const std::string& channel,
                                   const bot_core::rigid_transform_t* msg) {
  Eigen::Isometry3d aprilTagLocation;
  get_trans_with_utime("april_tag_car_beam", "local", msg->utime,
                       aprilTagLocation);
  cl_cfg_.gazeGoal(0) = aprilTagLocation.translation().x();
  cl_cfg_.gazeGoal(1) = aprilTagLocation.translation().y();
  cl_cfg_.gazeGoal(2) = aprilTagLocation.translation().z();
  if (aprilTagCounter % 30 == 0)
    std::cout << "New gaze goal: " << cl_cfg_.gazeGoal.transpose() << std::endl;
  aprilTagCounter++;
}

int main(int argc, char* argv[]) {
  CommandLineConfig cl_cfg;
  cl_cfg.gazeGoal =
      Eigen::Vector3d(2, 1, 1.2);  // Position we would like the head to gaze at

  TrackingControlMode mode;
  mode = TrackingControlMode::JOINT_POSITION_GOAL;

  ConciseArgs parser(argc, argv, "simple-fusion");
  parser.add(cl_cfg.urdf_filename, "u", "urdf", "urdf filename");
  parser.add(cl_cfg.gazeGoal(0), "x", "goal_x", "goal_x");
  parser.add(cl_cfg.gazeGoal(1), "y", "goal_y", "goal_y");
  parser.add(cl_cfg.gazeGoal(2), "z", "goal_z", "goal_z");
  // parser.add(mode, "m", "mode", "mode, 0 for DESIRED_HEAD_ORIENTATION, 1 for
  // JOINT_POSITION_GOAL");
  parser.parse();

  std::shared_ptr<lcm::LCM> lcm(new lcm::LCM);
  if (!lcm->good()) std::cerr << "ERROR: lcm is not good()" << std::endl;

  App app(lcm, cl_cfg, mode);
  std::cout << "Ready" << std::endl
       << "============================" << std::endl;
  while (0 == lcm->handle())
    ;
}
