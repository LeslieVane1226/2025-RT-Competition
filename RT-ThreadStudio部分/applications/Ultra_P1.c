#include <stdio.h>
#include <board.h>
#include <rtthread.h>
#include <rtdevice.h>

#define PIN_LED_B       GET_PIN(F, 12)
#define PIN_TRIG        GET_PIN(A, 5)

/* 振动马达 PWM 设备及通道 */
#define PWM1_DEV_NAME   "pwm1"
#define PWM_CHANNEL     4

/* 超声波测距定时器 */
TIM_HandleTypeDef htim3;

/* 信号量与线程 */
static rt_sem_t    ultrasonic_sem = RT_NULL;
static rt_thread_t ultrasonic_tid = RT_NULL;

/* PWM 设备句柄 */
static struct rt_device_pwm *pwm_dev1 = RT_NULL;

/* PWM 参数：高频振动 20 kHz，50% 占空比 */
static const rt_uint32_t vib_period = 50000;  /* ns，20 kHz */
static const rt_uint32_t vib_pulse  = 40000;  /* ns，占空比 50% */

/* -------- 前置声明 -------- */
static void MX_TIM3_Init(void);
static void thread_entry(void *parameter);

/* ========= TIM3 初始化（与原 Ultra.c 一致） ========= */
static void MX_TIM3_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    TIM_ClockConfigTypeDef  sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig      = {0};
    TIM_IC_InitTypeDef      sConfigIC          = {0};

    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 83;
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 65535;
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&htim3);

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig);
    HAL_TIM_IC_Init(&htim3);

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig);

    sConfigIC.ICPolarity  = TIM_INPUTCHANNELPOLARITY_RISING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    sConfigIC.ICFilter    = 0;
    HAL_TIM_IC_ConfigChannel(&htim3, &sConfigIC, TIM_CHANNEL_1);

    HAL_TIM_Base_Start_IT(&htim3);
    HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_1);
}

/* ========== 系统初始化：超声波 + PWM1 自动启动 ========== */
int SR04_int(void)
{
    /* 配置超声波 TRIG 和 LED 引脚 */
    rt_pin_mode(PIN_LED_B, PIN_MODE_OUTPUT);
    rt_pin_mode(PIN_TRIG,  PIN_MODE_OUTPUT);
    rt_pin_write(PIN_LED_B, PIN_HIGH);
    rt_pin_write(PIN_TRIG,  PIN_LOW);

    /* 初始化 PWM1 设备，初始关闭振动 */
    pwm_dev1 = (struct rt_device_pwm *)rt_device_find(PWM1_DEV_NAME);
    if (pwm_dev1 == RT_NULL)
    {
        rt_kprintf("ERROR: can't find %s device\n", PWM1_DEV_NAME);
        return -RT_ERROR;
    }
    rt_pwm_set(pwm_dev1, PWM_CHANNEL, vib_period, 0);
    rt_pwm_disable(pwm_dev1, PWM_CHANNEL);

    /* 初始化定时器与信号量 */
    MX_TIM3_Init();
    ultrasonic_sem = rt_sem_create("usem", 0, RT_IPC_FLAG_PRIO);
    if (ultrasonic_sem == RT_NULL)
    {
        rt_kprintf("create ultrasonic semaphore failed.\n");
        return -RT_ERROR;
    }

    /* 创建并启动测距线程 */
    ultrasonic_tid = rt_thread_create("ultrasonic",
                                      thread_entry, RT_NULL,
                                      1024, 25, 10);
    if (ultrasonic_tid != RT_NULL)
        rt_thread_startup(ultrasonic_tid);
    else
        rt_kprintf("ultrasonic thread create failed.\n");

    return 0;
}
INIT_APP_EXPORT(SR04_int);

/* ============= 捕获回调 ============= */
static uint8_t  TIM3_CH1_Edge = 0;
static uint32_t TIM3_CH1_VAL  = 0;
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM3) return;

    if (TIM3_CH1_Edge == 0)
    {
        TIM3_CH1_Edge++;
        __HAL_TIM_SET_CAPTUREPOLARITY(&htim3, TIM_CHANNEL_1, TIM_ICPOLARITY_FALLING);
        __HAL_TIM_SET_COUNTER(&htim3, 0);
    }
    else
    {
        HAL_TIM_IC_Stop_IT(&htim3, TIM_CHANNEL_1);
        TIM3_CH1_Edge++;
        TIM3_CH1_VAL = HAL_TIM_ReadCapturedValue(&htim3, TIM_CHANNEL_1);
        __HAL_TIM_SET_CAPTUREPOLARITY(&htim3, TIM_CHANNEL_1, TIM_ICPOLARITY_RISING);
    }
}

/* ============= 测距线程 ============= */
static void thread_entry(void *parameter)
{
    uint8_t below_cnt = 0;  /* 连续小于100cm计数 */
    float   distance;

    while (1)
    {
        /* 触发超声测距 */
        rt_pin_write(PIN_TRIG, PIN_HIGH);
        rt_hw_us_delay(10);
        rt_pin_write(PIN_TRIG, PIN_LOW);

        /* 简单闪烁指示 */
        rt_pin_write(PIN_LED_B, PIN_HIGH);
        rt_thread_delay(100);
        rt_pin_write(PIN_LED_B, PIN_LOW);
        rt_thread_delay(100);

        /* 计算距离 */
        if (TIM3_CH1_Edge == 2)
        {
            TIM3_CH1_Edge = 0;
            uint32_t time_us = TIM3_CH1_VAL;
            distance = time_us * 342.62f / 2.0f / 10000.0f;
            if (distance > 450) distance = 450;
            printf("Dis1: %.2f cm\n", distance);
            HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_1);
        }
        else
        {
            printf("Ultrasonic1 module error\n");
            rt_thread_delay(5000);
            continue;
        }

        /* —— 连续五次距离<100cm才开启振动 —— */
        if (distance < 100.0f)
        {
            below_cnt++;
            if (below_cnt >= 5)
            {
                rt_pwm_set(pwm_dev1, PWM_CHANNEL, vib_period, vib_pulse);
                rt_pwm_enable(pwm_dev1, PWM_CHANNEL);
            }
        }
        else
        {
            /* 重置计数并关闭振动 */
            below_cnt = 0;
            rt_pwm_disable(pwm_dev1, PWM_CHANNEL);
        }

        /* 可选：通过信号量退出线程 */
        if (rt_sem_take(ultrasonic_sem, 5) == RT_EOK) break;
    }
}

/* ========== Shell 控制（可选） ========== */
void ultrasonic(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: ultrasonic <pause|run>\n");
        return;
    }

    if (!rt_strcmp(argv[1], "pause"))
    {
        rt_sem_release(ultrasonic_sem);
        ultrasonic_tid = RT_NULL;
    }
    else if (!rt_strcmp(argv[1], "run"))
    {
        printf("Thread already running (auto-started at boot).\n");
    }
    else
    {
        printf("Usage: ultrasonic <pause|run>\n");
    }
}
MSH_CMD_EXPORT(ultrasonic, Control ultrasonic ranging thread);
