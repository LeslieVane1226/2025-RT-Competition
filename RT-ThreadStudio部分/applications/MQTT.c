#include "rtthread.h"
#include "dev_sign_api.h"
#include "mqtt_api.h"

extern volatile int   g_def_warning;     /* <<< 来自 MPU6050 */
extern volatile float g_lat, g_lon; /* <<< 来自 BK220 */

#define WIFI_SSID       "LeslieVane"        /* --- WiFi --- */
#define WIFI_PASSWORD   "wenruoyu"          /* --- WiFi --- */
#define WIFI_RETRY_MAX  5                   /* --- WiFi --- */

/* 若工程未包含 wlan_mgnt.h，可手动 extern 需要的函数 */    /* --- WiFi --- */
extern int rt_wlan_connect(const char *ssid, const char *password);    /* --- WiFi --- */
extern int rt_wlan_is_ready(void);                                     /* --- WiFi --- */

char DEMO_PRODUCT_KEY[IOTX_PRODUCT_KEY_LEN + 1]   = {0};
char DEMO_DEVICE_NAME[IOTX_DEVICE_NAME_LEN + 1]   = {0};
char DEMO_DEVICE_SECRET[IOTX_DEVICE_SECRET_LEN + 1] = {0};

void *HAL_Malloc(uint32_t size);
void  HAL_Free(void *ptr);
void  HAL_Printf(const char *fmt, ...);
int   HAL_GetProductKey(char product_key[IOTX_PRODUCT_KEY_LEN + 1]);
int   HAL_GetDeviceName(char device_name[IOTX_DEVICE_NAME_LEN + 1]);
int   HAL_GetDeviceSecret(char device_secret[IOTX_DEVICE_SECRET_LEN]);
uint64_t HAL_UptimeMs(void);
int   HAL_Snprintf(char *str, const int len, const char *fmt, ...);

#define EXAMPLE_TRACE(fmt, ...)                         \
    do {                                                \
        HAL_Printf("%s|%03d :: ", __func__, __LINE__);  \
        HAL_Printf(fmt, ##__VA_ARGS__);                 \
        HAL_Printf("%s", "\r\n");                       \
    } while (0)

/* ------------------------- 下行消息回调 ------------------------- */
static void example_message_arrive(void *pcontext,
                                   void *pclient,
                                   iotx_mqtt_event_msg_pt msg)
{
    iotx_mqtt_topic_info_pt info = msg->msg;

    if (msg->event_type == IOTX_MQTT_EVENT_PUBLISH_RECEIVED)
    {
        EXAMPLE_TRACE("Message Arrived:");
        EXAMPLE_TRACE("Topic  : %.*s", info->topic_len, info->ptopic);
        EXAMPLE_TRACE("Payload: %.*s", info->payload_len, info->payload);
        EXAMPLE_TRACE("\n");
    }
}

/* ------------------------- 订阅 /user/get ------------------------- */
static int example_subscribe(void *handle)
{
    const char *fmt = "/%s/%s/user/get";
    int   topic_len = strlen(fmt) + strlen(DEMO_PRODUCT_KEY)
                    + strlen(DEMO_DEVICE_NAME) + 1;
    char *topic = HAL_Malloc(topic_len);
    if (!topic)
    {
        EXAMPLE_TRACE("memory not enough");
        return -1;
    }

    HAL_Snprintf(topic, topic_len, fmt,
                 DEMO_PRODUCT_KEY, DEMO_DEVICE_NAME);

    int res = IOT_MQTT_Subscribe(handle, topic,
                                 IOTX_MQTT_QOS0,
                                 example_message_arrive,
                                 NULL);
    if (res < 0)
        EXAMPLE_TRACE("subscribe failed");

    HAL_Free(topic);
    return res;
}

/* ------------------------- 发布 /user/update ------------------------- */
static int example_publish(void *handle)
{
    const char *fmt_topic = "/%s/%s/user/get";
    char topic[128];
    HAL_Snprintf(topic, sizeof(topic), fmt_topic,
                 DEMO_PRODUCT_KEY, DEMO_DEVICE_NAME);

    /* ---------- 处理经纬度：可能是度，可能是度*1e6 ---------- */
    float lat = g_lat;
    float lon = g_lon;

    /* 若大于 1800 说明被放大了 1e6；(1800 = 1.8e3) */
    if (lat > 1800.0f || lat < -1800.0f)
        lat = lat / 1000000.0f;
    if (lon > 1800.0f || lon < -1800.0f)
        lon = lon / 1000000.0f;

    /* 把 float 拆分成 “整数 + 6位小数”，避免 %f */
    int lat_deg  = (int)lat;
    int lon_deg  = (int)lon;
    int lat_frac = (int)((lat - lat_deg) * 1000000);
    int lon_frac = (int)((lon - lon_deg) * 1000000);
    if (lat_frac < 0) lat_frac = -lat_frac;
    if (lon_frac < 0) lon_frac = -lon_frac;

    /* ---------- 组 JSON ---------- */
    char payload[160];
    rt_snprintf(payload, sizeof(payload),
                "{\"warn\":%d,\"lat\":%d.%06d,\"lon\":%d.%06d}",
                g_def_warning,
                lat_deg, lat_frac,
                lon_deg, lon_frac);

    return IOT_MQTT_Publish_Simple(0, topic,
                                   IOTX_MQTT_QOS0,
                                   payload, strlen(payload));
}


/* ------------------------- SDK 事件回调 ------------------------- */
static void example_event_handle(void *pcontext,
                                 void *pclient,
                                 iotx_mqtt_event_msg_pt msg)
{
    EXAMPLE_TRACE("msg->event_type : %d", msg->event_type);
}

/* ------------------------- 主函数 ------------------------- */
static int mqtt_example_main(int argc, char *argv[])
{
    void             *pclient = NULL;
    int               loop_cnt = 0;
    iotx_mqtt_param_t mqtt_params;

    /* >>>>>>>>>>>>>>>>>>>>>>>  Wi-Fi 自动连接  <<<<<<<<<<<<<<<<<<<<<< */
        int wifi_retry = 0;                                            /* --- WiFi --- */
        while (rt_wlan_connect(WIFI_SSID, WIFI_PASSWORD) != RT_EOK)    /* --- WiFi --- */
        {                                                              /* --- WiFi --- */
            if (++wifi_retry > WIFI_RETRY_MAX)                         /* --- WiFi --- */
            {                                                          /* --- WiFi --- */
                rt_kprintf("[WiFi] connect fail\n");                   /* --- WiFi --- */
                return -1;                                             /* --- WiFi --- */
            }                                                          /* --- WiFi --- */
            rt_thread_mdelay(1000);                                    /* --- WiFi --- */
        }                                                              /* --- WiFi --- */
        while (!rt_wlan_is_ready())                                    /* --- WiFi --- */
            rt_thread_mdelay(100);                                     /* --- WiFi --- */
        rt_kprintf("[WiFi] connected, got IP\n");
        rt_thread_mdelay(7000);
        /* --- WiFi --- */
    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */

    /* 获取三元组 */
    HAL_GetProductKey(DEMO_PRODUCT_KEY);
    HAL_GetDeviceName(DEMO_DEVICE_NAME);
    HAL_GetDeviceSecret(DEMO_DEVICE_SECRET);

    EXAMPLE_TRACE("mqtt example start");

    memset(&mqtt_params, 0, sizeof(mqtt_params));
    mqtt_params.handle_event.h_fp = example_event_handle;

    /* 建立 MQTT 连接 */
    pclient = IOT_MQTT_Construct(&mqtt_params);
    if (!pclient)
    {
        EXAMPLE_TRACE("MQTT construct failed");
        return -1;
    }

    /* 订阅下行 Topic */
    if (example_subscribe(pclient) < 0)
    {
        IOT_MQTT_Destroy(&pclient);
        return -1;
    }

    /* 主循环：每 20 次循环（约 4 秒）发布一次 */
    while (1)
    {
        if (loop_cnt % 20 == 0)
            example_publish(pclient);

        IOT_MQTT_Yield(pclient, 200);
        loop_cnt++;
    }
    return 0;
}

/* ========= 自动启动封装 ========= */
static int ali_mqtt_auto_start(void)
{
    /* 创建线程执行 mqtt_example_main，相当于你手动敲命令 */
    rt_thread_t tid = rt_thread_create("ali_mqtt",
                                       (void (*)(void *))mqtt_example_main,
                                       RT_NULL,          /* argc/argv 都传 NULL */
                                       4096, 20, 10);
    if (tid)
        rt_thread_startup(tid);
    else
        rt_kprintf("ali_mqtt thread create fail\n");

    return 0;   /* INIT_ 宏要求返回 0 */
}
INIT_APP_EXPORT(ali_mqtt_auto_start);   /* ↑ 仅启动线程，马上返回 */

/* -------- Shell 手动触发保持不变 -------- */
#ifdef FINSH_USING_MSH
MSH_CMD_EXPORT_ALIAS(mqtt_example_main, ali_mqtt_sample, ali mqtt sample);
#endif

