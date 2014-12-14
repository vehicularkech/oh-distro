function [ traj ] = gen_specifc_traj( iterations, param, trajtype )
%GEN_SPECIFC_TRAJ Summary of this function goes here
%   Detailed explanation goes here

t = (1:iterations).*param.dt;

traj.iterations = iterations;
traj.utime = 1E6*t;
traj.dt = param.dt;
traj.parameters.gravity = param.gravity;
traj.true.P_l = zeros(iterations,3);
traj.true.V_l = zeros(iterations,3);
traj.true.f_l = zeros(iterations,3);
traj.true.f_b = zeros(iterations,3);
traj.true.a_l = zeros(iterations,3);
traj.true.a_b = [zeros(iterations,2), param.gravity*ones(iterations,1)];
traj.true.w_l = zeros(iterations,3);
traj.true.w_b = zeros(iterations,3);
traj.true.E = zeros(iterations,3);
traj.true.q = [ones(iterations,1), zeros(iterations,3)];

switch (trajtype)
    case 'rotate'
        % Generate the speciic trajectory that we want
        traj = genTrajBasicRotations(param.dt, iterations, traj );
        
        % Pass through only the data we are interested in
        
    case 'gyro_bias'
        traj = genTrajBasicGyroBias(param.dt, iterations, traj );
        
    case 'acc_bias'
        traj = genTrajBasicAccBias(param.dt, iterations, traj ); 
        
end
