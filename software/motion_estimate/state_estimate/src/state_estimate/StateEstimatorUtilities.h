#ifndef STATEESTIMATORUTILITIES_H_
#define STATEESTIMATORUTILITIES_H_

#include <iostream>
#include <vector>
#include <map>
#include <string>


#include <Eigen/Dense>

#include <lcm/lcm-cpp.hpp>
#include "lcmtypes/bot_core.hpp"
#include "lcmtypes/drc_lcmtypes.hpp"

#include <bot_frames/bot_frames.h>
#include <leg-odometry/BotTransforms.hpp>

#include <leg-odometry/TwoLegOdometry.h>
#include <leg-odometry/sharedUtilities.hpp>

#include <inertial-odometry/InertialOdometry_Types.hpp>
#include <inertial-odometry/Odometry.hpp>

namespace StateEstimate {

struct Joints { 
  std::vector<float> position;
  std::vector<float> velocity;
  std::vector<float> effort;
  std::vector<std::string> name;
};

// Equivalent to bot_core_pose contents
struct PoseT { 
  int64_t utime;
  Eigen::Vector3d pos;
  Eigen::Vector3d vel;
  Eigen::Vector4d orientation;
  Eigen::Vector3d rotation_rate;
  Eigen::Vector3d accel;
};

// BDI POSE============================================================================
// Returns false if Pose BDI is old or hasn't appeared yet
bool convertBDIPose_ERS(const bot_core::pose_t* msg, drc::robot_state_t& ERS_msg);


void extractBDIPose(const bot_core::pose_t* msg, PoseT &pose_BDI_);
bool insertPoseBDI(const PoseT &pose_BDI_, drc::robot_state_t& msg);


// ATLAS STATE=========================================================================
bool insertAtlasState_ERS(const drc::atlas_state_t &atlasState, drc::robot_state_t &mERSMsg);


void appendJoints(drc::robot_state_t& msg_out, const Joints &joints);
void insertAtlasJoints(const drc::atlas_state_t* msg, Joints &jointContainer);


// Use forward kinematics to estimate the pelvis position as update to the KF
// Store result as StateEstimator:: state
void doLegOdometry(TwoLegs::FK_Data &_fk_data, const drc::atlas_state_t &atlasState, const bot_core::pose_t &_bdiPose, TwoLegs::TwoLegOdometry &_leg_odo, int firstpass);


// IMU DATA============================================================================
void handle_inertial_data_temp_name(
		const double dt,
		const drc::atlas_raw_imu_t &imu,
		const bot_core::pose_t &bdiPose,
		const Eigen::Isometry3d &IMU_to_body,
		InertialOdometry::Odometry &inert_odo,
		drc::robot_state_t& _ERSmsg,
		drc::ins_update_request_t& _DFRequest);

//int getIMUBodyAlignment(const unsigned long utime, Eigen::Isometry3d &IMU_to_body, boost::shared_ptr<lcm::LCM> &lcm_);


// DATA FUSION UTILITIES ==============================================================

void packDFUpdateRequestMsg(InertialOdometry::Odometry &inert_odo, TwoLegs::TwoLegOdometry &_leg_odo, drc::ins_update_request_t &msg);



} // namespace StateEstimate

#endif /*STATEESTIMATORUTILITIES_H_*/
