#pragma once
#define LV_CONF_H
#ifndef __ASSEMBLY__
#include <string.h> // ThorVG 9.4 uses memcpy/strchr through the LVGL config.
#endif

// One owner task calls LVGL. Canvas/vector allocations use PSRAM, not task stacks.
#define LV_COLOR_DEPTH 16
#define LV_USE_OS LV_OS_NONE
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CUSTOM
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
#define LV_DEF_REFR_PERIOD 33
#define LV_USE_FLOAT 1
#define LV_USE_MATRIX 1
#define LV_USE_VECTOR_GRAPHIC 1
#define LV_USE_THORVG_INTERNAL 1
#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_DRAW_UNIT_CNT 1
#define LV_DRAW_THREAD_STACK_SIZE (32 * 1024)
#define LV_USE_TINY_TTF 1
#define LV_TINY_TTF_CACHE_GLYPH_CNT 160
#define LV_TINY_TTF_CACHE_KERNING_CNT 160
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_BUILD_EXAMPLES 0
#define LV_USE_DEMO_WIDGETS 0
#define LV_USE_DEMO_BENCHMARK 0
