function [Kp,Kd] = getFullPDGains(manip)

%NOTEST

B = manip.getB; %For use in trunk
% B=manip.manip.B; %For use in scott's branch
idx = B'*(1:manip.getNumStates()/2)';

fr=CoordinateFrame('q_d',length(idx),'d',{manip.getStateFrame.coordinates{idx}});
Kp = Point(fr);
Kd = Point(fr);

Kp.l_leg_uhz = 6.25;
Kp.l_leg_mhx = 250.0;
Kp.l_leg_lhy = 2500.0;
Kp.l_leg_kny = 1250.0;
Kp.l_leg_uay = 1125.0;
Kp.l_leg_lax = 625.0;
Kp.r_leg_uhz = Kp.l_leg_uhz;
Kp.r_leg_mhx = Kp.l_leg_mhx;
Kp.r_leg_lhy = Kp.l_leg_lhy;
Kp.r_leg_kny = Kp.l_leg_kny;
Kp.r_leg_uay = Kp.l_leg_uay;
Kp.r_leg_lax = Kp.l_leg_lax;
Kp.l_arm_usy = 625.0;
Kp.l_arm_shx = 250.0;
Kp.l_arm_ely = 62.5;
Kp.l_arm_elx = 62.5;
Kp.l_arm_uwy = 12.5;
Kp.l_arm_mwx = 12.5;
Kp.r_arm_usy = Kp.l_arm_usy;
Kp.r_arm_shx = Kp.l_arm_shx;
Kp.r_arm_ely = Kp.l_arm_ely;
Kp.r_arm_elx = Kp.l_arm_elx;
Kp.r_arm_uwy = Kp.l_arm_uwy;
Kp.r_arm_mwx = Kp.l_arm_mwx;
Kp.neck_ay = 25.0;
Kp.back_lbz = 250.0;
Kp.back_mby = 2500.0;
Kp.back_ubx = 1000.0;

Kd.l_leg_uhz = 0.05;
Kd.l_leg_mhx = 13.5;
Kd.l_leg_lhy = 25.0;
Kd.l_leg_kny = 77.0;
Kd.l_leg_uay = 85.0;
Kd.l_leg_lax = 22.5;
Kd.r_leg_uhz = Kd.l_leg_uhz;
Kd.r_leg_mhx = Kd.l_leg_mhx;
Kd.r_leg_lhy = Kd.l_leg_lhy;
Kd.r_leg_kny = Kd.l_leg_kny;
Kd.r_leg_uay = Kd.l_leg_uay;
Kd.r_leg_lax = Kd.l_leg_lax;
Kd.l_arm_usy = 13.5;
Kd.l_arm_shx = 90.0;
Kd.l_arm_ely = 13.5;
Kd.l_arm_elx = 13.5;
Kd.l_arm_uwy = 1.125;
Kd.l_arm_mwx = 1.125;
Kd.r_arm_usy = Kd.l_arm_usy;
Kd.r_arm_shx = Kd.l_arm_shx;
Kd.r_arm_ely = Kd.l_arm_ely;
Kd.r_arm_elx = Kd.l_arm_elx;
Kd.r_arm_uwy = Kd.l_arm_uwy;
Kd.r_arm_mwx = Kd.l_arm_mwx;
Kd.neck_ay = 4.5;
Kd.back_lbz = 36.0;
Kd.back_mby = 72.0;
Kd.back_ubx = 27.0;

Kp.r_f0_j0 = 12.0;
Kp.r_f0_j1 = 8.0;
Kp.r_f0_j2 = 10.0;
Kp.r_f1_j0 = 12.0;
Kp.r_f1_j1 = 8.0;
Kp.r_f1_j2 = 8.0;
Kp.r_f2_j0 = 12.0;
Kp.r_f2_j1 = 8.0;
Kp.r_f2_j2 = 8.0;
Kp.r_f3_j0 = 12.0;
Kp.r_f3_j1 = 8.0;
Kp.r_f3_j2 = 8.0;

Kd.r_f0_j0 = 1.4;
Kd.r_f0_j1 = 0.85;
Kd.r_f0_j2 = 0.9;
Kd.r_f1_j0 = 1.4;
Kd.r_f1_j1 = 0.9;
Kd.r_f1_j2 = 1.1;
Kd.r_f2_j0 = 1.4;
Kd.r_f2_j1 = 0.9;
Kd.r_f2_j2 = 1.1;
Kd.r_f3_j0 = 1.4;
Kd.r_f3_j1 = 0.9;
Kd.r_f3_j2 = 1.1;

Kp.l_f0_j0 = Kp.r_f0_j0;
Kp.l_f0_j1 = Kp.r_f0_j1;
Kp.l_f0_j2 = Kp.r_f0_j2;
Kp.l_f1_j0 = Kp.r_f1_j0;
Kp.l_f1_j1 = Kp.r_f1_j1;
Kp.l_f1_j2 = Kp.r_f1_j2;
Kp.l_f2_j0 = Kp.r_f2_j0;
Kp.l_f2_j1 = Kp.r_f2_j1;
Kp.l_f2_j2 = Kp.r_f2_j2;
Kp.l_f3_j0 = Kp.r_f3_j0;
Kp.l_f3_j1 = Kp.r_f3_j1;
Kp.l_f3_j2 = Kp.r_f3_j2;

Kd.l_f0_j0 = Kd.r_f0_j0;
Kd.l_f0_j1 = Kd.r_f0_j1;
Kd.l_f0_j2 = Kd.r_f0_j2;
Kd.l_f1_j0 = Kd.r_f1_j0;
Kd.l_f1_j1 = Kd.r_f1_j1;
Kd.l_f1_j2 = Kd.r_f1_j2;
Kd.l_f2_j0 = Kd.r_f2_j0;
Kd.l_f2_j1 = Kd.r_f2_j1;
Kd.l_f2_j2 = Kd.r_f2_j2;
Kd.l_f3_j0 = Kd.r_f3_j0;
Kd.l_f3_j1 = Kd.r_f3_j1;
Kd.l_f3_j2 = Kd.r_f3_j2;

Kp = diag(double(Kp));
Kd = diag(double(Kd));

end