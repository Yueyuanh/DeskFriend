#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>
#include <freertos/FreeRTOS.h>

class ServoTask_t {
public:
  void begin();
  void run(void *pvParameters);
  static void runTask(void *pvParameters);

private:
  Servo servo;
  Servo yawServo;
};

extern ServoTask_t servoTaskInstance;
extern TaskHandle_t servoTaskHandle;
extern volatile uint8_t servoPitchAngle;
extern volatile uint8_t servoYawAngle;
