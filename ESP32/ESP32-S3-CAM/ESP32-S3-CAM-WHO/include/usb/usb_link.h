#pragma once

#include "esp_err.h"

// CDC ACM side channel of the composite USB device.
//
// Device -> PC: one JSON line every 500 ms with chip temperature, frame
// geometry, stream frame rate, face boxes and the recognition result.
// PC -> device: one text command per line. Every received line is logged on
// the UART console (that is the data-link check) and answered with "ACK ...".
//
// Must be called after uvc_camera_start(): TinyUSB is initialized there.
esp_err_t usb_link_start();

// True while a host has the CDC device open, i.e. the desktop client (or any
// serial terminal) is attached. The status LED shows solid blue in that state.
bool usb_link_connected();
