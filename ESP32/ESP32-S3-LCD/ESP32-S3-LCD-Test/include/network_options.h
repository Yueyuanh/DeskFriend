#pragma once
#include <IPAddress.h>
// Windows mobile hotspot / Internet Connection Sharing uses 192.168.137.0/24.
// Keep credentials in ignored network_config.h; only addressing lives here.
static const IPAddress kDeviceIp(192,168,137,73);
static const IPAddress kHotspotGateway(192,168,137,1);
static const IPAddress kHotspotMask(255,255,255,0);
