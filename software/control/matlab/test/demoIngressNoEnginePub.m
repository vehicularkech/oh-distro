% function [t_test,com_test,com_nom] = testIngressPlanPub(scale_t,foot_support_qs)
function demoIngressPlanPub
%NOTEST

scale_t = 2;
addpath(fullfile(pwd,'frames'));

options.floating = true;
r = Atlas(strcat(getenv('DRC_PATH'),'/models/mit_gazebo_models/mit_robot_drake/model_minimal_contact.urdf'),options);

load('data/atlas_fp.mat');
load('data/aa_step_in_no_engine.mat');
t_qs_breaks = t_qs_breaks*scale_t;

if size(foot_support_qs,1) == 35
  foot_support_qs(1:5,:) = [];
end

l_foot_ind = r.findLinkInd('l_foot');
r_foot_ind = r.findLinkInd('r_foot');
not_feet_ind = (1:r.getNumBodies ~= l_foot_ind) &...
               (1:r.getNumBodies ~= r_foot_ind);
foot_support_qs(not_feet_ind,:)=0;

state_frame = getStateFrame(r);
state_frame.subscribe('TRUE_ROBOT_STATE');

while true
  [x,~] = getNextMessage(state_frame,10);
  if (~isempty(x))
    q0=x(1:r.getNumDOF());
    break
  end
end

nt = numel(t_qs_breaks);
nq = getNumDOF(r);
q_nom = xstar(1:nq);
q0_nom = q_qs_plan(1:nq,1);
x0_nom = [q0,zeros(size(q0))];

ref_link = r.findLink(ref_link_str);

%kinsol = doKinematics(r,q0_nom);
%r_foot_xyz_nom = forwardKin(r,kinsol,r_foot_body,[0;0;0]);
%fprintf(['Nominal right foot position:\n\t' ...
            %'x: %5.3f\n\t' ...
            %'y: %5.3f\n'], r_foot_xyz_nom(1:2))
kinsol = doKinematics(r,q0,false,false);
wTf = ref_link.T;

com_qs_plan = homogTransMult(wTf,com_qs_plan);
for i = 1:nt
  fTr_i = [ [rpy2rotmat(q_qs_plan(4:6,i)); zeros(1,3)], [q_qs_plan(1:3,i); 1] ];
  wTr_i = wTf*fTr_i;
  q_qs_plan(1:6,i) = [wTr_i(1:3,4); rotmat2rpy(wTr_i(1:3,1:3))];
end

qtraj = PPTrajectory(spline(t_qs_breaks,q_qs_plan));
comtraj = PPTrajectory(spline(t_qs_breaks,com_qs_plan(1:2,:)));
htraj = PPTrajectory(spline(t_qs_breaks,com_qs_plan(3,:)));
foot_support=PPTrajectory(zoh(t_qs_breaks,foot_support_qs));

Q = 10*eye(4);
R = 0.001*eye(2);
comgoal = com_qs_plan(1:2,end);
ltisys = LinearSystem([zeros(2),eye(2); zeros(2,4)],[zeros(2); eye(2)],[],[],[],[]);
[~,V] = tilqr(ltisys,Point(getStateFrame(ltisys),[comgoal;0*comgoal]),Point(getInputFrame(ltisys)),Q,R);

% compute TVLQR
options.tspan = linspace(comtraj.tspan(1),comtraj.tspan(2),10);
options.sqrtmethod = false;
x0traj = setOutputFrame([comtraj;fnder(comtraj)],ltisys.getStateFrame);
u0traj = setOutputFrame(ConstantTrajectory([0;0]),ltisys.getInputFrame);
S = warning('off','Drake:TVLQR:NegativeS');  % i expect to have some zero eigenvalues, which numerically fluctuate below 0
warning(S);
[~,V] = tvlqr(ltisys,x0traj,u0traj,Q,R,V,options);

% Simple PD parameters
Kp = [];
data = struct('qtraj',qtraj,'comtraj',comtraj,...
      'zmptraj',[],...
      'supptraj',foot_support,'htraj',[],'hddtraj',[],...
      'S',V.S,'s1',V.s1,'link_constraints',[]);

pub=WalkingPlanPublisher('QUASISTATIC_ROBOT_PLAN'); % hijacking walking plan type for now
still_going = true;
while still_going
  pub.publish(0,data);
  in = input('Re-publish? (y/N): ')
  if isempty(in) || strcmpi(in,'n')
    still_going = false;
  end
end

