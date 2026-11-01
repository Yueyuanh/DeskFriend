#pragma once

// Camera diagnostic mode. Keep 0 for the normal JPEG pipeline.
//   0 = normal business path (JPEG/QVGA/fb_count 3)
//   1 = experiment A: minimal RGB565 capture (QQVGA, 1 buffer)
//   2 = experiment B: JPEG mode + sensor register readback
// Every experiment runs with a single build switch; set it back to 0 and
// rebuild to restore the shipped behaviour.
#ifndef DF_CAMERA_DIAG
#define DF_CAMERA_DIAG 0
#endif
