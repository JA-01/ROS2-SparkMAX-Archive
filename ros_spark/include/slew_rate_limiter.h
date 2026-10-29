#include <chrono>

#include <rclcpp/rclcpp.hpp>

class SlewRateLimiter {
public:
    explicit SlewRateLimiter(double rate)
        : rate(rate) {};
    double calculate(double now_secs, double requested_speed)
    {
        double allowed_change = std::min(0.1, ((now_secs - last_update) / rate));
        double ret;
        if (last_speed == requested_speed) {
            ret = requested_speed;
        } else if (last_speed < requested_speed) {
            ret = std::min(1.0, std::min(requested_speed, last_speed + allowed_change));
        } else if (last_speed > requested_speed) {
            ret = std::max(-1.0, std::max(requested_speed, last_speed - allowed_change));
        }

        last_update = now_secs;
        last_speed = ret;
        return requested_speed;
    }
    void reset(double now_secs, double reset_value)
    {
        last_update = now_secs;
        last_speed = reset_value;
    }

private:
    double last_update = 0;
    double last_speed = 0;
    double rate;
};