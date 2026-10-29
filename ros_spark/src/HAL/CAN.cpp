#include <rclcpp/clock.hpp>
#include <rclcpp/logging.hpp>

#include <hal/CAN.h>
#include <hal/HAL.h>

#include <array>
#include <cstring>
#include <linux/can.h>
#include <thread>
#include <unistd.h>

extern int socketID;
struct CANData {
    std::array<uint8_t, 8> canData;
    uint8_t len;
    uint64_t timestamp;
};
extern std::unordered_map<uint32_t, CANData> recvCANData;
extern std::mutex recvCANDataMutex;

std::recursive_mutex canTimersMutex;
std::unordered_map<uint32_t, std::pair<std::thread, rclcpp::Context::SharedPtr>> canTimers;

extern "C" {

void HAL_CAN_SendMessage(uint32_t messageID, const uint8_t* data, uint8_t dataSize, int32_t periodMs, int32_t* status)
{
    *status = 0;

    can_frame frame;
    RCLCPP_DEBUG(rclcpp::get_logger("HAL"), "CAN_SendMessage: %x=%x Period: %d", messageID,
        ((messageID & 0x1FFFFFFF) >> 6) & 0x3FF, periodMs);

    std::memcpy(frame.data, data, dataSize);
    frame.can_id = messageID | CAN_EFF_FLAG;
    frame.can_dlc = dataSize;

    canTimersMutex.lock();
    if (periodMs == HAL_CAN_SEND_PERIOD_STOP_REPEATING) {
        auto it = canTimers.find(messageID);
        if (it == canTimers.end()) {
            RCLCPP_DEBUG(rclcpp::get_logger("HAL"), "No periodic message to %x", messageID);
            canTimersMutex.unlock();
            return;
        }
        it->second.second->shutdown("StopRepeating");
        // RCLCPP_ERROR(rclcpp::get_logger("HAL"), "Joining Periodic Message for %x", messageID);
        it->second.first.join();
        canTimers.erase(it);
        // RCLCPP_ERROR(rclcpp::get_logger("HAL"), "Stopping Periodic Message for %x", messageID);
        canTimersMutex.unlock();
        return;
    } else if (periodMs != HAL_CAN_SEND_PERIOD_NO_REPEAT) {
        RCLCPP_DEBUG(rclcpp::get_logger("HAL"), "Periodic Message to %x for %d ms", messageID, periodMs);
        rclcpp::Context::SharedPtr context = std::make_shared<rclcpp::Context>();
        rclcpp::InitOptions opts;
        opts.auto_initialize_logging(false);
        context->init(0, nullptr, opts);
        if (canTimers.find(messageID) != canTimers.end())
            HAL_CAN_SendMessage(messageID, NULL, 0, HAL_CAN_SEND_PERIOD_STOP_REPEATING, status);
        auto& timer = canTimers[messageID];

        // RCLCPP_INFO(rclcpp::get_logger("HAL"), "Periodic Message Starting Thread");
        timer.second = context;
        timer.first = std::thread(
            [periodMs, messageID](can_frame frame) {
                // RCLCPP_INFO(rclcpp::get_logger("HAL"), "In Thread");
                auto& timer = canTimers[messageID];
                while (timer.second->is_valid()) {
                    ssize_t err = write(socketID, &frame, sizeof(can_frame));
                    int errnoVal = errno;

                    if (err == -1) {
                        RCLCPP_ERROR(rclcpp::get_logger("HAL"), "Socket Can Error: %s", strerror(errnoVal));
                    }
                    rclcpp::sleep_for(std::chrono::milliseconds(periodMs), timer.second);
                }
            },
            frame);
        // RCLCPP_INFO(rclcpp::get_logger("HAL"), "Periodic Message for after Thread");
        // RCLCPP_INFO(rclcpp::get_logger("HAL"), "Assignment");
        canTimersMutex.unlock();
        return;
    }
    // RCLCPP_INFO(rclcpp::get_logger("HAL"), "Unlocking");
    canTimersMutex.unlock();
    // RCLCPP_INFO(rclcpp::get_logger("HAL"), "Done Unloccking");

    errno = 0;
    ssize_t err = write(socketID, &frame, sizeof(can_frame));
    int errnoVal = errno;

    if (err == -1) {
        RCLCPP_ERROR(rclcpp::get_logger("HAL"), "Socket Can Error: %s", strerror(errnoVal));
        *status = -1;
    }
    // RCLCPP_INFO(rclcpp::get_logger("HAL"), "End Sending %x=%x", messageID, ((messageID & 0x1FFFFFFF) >> 6) & 0x3FF);
}
void HAL_CAN_ReceiveMessage(
    uint32_t* messageID, uint32_t messageIDMask, uint8_t* data, uint8_t* dataSize, uint32_t* timeStamp, int32_t* status)
{
    RCLCPP_DEBUG(
        rclcpp::get_logger("HAL"), "CAN_ReciveMessage: %x=%x", *messageID, ((*messageID & 0x1FFFFFFF) >> 6) & 0x3FF);
    *status = HAL_ERR_CANSessionMux_MessageNotFound;
    recvCANDataMutex.lock();
    uint32_t checkMessageID = *messageID & messageIDMask;
    for (auto it = recvCANData.begin(); it != recvCANData.end(); ++it) {
        auto& canData = *it;
        if ((canData.first & messageIDMask) != checkMessageID)
            continue;
        *messageID = canData.first;
        std::copy(canData.second.canData.begin(), canData.second.canData.end(), data);
        *dataSize = canData.second.len;
        *timeStamp = canData.second.timestamp;
        *status = 0;
        recvCANData.erase(it);
        break;
    }
    recvCANDataMutex.unlock();
    if (*status != 0) {
        RCLCPP_DEBUG(rclcpp::get_logger("HAL"), "Couldn't find message %x=%x", *messageID,
            ((*messageID & 0x1FFFFFFF) >> 6) & 0x3FF);
    }
}
void HAL_CAN_OpenStreamSession(
    uint32_t* sessionHandle, uint32_t messageID, uint32_t messageIDMask, uint32_t maxMessages, int32_t* status)
{
    RCLCPP_INFO(rclcpp::get_logger("HAL"), "Opening CAN Stream Sesson");
    *status = 1;
}
void HAL_CAN_CloseStreamSession(uint32_t sessionHandle)
{
    RCLCPP_INFO(rclcpp::get_logger("HAL"), "Closing CAN Stream Session");
}
void HAL_CAN_ReadStreamSession(uint32_t sessionHandle, struct HAL_CANStreamMessage* messages, uint32_t messagesToRead,
    uint32_t* messagesRead, int32_t* status)
{
    RCLCPP_INFO(rclcpp::get_logger("HAL"), "Reading CAN Stream Session");
    *status = 1;
}
void HAL_CAN_GetCANStatus(float* percentBusUtilization, uint32_t* busOffCount, uint32_t* txFullCount,
    uint32_t* receiveErrorCount, uint32_t* transmitErrorCount, int32_t* status)
{
    RCLCPP_INFO(rclcpp::get_logger("HAL"), "Getting CAN status");
    *status = 1;
}
} // extern "C"
