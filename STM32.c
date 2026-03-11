/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Smart Industry Safety System
  *                   10 Second Delay ONLY for Backward Movement
  ******************************************************************************
  */

#include "main.h"
#include "lcd_i2c.h"
#include <stdio.h>
#include <string.h>

/*==============================================================
                         DEFINES
==============================================================*/

// Relay Pins (Active LOW)
#define RELAY1_PIN      GPIO_PIN_5
#define RELAY1_PORT     GPIOA
#define RELAY2_PIN      GPIO_PIN_6
#define RELAY2_PORT     GPIOA
#define RELAY3_PIN      GPIO_PIN_7
#define RELAY3_PORT     GPIOA
#define RELAY4_PIN      GPIO_PIN_6
#define RELAY4_PORT     GPIOB

// Buzzer
#define BUZZER_PIN      GPIO_PIN_8
#define BUZZER_PORT     GPIOA

// Ultrasonic
#define TRIG_PIN        GPIO_PIN_0
#define TRIG_PORT       GPIOB
#define ECHO_PIN        GPIO_PIN_1
#define ECHO_PORT       GPIOB

// Push Button
#define BUTTON_PIN      GPIO_PIN_13
#define BUTTON_PORT     GPIOC

// Distance Thresholds (10cm increments)
#define LEVEL_SAFE      40.0f   // > 40cm: All relays ON
#define LEVEL_ZONE1     30.0f   // 30-40cm: Relay 1 OFF
#define LEVEL_ZONE2     20.0f   // 20-30cm: Relay 1,2 OFF
#define LEVEL_ZONE3     10.0f   // 10-20cm: Relay 1,2,3 OFF
                                // < 10cm: All OFF (DANGER)

// Backward delay time (milliseconds)
#define BACKWARD_DELAY_MS   10000   // 10 seconds

// LCD
#define LCD_WIDTH       16
#define SCROLL_SPEED    180

/*==============================================================
                    WARNING MESSAGES
==============================================================*/

const char msg_safe[] = "    >>> WELCOME TO MOTOR CONTROL ROOM <<< SYSTEM ACTIVE <<< ALL CLEAR <<<    ";
const char msg_zone1[] = "    >>> ENTERING RESTRICTED AREA <<< PROCEED WITH CAUTION <<< SAFETY FIRST <<<    ";
const char msg_zone2[] = "    !! CAUTION: LIVE ELECTRICAL WIRING AHEAD !! MAINTAIN SAFE DISTANCE !!    ";
const char msg_zone3[] = "    ** WARNING: HIGH VOLTAGE EQUIPMENT ** AUTHORIZED PERSONNEL ONLY **    ";
const char msg_danger[] = "    !!! STOP !!! DANGER ZONE !!! EMERGENCY SHUTDOWN ACTIVATED !!! DO NOT CROSS !!!    ";

/*==============================================================
                         VARIABLES
==============================================================*/

I2C_HandleTypeDef hi2c1;

// Current level from sensor
uint8_t detected_level = 0;

// Applied level (actual relay state)
uint8_t applied_level = 0;

// Previous detected level (to detect direction)
uint8_t prev_detected_level = 0;

// Backward tracking
uint8_t backward_in_progress = 0;
uint8_t target_backward_level = 255;
uint32_t backward_start_time = 0;

int16_t scroll_position = 0;
uint32_t last_scroll_time = 0;

uint8_t system_locked = 0;

/*==============================================================
                    FUNCTION PROTOTYPES
==============================================================*/

void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_I2C1_Init(void);
void delay_us(uint32_t us);
float measure_distance(void);
void relay1_on(void);
void relay1_off(void);
void relay2_on(void);
void relay2_off(void);
void relay3_on(void);
void relay3_off(void);
void relay4_on(void);
void relay4_off(void);
void all_relays_on(void);
void all_relays_off(void);
void apply_relay_state(uint8_t level);
void beep(uint8_t count);
uint8_t is_button_pressed(void);
uint8_t get_warning_level(float distance);
void scroll_message(const char* message, const char* line2_text);
void display_danger_alert(void);
void display_locked(void);
void display_backward_countdown(uint8_t seconds_left, uint8_t from_zone, uint8_t to_zone);
void startup_sequence(void);
const char* get_zone_message(uint8_t level);
const char* get_zone_label(uint8_t level);

/*==============================================================
                    DELAY MICROSECONDS
==============================================================*/

void delay_us(uint32_t us)
{
    uint32_t cycles = (SystemCoreClock / 1000000UL) * us;
    uint32_t start = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < cycles);
}

/*==============================================================
                    MEASURE DISTANCE
==============================================================*/

float measure_distance(void)
{
    uint32_t start_time = 0;
    uint32_t stop_time = 0;
    uint32_t timeout = 0;

    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);
    delay_us(2);
    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_SET);
    delay_us(10);
    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);

    timeout = 100000;
    while (HAL_GPIO_ReadPin(ECHO_PORT, ECHO_PIN) == GPIO_PIN_RESET)
    {
        if (--timeout == 0) return 999.0f;
    }
    start_time = DWT->CYCCNT;

    timeout = 100000;
    while (HAL_GPIO_ReadPin(ECHO_PORT, ECHO_PIN) == GPIO_PIN_SET)
    {
        if (--timeout == 0) return 999.0f;
    }
    stop_time = DWT->CYCCNT;

    uint32_t elapsed = stop_time - start_time;
    float time_us = (float)elapsed * 1000000.0f / (float)SystemCoreClock;
    float distance = (time_us * 0.0343f) / 2.0f;

    if (distance > 400.0f) distance = 400.0f;
    if (distance < 2.0f) distance = 2.0f;

    return distance;
}

/*==============================================================
                    RELAY CONTROL
==============================================================*/

void relay1_on(void)  { HAL_GPIO_WritePin(RELAY1_PORT, RELAY1_PIN, GPIO_PIN_RESET); }
void relay1_off(void) { HAL_GPIO_WritePin(RELAY1_PORT, RELAY1_PIN, GPIO_PIN_SET); }
void relay2_on(void)  { HAL_GPIO_WritePin(RELAY2_PORT, RELAY2_PIN, GPIO_PIN_RESET); }
void relay2_off(void) { HAL_GPIO_WritePin(RELAY2_PORT, RELAY2_PIN, GPIO_PIN_SET); }
void relay3_on(void)  { HAL_GPIO_WritePin(RELAY3_PORT, RELAY3_PIN, GPIO_PIN_RESET); }
void relay3_off(void) { HAL_GPIO_WritePin(RELAY3_PORT, RELAY3_PIN, GPIO_PIN_SET); }
void relay4_on(void)  { HAL_GPIO_WritePin(RELAY4_PORT, RELAY4_PIN, GPIO_PIN_RESET); }
void relay4_off(void) { HAL_GPIO_WritePin(RELAY4_PORT, RELAY4_PIN, GPIO_PIN_SET); }

void all_relays_on(void)
{
    relay1_on();
    relay2_on();
    relay3_on();
    relay4_on();
}

void all_relays_off(void)
{
    relay1_off();
    relay2_off();
    relay3_off();
    relay4_off();
}

void apply_relay_state(uint8_t level)
{
    switch (level)
    {
        case 0:
            relay1_on();
            relay2_on();
            relay3_on();
            relay4_on();
            break;
        case 1:
            relay1_off();
            relay2_on();
            relay3_on();
            relay4_on();
            break;
        case 2:
            relay1_off();
            relay2_off();
            relay3_on();
            relay4_on();
            break;
        case 3:
            relay1_off();
            relay2_off();
            relay3_off();
            relay4_on();
            break;
        case 4:
            relay1_off();
            relay2_off();
            relay3_off();
            relay4_off();
            break;
    }
}

/*==============================================================
                    BUZZER CONTROL
==============================================================*/

void beep(uint8_t count)
{
    for (uint8_t i = 0; i < count; i++)
    {
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
        HAL_Delay(80);
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        HAL_Delay(80);
    }
}

/*==============================================================
                    BUTTON CHECK
==============================================================*/

uint8_t is_button_pressed(void)
{
    if (HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN) == GPIO_PIN_RESET)
    {
        HAL_Delay(50);
        if (HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN) == GPIO_PIN_RESET)
        {
            while (HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN) == GPIO_PIN_RESET);
            HAL_Delay(50);
            return 1;
        }
    }
    return 0;
}

/*==============================================================
                    GET WARNING LEVEL
==============================================================*/

uint8_t get_warning_level(float distance)
{
    if (distance > LEVEL_SAFE)
        return 0;
    else if (distance > LEVEL_ZONE1)
        return 1;
    else if (distance > LEVEL_ZONE2)
        return 2;
    else if (distance > LEVEL_ZONE3)
        return 3;
    else
        return 4;
}

/*==============================================================
                    GET ZONE MESSAGE
==============================================================*/

const char* get_zone_message(uint8_t level)
{
    switch (level)
    {
        case 0: return msg_safe;
        case 1: return msg_zone1;
        case 2: return msg_zone2;
        case 3: return msg_zone3;
        case 4: return msg_danger;
        default: return msg_safe;
    }
}

const char* get_zone_label(uint8_t level)
{
    switch (level)
    {
        case 0: return ">> ALL CLEAR << ";
        case 1: return ">> ZONE 1: 30cm ";
        case 2: return ">> ZONE 2: 20cm ";
        case 3: return ">> ZONE 3: 10cm ";
        case 4: return "!! DANGER !!    ";
        default: return "                ";
    }
}

/*==============================================================
                    SCROLL MESSAGE
==============================================================*/

void scroll_message(const char* message, const char* line2_text)
{
    char buffer[17];
    int16_t len = strlen(message);

    for (int i = 0; i < LCD_WIDTH; i++)
    {
        buffer[i] = message[(scroll_position + i) % len];
    }
    buffer[LCD_WIDTH] = '\0';

    lcd_set_cursor(0, 0);
    lcd_send_string(buffer);

    lcd_set_cursor(1, 0);
    if (line2_text != NULL)
    {
        lcd_send_string(line2_text);
    }
    else
    {
        int16_t offset = (scroll_position + len/2) % len;
        for (int i = 0; i < LCD_WIDTH; i++)
        {
            buffer[i] = message[(offset + i) % len];
        }
        buffer[LCD_WIDTH] = '\0';
        lcd_send_string(buffer);
    }
}

/*==============================================================
                    DISPLAY DANGER ALERT
==============================================================*/

void display_danger_alert(void)
{
    for (uint8_t i = 0; i < 5; i++)
    {
        lcd_clear();
        HAL_Delay(2);
        lcd_set_cursor(0, 0);
        lcd_send_string("!!! DANGER !!!");
        lcd_set_cursor(1, 0);
        lcd_send_string("EMERGENCY STOP!");
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
        HAL_Delay(150);

        lcd_clear();
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        HAL_Delay(150);
    }
}

/*==============================================================
                    DISPLAY LOCKED
==============================================================*/

void display_locked(void)
{
    lcd_set_cursor(0, 0);
    lcd_send_string("SYSTEM SHUTDOWN   ");
    lcd_set_cursor(1, 0);
    lcd_send_string("Press RESET BTN ");
}

/*==============================================================
                    DISPLAY BACKWARD COUNTDOWN
==============================================================*/

void display_backward_countdown(uint8_t seconds_left, uint8_t from_zone, uint8_t to_zone)
{
    char line1[17];
    char line2[17];

    sprintf(line1, "MOVING BACK...  ");
    sprintf(line2, "Wait: %02d sec    ", seconds_left);

    lcd_set_cursor(0, 0);
    lcd_send_string(line1);
    lcd_set_cursor(1, 0);
    lcd_send_string(line2);
}

/*==============================================================
                    STARTUP SEQUENCE
==============================================================*/

void startup_sequence(void)
{
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_send_string("****************");
    lcd_set_cursor(1, 0);
    lcd_send_string("  WELCOME TO    ");
    HAL_Delay(1500);

    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_send_string("     SMART      ");
    lcd_set_cursor(1, 0);
    lcd_send_string("   INDUSTRY     ");
    HAL_Delay(1500);

    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_send_string("    SAFETY      ");
    lcd_set_cursor(1, 0);
    lcd_send_string("    SYSTEM      ");
    HAL_Delay(1500);

    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_send_string("Protection Zones");
    lcd_set_cursor(1, 0);
    lcd_send_string("40-30-20-10 cm  ");
    HAL_Delay(1500);

    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_send_string("Initializing....");

    lcd_set_cursor(1, 0);
    for (uint8_t i = 0; i < 16; i++)
    {
        lcd_send_data(0xFF);
        HAL_Delay(80);
    }

    HAL_Delay(300);

    all_relays_on();

    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_send_string(">> SYSTEM ONLINE");
    lcd_set_cursor(1, 0);
    lcd_send_string("All Systems GO! ");
    beep(2);
    HAL_Delay(1500);
}

/*==============================================================
                    MAIN FUNCTION
==============================================================*/

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();

    // Enable DWT
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    // Initialize LCD
    HAL_Delay(100);
    lcd_init();
    HAL_Delay(100);
    lcd_clear();
    HAL_Delay(10);

    // Startup
    startup_sequence();

    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);

    lcd_clear();
    HAL_Delay(5);
    last_scroll_time = HAL_GetTick();

    // Initialize
    applied_level = 0;
    detected_level = 0;
    prev_detected_level = 0;
    backward_in_progress = 0;

    // ===== MAIN LOOP =====
    while (1)
    {
        // Check button press
        if (is_button_pressed())
        {
            system_locked = 0;
            all_relays_on();
            applied_level = 0;
            detected_level = 0;
            prev_detected_level = 0;
            backward_in_progress = 0;
            target_backward_level = 255;
            scroll_position = 0;

            lcd_clear();
            lcd_set_cursor(0, 0);
            lcd_send_string(">> RESET OK <<  ");
            lcd_set_cursor(1, 0);
            lcd_send_string("System Restored ");
            beep(2);
            HAL_Delay(1500);
            lcd_clear();
            continue;
        }

        // If locked
        if (system_locked)
        {
            display_locked();

            HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
            HAL_Delay(50);
            HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
            HAL_Delay(300);

            continue;
        }

        // Measure distance
        float distance = measure_distance();

        // Get current detected level
        detected_level = get_warning_level(distance);

        // ===== MOVEMENT DETECTION =====

        // FORWARD MOVEMENT (closer - level increases)
        if (detected_level > applied_level)
        {
            // Cancel any backward countdown
            backward_in_progress = 0;
            target_backward_level = 255;

            // Apply IMMEDIATELY
            applied_level = detected_level;
            scroll_position = 0;

            lcd_clear();
            HAL_Delay(5);

            apply_relay_state(applied_level);

            if (applied_level > 0 && applied_level < 4)
            {
                beep(applied_level);
            }

            if (applied_level == 4)
            {
                system_locked = 1;
                display_danger_alert();
                continue;
            }
        }
        // BACKWARD MOVEMENT (away - level decreases)
        else if (detected_level < applied_level)
        {
            // Start backward countdown if not already running
            if (!backward_in_progress)
            {
                backward_in_progress = 1;
                target_backward_level = detected_level;
                backward_start_time = HAL_GetTick();
            }
            // If moving further back, update target
            else if (detected_level < target_backward_level)
            {
                target_backward_level = detected_level;
                backward_start_time = HAL_GetTick();  // Restart timer
            }
            // If moved forward again (but still behind applied level)
            else if (detected_level > target_backward_level)
            {
                // Cancel backward movement
                backward_in_progress = 0;
                target_backward_level = 255;
            }

            // Check if countdown complete
            if (backward_in_progress)
            {
                uint32_t elapsed = HAL_GetTick() - backward_start_time;

                if (elapsed >= BACKWARD_DELAY_MS)
                {
                    // 10 seconds passed - apply one level back only
                    backward_in_progress = 0;

                    // Move back one level at a time
                    if (applied_level > 0)
                    {
                        applied_level--;
                        apply_relay_state(applied_level);
                        beep(1);
                        scroll_position = 0;
                        lcd_clear();
                        HAL_Delay(5);
                    }

                    // If still need to go back more, start new countdown
                    if (detected_level < applied_level)
                    {
                        backward_in_progress = 1;
                        target_backward_level = detected_level;
                        backward_start_time = HAL_GetTick();
                    }
                    else
                    {
                        target_backward_level = 255;
                    }
                }
            }
        }
        // SAME LEVEL - No movement
        else
        {
            // If person stays in same zone, cancel backward countdown
            if (backward_in_progress && detected_level == applied_level)
            {
                backward_in_progress = 0;
                target_backward_level = 255;
            }
        }

        // Update previous level
        prev_detected_level = detected_level;

        // ===== DISPLAY UPDATE =====
        if (!system_locked)
        {
            uint32_t current_time = HAL_GetTick();
            uint16_t scroll_delay = SCROLL_SPEED;

            if (applied_level >= 3) scroll_delay = 120;

            if ((current_time - last_scroll_time) >= scroll_delay)
            {
                last_scroll_time = current_time;

                // Show countdown if backward in progress
                if (backward_in_progress)
                {
                    uint32_t elapsed = HAL_GetTick() - backward_start_time;
                    uint8_t seconds_left = (BACKWARD_DELAY_MS - elapsed) / 1000;
                    if (seconds_left > 10) seconds_left = 10;

                    display_backward_countdown(seconds_left + 1, applied_level, target_backward_level);
                }
                else
                {
                    // Normal display
                    const char* msg = get_zone_message(applied_level);
                    const char* label = get_zone_label(applied_level);

                    if (applied_level == 4)
                    {
                        scroll_message(msg, NULL);
                    }
                    else
                    {
                        scroll_message(msg, label);
                    }

                    scroll_position++;
                    if (scroll_position >= (int16_t)strlen(msg))
                        scroll_position = 0;
                }
            }
        }

        HAL_Delay(30);
    }
}

/*==============================================================
                SYSTEM CLOCK CONFIG
==============================================================*/

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 16;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ = 7;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

/*==============================================================
                GPIO INITIALIZATION
==============================================================*/

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

    // RELAY 1: PA5
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // RELAY 2: PA6
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // RELAY 3: PA7
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // BUZZER: PA8
    GPIO_InitStruct.Pin = GPIO_PIN_8;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // TRIG: PB0
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // ECHO: PB1
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // RELAY 4: PB6
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // I2C1 SCL: PB8
    GPIO_InitStruct.Pin = GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // I2C1 SDA: PB9
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // BUTTON: PC13
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/*==============================================================
                I2C1 INIT
==============================================================*/

void MX_I2C1_Init(void)
{
    __HAL_RCC_I2C1_CLK_ENABLE();

    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    {
        Error_Handler();
    }
}

/*==============================================================
                ERROR HANDLER
==============================================================*/

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif