#include <rclcpp/logging.hpp>
#include <rclcpp/timer.hpp>

#include <hal/Notifier.h>

#include <unordered_map>

extern "C" {

HAL_NotifierHandle curNotifierHandle = 0;
std::unordered_map<HAL_NotifierHandle, std::pair<uint64_t, rclcpp::Context::SharedPtr>> timers;

HAL_NotifierHandle HAL_InitializeNotifier(int32_t* status)
{
    RCLCPP_INFO(rclcpp::get_logger("HAL"), "HAL_InitializeNotifier");
    *status = 0;
    HAL_NotifierHandle handle = ++curNotifierHandle;
    rclcpp::Context::SharedPtr context = std::make_shared<rclcpp::Context>();
    rclcpp::InitOptions opts;
    opts.auto_initialize_logging(false);
    context->init(0, nullptr, opts);
    timers[handle] = std::make_pair(0, context);
    return handle;
}

void HAL_StopNotifier(HAL_NotifierHandle notifierHandle, int32_t* status)
{
    RCLCPP_INFO(rclcpp::get_logger("HAL"), "HAL_StopNotifier");
    *status = 0;
    auto it = timers.find(notifierHandle);
    if (it == timers.end()) {
        RCLCPP_ERROR(rclcpp::get_logger("HAL"), "Notifier Handle not valid in HAL_CleanNotifier");
        return;
    }
    it->second.second->shutdown("HAL_StopNotifier");
}

void HAL_CleanNotifier(HAL_NotifierHandle notifierHandle, int32_t* status)
{
    RCLCPP_INFO(rclcpp::get_logger("HAL"), "HAL_CleanNotifier");
    *status = 0;
    auto it = timers.find(notifierHandle);
    if (it == timers.end()) {
        RCLCPP_ERROR(rclcpp::get_logger("HAL"), "Notifier Handle not valid in HAL_CleanNotifier");
        return;
    }
    it->second.second->shutdown("HAL_CleanNotifier");
    timers.erase(it);
}

void HAL_UpdateNotifierAlarm(HAL_NotifierHandle notifierHandle, uint64_t triggerTime, int32_t* status)
{
    RCLCPP_INFO(rclcpp::get_logger("HAL"), "HAL_UpdateNotifierAlarm");
    *status = 0;
    auto it = timers.find(notifierHandle);
    if (it == timers.end()) {
        RCLCPP_ERROR(rclcpp::get_logger("HAL"), "Notifier Handle not valid in HAL_UpdateNotifierAlarm");
        *status = 1;
    }
    it->second.first = triggerTime;
}

uint64_t HAL_WaitForNotifierAlarm(HAL_NotifierHandle notifierHandle, int32_t* status)
{
    RCLCPP_INFO(rclcpp::get_logger("HAL"), "HAL_WaitForNotifierAlarm");
    *status = 0;
    auto it = timers.find(notifierHandle);
    if (it == timers.end()) {
        RCLCPP_ERROR(rclcpp::get_logger("HAL"), "Notifier Handle not valid in HAL_UpdateNotifierAlarm");
        *status = 1;
    }
    rclcpp::sleep_for(std::chrono::microseconds(it->second.first), it->second.second);
    return 0;
}

} // extern "C"
