idf_component_register(
    SRCS "main.cpp"
         "app_state.cpp"
         "camera_manager.cpp"
         "led_status.cpp"
         "temp_monitor.cpp"
         "wifi_manager.cpp"
         "vision_task.cpp"
         "web_task.cpp"
    INCLUDE_DIRS "." "../include"
    REQUIRES esp32-camera pedestrian_detect human_face_recognition
             esp_http_server esp_netif esp_event esp_wifi nvs_flash esp_timer
             esp_driver_rmt esp_driver_gpio esp_driver_tsens
)
