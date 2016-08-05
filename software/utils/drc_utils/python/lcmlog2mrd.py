import lcm
import inspect
from compiler.ast import flatten
import struct
from array import array
from mrdplot import *

def lcmtype_classes(module):
    return [c[1] for c in inspect.getmembers(module) if inspect.isclass(c[1]) and hasattr(c[1], "_get_packed_fingerprint")]

def fingerprint_map(classes):
    return {c._get_packed_fingerprint(): c for c in classes}

class FingerprintNotFoundException(Exception):
    pass

class MessageTypeManager(object):
    def __init__(self, lcmtype_module_names):
        self.fingerprint_to_type = {}
        for name in lcmtype_module_names:
            try:
                module = __import__(name)
                self.fingerprint_to_type.update(fingerprint_map(lcmtype_classes(module)))
            except ImportError:
                print "Warning: could not import module {:s}".format(name)

    def get_message_type(self, event):
        fingerprint = event.data[:8]
        try:
            return self.fingerprint_to_type[fingerprint]
        except KeyError:
            raise FingerprintNotFoundException("Could not find fingerprint to match event on channel: {:s}".format(event.channel))

    def decode_event(self, event):
        msg_type = self.get_message_type(event)
        return msg_type.decode(event.data)

def extract_mrd(manager, input_log):
    warned_channels = set()
    if not isinstance(input_log, lcm.EventLog):
        input_log = lcm.EventLog(input_log, "r")

    list_jointNames = ['torsoYaw', 'torsoPitch', 'torsoRoll',
                       'leftHipYaw', 'leftHipPitch', 'leftHipRoll', 'leftKneePitch', 'leftAnklePitch', 'leftAnkleRoll', 
                       'rightHipYaw', 'rightHipPitch', 'rightHipRoll', 'rightKneePitch', 'rightAnklePitch', 'rightAnkleRoll', 
                       'leftShoulderPitch', 'leftShoulderRoll', 'leftShoulderYaw', 'leftElbowPitch', 
                       'rightShoulderPitch', 'rightShoulderRoll', 'rightShoulderYaw', 'rightElbowPitch'
                       ]
    rs_data = []
    rs_ctr = 0
    cmd_data = []
    cmd_ctr = 0
    qp_data = []
    qp_ctr = 0
    qpinput_data = []
    qpinput_ctr = []

    ft_data = []
    ft_ctr = 0

    t0 = -1
    rs_name2idx = dict()
    cmd_name2idx = dict()
    qp_name2idx = dict()

    for event in input_log:
        try:
            msg = manager.decode_event(event) 
            
            # FT
            if event.channel == 'FORCE_TORQUE':
                if t0 == -1:
                    t0 = msg.utime

                ft_data.append((msg.utime-t0)/1e6)

                for i in xrange(2):
                    ft_data.append(msg.sensors[i].force[0])
                    ft_data.append(msg.sensors[i].force[1])
                    ft_data.append(msg.sensors[i].force[2])
                    ft_data.append(msg.sensors[i].moment[0])
                    ft_data.append(msg.sensors[i].moment[1])
                    ft_data.append(msg.sensors[i].moment[2])

                ft_ctr = ft_ctr + 1
    
            # EST_ROBOT_STATE
            if event.channel == 'EST_ROBOT_STATE':
                # init name look up
                if len(rs_name2idx) == 0:
                    for i, name in enumerate(msg.joint_name):
                        rs_name2idx[name] = i
                if t0 == -1:
                    t0 = msg.utime

                # add time
                rs_data.append((msg.utime-t0)/1e6)

                # pelvis states
                rs_data.append(msg.pose.translation.x)
                rs_data.append(msg.pose.translation.y)
                rs_data.append(msg.pose.translation.z)
                rs_data.append(msg.pose.rotation.w)
                rs_data.append(msg.pose.rotation.x)
                rs_data.append(msg.pose.rotation.y)
                rs_data.append(msg.pose.rotation.z)
                rs_data.append(msg.twist.linear_velocity.x)
                rs_data.append(msg.twist.linear_velocity.y)
                rs_data.append(msg.twist.linear_velocity.z)
                rs_data.append(msg.twist.angular_velocity.x)
                rs_data.append(msg.twist.angular_velocity.y)
                rs_data.append(msg.twist.angular_velocity.z)

                # joint pos, vel, trq
                for name in list_jointNames:
                    idx = rs_name2idx[name]
                    rs_data.append(msg.joint_position[idx])
                for name in list_jointNames:
                    idx = rs_name2idx[name]
                    rs_data.append(msg.joint_velocity[idx])
                for name in list_jointNames:
                    idx = rs_name2idx[name]
                    rs_data.append(msg.joint_effort[idx])

                rs_ctr = rs_ctr + 1

            # ROBOT_COMMAND
            if event.channel == 'ROBOT_COMMAND':
                # init name look up
                if len(cmd_name2idx) == 0:
                    for i, name in enumerate(msg.joint_names):
                        cmd_name2idx[name] = i
                if t0 == -1:
                    t0 = msg.utime

                # add time
                cmd_data.append((msg.utime-t0)/1e6)

                # joint pos, vel, trq
                for name in list_jointNames:
                    idx = cmd_name2idx[name]
                    cmd_data.append(msg.position[idx])
                for name in list_jointNames:
                    idx = cmd_name2idx[name]
                    cmd_data.append(msg.velocity[idx])
                for name in list_jointNames:
                    idx = cmd_name2idx[name]
                    cmd_data.append(msg.effort[idx])
                
                cmd_ctr = cmd_ctr + 1
    
            # CONTROLLER_STATE
            if event.channel == 'CONTROLLER_STATE':
                # init name look up
                if len(qp_name2idx) == 0:
                    for i, name in enumerate(msg.joint_name):
                        qp_name2idx[name] = i 
                if t0 == -1:
                    t0 = msg.timestamp

                # add time
                qp_data.append((msg.timestamp-t0)/1e6)

                # q, qd, qdd
                for name in list_jointNames:
                    idx = qp_name2idx[name]
                    qp_data.append(msg.q_ref[idx])
                for name in list_jointNames:
                    idx = qp_name2idx[name]
                    qp_data.append(msg.qd_ref[idx])
                for name in list_jointNames:
                    idx = qp_name2idx[name]
                    qp_data.append(msg.qdd[idx])
                
                qp_ctr = qp_ctr + 1
             

        except FingerprintNotFoundException:
            if event.channel not in warned_channels:
                print "Warning: fingerprint not found for message on channel: {:s}".format(event.channel)
                warned_channels.add(event.channel)

    print rs_ctr
    print cmd_ctr
    print qp_ctr

    ########
    rs_mrd = MRDPLOT()
    rs_mrd.name = 'rs.mrd'
    rs_mrd.n_channels = 1 + 13 + 3 * len(list_jointNames)
    rs_mrd.n_points = rs_ctr
    rs_mrd.freq = int(rs_mrd.n_points / (rs_data[(rs_mrd.n_points-1)*rs_mrd.n_channels]-rs_data[0]))
    rs_mrd.data = array('f', rs_data)
    
    rs_mrd.channel_names.append('time')
    rs_mrd.channel_names.append('pelv[x]')
    rs_mrd.channel_names.append('pelv[y]')
    rs_mrd.channel_names.append('pelv[z]')
    rs_mrd.channel_names.append('pelv[qw]')
    rs_mrd.channel_names.append('pelv[qx]')
    rs_mrd.channel_names.append('pelv[qy]')
    rs_mrd.channel_names.append('pelv[qz]')
    rs_mrd.channel_names.append('pelvd[x]')
    rs_mrd.channel_names.append('pelvd[y]')
    rs_mrd.channel_names.append('pelvd[z]')
    rs_mrd.channel_names.append('pelvw[x]')
    rs_mrd.channel_names.append('pelvw[y]')
    rs_mrd.channel_names.append('pelvw[z]')
    for name in list_jointNames:
        rs_mrd.channel_names.append('q[' + name + ']')
    for name in list_jointNames:
        rs_mrd.channel_names.append('qd[' + name + ']')
    for name in list_jointNames:
        rs_mrd.channel_names.append('trq[' + name + ']')

    rs_mrd.channel_units.append('s')
    rs_mrd.channel_units.append('m')
    rs_mrd.channel_units.append('m')
    rs_mrd.channel_units.append('m')
    rs_mrd.channel_units.append('-')
    rs_mrd.channel_units.append('-')
    rs_mrd.channel_units.append('-')
    rs_mrd.channel_units.append('-')
    rs_mrd.channel_units.append('m/s')
    rs_mrd.channel_units.append('m/s')
    rs_mrd.channel_units.append('m/s')
    rs_mrd.channel_units.append('rad/s')
    rs_mrd.channel_units.append('rad/s')
    rs_mrd.channel_units.append('rad/s')
    for name in list_jointNames:
        rs_mrd.channel_units.append('rad')
    for name in list_jointNames:
        rs_mrd.channel_units.append('rad/s')
    for name in list_jointNames:
        rs_mrd.channel_units.append('Nm')

    ########
    cmd_mrd = MRDPLOT()
    cmd_mrd.name = 'cmd.mrd'
    cmd_mrd.n_channels = 1 + 3 * len(list_jointNames)
    cmd_mrd.n_points = cmd_ctr
    cmd_mrd.freq = int(cmd_mrd.n_points / (cmd_data[(cmd_mrd.n_points-1)*cmd_mrd.n_channels]-cmd_data[0]))
    cmd_mrd.data = array('f', cmd_data)

    cmd_mrd.channel_names.append('time')
    for name in list_jointNames:
      cmd_mrd.channel_names.append('pos_d[' + name + ']')
    for name in list_jointNames:
      cmd_mrd.channel_names.append('vel_d[' + name + ']')
    for name in list_jointNames:
      cmd_mrd.channel_names.append('trq_d[' + name + ']')

    cmd_mrd.channel_units.append('s')
    for name in list_jointNames:
      cmd_mrd.channel_units.append('rad')
    for name in list_jointNames:
      cmd_mrd.channel_units.append('rad/s')
    for name in list_jointNames:
      cmd_mrd.channel_units.append('Nm')

    #######
    qp_mrd = MRDPLOT()
    qp_mrd.name = 'qp.mrd'
    qp_mrd.n_channels = 1 + 3 * len(list_jointNames)
    qp_mrd.n_points = qp_ctr
    qp_mrd.freq = 500
    qp_mrd.data = array('f', qp_data)
    
    qp_mrd.channel_names.append('time')
    for name in list_jointNames:
      qp_mrd.channel_names.append('qref[' + name + ']')
    for name in list_jointNames:
      qp_mrd.channel_names.append('qdref[' + name + ']')
    for name in list_jointNames:
      qp_mrd.channel_names.append('qdd[' + name + ']')
    
    qp_mrd.channel_units.append('s')
    for name in list_jointNames:
      qp_mrd.channel_units.append('rad')
    for name in list_jointNames:
      qp_mrd.channel_units.append('rad/s')
    for name in list_jointNames:
      qp_mrd.channel_units.append('rad/s^2')

    #######
    ft_mrd = MRDPLOT()
    ft_mrd.name = 'ft.mrd'
    ft_mrd.n_channels = 1 + 2*6
    ft_mrd.n_points = ft_ctr
    ft_mrd.freq = 500
    ft_mrd.data = array('f', ft_data)
    
    ft_mrd.channel_names.append('time')
    ft_mrd.channel_names.append('Fx[L]')
    ft_mrd.channel_names.append('Fy[L]')
    ft_mrd.channel_names.append('Fz[L]')
    ft_mrd.channel_names.append('Mx[L]')
    ft_mrd.channel_names.append('My[L]')
    ft_mrd.channel_names.append('Mz[L]')
    ft_mrd.channel_names.append('Fx[R]')
    ft_mrd.channel_names.append('Fy[R]')
    ft_mrd.channel_names.append('Fz[R]')
    ft_mrd.channel_names.append('Mx[R]')
    ft_mrd.channel_names.append('My[R]')
    ft_mrd.channel_names.append('Mz[R]')

    ft_mrd.channel_units.append('s')
    ft_mrd.channel_units.append('N')
    ft_mrd.channel_units.append('N')
    ft_mrd.channel_units.append('N')
    ft_mrd.channel_units.append('Nm')
    ft_mrd.channel_units.append('Nm')
    ft_mrd.channel_units.append('Nm')
    ft_mrd.channel_units.append('N')
    ft_mrd.channel_units.append('N')
    ft_mrd.channel_units.append('N')
    ft_mrd.channel_units.append('Nm')
    ft_mrd.channel_units.append('Nm')
    ft_mrd.channel_units.append('Nm')

    assert(rs_mrd.checkSize() and cmd_mrd.checkSize() and qp_mrd.checkSize() and ft_mrd.checkSize())

    return rs_mrd, cmd_mrd, qp_mrd, ft_mrd

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(description="Replace the utime or timestamp field of all LCM messages in a log with the timestamp recorded by the lcm-logger process")
    parser.add_argument('source', type=str, help="source log file")
    parser.add_argument('lcmtype_module_names', type=str, nargs='*', default=["drc", "drake", "bot_core", "bot_procman", "bot_frames", "bot_lcmgl", "bot_param"], help="names of python modules containing lcm type definitions")
    args = parser.parse_args()
    print args.lcmtype_module_names
    manager = MessageTypeManager(args.lcmtype_module_names)
    
    rs_mrd, cmd_mrd, qp_mrd, ft_mrd = extract_mrd(manager, args.source)
    rs_mrd.toFile()
    cmd_mrd.toFile()
    qp_mrd.toFile()
    ft_mrd.toFile()