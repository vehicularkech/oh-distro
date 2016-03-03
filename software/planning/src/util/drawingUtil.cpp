#include <iostream>
#include "drawingUtil.hpp"

#include "lcmtypes/drc/robot_state_t.hpp"
#include "drake/util/drakeGeometryUtil.h"

using namespace std;
using namespace Eigen;

void draw3dLine(bot_lcmgl_t *lcmgl, Vector3d start, Vector3d end)
{
	bot_lcmgl_begin(lcmgl, LCMGL_LINES);
	bot_lcmgl_vertex3f(lcmgl, start[0], start[1], start[2]);
	bot_lcmgl_vertex3f(lcmgl, end[0], end[1], end[2]);
	bot_lcmgl_end(lcmgl);
}

void draw3dLine(bot_lcmgl_t *lcmgl, double start_x, double start_y, double start_z, double end_x, double end_y, double end_z)
{
	bot_lcmgl_begin(lcmgl, LCMGL_LINES);
	bot_lcmgl_vertex3f(lcmgl, start_x, start_y, start_z);
	bot_lcmgl_vertex3f(lcmgl, end_x, end_y, end_z);
	bot_lcmgl_end(lcmgl);
}

void HSVtoRGB( float &r, float &g, float &b, float h, float s, float v )
{
	int i;
	float f, p, q, t;

	if( s == 0 ) {
		// achromatic (grey)
		r = g = b = v;
		return;
	}

	h /= 60;			// sector 0 to 5
	i = floor( h );
	f = h - i;			// factorial part of h
	p = v * ( 1 - s );
	q = v * ( 1 - s * f );
	t = v * ( 1 - s * ( 1 - f ) );

	switch( i ) {
		case 0:
			r = v;
			g = t;
			b = p;
			break;
		case 1:
			r = q;
			g = v;
			b = p;
			break;
		case 2:
			r = p;
			g = v;
			b = t;
			break;
		case 3:
			r = p;
			g = q;
			b = v;
			break;
		case 4:
			r = t;
			g = p;
			b = v;
			break;
		default:		// case 5:
			r = v;
			g = p;
			b = q;
			break;
	}

}

void drawPointCloud(bot_lcmgl_t *lcmgl, std::vector<Vector3d> point_cloud)
{
//	bot_lcmgl_point_size(lcmgl, 5);
//	bot_lcmgl_color3f(lcmgl, .5, .5, .5);
	bot_lcmgl_point_size(lcmgl, 15);
	bot_lcmgl_color3f(lcmgl, 1, 0, 0);
	bot_lcmgl_begin(lcmgl, LCMGL_POINTS);
	for (Vector3d point : point_cloud)
	{
		bot_lcmgl_vertex3d(lcmgl, point(0), point(1), point(2));
	}
	bot_lcmgl_end(lcmgl);
	bot_lcmgl_switch_buffer(lcmgl);
}

CandidateRobotPosePublisher::CandidateRobotPosePublisher()
{

}

int64_t CandidateRobotPosePublisher::timestamp_now()
{
    timeval tv;
    gettimeofday (&tv, NULL);
    return (int64_t) tv.tv_sec * 1000000 + tv.tv_usec;
}

void CandidateRobotPosePublisher::publish(boost::shared_ptr<lcm::LCM> lcm, RigidBodyTree &robot, VectorXd &pose)
{
	drc::robot_state_t pose_msg;
	pose_msg.utime = this->timestamp_now();
	pose_msg.pose.translation.x = pose(0);
	pose_msg.pose.translation.y = pose(1);
	pose_msg.pose.translation.z = pose(2);
	VectorXd quaternion(4);
	quaternion = rpy2quat(pose.block<3,1>(3,0));
	pose_msg.pose.rotation.w = quaternion(0);
	pose_msg.pose.rotation.x = quaternion(1);
	pose_msg.pose.rotation.y = quaternion(2);
	pose_msg.pose.rotation.z = quaternion(3);
	pose_msg.twist.linear_velocity.x = 0;
	pose_msg.twist.linear_velocity.y = 0;
	pose_msg.twist.linear_velocity.z = 0;
	pose_msg.twist.angular_velocity.x = 0;
	pose_msg.twist.angular_velocity.y = 0;
	pose_msg.twist.angular_velocity.z = 0;
	pose_msg.num_joints = robot.num_positions;
	vector<string> joint_names;
	 joint_names.push_back("base_x");
	 joint_names.push_back("base_y");
	 joint_names.push_back("base_z");
	 joint_names.push_back("base_roll");
	 joint_names.push_back("base_pitch");
	 joint_names.push_back("base_yaw");
	 joint_names.push_back("torsoYaw");
	 joint_names.push_back("torsoPitch");
	 joint_names.push_back("torsoRoll");
	 joint_names.push_back("lowerNeckPitch");
	 joint_names.push_back("neckYaw");
	 joint_names.push_back("upperNeckPitch");
	 joint_names.push_back("rightShoulderPitch");
	 joint_names.push_back("rightShoulderRoll");
	 joint_names.push_back("rightShoulderYaw");
	 joint_names.push_back("rightElbowPitch");
	 joint_names.push_back("rightForearmYaw");
	 joint_names.push_back("rightWristRoll");
	 joint_names.push_back("rightWristPitch");
	 joint_names.push_back("leftShoulderPitch");
	 joint_names.push_back("leftShoulderRoll");
	 joint_names.push_back("leftShoulderYaw");
	 joint_names.push_back("leftElbowPitch");
	 joint_names.push_back("leftForearmYaw");
	 joint_names.push_back("leftWristRoll");
	 joint_names.push_back("leftWristPitch");
	 joint_names.push_back("rightHipYaw");
	 joint_names.push_back("rightHipRoll");
	 joint_names.push_back("rightHipPitch");
	 joint_names.push_back("rightKneePitch");
	 joint_names.push_back("rightAnklePitch");
	 joint_names.push_back("rightAnkleRoll");
	 joint_names.push_back("leftHipYaw");
	 joint_names.push_back("leftHipRoll");
	 joint_names.push_back("leftHipPitch");
	 joint_names.push_back("leftKneePitch");
	 joint_names.push_back("leftAnklePitch");
	 joint_names.push_back("leftAnkleRoll");
	 joint_names.push_back("hokuyo_joint");
	pose_msg.joint_name = joint_names;
	vector<float> joint_positions(pose.data(), pose.data() + pose.size());
	pose_msg.joint_position = joint_positions;
	vector<float> joint_velocities(robot.num_positions, 0);
	pose_msg.joint_velocity = joint_velocities;
	vector<float> joint_efforts(robot.num_positions, 0);
	pose_msg.joint_effort = joint_efforts;
	pose_msg.force_torque.l_foot_force_z = 0;
	pose_msg.force_torque.l_foot_torque_x = 0;
	pose_msg.force_torque.l_foot_torque_y = 0;
	pose_msg.force_torque.r_foot_force_z = 0;
	pose_msg.force_torque.r_foot_torque_x = 0;
	pose_msg.force_torque.r_foot_torque_y = 0;
	pose_msg.force_torque.l_hand_force[0] = 0;
	pose_msg.force_torque.l_hand_force[1] = 0;
	pose_msg.force_torque.l_hand_force[2] = 0;
	pose_msg.force_torque.l_hand_torque[0] = 0;
	pose_msg.force_torque.l_hand_torque[1] = 0;
	pose_msg.force_torque.l_hand_torque[2] = 0;
	pose_msg.force_torque.r_hand_force[0] = 0;
	pose_msg.force_torque.r_hand_force[1] = 0;
	pose_msg.force_torque.r_hand_force[2] = 0;
	pose_msg.force_torque.r_hand_torque[0] = 0;
	pose_msg.force_torque.r_hand_torque[1] = 0;
	pose_msg.force_torque.r_hand_torque[2] = 0;


	lcm->publish("CANDIDATE_ROBOT_ENDPOSE", &pose_msg);
}
