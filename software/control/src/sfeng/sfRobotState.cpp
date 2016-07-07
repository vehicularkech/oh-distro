#include "sfRobotState.h"
#include <iostream>

void sfRobotState::_fillKinematics(const std::string &name, Isometry3d &pose, Vector6d &vel, MatrixXd &J, Vector6d &Jdv, const Vector3d &local_offset)
{
  int id = bodyName2ID.at(name);
  pose = Isometry3d::Identity();
  pose.translation() = local_offset;
  pose = robot->relativeTransform(cache, 0, id) * pose;

  vel = getTaskSpaceVel(*(robot), cache, id, local_offset);
  J = getTaskSpaceJacobian(*(robot), cache, id, local_offset);
  Jdv = getTaskSpaceJacobianDotTimesV(*(robot), cache, id, local_offset);
}

void sfRobotState::addToLog(MRDLogger &logger) const
{
  logger.addChannel("time", "s", &time);
  
  pelv.addToLog(logger);
  l_foot.addToLog(logger);
  r_foot.addToLog(logger);
  torso.addToLog(logger);

  logger.addChannel("com[x]", "m", com.data());
  logger.addChannel("com[y]", "m", com.data()+1);
  logger.addChannel("com[z]", "m", com.data()+2);
  logger.addChannel("comd[x]", "m/s", comd.data());
  logger.addChannel("comd[y]", "m/s", comd.data()+1);
  logger.addChannel("comd[z]", "m/s", comd.data()+2);

  logger.addChannel("cop[x]", "m", cop.data());
  logger.addChannel("cop[y]", "m", cop.data()+1);

  logger.addChannel("F_w[L][x]", "N", footFT_w[Side::LEFT].data()+3);
  logger.addChannel("F_w[L][y]", "N", footFT_w[Side::LEFT].data()+4);
  logger.addChannel("F_w[L][z]", "N", footFT_w[Side::LEFT].data()+5);
  logger.addChannel("M_w[L][x]", "Nm", footFT_w[Side::LEFT].data()+0);
  logger.addChannel("M_w[L][y]", "Nm", footFT_w[Side::LEFT].data()+1);
  logger.addChannel("M_w[L][z]", "Nm", footFT_w[Side::LEFT].data()+1);
  logger.addChannel("F_w[R][x]", "N", footFT_w[Side::RIGHT].data()+3);
  logger.addChannel("F_w[R][y]", "N", footFT_w[Side::RIGHT].data()+4);
  logger.addChannel("F_w[R][z]", "N", footFT_w[Side::RIGHT].data()+5);
  logger.addChannel("M_w[R][x]", "Nm", footFT_w[Side::RIGHT].data()+0);
  logger.addChannel("M_w[R][y]", "Nm", footFT_w[Side::RIGHT].data()+1);
  logger.addChannel("M_w[R][z]", "Nm", footFT_w[Side::RIGHT].data()+1);

  for (int i = 0; i < pos.size(); i++)
    logger.addChannel("q["+robot->getPositionName(i)+"]", "rad", pos.data()+i);
  for (int i = 0; i < vel.size(); i++)
    logger.addChannel("v["+robot->getPositionName(i)+"]", "rad/s", vel.data()+i);
  for (int i = 0; i < trq.size(); i++) {
    logger.addChannel("trq["+robot->getPositionName(i+6)+"]", "Nm", trq.data()+i);
  }
}

void sfRobotState::update(double t, const VectorXd &q, const VectorXd &v, const VectorXd &trq, const Vector6d &l_ft, const Vector6d &r_ft, bool rotateFootFT)
{
  if (q.size() != this->pos.size() || 
      v.size() != this->vel.size() || 
      trq.size() != this->trq.size()) {
    throw std::runtime_error("robot state update dimension mismatch");
  }

  time = t;
  this->pos = q;
  this->vel = v;
  this->trq = trq;

  cache.initialize(pos, vel);
  robot->doKinematics(cache, true);

  M = robot->massMatrix(cache);
  eigen_aligned_unordered_map<RigidBody const*, Matrix<double, TWIST_SIZE, 1>> f_ext;
  h = robot->dynamicsBiasTerm(cache, f_ext);
  
  // com
  com = robot->centerOfMass(cache);
  J_com = robot->centerOfMassJacobian(cache);
  Jdv_com = robot->centerOfMassJacobianDotTimesV(cache);
  comd = J_com * v;

  // body parts
  _fillKinematics(pelv.link_name, pelv.pose, pelv.vel, pelv.J, pelv.Jdv);
  _fillKinematics(l_foot.link_name, l_foot.pose, l_foot.vel, l_foot.J, l_foot.Jdv, Vector3d(0, 0, -0.09)); // 9cm below
  _fillKinematics(r_foot.link_name, r_foot.pose, r_foot.vel, r_foot.J, r_foot.Jdv, Vector3d(0, 0, -0.09)); // 9cm below
  _fillKinematics(torso.link_name, torso.pose, torso.vel, torso.J, torso.Jdv);
  
  _fillKinematics(l_foot_sensor.link_name, l_foot_sensor.pose, l_foot_sensor.vel, l_foot_sensor.J, l_foot_sensor.Jdv, Vector3d(0.0215646, 0.0, -0.051054));
  _fillKinematics(r_foot_sensor.link_name, r_foot_sensor.pose, r_foot_sensor.vel, r_foot_sensor.J, r_foot_sensor.Jdv, Vector3d(0.0215646, 0.0, -0.051054));

  // ft sensor
  footFT_b[Side::LEFT] = l_ft;
  footFT_b[Side::RIGHT] = r_ft;
  if (rotateFootFT) {
    for (int i = 0; i < 2; i++) {
      footFT_b[i].head(3) = AngleAxisd(M_PI, Vector3d::UnitX()) * footFT_b[i].head(3);
      footFT_b[i].tail(3) = AngleAxisd(M_PI, Vector3d::UnitX()) * footFT_b[i].tail(3);
    }
  }
  for (int i = 0; i < 2; i++) {
    footFT_w[i].segment<3>(0) = foot_sensor[i]->pose.linear() * footFT_b[i].segment<3>(0);
    footFT_w[i].segment<3>(3) = foot_sensor[i]->pose.linear() * footFT_b[i].segment<3>(3);
  }
  
  // cop
  Vector2d cop_w[2];
  for (int i = 0; i < 2; i++) {
    // cop relative to the ft sensor
    cop_b[i][0] = -footFT_b[i][1] / footFT_b[i][5];
    cop_b[i][1] = footFT_b[i][0] / footFT_b[i][5];
    
    cop_w[i][0] = -footFT_w[i][1] / footFT_w[i][5] + foot_sensor[i]->pose.translation()[0];
    cop_w[i][1] = footFT_w[i][0] / footFT_w[i][5] + foot_sensor[i]->pose.translation()[1];
  }
  cop = (cop_w[Side::LEFT]*footFT_b[Side::LEFT][5]+cop_w[Side::RIGHT]*footFT_b[Side::RIGHT][5]) / (footFT_b[Side::LEFT][5]+footFT_b[Side::RIGHT][5]);

  // sanity check
  //std::cout << (pelv.vel.isApprox(pelv.J * qd, 1e-6)) << std::endl;
  //std::cout << (l_foot.vel.isApprox(l_foot.J * qd, 1e-6)) << std::endl;
  //std::cout << (r_foot.vel.isApprox(r_foot.J * qd, 1e-6)) << std::endl;
  //std::cout << (torso.vel.isApprox(torso.J * qd, 1e-6)) << std::endl;
}

