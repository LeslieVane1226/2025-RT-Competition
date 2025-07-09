/*
 * PD11 高电平触发蜂鸣器一次示例
 * 当 PD11 变为高电平时，蜂鸣器“叮”一声（100 ms），变为低电平时保持静音
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>

#define DBG_TAG "gpio_beep"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

/* 输入和蜂鸣器引脚 */
#define PIN_IN    GET_PIN(D, 11)   /* PD11 作为触发输入 */
#define PIN_BEEP  GET_PIN(B, 0)    /* PB0 作为蜂鸣器输出 */

/* 程序入口 */
int beep_OpenMV(void)
{
    /* 配置引脚模式 */
    rt_pin_mode(PIN_IN,   PIN_MODE_INPUT_PULLDOWN);  /* 下拉保持低电平空闲 */
    rt_pin_mode(PIN_BEEP, PIN_MODE_OUTPUT);
    rt_pin_write(PIN_BEEP, PIN_LOW);

    LOG_I("GPIO Beep Demo start. Waiting for PD11 risifng edge...");

    while (1)
    {
        /* 轮询检测 PD11 上升沿 */
        if (rt_pin_read(PIN_IN) == PIN_HIGH)
        {
            /* 发出一次蜂鸣（100 ms 高电平） */
            rt_pin_write(PIN_BEEP, PIN_HIGH);
            rt_kprintf("Taking a Picture\n");
            rt_thread_mdelay(500);
            rt_pin_write(PIN_BEEP, PIN_LOW);

            /* 等待 PD11 回到低电平，避免连续触发 */
            while (rt_pin_read(PIN_IN) == PIN_HIGH)
            {
                rt_thread_mdelay(10);
            }
        }

        /* 每 50 ms 再次检测 */
        rt_thread_mdelay(50);
    }

    return 0;
}

INIT_APP_EXPORT(beep_OpenMV);
