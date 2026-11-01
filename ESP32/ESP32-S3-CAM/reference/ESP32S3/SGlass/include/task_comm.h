#pragma once


// 摄像头状态
enum CAM_STATUS_e{
    CAM_READY=0,    // 正在准备
    CAM_OK,         // 初始化完成
    CAM_RESET,      // 重启
    CAM_BUSY,       // 繁忙/过热
    CAM_FAIL,       // 初始化失败
};

typedef struct
{
    /* data */
    CAM_STATUS_e cam_status;
}task_commu;
