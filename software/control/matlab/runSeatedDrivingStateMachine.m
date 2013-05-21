function runSeatedDrivingStateMachine()

addpath(fullfile(pwd,'frames'));
addpath(fullfile(getDrakePath,'examples','ZMP'));

% load atlas model
options.floating = false;  %turn this to true to do the muddy seat driving
% NOTE: floating = true does not work with harness controller, 
% something wierd with the neck and torso.
r = Atlas(strcat(getenv('DRC_PATH'),'/models/mit_gazebo_models/mit_robot_drake/model_minimal_contact_point_hands.urdf'),options);
r = removeCollisionGroupsExcept(r,{'heel','toe'});
r = compile(r);

harness_controller = HarnessController('seated_driving',r,Inf);

controllers = struct(harness_controller.name,harness_controller);

state_machine = DRCStateMachine(controllers,harness_controller.name);
state_machine.run();

end


