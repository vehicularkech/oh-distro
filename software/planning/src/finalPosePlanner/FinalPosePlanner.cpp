#include "finalPosePlanner/FinalPosePlanner.hpp"

#include <memory>
#include <chrono>

#include "drake/util/drakeGeometryUtil.h"
#include "drawingUtil/drawingUtil.hpp"

using namespace std;
using namespace Eigen;

FinalPosePlanner::FinalPosePlanner()
{

}

int FinalPosePlanner::findFinalPose(RigidBodyTree &robot, string end_effector, string endeffector_side, VectorXd start_configuration,
		VectorXd endeffector_final_pose, const vector<RigidBodyConstraint *> &additional_constraints, VectorXd nominal_configuration,
		CapabilityMap &capability_map, vector<Vector3d> point_cloud, IKoptions ik_options, boost::shared_ptr<lcm::LCM> lcm, FPPOutput &output, Vector3d endeffector_point, int max_iterations,  double min_distance){

//	timing variables
    chrono::high_resolution_clock::time_point before_FPP, after_FPP, before_IK, after_IK, before_CM, after_CM, before_collision, after_collision, before_constraints, after_constraints, before_kin, after_kin, before_sampling, after_sampling;
    double IK_time = 0.;
    double collision_time = 0.;
    double constraints_time = 0.;
    double kin_time = 0.;
    double sampling_time = 0.;
	before_FPP= chrono::high_resolution_clock::now();
//	INPUT CHECKS
	int endeffector_id;
	try
	{
		endeffector_id = robot.findLinkId(end_effector);
	}
	catch (const runtime_error &)
	{
		cout << "ERROR: FinalPosePlanner::Robot has no link named " << end_effector << endl;
		return 12;
	}
	if (this->checkConfiguration(robot, start_configuration, "start_configuration") != 0) {return 12;};
	if (endeffector_final_pose.rows() == 6)
	{
		endeffector_final_pose.conservativeResize(7);
		endeffector_final_pose.block(3,0,4,1) << rpy2quat(endeffector_final_pose.block<3,1>(3,0));
	}
	if (endeffector_final_pose.rows() != 7)
	{
		cout << "ERROR: FinalPosePlanner::endeffector_final_pose must be (6x1) or (7x1). Got (" << endeffector_final_pose.size() << "x1)" << endl;
		return 12;
	}
	if (this->checkConfiguration(robot, nominal_configuration, "nominal_configuration") != 0) {return 12;};

//	CAPABILITY MAP PREPARATION
	before_CM = chrono::high_resolution_clock::now();
	capability_map.setEndeffectorPose(endeffector_final_pose);
	capability_map.setActiveSide(endeffector_side);
	capability_map.reduceActiveSet(true, point_cloud);
	capability_map.computeOrientationProbabilityDistribution();
	capability_map.computePositionProbabilityDistribution(capability_map.getMapCentre());
	after_CM = chrono::high_resolution_clock::now();


//	FINAL POSE SEARCH
	CandidateRobotPosePublisher publisher;
		before_constraints = chrono::high_resolution_clock::now();
	vector<RigidBodyConstraint *> constraints = additional_constraints;
	Point2PointDistanceConstraint position_constraint(&robot, endeffector_id, robot.findLinkId("world"), endeffector_point, endeffector_final_pose.block<3,1>(0,0), Vector3d(0,0,0), Vector3d(0,0,0));
	WorldQuatConstraint quaternion_constraint(&robot, endeffector_id, endeffector_final_pose.block<4,1>(3,0), 1./180.*M_PI);
	constraints.push_back(&position_constraint);
	constraints.push_back(&quaternion_constraint);

	constraints.resize(constraints.size() + 2);
	after_constraints = chrono::high_resolution_clock::now();
	constraints_time += chrono::duration_cast<chrono::microseconds>(after_constraints - before_constraints).count();
	VectorXd final_pose(robot.num_positions);
	vector<string> infeasible_constraints;
    VectorXd phi;
    Matrix3Xd normal, xA, xB;
    vector<int> bodyA_idx, bodyB_idx;
	int info = 14;
	int ik_info;
	int sample_info;
	int n_iter = 0;
	vector<int> sample(2);
	while (info != 1 && n_iter <= max_iterations)
	{
		n_iter++;
		before_sampling = chrono::high_resolution_clock::now();
		sample_info = capability_map.drawCapabilityMapSample(sample);
		if (sample_info != 1)
		{
			cout << "Error: FinalPosePlanner::No more sample to use" << endl;
			return sample_info;
		}
		after_sampling = chrono::high_resolution_clock::now();
		sampling_time += chrono::duration_cast<chrono::microseconds>(after_sampling - before_sampling).count();
//		GENERATE CONSTRAINTS
		before_constraints = chrono::high_resolution_clock::now();
		int base_id = robot.findLinkId(capability_map.getBaseLink());
		Vector3d orientation = capability_map.getOrientation(sample[1]);
		Vector3d position = rpy2rotmat(orientation) * capability_map.getVoxelCentre(sample[0]) + endeffector_final_pose.block<3,1>(0,0);
		WorldPositionConstraint base_position_constraint(&robot, base_id, capability_map.getMapCentre(), position, position);
		WorldEulerConstraint base_euler_constraint(&robot, base_id, orientation, orientation);
		constraints.end()[-1] = (&base_position_constraint);
		constraints.end()[-2] = (&base_euler_constraint);
		after_constraints = chrono::high_resolution_clock::now();
		constraints_time += chrono::duration_cast<chrono::microseconds>(after_constraints - before_constraints).count();

//		COMPUTE CONFIGURATION
		before_IK = chrono::high_resolution_clock::now();
		inverseKin(&robot, nominal_configuration, nominal_configuration, constraints.size(), constraints.data(), final_pose, ik_info, infeasible_constraints, ik_options);
		after_IK = chrono::high_resolution_clock::now();
	    IK_time += chrono::duration_cast<chrono::microseconds>(after_IK - before_IK).count();
		if (ik_info < 10)
		{
			before_kin = chrono::high_resolution_clock::now();
			KinematicsCache<double> kinsol = robot.doKinematics(final_pose);
			after_kin = chrono::high_resolution_clock::now();
			kin_time += chrono::duration_cast<chrono::microseconds>(after_kin - before_kin).count();
			before_collision = chrono::high_resolution_clock::now();
			bool is_valid = !robot.collidingPointsCheckOnly(kinsol, point_cloud, min_distance);
			if (is_valid)
			{
				robot.collisionDetect(kinsol, phi, normal, xA, xB, bodyA_idx, bodyB_idx, false);
				after_collision = chrono::high_resolution_clock::now();
				collision_time += chrono::duration_cast<chrono::microseconds>(after_collision - before_collision).count();
				if (((ArrayXd)phi > min_distance).all())
				{
					info = 1;
					publisher.publish(lcm, robot, final_pose);
				}
			}
		}
	}
	after_FPP = chrono::high_resolution_clock::now();
    auto computation_time = chrono::duration_cast<chrono::microseconds>(after_FPP - before_FPP).count();
    auto capability_map_time = chrono::duration_cast<chrono::microseconds>(after_CM - before_CM).count();
    output.IK_time = IK_time/1.e6;
    output.computation_time = computation_time/1.e6;
    output.capability_map_time = capability_map_time/1.e6;
    output.collision_time = collision_time/1.e6;
    output.constraints_time = constraints_time/1.e6;
    output.kinematics_time = kin_time/1.e6;
    output.sampling_time = sampling_time/1.e6;
    if(info == 1)
    {
    	cout << "Solution found in " << computation_time/1.e6 << " s" << endl;
    }
    else
    {
		cout << "Error: FinalPosePlanner::Iteration limit reached" << endl;
    }
	return info;
}

int FinalPosePlanner::checkConfiguration(const RigidBodyTree &robot, const VectorXd &configuration, string variable_name)
{
	if (configuration.rows() != robot.num_positions)
	{
		cout << "ERROR: FinalPosePlanner::" << variable_name << " does not match with DOF number" << endl;
		return 12;
	}
	for (int joint = 0; joint < configuration.size(); joint++)
	{
		if (*(configuration.data() + joint) < *(robot.joint_limit_min.data() + joint) ||
				*(configuration.data() + joint) > *(robot.joint_limit_max.data() + joint))
		{
			cout << "ERROR: FinalPosePlanner::" << variable_name << " has joints outside joint limits (Got q(" << joint << ") = " <<
					*(configuration.data() + joint) << " but it must be "<< *(robot.joint_limit_min.data() + joint) <<
					" <= q(" << joint  << ") <= " << *(robot.joint_limit_max.data() + joint) << endl;
			return 12;
		}
	}
	return 0;
}
