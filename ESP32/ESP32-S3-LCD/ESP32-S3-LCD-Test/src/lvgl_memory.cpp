#include <lvgl.h>
#include <esp_heap_caps.h>
#include <string.h>

// LVGL, ThorVG and glyph caches may be large. Keep Wi-Fi's internal heap free.
extern "C" {
void lv_mem_init(void) {}
void lv_mem_deinit(void) {}
void* lv_malloc_core(size_t size) {
  // Path descriptors, glyph metadata and linked lists are traversed frequently.
  // Keep small allocations in SRAM; reserve PSRAM for image/raster buffers.
  if (size <= 4096 && heap_caps_get_free_size(MALLOC_CAP_INTERNAL) > 65536) {
    void* internal = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (internal) return internal;
  }
  void* result = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return result ? result : heap_caps_malloc(size, MALLOC_CAP_8BIT);
}
void lv_free_core(void* data) { heap_caps_free(data); }
void* lv_realloc_core(void* data, size_t size) {
  if (size <= 4096 && heap_caps_get_free_size(MALLOC_CAP_INTERNAL) > 65536) {
    void* internal = heap_caps_realloc(data, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (internal) return internal;
  }
  return heap_caps_realloc(data, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
lv_mem_pool_t lv_mem_add_pool(void*, size_t) { return nullptr; }
void lv_mem_remove_pool(lv_mem_pool_t) {}
lv_result_t lv_mem_test_core(void) { return LV_RESULT_OK; }
void lv_mem_monitor_core(lv_mem_monitor_t* value) {
  memset(value, 0, sizeof(*value));
  value->total_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  value->free_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  value->free_biggest_size = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
}
}
