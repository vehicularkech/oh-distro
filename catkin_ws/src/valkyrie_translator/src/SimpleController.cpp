#include <SimpleController.hpp>

namespace valkyrie_translator
{
   SimpleController::SimpleController()
   {}

   SimpleController::~SimpleController()
   {}

   bool SimpleController::init(hardware_interface::EffortJointInterface* hw, ros::NodeHandle& n)
   {

      lcm_ = boost::shared_ptr<lcm::LCM>(new lcm::LCM);
      if (!lcm_->good())
      {
        std::cerr << "ERROR: lcm is not good()" << std::endl;
        return false;
      }
      lcm_->subscribe("VAL_TRANSLATOR_JOINT_COMMAND", &SimpleController::jointCommandHandler, this);

      effortJointHandles.clear();
      std::vector<std::string> jointNames;

      if(n.getParam("joints", jointNames))
      {   
          if(hw)
          {
             for(unsigned int i=0; i < jointNames.size(); ++i)
             {
                 effortJointHandles.push_back(hw->getHandle(jointNames[i]));
             }

             buffer_command_effort.resize(effortJointHandles.size(), 0.0);
             buffer_current_positions.resize(effortJointHandles.size(), 0.0);
             buffer_current_velocities.resize(effortJointHandles.size(), 0.0);
             buffer_current_efforts.resize(effortJointHandles.size(), 0.0);
             latest_setpoints.resize(effortJointHandles.size(), 0.0);
          }
          else
          {
              ROS_ERROR("Effort Joint Interface is empty in hardware interace.");
              return false;
          }
       }
       else
       {
          ROS_ERROR("No joints in given namespace: %s", n.getNamespace().c_str());
          return false;
       }

       return true;
   } 

   void SimpleController::starting(const ros::Time& time)
   {
      for(unsigned int i = 0; i < effortJointHandles.size(); ++i)
      {
          buffer_current_positions[i] = effortJointHandles[i].getPosition();
          buffer_current_velocities[i] = effortJointHandles[i].getVelocity();
          buffer_current_efforts[i] = effortJointHandles[i].getEffort();
      }
   }

   void SimpleController::update(const ros::Time& time, const ros::Duration& period)
   {

      drc::joint_state_t lcm_pose_msg;
      lcm_pose_msg.num_joints = effortJointHandles.size();
      lcm_pose_msg.joint_name.assign(effortJointHandles.size(), "");
      lcm_pose_msg.joint_position.assign(effortJointHandles.size(), 0.);
      lcm_pose_msg.joint_velocity.assign(effortJointHandles.size(), 0.);
      lcm_pose_msg.joint_effort.assign(effortJointHandles.size(), 0.);

      for(unsigned int i = 0; i < effortJointHandles.size(); ++i)
      {
          buffer_current_positions[i] = effortJointHandles[i].getPosition();
          buffer_current_velocities[i] = effortJointHandles[i].getVelocity();
          buffer_command_effort[i] = -30*(buffer_current_positions[i] - latest_setpoints[i]) - buffer_current_velocities[i];
          effortJointHandles[i].setCommand(buffer_command_effort[i]);

          lcm_pose_msg.joint_position[i] = buffer_current_positions[i];
          lcm_pose_msg.joint_velocity[i] = buffer_current_velocities[i];
          lcm_pose_msg.joint_effort[i] = buffer_command_effort[i];
          lcm_pose_msg.joint_name[i] = effortJointHandles[i].getName();
      }   

      lcm_->publish("VAL_TRANSLATOR_JOINT_STATE", &lcm_pose_msg);
      
   }

   void SimpleController::stopping(const ros::Time& time)
   {}

   void SimpleController::jointCommandHandler(const lcm::ReceiveBuffer* rbuf, const std::string &channel,
                           const drc::joint_angles_t* msg)
   {
      printf("Got new setpoints\n");
      for(unsigned int i = 0; i < msg->num_joints; ++i){
        printf("Comparing joint %s...\n", msg->joint_name[i].c_str());
        for(unsigned int j = 0; j < effortJointHandles.size(); j++){
          printf("\t to jointname %s...\n", effortJointHandles[j].getName().c_str());
          if (msg->joint_name[i] == effortJointHandles[j].getName()){
            printf("\t\tMatch, updating setpoint to %f\n", msg->joint_position[i]);
            latest_setpoints[j] = msg->joint_position[i];
          }
        }
      }
   }
}
PLUGINLIB_EXPORT_CLASS(valkyrie_translator::SimpleController, controller_interface::ControllerBase)