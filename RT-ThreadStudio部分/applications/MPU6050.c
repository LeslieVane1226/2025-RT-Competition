#include <rtthread.h>
#include "mpu6xxx.h"

#define MPU6XXX_DEVICE_NAME  "i2c1"

volatile int g_def_warning = 0;      /* 0/1 供 MQTT 上传 */
volatile int trig = 0;

/* ---------- 线程入口 ---------- */
static void mpu6050_thread(void *parameter)
{
    struct mpu6xxx_device *dev;
    struct mpu6xxx_3axes   accel, gyro;

    dev = mpu6xxx_init(MPU6XXX_DEVICE_NAME, RT_NULL);
    if (!dev)
    {
        rt_kprintf("mpu6xxx init failed\n");
        return;                 /* 线程直接退出 */
    }
    rt_kprintf("mpu6xxx init succeed\n");

    /* <-- 修改：连续低计数 & 状态标志 */
    uint8_t low_cnt   = 0;
    rt_bool_t falling = RT_FALSE;

    while (1)
    {
        mpu6xxx_get_accel(dev, &accel);
        mpu6xxx_get_gyro(dev,  &gyro);

        /* 判断 Z 轴加速度 */
        if (accel.z < 500)
        {
            if (low_cnt < 3) low_cnt++;      /* 累计不超过 3 */
            if (low_cnt >= 3) falling = RT_TRUE;  /* 连续三次 → 跌倒状态 */
        }
        else
        {
            /* 只要一次 ≥500 就认为已恢复，计数清零 */
            low_cnt = 0;                     /* <-- 修改：这里才清零 */
            falling = RT_FALSE;
        }

        /* 输出状态 */
        if (falling)
        {
            rt_kprintf("Warning!! Falling Down Detected\n");
            g_def_warning = 1;
            trig = 1;
            rt_kprintf("def_warning = %d\n", g_def_warning);
        }
        else
        {
            rt_kprintf("No Falling\n");
            g_def_warning = 0;
            trig = 0;
            rt_kprintf("def_warning = %d\n", g_def_warning);
        }
        /* 传感器原始数据 */
        rt_kprintf("MPU  Ax=%4d Ay=%4d Az=%4d  "
                   "Gx=%4d Gy=%4d Gz=%4d\n",
                   accel.x, accel.y, accel.z,
                   gyro.x,  gyro.y,  gyro.z);

        rt_thread_mdelay(1000);
    }
}

/* ---------- 只做线程创建 ---------- */
static int mpu6050_app_init(void)
{
    rt_thread_t tid = rt_thread_create("mpu6050",
                                       mpu6050_thread, RT_NULL,
                                       1024, 25, 10);
    if (tid) rt_thread_startup(tid);
    return 0;
}
INIT_APP_EXPORT(mpu6050_app_init);
