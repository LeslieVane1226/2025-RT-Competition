#include <rtthread.h>
#include <rtdevice.h>
#include "lwgps/lwgps.h"

/* ======== 用户可按需修改的宏 ======== */
#define GPS_UART_NAME     "uart2"   /* 目标串口 */
#define BSP_UART2_TX_PIN  GET_PIN(A, 2)
#define BSP_UART2_RX_PIN  GET_PIN(A, 3)
#define GPS_BAUDRATE      38400     /* BK-220 出厂波特率 */

/* ======== 全局对象 ======== */
static lwgps_t gps;         /* NMEA 解析器实例 */
static char    rx_byte;     /* 单字节临时缓存 */

/* ---------- 串口 RX 中断回调 ---------- */
static rt_err_t uart_rx_ind(rt_device_t dev, rt_size_t size)
{
    /* 逐字节读取，保证把 FIFO 清空 */
    while (rt_device_read(dev, 0, &rx_byte, 1) == 1)
    {
        /* 喂给 lwgps；返回值 1 代表解析出一条完整句子 */
        if (lwgps_process(&gps, &rx_byte, 1))
        {
//            rt_kprintf("%c", rx_byte);//test


            if (gps.is_valid)          /* 已得到有效 RMC/GGA */
            {
                /* 速度：lwgps 新版有 speed_kph，旧版只有 speed(knots) */
#if defined(LWGPS_CFG_OUTPUT_SPEED_KPH) || defined(LWGPS_CFG_OUTPUT_SPEED_KMPH)
                float spd_kph = gps.speed_kph;
#else
                float spd_kph = gps.speed * 1.852f;    /* knots → km/h */
#endif
                rt_kprintf("Lat: %.6f  Lon: %.6f  Alt: %.1f m  "
                           "Sats: %d  Speed: %.2f km/h\r\n",
                           gps.latitude, gps.longitude, gps.altitude,
                           gps.sats_in_use, spd_kph);
            }
        }
    }
    return RT_EOK;
}

/* ---------- 后台线程：打开串口并绑定回调 ---------- */
static void gps_thread(void *parameter)
{
    rt_device_t uart = rt_device_find(GPS_UART_NAME);
    if (uart == RT_NULL)
    {
        rt_kprintf("❌ Cannot find %s\r\n", GPS_UART_NAME);
        return;
    }

    /* 1. 打开串口，开启 RX 中断 */
    rt_device_open(uart,
                   RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX);

    /* 2. 配置波特率 */
    rt_device_control(uart,
                      RT_DEVICE_CTRL_CONFIG,
                      (void *)GPS_BAUDRATE);

    /* 3. 注册回调 */
    rt_device_set_rx_indicate(uart, uart_rx_ind);
    rt_kprintf("✅ %s opened at %d bps, waiting for GPS data...\r\n",
               GPS_UART_NAME, GPS_BAUDRATE);

    /* 主线程保持存活，可扩展其它逻辑 */
    while (1)
    {
        rt_thread_mdelay(1000);
        /* 此处可输出时间、喂狗等 */
    }
}

/* ---------- 初始化函数：系统启动时自动执行 ---------- */
static int gps_demo_init(void)
{
    /* 初始化解析器（清零内部状态机） */
    lwgps_init(&gps);

    /* 创建后台线程 */
    rt_thread_t tid = rt_thread_create("gps",
                                       gps_thread, RT_NULL,
                                       2048,        /* 栈 2 KB */
                                       22,          /* 优先级 */
                                       10);         /* 时间片 */
    if (tid)
        rt_thread_startup(tid);
    else
        rt_kprintf("❌ Failed to create gps thread\r\n");

    return 0;
}
/* 挂到 application 初始化阶段，比 main() 更早执行 */
INIT_APP_EXPORT(gps_demo_init);
