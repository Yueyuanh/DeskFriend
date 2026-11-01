#pragma once
#include <Arduino.h>


class CamTask_t
{
private:
    // void initPeripheral();

public:
    CamTask_t();
    int  cameraSetup(void);
    void run(void *pvParameters);
    static void runTask(void *pvParameters);
};

extern CamTask_t camTaskInstance;