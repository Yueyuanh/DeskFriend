idf_component_register(
    SRCS "main.cpp"
         "core/app_state.cpp"
         "core/tuning.cpp"
         "board/led_status.cpp"
         "board/temp_monitor.cpp"
         "net/wifi_manager.cpp"
         "vision/camera_manager.cpp"
         "vision/face_recognizer.cpp"
         "vision/vision_task.cpp"
         "web/web_task.cpp"
         "usb/uvc_camera.cpp"
         "usb/usb_link.cpp"
    INCLUDE_DIRS "." "../include" "web"
    REQUIRES esp32-camera human_face_recognition
             esp_http_server esp_netif esp_event esp_wifi nvs_flash esp_timer
             esp_driver_rmt esp_driver_gpio esp_driver_tsens
             usb_device_uvc espressif__tinyusb
)
