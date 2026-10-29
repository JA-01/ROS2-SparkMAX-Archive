#include <rclcpp/logging.hpp>

#include <hal/CAN.h>
#include <hal/CANAPI.h>
#include <hal/Errors.h>
#include <hal/HAL.h>
#include <hal/Notifier.h>

#include <algorithm>
#include <cstring>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

int socketID = -1;

HAL_CANHandle curCANHandle = 0;
struct CANHandleData {
    HAL_CANDeviceType deviceType;
    HAL_CANManufacturer manufacturer;
    int32_t deviceId;
};
std::unordered_map<HAL_CANHandle, CANHandleData> canHandles;

std::thread recvCANThread;
struct CANData {
    std::array<uint8_t, 8> canData;
    uint8_t len;
    uint64_t timestamp;
};
std::mutex recvCANDataMutex;
std::unordered_map<uint32_t, CANData> recvCANData;

static int32_t CreateCANId(CANHandleData& data, int32_t apiId)
{
    int32_t createdId = 0;
    createdId |= (static_cast<int32_t>(data.deviceType) & 0x1F) << 24;
    createdId |= (static_cast<int32_t>(data.manufacturer) & 0xFF) << 16;
    createdId |= (apiId & 0x3FF) << 6;
    createdId |= (data.deviceId & 0x3F);
    return createdId;
}

extern "C" {

HAL_CANHandle HAL_InitializeCAN(
    HAL_CANManufacturer manufacturer, int32_t deviceId, HAL_CANDeviceType deviceType, int32_t* status)
{
    HAL_CANHandle handle = ++curCANHandle;
    auto& data = canHandles[handle];
    data.manufacturer = manufacturer;
    data.deviceId = deviceId;
    data.deviceType = deviceType;

    // Initialize CAN if not yet done
    if (socketID == -1) {
        socketID = socket(PF_CAN, SOCK_RAW, CAN_RAW);
        struct ifreq ifr;
        strcpy(ifr.ifr_name, "can0");
        ioctl(socketID, SIOCGIFINDEX, &ifr);

        struct sockaddr_can addr;
        addr.can_family = AF_CAN;
        addr.can_ifindex = ifr.ifr_ifindex;

        bind(socketID, (struct sockaddr*)&addr, sizeof(addr));
        can_filter rfilter[1];
        // TODO: Need to filter to only REV Robotics messages
        rfilter[0].can_id = CAN_INV_FILTER;
        rfilter[0].can_mask = (CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_EFF_MASK);
        setsockopt(socketID, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));

        recvCANThread = std::thread([]() {
            can_frame frame;
            while (rclcpp::ok()) {
                CANData data;

                // Only can read one at a time as it appears
                auto bytesRead = read(socketID, &frame, sizeof(can_frame));
                int status = 0;
                data.timestamp = HAL_GetFPGATime(&status) / 1000;

                // Error or nothing recieved
                if (bytesRead <= 0) {
                    RCLCPP_ERROR(rclcpp::get_logger("HAL"), "No CAN Bytes Read");
                    continue;
                }

                // Only can read one at a time as it appears
                if ((bytesRead < sizeof(can_frame)) != 0) {
                    RCLCPP_ERROR(rclcpp::get_logger("HAL"), "Didn't read enough CAN bytes");
                    continue;
                }

                // Too much recieved, shouldn't ever happen
                if (bytesRead > sizeof(can_frame)) {
                    RCLCPP_ERROR(rclcpp::get_logger("HAL"), "Too many CAN bytes received");
                    continue;
                }

                // Success
                // See https://www.kernel.org/doc/Documentation/networking/can.txt section
                // 4.1.1.1 CAN filter usage optimisation for masking details
                std::memcpy(data.canData.data(), frame.data, frame.can_dlc);
                data.len = frame.can_dlc;
                RCLCPP_DEBUG(rclcpp::get_logger("HAL"), "Received canID: %x=%x", frame.can_id & 0x1FFFFFFF,
                    ((frame.can_id & 0x1FFFFFFF) >> 6) & 0x3FF);

                recvCANDataMutex.lock();
                recvCANData[frame.can_id] = data;
                recvCANDataMutex.unlock();
            }
        });

        // SPARK MAX heartbeat (motor safety -- tells SPARK it's okay to take commands)
        std::array<uint8_t, 8> heartbeatData1 { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        HAL_CAN_SendMessage(0x02052C80, heartbeatData1.data(), 8, 10, status);
        if (*status != 0) {
            RCLCPP_ERROR(rclcpp::get_logger("HAL"), "Failed to send periodic spark max heartbeat data");
            return 0;
        }
    }

    *status = 0;
    return handle;
}

void HAL_CleanCAN(HAL_CANHandle handle) { RCLCPP_INFO(rclcpp::get_logger("HAL"), "Cleaning CAN"); }

void HAL_WriteCANPacket(HAL_CANHandle handle, const uint8_t* data, int32_t length, int32_t apiId, int32_t* status)
{
    auto it = canHandles.find(handle);
    if (it == canHandles.end()) {
        *status = HAL_HANDLE_ERROR;
        return;
    }

    auto id = CreateCANId(it->second, apiId);

    RCLCPP_DEBUG(rclcpp::get_logger("HAL"), "WriteCANPacket: %x Data: %x %x %x %x %x %x %x %x Size: %d", apiId, data[0],
        data[1], data[2], data[3], data[4], data[5], data[6], data[7], length);
    HAL_CAN_SendMessage(id, data, length, HAL_CAN_SEND_PERIOD_NO_REPEAT, status);
}

void HAL_WriteCANPacketRepeating(
    HAL_CANHandle handle, const uint8_t* data, int32_t length, int32_t apiId, int32_t repeatMs, int32_t* status)
{
    auto it = canHandles.find(handle);
    if (it == canHandles.end()) {
        *status = HAL_HANDLE_ERROR;
        return;
    }

    auto id = CreateCANId(it->second, apiId);

    HAL_CAN_SendMessage(id, data, length, repeatMs, status);
}

void HAL_StopCANPacketRepeating(HAL_CANHandle handle, int32_t apiId, int32_t* status)
{
    auto it = canHandles.find(handle);
    if (it == canHandles.end()) {
        *status = HAL_HANDLE_ERROR;
        return;
    }

    auto id = CreateCANId(it->second, apiId);

    HAL_CAN_SendMessage(id, nullptr, 0, HAL_CAN_SEND_PERIOD_STOP_REPEATING, status);
}

void HAL_ReadCANPacketNew(
    HAL_CANHandle handle, int32_t apiId, uint8_t* data, int32_t* length, uint64_t* receivedTimestamp, int32_t* status)
{
    auto it = canHandles.find(handle);
    if (it == canHandles.end()) {
        *status = HAL_HANDLE_ERROR;
        return;
    }

    uint32_t messageID = CreateCANId(it->second, apiId);
    RCLCPP_DEBUG(
        rclcpp::get_logger("HAL"), "ReadCANPacketNew: %x=%x", messageID, ((messageID & 0x1FFFFFFF) >> 6) & 0x3FF);
    uint8_t dataSize = 0;
    uint32_t ts = 0;
    HAL_CAN_ReceiveMessage(&messageID, 0x1FFFFFFF, data, &dataSize, &ts, status);

    *length = dataSize;
    *receivedTimestamp = ts;
}

void HAL_ReadCANPacketTimeout(HAL_CANHandle handle, int32_t apiId, uint8_t* data, int32_t* length,
    uint64_t* receivedTimestamp, int32_t timeoutMs, int32_t* status)
{
    auto it = canHandles.find(handle);
    if (it == canHandles.end()) {
        *status = HAL_HANDLE_ERROR;
        return;
    }

    uint32_t messageID = CreateCANId(it->second, apiId);
    RCLCPP_DEBUG(rclcpp::get_logger("HAL"), "ReadCANPacketTimeout: %x=%x %d", messageID,
        ((messageID & 0x1FFFFFFF) >> 6) & 0x3FF, recvCANThread.joinable());
    uint8_t dataSize = 0;
    uint32_t ts = 0;
    HAL_CAN_ReceiveMessage(&messageID, 0x1FFFFFFF, data, &dataSize, &ts, status);

    if (*status != 0)
        return;

    uint64_t timeNow = HAL_GetFPGATime(status) / 1000;
    if (timeNow - timeoutMs > ts) {
        *status = 1;
        return;
    }
    *length = dataSize;
    *receivedTimestamp = ts;
}

} // extern "C"
