#include "app/cartesian_controller.h"
#include "app/common.h"
#include "app/config.h"
#include "utils.h"
#include <stdexcept>
#include <sys/syscall.h>
#include <sys/types.h>

using namespace arx;

Arx5CartesianController::Arx5CartesianController(RobotConfig robot_config, ControllerConfig controller_config,
                                                 std::string interface_name, std::string urdf_path)
    : Arx5ControllerBase(robot_config, controller_config, interface_name, urdf_path)
{
}

Arx5CartesianController::Arx5CartesianController(std::string model, std::string interface_name, std::string urdf_path)
    : Arx5CartesianController::Arx5CartesianController(
          RobotConfigFactory::get_instance().get_config(model),
          ControllerConfigFactory::get_instance().get_config(
              "cartesian_controller", RobotConfigFactory::get_instance().get_config(model).joint_dof),
          interface_name, urdf_path)
{
}

void Arx5CartesianController::set_eef_cmd(EEFState new_cmd)
{
    std::lock_guard<std::mutex> lock(_cmd_mutex);
    JointState current_joint_state = get_joint_state();

    std::tuple<bool, VecDoF> ik_results;
    ik_results = _solver->multi_trial_ik(new_cmd.pose_6d, _joint_state.pos);
    bool success = std::get<0>(ik_results);
    VecDoF target_joint_pos = std::get<1>(ik_results);

    // The following line only works under c++17
    // auto [success, target_joint_pos] = _solver->inverse_kinematics(new_cmd.pose_6d, current_joint_state.pos);
    _logger->debug("target_pose: {}, current_joint_pos: {}", vec2str(new_cmd.pose_6d),
                   vec2str(current_joint_state.pos));
    if (success)
    {
        double current_time = get_timestamp();
        // TODO: include velocity
        std::lock_guard<std::mutex> lock(_interpolator_mutex);
        _logger->debug("target_joint_pos: {}", vec2str(target_joint_pos));
        _joint_interpolator.update(current_time, target_joint_pos, Pose6d::Zero(), new_cmd.timestamp);
        _gripper_interpolator.update(current_time, new_cmd.gripper_pos, 0, new_cmd.timestamp);
    }
    else
    {
        _logger->warn("Inverse kinematics failed");
    }
}
