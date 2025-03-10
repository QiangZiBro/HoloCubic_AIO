#include "app_contorller.h"
#include "app_contorller_gui.h"
#include "common.h"
#include "interface.h"
#include "Arduino.h"

AppController::AppController()
{
    app_num = 0;
    app_exit_flag = 0;
    cur_app_index = 0;
    pre_app_index = 0;
    appList = new APP_OBJ[APP_MAX_NUM];
    m_app_req = NULL;
    m_app_event_type = APP_EVENT_NONE;
    m_wifi_status = false;
    m_preWifiReqMillis = millis();
    // 设置CPU主频
    setCpuFrequencyMhz(80);
    uint32_t freq = getCpuFrequencyMhz(); // In MHz
    // uint32_t freq = getXtalFrequencyMhz(); // In MHz
    Serial.print(F("getCpuFrequencyMhz: "));
    Serial.println(freq);

    app_control_gui_init();
    appList[0].app_image = &app_loading;
    appList[0].app_name = "None";
    app_contorl_display_scr(appList[cur_app_index].app_image,
                            appList[cur_app_index].app_name,
                            LV_SCR_LOAD_ANIM_NONE, true);
    // Display();
}

void AppController::Display()
{
    // appList[0].app_image = &app_loading;
    app_contorl_display_scr(appList[cur_app_index].app_image,
                            appList[cur_app_index].app_name,
                            LV_SCR_LOAD_ANIM_NONE, true);
}

AppController::~AppController()
{
    rgb_thread_del();
}

int AppController::app_is_legal(const APP_OBJ *app_obj)
{
    // APP的合法性检测
    if (NULL == app_obj)
        return 1;
    if (APP_MAX_NUM <= app_num)
        return 2;
    return 0;
}

int AppController::app_register(const APP_OBJ *app) // 将APP注册到app_controller中
{
    int ret_code = app_is_legal(app);
    if (0 != ret_code)
    {
        return ret_code;
    }

    appList[app_num].app_name = app->app_name;
    appList[app_num].app_image = app->app_image;
    appList[app_num].app_init = app->app_init;
    appList[app_num].main_process = app->main_process;
    appList[app_num].exit_callback = app->exit_callback;
    appList[app_num].on_event = app->on_event;
    ++app_num;
    return 0; //注册成功
}

int AppController::app_unregister(const APP_OBJ *app) // 将APP从app_controller中去注册（删除）
{
    // todo
    return 0;
}

int AppController::main_process(Imu_Action *act_info)
{
    if (UNKNOWN != act_info->active)
    {
        Serial.print(F("act_info->active: "));
        Serial.println(act_info->active);
    }
    // 扫描事件
    req_event_deal();

    if (0 == app_exit_flag)
    {
        // 当前没有进入任何app
        lv_scr_load_anim_t anim_type = LV_SCR_LOAD_ANIM_NONE;
        if (TURN_RIGHT == act_info->active)
        {
            anim_type = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
            pre_app_index = cur_app_index;
            cur_app_index = (cur_app_index + 1) % app_num;
        }
        else if (TURN_LEFT == act_info->active)
        {
            anim_type = LV_SCR_LOAD_ANIM_MOVE_LEFT;
            pre_app_index = cur_app_index;
            // 以下等效与 processId = (processId - 1 + APP_NUM) % 4;
            // +3为了不让数据溢出成负数，而导致取模逻辑错误
            cur_app_index = (cur_app_index - 1 + app_num) % app_num; // 此处的3与p_processList的长度一致
        }
        else if (GO_FORWORD == act_info->active)
        {
            app_exit_flag = 1; // 进入app
            if (NULL != appList[cur_app_index].app_init)
            {
                (*(appList[cur_app_index].app_init))(); // 执行APP初始化
            }
        }

        if (GO_FORWORD != act_info->active) // && UNKNOWN != act_info->active
        {
            // uint32_t freq = getCpuFrequencyMhz(); // In MHz
            // Serial.print("getXtalFrequencyMhz: ");
            // Serial.println(freq);
            app_contorl_display_scr(appList[cur_app_index].app_image,
                                    appList[cur_app_index].app_name,
                                    anim_type, false);
            delay(300);
        }
    }
    else
    {
        // 运行APP进程 等效于把控制权交给当前APP
        (*(appList[cur_app_index].main_process))(this, act_info);
    }
    act_info->active = UNKNOWN;
    act_info->isValid = 0;
    return 0;
}

// 事件请求
int AppController::req_event(const APP_OBJ *from, APP_EVENT event, int event_id)
{
    // 更新事件的请求者
    m_app_req = from;
    m_app_event_id = event_id;
    m_app_event_type = event;
    switch (event)
    {
    case APP_EVENT_WIFI_CONN:
    {
        // 更新请求
        m_preWifiReqMillis = millis();
        /*** Read WiFi info in SD-Card, then scan & connect WiFi ***/
        g_network.start_conn_wifi(g_cfg.ssid.c_str(), g_cfg.password.c_str());
        m_wifi_status = true;
    }
    break;
    case APP_EVENT_WIFI_AP:
    {
        // 更新请求
        m_preWifiReqMillis = millis();
        g_network.open_ap(AP_SSID);
        m_wifi_status = true;
    }
    break;
    case APP_EVENT_WIFI_ALIVE:
    {
        // wifi开关的心跳 持续收到心跳 wifi才不会被关闭
        m_wifi_status = true;
        // 更新请求
        m_preWifiReqMillis = millis();
    }
    break;
    case APP_EVENT_WIFI_DISCONN:
    {
        g_network.close_wifi();
        m_wifi_status = false; // 标志位
    }
    break;
    case APP_EVENT_UPDATE_TIME:
    {
    }
    break;
    default:
        break;
    }
    return 0;
}

int AppController::req_event_deal(void)
{
    // wifi事件
    if (false == m_wifi_status)
    {
        return 0;
    }
    if (millis() - m_preWifiReqMillis > WIFI_LIFE_CYCLE)
    {
        g_network.close_wifi();
        m_wifi_status = false; // 标志位
        return true;
    }

    if (NULL == m_app_req || APP_EVENT_WIFI_ALIVE <= m_app_event_type)
        return 0;

    if ((WiFi.getMode() & WIFI_MODE_STA) == WIFI_MODE_STA && CONN_SUCC != g_network.end_conn_wifi())
    {
        // 在STA模式下 并且还没连接上wifi
        return 0;
    }
    // 事件回调
    (*(appList[cur_app_index].on_event))(m_app_event_type, m_app_event_id);
    // 取消回调标志
    m_app_req = NULL;
    m_app_event_id = 0;
    m_app_event_type = APP_EVENT_NONE;
    return 0;
}

void AppController::app_exit()
{
    app_exit_flag = 0; // 退出APP
    m_app_req = NULL;  // 事件对象赋空
    if (NULL != appList[cur_app_index].exit_callback)
    {
        // 执行APP退出回调
        (*(appList[cur_app_index].exit_callback))();
    }
    app_contorl_display_scr(appList[cur_app_index].app_image,
                            appList[cur_app_index].app_name,
                            LV_SCR_LOAD_ANIM_NONE, true);
    // 设置CPU主频
    setCpuFrequencyMhz(80);
}