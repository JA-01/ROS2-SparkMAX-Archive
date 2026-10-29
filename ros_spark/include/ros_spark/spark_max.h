#ifndef ROS_SPARK_SPARK_MAX
#define ROS_SPARK_SPARK_MAX

#include "../slew_rate_limiter.h"
#include "ros_spark_msgs/msg/motor_control.hpp"
#include "ros_spark_msgs/msg/motor_status.hpp"

#include <rclcpp/rclcpp.hpp>
#include <rcutils/logging_macros.h>

#include <rev/CANSparkMax.h>

#include <chrono>
#include <stdexcept>

namespace ros_spark {

class SparkMax : public rclcpp::Node {
public:
    explicit SparkMax(const rclcpp::NodeOptions& options);
    ~SparkMax() = default;
    void set(ros_spark_msgs::msg::MotorControl::UniquePtr msg);

protected:
    void configure();
    void onTimer();

    // Methods to be reimplemented by specific motor controllers with template specialization
    // void configure_current_limit(Configuration& config UNUSED_VAR) {};
    // void configure_sensor() {};
    // double get_output_current() { return 0.0; };

private:
    int period_ms_;
    int watchdog_ms_;
    bool watchdog_warned_ = true;
    int follow_id_;
    bool configured_ = false;

    std::unique_ptr<rev::CANSparkMax> controller_;
    std::unique_ptr<rev::SparkMaxPIDController> pid_;

    rclcpp::Time last_update_;
    rclcpp::TimerBase::SharedPtr timer_;
    SlewRateLimiter limiter = SlewRateLimiter(1.0);

    rclcpp::Publisher<ros_spark_msgs::msg::MotorStatus>::SharedPtr pub_;
    rclcpp::Subscription<ros_spark_msgs::msg::MotorControl>::SharedPtr sub_;
};

} // namespace ros_spark

#endif // ROS_SPARK_SPARK_MAX
