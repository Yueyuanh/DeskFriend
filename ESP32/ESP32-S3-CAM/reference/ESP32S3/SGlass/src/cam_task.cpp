#include "cam_task.h"
#include "cam_pin_ov5640.h"
#include "esp_camera.h"

CamTask_t camTaskInstance;

CamTask_t::CamTask_t()
{

}



void CamTask_t::run(void *pvParameters)
{
    cameraSetup();
    while (true)
    {

    }
}

void CamTask_t::runTask(void *pvParameters)
{
    camTaskInstance.run(pvParameters);
}



// 摄像头设置函数
int CamTask_t::cameraSetup(void) {
  // 创建并初始化摄像头配置结构体
  camera_config_t config;
  // 配置摄像头数据引脚
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  // 配置时钟和同步引脚
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  // 配置I2C引脚
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  // 配置电源和复位引脚
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  // 设置时钟频率
  config.xclk_freq_hz = 20000000;
  // 设置图像尺寸
  config.frame_size = FRAMESIZE_UXGA;
  // 设置像素格式
  config.pixel_format = PIXFORMAT_JPEG; // JPEG格式用于流式传输
  // 设置帧缓冲区获取模式
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  // 设置帧缓冲区位置
  config.fb_location = CAMERA_FB_IN_PSRAM;
  // 设置JPEG质量
  config.jpeg_quality = 12;
  // 设置帧缓冲区数量
  config.fb_count = 1;
 
  // 如果检测到PSRAM，则使用更高的分辨率和JPEG质量
  if(psramFound()){
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    // 如果没有PSRAM，限制帧大小并使用DRAM
    config.frame_size = FRAMESIZE_SVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }
 
  // 初始化摄像头
  esp_err_t err = esp_camera_init(&config);
  // 检查摄像头是否初始化成功
  if (err != ESP_OK) {
    // 如果初始化失败，打印错误代码
    Serial.printf("Camera init failed with error 0x%x", err);
    return 0; // 返回0表示失败
  }
 
  // 获取传感器指针
  sensor_t * s = esp_camera_sensor_get();
  // 传感器初始设置：翻转图像、调整亮度和饱和度
  s->set_vflip(s, 1); // 翻转图像垂直方向
  s->set_brightness(s, 1); // 调整亮度
  s->set_saturation(s, 0); // lower the saturation
 
  Serial.println("Camera configuration complete!");
  return 1;
}