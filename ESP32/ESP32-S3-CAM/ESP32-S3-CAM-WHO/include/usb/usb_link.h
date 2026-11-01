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
