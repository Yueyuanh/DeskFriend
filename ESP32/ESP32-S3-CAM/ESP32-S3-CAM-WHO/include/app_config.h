#pragma once

// Wi-Fi credentials are kept out of the repository. Copy
// include/wifi_secrets.example.h to include/wifi_secrets.h and fill it in;
// the real file is git-ignored.
#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#else
#define DF_WIFI_SSID ""
#define DF_WIFI_PASSWORD ""
#endif

// Windows mobile hotspot commonly uses 192.168.137.0/24.
#define DF_WIFI_STATIC_IP "192.168.137.50"
#define DF_WIFI_GATEWAY "192.168.137.1"
#define DF_WIFI_NETMASK "255.255.255.0"

// Camera wiring for the Goouuu ESP32-S3-CAM + OV5640 reference design.
// Capture parameters (XCLK, frame size, quality, fb_count) live in
// src/vision/camera_manager.cpp next to the camera_config_t they configure.
#define DF_CAM_PWDN -1
#define DF_CAM_RESET -1
#define DF_CAM_XCLK 15
#define DF_CAM_SIOD 4
#define DF_CAM_SIOC 5
#define DF_CAM_D0 11
#define DF_CAM_D1 9
#define DF_CAM_D2 8
#define DF_CAM_D3 10
#define DF_CAM_D4 12
#define DF_CAM_D5 18
#define DF_CAM_D6 17
#define DF_CAM_D7 16
#define DF_CAM_VSYNC 6
#define DF_CAM_HREF 7
#define DF_CAM_PCLK 13
