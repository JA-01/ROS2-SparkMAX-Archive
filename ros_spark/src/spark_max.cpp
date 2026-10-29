#include "ros_spark/spark_max.h"

namespace ros_spark {
SparkMax::SparkMax(const rclcpp::NodeOptions& options)
    : Node("motor", options)
{
    declare_parameter<int>("id", 0);
    declare_parameter<int>("period_ms", 20);
    declare_parameter<int>("watchdog_ms", 100);
    declare_parameter<int>("follow_id", -1);
    // declare_parameter<int>("edges_per_rot", 4096); // Encoder edges per rotation (4096 is for built-in encoder)
    declare_parameter<bool>("invert", false);
    // declare_parameter<bool>("invert_sensor", false);
    declare_parameter<bool>("brake_mode", false);
    // declare_parameter<bool>("analog_input", false);
    declare_parameter<double>("max_voltage", 12);
    declare_parameter<int>("max_current", 20);
    // declare_parameter<double>("sensor_multiplier", 1.0);

    // declare_parameter<double>("P", 0);
    // declare_parameter<double>("I", 0);
    // declare_parameter<double>("D", 0);
    // declare_parameter<double>("F", 0);

    period_ms_ = get_parameter("period_ms").as_int();
    watchdog_ms_ = get_parameter("watchdog_ms").as_int();
    follow_id_ = get_parameter("follow_id").as_int();

    controller_
        = std::make_unique<rev::CANSparkMax>(get_parameter("id").as_int(), rev::CANSparkMax::MotorType::kBrushless);
    pid_ = std::make_unique<rev::SparkMaxPIDController>(controller_->GetPIDController());

    std::string name(get_name());
    pub_ = create_publisher<ros_spark_msgs::msg::MotorStatus>(name + "/status", 1);
    sub_ = create_subscription<ros_spark_msgs::msg::MotorControl>(
        name + "/set", 1, std::bind(&SparkMax::set, this, std::placeholders::_1));
    this->configured_ = false;
    // controller_->SetEnable(true);
    configure();

    last_update_ = now();
    timer_ = create_wall_timer(std::chrono::milliseconds(period_ms_), std::bind(&SparkMax::onTimer, this));
}

void SparkMax::set(ros_spark_msgs::msg::MotorControl::UniquePtr msg)
{
    last_update_ = now();
    watchdog_warned_ = false;

    // if (msg->value < -1 || msg->value > 1)
    //     RCLCPP_WARN(get_logger(), "Trying to set motor output past limits: %f", msg->value);

    RCLCPP_DEBUG(get_logger(), "Setting %f", msg->value);
    if (configured_ && msg->mode == ros_spark_msgs::msg::MotorControl::SPEED) {
        //double d = limiter.calculate(now().seconds(), msg->value);
        controller_->Set(msg->value);
    }
    RCLCPP_DEBUG(get_logger(), "Set Error: %d", controller_->GetLastError());
}

void SparkMax::configure()
{
    bool warned = false;
    while (!configured_) {
        bool hadError = false;
        auto check = [this, &hadError](auto error, auto name) {
            if (error != rev::REVLibError::kOk) {
                hadError = true;
                RCLCPP_WARN(get_logger(), "Failed to configure %s with error %d", name, error);
            }
        };

        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (configured_)
            break; // Break out if signaled to stop while sleeping

        if (controller_->GetFirmwareVersion() == 0) {
            if (!warned) {
                RCLCPP_WARN(get_logger(), "Motor controller has not been seen and cannot be configured!");
                warned = true;
            }
            continue;
        }

        check(controller_->RestoreFactoryDefaults(), "RestoreFactoryDefaults");

        check(controller_->EnableVoltageCompensation(get_parameter("max_voltage").as_double()),
            "EnableVoltageCompensation");
        // edges_per_rot
        check(controller_->SetSmartCurrentLimit(20, 30), "SetSmartCurrentLimit");
        check(controller_->SetSecondaryCurrentLimit(31), "SetSecondaryCurrentLimit");
        check(controller_->SetOpenLoopRampRate(1), "SetOpenLoopRampRate");

        if (get_parameter("brake_mode").as_bool())
            check(controller_->SetIdleMode(rev::CANSparkMax::IdleMode::kBrake), "SetIdleMode(brake)");
        else
            check(controller_->SetIdleMode(rev::CANSparkMax::IdleMode::kCoast), "SetIdleMode(coast)");

        controller_->SetInverted(get_parameter("invert").as_bool());

        if (!hadError) {
            RCLCPP_INFO(get_logger(), "Successfully configured Motor Controller");
            configured_ = true;
        }
    }
}

void SparkMax::onTimer()
{
    if (!configured_)
        return;

    if (follow_id_ < 0 && now() - last_update_ > rclcpp::Duration(std::chrono::milliseconds(watchdog_ms_))) {
        //controller_->Set(limiter.calculate(now().seconds(), 0));
        controller_->Set(0);
        if (!watchdog_warned_) {
            RCLCPP_WARN(get_logger(), "Watchdog timer expired: Motor ouput disabled");
            watchdog_warned_ = true;
        }
    }

    ros_spark_msgs::msg::MotorStatus status;

    status.temperature = controller_->GetMotorTemperature();
    status.bus_voltage = controller_->GetBusVoltage();

    status.output_percent = controller_->GetAppliedOutput();
    status.output_voltage = controller_->GetBusVoltage();
    status.output_current = controller_->GetOutputCurrent();
    status.faults = controller_->GetFaults();
    status.sticky_faults = controller_->GetStickyFaults();

    // status.position = controller_->GetSelectedSensorPosition();
    // status.velocity = controller_->GetSelectedSensorVelocity();

    pub_->publish(status);
}

} // namespace ros_spark

// Register component
#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(ros_spark::SparkMax)
