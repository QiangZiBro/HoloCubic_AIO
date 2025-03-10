#ifndef INTERFACE_H
#define INTERFACE_H

enum APP_EVENT
{
    APP_EVENT_WIFI_CONN = 0, // 开启连接
    APP_EVENT_WIFI_AP,       // 开启AP事件
    APP_EVENT_WIFI_ALIVE,    // wifi开关的心跳维持
    APP_EVENT_WIFI_DISCONN,  // 连接断开
    APP_EVENT_UPDATE_TIME,
    APP_EVENT_NONE
};

class AppController;
struct Imu_Action;

struct APP_OBJ
{
    const char *app_name;  // 应用程序名称 及title
    const void *app_image; // APP的图片存放地址    APP应用图标 128*128
    void (*app_init)();    // APP的初始化函数 也可以为空或什么都不做（作用等效于arduino setup()函数）
    void (*main_process)(AppController *sys,
                         const Imu_Action *act_info); // APP的主程序函数入口指针
    void (*exit_callback)();                          // 退出之前需要处理的回调函数 可为空
    void (*on_event)(APP_EVENT event, int event_id);  // 事件通知
};

#endif