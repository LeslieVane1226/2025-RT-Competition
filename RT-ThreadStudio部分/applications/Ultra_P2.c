#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "stm32f4xx.h"    /* 或者你 MCU 对应的 HAL 头 */

/* 超声波 TRIG、ECHO 引脚 */
#define PIN_TRIG GET_PIN(A, 7)
#define PIN_ECHO GET_PIN(A, 4)

/* --- PWM2 ----------------------------------------------------------------- */
#define PWM2_DEV_NAME   "pwm2"
#define PWM_CHANNEL     4
static struct rt_device_pwm *pwm_dev2 = RT_NULL;
/* 20 kHz，50 % 占空比 */
static const rt_uint32_t vib_period = 50000;   /* ns */
static const rt_uint32_t vib_pulse  = 40000;   /* ns */
/* -------------------------------------------------------------------------- */

/* ------ DWT 微秒级时间戳 ------ */
/* 使能 DWT CYCCNT */
static void DWT_Delay_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL    |= DWT_CTRL_CYCCNTENA_Msk;
}
/* 返回自 DWT 使能以来的周期数除以 MHz 得到微秒 */
static uint32_t micros(void)
{
    return DWT->CYCCNT / (SystemCoreClock / 1000000U);
}

/* ----- pulseIn 实现 ----- */
/* 在 pin 上等待 level，然后测量保持该 level 的时间（单位 µs），超时返回 timeout */
static uint32_t pulseIn(rt_base_t pin, rt_uint8_t level, uint32_t timeout)
{
    uint32_t t_start = micros();
    /* 等待引脚变为目标电平 */
    while (rt_pin_read(pin) != level)
    {
        if ((micros() - t_start) > timeout)
            return timeout;
    }
    /* 开始计时 */
    uint32_t t0 = micros();
    /* 等待引脚离开该电平 */
    while (rt_pin_read(pin) == level)
    {
        if ((micros() - t0) > timeout)
            break;
    }
    return (micros() - t0);
}

/* ===== 超声波线程 ===== */
static void ultrasonic_thread(void *parameter)
{
    /* 初始化 DWT */
    DWT_Delay_Init();

    /* --- PWM2 连续计数 --- */
    uint8_t below_cnt = 0;
    /* -------------------- */

    while (1)
    {
        /* 1) 发送 15 µs 触发脉冲 */
        rt_pin_write(PIN_TRIG, PIN_HIGH);
        rt_hw_us_delay(15);
        rt_pin_write(PIN_TRIG, PIN_LOW);

        /* 2) 测量高电平宽度，超时 38000 µs */
        uint32_t Time = pulseIn(PIN_ECHO, PIN_HIGH, 38000);

        /* 3) 按照 Arduino 代码逻辑打印 */
        if (Time >= 38000)
        {
            rt_kprintf("Error2\n");
            /* --- 超时立即停振并清零计数 --- */
            below_cnt = 0;
            rt_pwm_disable(pwm_dev2, PWM_CHANNEL);
        }
        else
        {
            float Dis = 17.0f * Time / 1000.0f;
            int  cm   = (int)Dis;
            int  frac = (int)((Dis - cm) * 100);
            rt_kprintf("Dis2: %d.%02d cm\n", cm, frac);

            /* --- 距离逻辑：连续 5 次 < 50 cm 才振动 --- */
            if (Dis < 50.0f)
            {
                if (++below_cnt >= 5)
                {
                    rt_pwm_set(pwm_dev2, PWM_CHANNEL, vib_period, vib_pulse);
                    rt_pwm_enable(pwm_dev2, PWM_CHANNEL);
                }
            }
            else
            {
                below_cnt = 0;
                rt_pwm_disable(pwm_dev2, PWM_CHANNEL);
            }
            /* ------------------------------------------------ */
        }

        /* 4) 等 500 ms 再测 */
        rt_thread_mdelay(300);
    }
}

/* ===== 系统初始化 ===== */
static int ultrasonic_start(void)
{
    /* 配置 TRIG 输出、ECHO 输入 */
    rt_pin_mode(PIN_TRIG, PIN_MODE_OUTPUT);
    rt_pin_mode(PIN_ECHO, PIN_MODE_INPUT);

    /* 确保 Trigger 低电平 */
    rt_pin_write(PIN_TRIG, PIN_LOW);

    /* --- PWM2 初始化，默认关闭 -------------------------------------------- */
    pwm_dev2 = (struct rt_device_pwm *)rt_device_find(PWM2_DEV_NAME);
    if (pwm_dev2 == RT_NULL)
    {
        rt_kprintf("ERROR: can't find %s device\n", PWM2_DEV_NAME);
        return -RT_ERROR;
    }
    rt_pwm_set(pwm_dev2, PWM_CHANNEL, vib_period, 0);
    rt_pwm_disable(pwm_dev2, PWM_CHANNEL);
    /* ---------------------------------------------------------------------- */

    /* 启动测距线程 */
    rt_thread_t tid = rt_thread_create("ultra",
                                       ultrasonic_thread,
                                       RT_NULL,
                                       512,
                                       20,
                                       10);
    if (tid)
        rt_thread_startup(tid);
    else
        rt_kprintf("ultrasonic thread create failed\n");

    return 0;
}
/* 开机自动运行 */
INIT_APP_EXPORT(ultrasonic_start);
