#include <hal/Power.h>

extern "C" {

double HAL_GetVinVoltage(int32_t* status)
{
    *status = 0;
    return 15;
}

double HAL_GetVinCurrent(int32_t* status)
{
    *status = 0;
    return 1;
}

double HAL_GetUserVoltage6V(int32_t* status)
{
    *status = 0;
    return 6;
}

double HAL_GetUserCurrent6V(int32_t* status)
{
    *status = 0;
    return 0.5;
}

HAL_Bool HAL_GetUserActive6V(int32_t* status)
{
    *status = 0;
    return true;
}

int32_t HAL_GetUserCurrentFaults6V(int32_t* status)
{
    *status = 0;
    return 0;
}

double HAL_GetUserVoltage5V(int32_t* status)
{
    *status = 0;
    return 5;
}

double HAL_GetUserCurrent5V(int32_t* status)
{
    *status = 0;
    return 0.5;
}

HAL_Bool HAL_GetUserActive5V(int32_t* status)
{
    *status = 0;
    return true;
}

int32_t HAL_GetUserCurrentFaults5V(int32_t* status)
{
    *status = 0;
    return 0;
}

double HAL_GetUserVoltage3V3(int32_t* status)
{
    *status = 0;
    return 3.3;
}

double HAL_GetUserCurrent3V3(int32_t* status)
{
    *status = 0;
    return 0.5;
}

HAL_Bool HAL_GetUserActive3V3(int32_t* status)
{
    *status = 0;
    return true;
}

int32_t HAL_GetUserCurrentFaults3V3(int32_t* status)
{
    *status = 0;
    return 0;
}

double brownout = 8;

void HAL_SetBrownoutVoltage(double voltage, int32_t* status)
{
    *status = 0;
    brownout = voltage;
}

double HAL_GetBrownoutVoltage(int32_t* status)
{
    *status = 0;
    return brownout;
}

} // extern "C"
