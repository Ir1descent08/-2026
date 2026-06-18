#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "hw_memmap.h"
#include "debug.h"
#include "gpio.h"
#include "hw_i2c.h"
#include "hw_types.h"
#include "i2c.h"
#include "pin_map.h"
#include "sysctl.h"
#include "systick.h"
#include "interrupt.h"
#include "uart.h"
#include "hw_ints.h"
#include "pwm.h"

#define SYSTICK_FREQUENCY           1000
#define UART_FRAME_MAX_LEN          64
#define MSG_MAX_LEN                 32
#define KEY_NAME_MAX_LEN            15
#define DISPLAY_DIGITS              8
#define DISPLAY_REFRESH_MS          2
#define KEY_SCAN_PERIOD_MS          10
#define KEY_DEBOUNCE_TICKS          2
#define SCROLL_SLOW_MS              600
#define SCROLL_MEDIUM_MS            300
#define SCROLL_FAST_MS              150
#define BOOT_ON_MS                  1500
#define BOOT_OFF_MS                 1000
#define BOOT_VERSION_MS             2000
#define BOOT_PHASE_DONE             0
#define BOOT_PHASE_ALL_ON           1
#define BOOT_PHASE_ALL_OFF          2
#define BOOT_PHASE_ID_ON            3
#define BOOT_PHASE_ID_OFF           4
#define BOOT_PHASE_NAME_ON          5
#define BOOT_PHASE_NAME_OFF         6
#define BOOT_PHASE_VERSION          7
#define BOOT_ID_TEXT                "31910429"
#define BOOT_NAME_TEXT              "FANSIZHE"
#define BOOT_VERSION_TEXT           "0.0.1"
#define SW_DEBUG_DISABLE_DISPLAY_AFTER_BOOT 1
#define BEEP_PWM_PERIOD             8000
#define DEFAULT_YEAR                2026
#define DEFAULT_MONTH               1
#define DEFAULT_DATE                1
#define DEFAULT_HOUR                0
#define DEFAULT_MINUTE              0
#define DEFAULT_SECOND              0
#define DEFAULT_ALARM_BEEP_MS       1000
#define KEY_EVENT_NONE              0
#define KEY_EVENT_PANEL_DISP        1
#define KEY_EVENT_PANEL_FORMAT      2
#define KEY_EVENT_FUNC              3
#define KEY_EVENT_SAVE              4
#define KEY_EVENT_DISP              5
#define KEY_EVENT_SPEED             6
#define KEY_EVENT_FORMAT            7
#define KEY_EVENT_EXT               8
#define KEY_EVENT_SHIFT             9
#define KEY_EVENT_ADD               10

#define TCA6424_I2CADDR             0x22
#define PCA9557_I2CADDR             0x18

#define TCA6424_INPUT_PORT0         0x00
#define TCA6424_OUTPUT_PORT0        0x04
#define TCA6424_POLINVERT_PORT0     0x08
#define PCA9557_INPUT               0x00
#define PCA9557_OUTPUT              0x01
#define PCA9557_POLINVERT           0x02
#define PCA9557_CONFIG              0x03

#define TCA6424_CONFIG_PORT0        0x0c
#define TCA6424_CONFIG_PORT1        0x0d
#define TCA6424_CONFIG_PORT2        0x0e

#define TCA6424_OUTPUT_PORT1        0x05
#define TCA6424_OUTPUT_PORT2        0x06

typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t date;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} DateTime;

void Delay(uint32_t value);
void S800_GPIO_Init(void);
uint8_t I2C0_WriteByte(uint8_t DevAddr, uint8_t RegAddr, uint8_t WriteData);
uint8_t I2C0_ReadByte(uint8_t DevAddr, uint8_t RegAddr);
void S800_I2C0_Init(void);
void S800_UART_Init(void);
void S800_PWM_Init(void);
void SysTick_Handler(void);
void UART0_Handler(void);

static void UARTStringPut(const char *message);
static void UARTReplyOK(const char *data);
static void UARTReplyError(const char *reason);
static void SetBeepOutput(uint8_t enabled);
static void ResetClockState(void);
static void ResetProtocolState(void);
static void ProcessPendingUart(void);
static void ProcessDisplayTask(void);
static void ProcessKeyTask(void);
static void ProcessCommand(char *line);
static void HandleSetCommand(char *subcommand, char *cursor);
static void HandleGetCommand(char *subcommand, char *cursor);
static void HandleSetDate(char *tokens[], uint8_t count);
static void HandleSetTime(char *tokens[], uint8_t count);
static void HandleSetAlarm(char *tokens[], uint8_t count);
static void HandleGetDate(char *tokens[], uint8_t count);
static void HandleGetTime(char *tokens[], uint8_t count);
static void HandleGetAlarm(char *tokens[], uint8_t count);
static void HandleGetKey(char *tokens[], uint8_t count);
static void HandleSetDisplay(char *tokens[], uint8_t count);
static void HandleSetFormat(char *tokens[], uint8_t count);
static void HandleSetMode(char *tokens[], uint8_t count);
static void HandleGetDisplay(char *tokens[], uint8_t count);
static void HandleGetFormat(char *tokens[], uint8_t count);
static void HandleSetLed(char *tokens[], uint8_t count);
static void HandleSetBeep(char *tokens[], uint8_t count);
static void HandleSetKey(char *tokens[], uint8_t count);
static void HandleSetMsg(char *cursor);
static void ApplyLedOutput(void);
static void ApplyDisplayOutput(void);
static void AdvanceBootSequence(void);
static void UpdateDisplayBuffer(void);
static void FillDisplayText(const char *text);
static void ReverseText(char *text);
static void BuildCompactDateText(char *text, size_t size);
static void BuildCompactTimeText(char *text, size_t size);
static void BuildCompactAlarmText(char *text, size_t size);
static void RefreshDisplayDigit(void);
static uint8_t EncodeDisplayChar(char c);
static void SampleBoardKeys(void);
static void QueueKeyEvent(uint8_t event_code);
static void EmitKeyEvent(const char *name);
static void ApplyVirtualKey(const char *name);
static bool MatchToken(const char *token, const char *pattern);
static bool ParseUnsigned(const char *token, uint32_t *value);
static bool ParseHexByte(const char *token, uint8_t *value);
static char *NextToken(char **cursor);
static void SkipSpaces(char **cursor);
static uint8_t CollectTokens(char *cursor, char *tokens[], uint8_t max_tokens);
static bool BuildDateFieldList(char *tokens[], uint8_t count, bool selected[3]);
static bool BuildTimeFieldList(char *tokens[], uint8_t count, bool selected[3]);
static bool IsLeapYear(uint16_t year);
static uint8_t DaysInMonth(uint16_t year, uint8_t month);
static bool IsValidDate(uint16_t year, uint8_t month, uint8_t date);
static void AdvanceClockOneSecond(void);
static void CheckAlarmTrigger(void);

uint32_t ui32SysClock;

static volatile DateTime g_now;
static volatile uint8_t g_alarm_hour;
static volatile uint8_t g_alarm_minute;
static volatile uint8_t g_alarm_second;
static volatile uint8_t g_alarm_enabled;
static volatile uint8_t g_display_on;
static volatile uint8_t g_format_left;
static volatile uint8_t g_mode_day;
static volatile uint8_t g_led_value;
static volatile uint16_t g_beep_remaining_ms;
static volatile uint32_t g_uptime_s;
static volatile uint16_t g_tick_1s;
static volatile uint8_t g_uart_write_index;
static volatile uint8_t g_uart_line_length;
static volatile uint8_t g_uart_line_overflow;
static volatile int8_t g_uart_ready_index;
static volatile uint8_t g_uart_error_len;
static volatile uint8_t g_uart_error_busy;
static volatile uint8_t g_display_refresh_pending;
static volatile uint8_t g_display_dirty;
static volatile uint8_t g_display_digit_index;
static volatile uint8_t g_display_page;
static volatile uint8_t g_key_scan_pending;
static volatile uint8_t g_pending_key_event;
static volatile uint8_t g_key_disp_state;
static volatile uint8_t g_key_format_state;
static volatile uint8_t g_key_disp_count;
static volatile uint8_t g_key_format_count;
static volatile uint8_t g_board_key_raw_value;
static volatile uint8_t g_board_key_stable_value;
static volatile uint8_t g_board_key_debounce_count;
static volatile uint8_t g_board_key_state_mask;
static volatile uint8_t g_boot_phase;
static volatile uint16_t g_boot_splash_ms;
static volatile uint16_t g_scroll_speed_ms;
static volatile uint16_t g_scroll_elapsed_ms;
static volatile uint8_t g_scroll_offset;
static volatile uint8_t g_beep_output_state;
static volatile uint8_t g_display_dot_mask;
static volatile char g_uart_lines[2][UART_FRAME_MAX_LEN + 1];
static char g_message[MSG_MAX_LEN + 1];
static char g_last_key[KEY_NAME_MAX_LEN + 1];
static char g_display_chars[DISPLAY_DIGITS + 1];
static const uint8_t g_seg7_digits[16] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07,
                                          0x7f, 0x6f, 0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71};

int main(void)
{
    ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_16MHZ | SYSCTL_OSC_INT | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 20000000);

    SysTickPeriodSet(ui32SysClock / SYSTICK_FREQUENCY);
    SysTickEnable();
    SysTickIntEnable();

    S800_GPIO_Init();
    S800_I2C0_Init();
    S800_PWM_Init();
    S800_UART_Init();
    ResetProtocolState();
    ApplyLedOutput();
    ApplyDisplayOutput();

    IntEnable(INT_UART0);
    UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT);
    IntMasterEnable();

    while (1)
    {
        ProcessPendingUart();
        ProcessKeyTask();
        ProcessDisplayTask();
    }
}

void Delay(uint32_t value)
{
    uint32_t ui32Loop;
    for (ui32Loop = 0; ui32Loop < value; ui32Loop++)
    {
    }
}

static void UARTStringPut(const char *message)
{
    while (*message != '\0')
    {
        UARTCharPut(UART0_BASE, *message++);
    }
}

static void UARTReplyOK(const char *data)
{
    UARTStringPut("OK");
    if ((data != 0) && (*data != '\0'))
    {
        UARTStringPut(" ");
        UARTStringPut(data);
    }
    UARTStringPut("\r\n");
}

static void UARTReplyError(const char *reason)
{
    UARTStringPut("ERROR ");
    UARTStringPut(reason);
    UARTStringPut("\r\n");
}

static void SetBeepOutput(uint8_t enabled)
{
    PWMOutputState(PWM0_BASE, PWM_OUT_7_BIT, (enabled != 0) ? true : false);
}

static void ResetClockState(void)
{
    g_now.year = DEFAULT_YEAR;
    g_now.month = DEFAULT_MONTH;
    g_now.date = DEFAULT_DATE;
    g_now.hour = DEFAULT_HOUR;
    g_now.minute = DEFAULT_MINUTE;
    g_now.second = DEFAULT_SECOND;
    g_alarm_hour = DEFAULT_HOUR;
    g_alarm_minute = DEFAULT_MINUTE;
    g_alarm_second = DEFAULT_SECOND;
    g_alarm_enabled = 0;
    g_display_on = 1;
    g_format_left = 1;
    g_display_page = 0;
    g_boot_phase = BOOT_PHASE_ALL_ON;
    g_boot_splash_ms = BOOT_ON_MS;
    g_led_value = 0xff;
    g_beep_remaining_ms = 0;
    g_display_dirty = 1;
    g_scroll_speed_ms = SCROLL_MEDIUM_MS;
    g_scroll_elapsed_ms = 0;
    g_scroll_offset = 0;
    g_beep_output_state = 0;
    SetBeepOutput(0);
}

static void ResetProtocolState(void)
{
    memset((void *)g_uart_lines, 0, sizeof(g_uart_lines));
    memset(g_message, 0, sizeof(g_message));
    memset(g_last_key, 0, sizeof(g_last_key));
    memset(g_display_chars, ' ', DISPLAY_DIGITS);
    g_display_chars[DISPLAY_DIGITS] = '\0';
    g_display_dot_mask = 0;
    g_uptime_s = 0;
    g_tick_1s = 0;
    g_uart_write_index = 0;
    g_uart_line_length = 0;
    g_uart_line_overflow = 0;
    g_uart_ready_index = -1;
    g_uart_error_len = 0;
    g_uart_error_busy = 0;
    g_display_refresh_pending = 1;
    g_display_dirty = 1;
    g_display_digit_index = 0;
    g_key_scan_pending = 0;
    g_pending_key_event = KEY_EVENT_NONE;
    g_key_disp_state = 0;
    g_key_format_state = 0;
    g_key_disp_count = 0;
    g_key_format_count = 0;
    g_board_key_raw_value = 0xff;
    g_board_key_stable_value = 0xff;
    g_board_key_debounce_count = 0;
    g_board_key_state_mask = 0;
    g_boot_splash_ms = BOOT_ON_MS;
    g_scroll_elapsed_ms = 0;
    g_scroll_offset = 0;
    g_beep_output_state = 0;
    g_mode_day = 1;
    ResetClockState();
}

static void ProcessPendingUart(void)
{
    if (g_uart_error_len != 0)
    {
        g_uart_error_len = 0;
        UARTReplyError("LEN");
    }

    if (g_uart_error_busy != 0)
    {
        g_uart_error_busy = 0;
        UARTReplyError("BUSY");
    }

    if (g_uart_ready_index >= 0)
    {
        ProcessCommand((char *)g_uart_lines[g_uart_ready_index]);
        g_uart_ready_index = -1;
    }
}

static void ProcessCommand(char *line)
{
    char *cursor = line;
    char *command;
    char *subcommand = 0;
    char *colon;

    SkipSpaces(&cursor);
    if (*cursor == '\0')
    {
        return;
    }

    command = NextToken(&cursor);
    if (command == 0)
    {
        return;
    }

    colon = strchr(command, ':');
    if ((colon != 0) && (colon != command))
    {
        *colon = '\0';
        subcommand = colon + 1;
    }

    if (MatchToken(command, "*PING"))
    {
        char response[24];
        SkipSpaces(&cursor);
        if ((subcommand != 0) || (*cursor != '\0'))
        {
            UARTReplyError("SYNTAX");
            return;
        }
        snprintf(response, sizeof(response), "*PONG %lu", (unsigned long)g_uptime_s);
        UARTStringPut(response);
        UARTStringPut("\r\n");
        return;
    }

    if (MatchToken(command, "*RST"))
    {
        SkipSpaces(&cursor);
        if ((subcommand != 0) || (*cursor != '\0'))
        {
            UARTReplyError("SYNTAX");
            return;
        }
        ResetClockState();
        ApplyDisplayOutput();
        UARTReplyOK(0);
        return;
    }

    if (MatchToken(command, "*SET"))
    {
        if (subcommand == 0)
        {
            subcommand = NextToken(&cursor);
        }
        if (subcommand == 0)
        {
            UARTReplyError("SYNTAX");
            return;
        }
        HandleSetCommand(subcommand, cursor);
        return;
    }

    if (MatchToken(command, "*GET"))
    {
        if (subcommand == 0)
        {
            subcommand = NextToken(&cursor);
        }
        if (subcommand == 0)
        {
            UARTReplyError("SYNTAX");
            return;
        }
        HandleGetCommand(subcommand, cursor);
        return;
    }

    UARTReplyError("SYNTAX");
}

static void HandleSetCommand(char *subcommand, char *cursor)
{
    char *tokens[8];
    uint8_t count;

    if ((subcommand != 0) && (*subcommand == ':'))
    {
        subcommand++;
    }

    if (MatchToken(subcommand, "MSG"))
    {
        HandleSetMsg(cursor);
        return;
    }

    count = CollectTokens(cursor, tokens, 8);

    if (MatchToken(subcommand, "DATE"))
    {
        HandleSetDate(tokens, count);
        return;
    }
    if (MatchToken(subcommand, "TIME"))
    {
        HandleSetTime(tokens, count);
        return;
    }
    if (MatchToken(subcommand, "ALARM"))
    {
        HandleSetAlarm(tokens, count);
        return;
    }
    if (MatchToken(subcommand, "DISPlay"))
    {
        HandleSetDisplay(tokens, count);
        return;
    }
    if (MatchToken(subcommand, "FORMAT"))
    {
        HandleSetFormat(tokens, count);
        return;
    }
    if (MatchToken(subcommand, "BEEP"))
    {
        HandleSetBeep(tokens, count);
        return;
    }
    if (MatchToken(subcommand, "LED"))
    {
        HandleSetLed(tokens, count);
        return;
    }
    if (MatchToken(subcommand, "KEY"))
    {
        HandleSetKey(tokens, count);
        return;
    }
    if (MatchToken(subcommand, "MODE"))
    {
        HandleSetMode(tokens, count);
        return;
    }

    UARTReplyError("PARAM");
}

static void HandleGetCommand(char *subcommand, char *cursor)
{
    char *tokens[8];
    uint8_t count;

    if ((subcommand != 0) && (*subcommand == ':'))
    {
        subcommand++;
    }

    count = CollectTokens(cursor, tokens, 8);

    if (MatchToken(subcommand, "DATE"))
    {
        HandleGetDate(tokens, count);
        return;
    }
    if (MatchToken(subcommand, "TIME"))
    {
        HandleGetTime(tokens, count);
        return;
    }
    if (MatchToken(subcommand, "ALARM"))
    {
        HandleGetAlarm(tokens, count);
        return;
    }
    if (MatchToken(subcommand, "KEY"))
    {
        HandleGetKey(tokens, count);
        return;
    }
    if (MatchToken(subcommand, "DISPlay"))
    {
        HandleGetDisplay(tokens, count);
        return;
    }
    if (MatchToken(subcommand, "FORMAT"))
    {
        HandleGetFormat(tokens, count);
        return;
    }

    UARTReplyError("PARAM");
}

static void HandleSetDate(char *tokens[], uint8_t count)
{
    uint8_t field_ids[3];
    uint8_t field_count = 0;
    uint8_t index = 0;
    uint8_t used[3] = {0, 0, 0};
    uint16_t year = g_now.year;
    uint8_t month = g_now.month;
    uint8_t date = g_now.date;
    uint32_t value;
    uint8_t pair_mode = 0;

    if ((count >= 2) &&
        (MatchToken(tokens[0], "YEAR") || MatchToken(tokens[0], "MONTH") || MatchToken(tokens[0], "DATE")) &&
        ParseUnsigned(tokens[1], &value))
    {
        pair_mode = 1;
    }

    if (pair_mode != 0)
    {
        while (index < count)
        {
            if ((index + 1) >= count)
            {
                UARTReplyError("SYNTAX");
                return;
            }
            if (MatchToken(tokens[index], "YEAR"))
            {
                if ((used[0] != 0) || !ParseUnsigned(tokens[index + 1], &value))
                {
                    UARTReplyError((used[0] != 0) ? "PARAM" : "SYNTAX");
                    return;
                }
                if ((value < 2000) || (value > 2099))
                {
                    UARTReplyError("RANGE");
                    return;
                }
                year = (uint16_t)value;
                used[0] = 1;
                index += 2;
                continue;
            }
            if (MatchToken(tokens[index], "MONTH"))
            {
                if ((used[1] != 0) || !ParseUnsigned(tokens[index + 1], &value))
                {
                    UARTReplyError((used[1] != 0) ? "PARAM" : "SYNTAX");
                    return;
                }
                if ((value < 1) || (value > 12))
                {
                    UARTReplyError("RANGE");
                    return;
                }
                month = (uint8_t)value;
                used[1] = 1;
                index += 2;
                continue;
            }
            if (MatchToken(tokens[index], "DATE"))
            {
                if ((used[2] != 0) || !ParseUnsigned(tokens[index + 1], &value))
                {
                    UARTReplyError((used[2] != 0) ? "PARAM" : "SYNTAX");
                    return;
                }
                if ((value < 1) || (value > 31))
                {
                    UARTReplyError("RANGE");
                    return;
                }
                date = (uint8_t)value;
                used[2] = 1;
                index += 2;
                continue;
            }
            UARTReplyError("SYNTAX");
            return;
        }
    }
    else
    {
        while (index < count)
        {
            if (MatchToken(tokens[index], "YEAR"))
            {
                if (used[0] != 0)
                {
                    UARTReplyError("PARAM");
                    return;
                }
                used[0] = 1;
                field_ids[field_count++] = 0;
                index++;
                continue;
            }
            if (MatchToken(tokens[index], "MONTH"))
            {
                if (used[1] != 0)
                {
                    UARTReplyError("PARAM");
                    return;
                }
                used[1] = 1;
                field_ids[field_count++] = 1;
                index++;
                continue;
            }
            if (MatchToken(tokens[index], "DATE"))
            {
                if (used[2] != 0)
                {
                    UARTReplyError("PARAM");
                    return;
                }
                used[2] = 1;
                field_ids[field_count++] = 2;
                index++;
                continue;
            }
            break;
        }

        if ((field_count == 0) || ((count - index) != field_count))
        {
            UARTReplyError("SYNTAX");
            return;
        }

        while (index < count)
        {
            if (!ParseUnsigned(tokens[index], &value))
            {
                UARTReplyError("PARAM");
                return;
            }

            switch (field_ids[index - field_count])
            {
            case 0:
                if ((value < 2000) || (value > 2099))
                {
                    UARTReplyError("RANGE");
                    return;
                }
                year = (uint16_t)value;
                break;
            case 1:
                if ((value < 1) || (value > 12))
                {
                    UARTReplyError("RANGE");
                    return;
                }
                month = (uint8_t)value;
                break;
            default:
                if ((value < 1) || (value > 31))
                {
                    UARTReplyError("RANGE");
                    return;
                }
                date = (uint8_t)value;
                break;
            }
            index++;
        }
    }

    if (!IsValidDate(year, month, date))
    {
        UARTReplyError("RANGE");
        return;
    }

    g_now.year = year;
    g_now.month = month;
    g_now.date = date;
    g_display_dirty = 1;
    UARTReplyOK(0);
}

static void HandleSetTime(char *tokens[], uint8_t count)
{
    uint8_t field_ids[3];
    uint8_t field_count = 0;
    uint8_t index = 0;
    uint8_t used[3] = {0, 0, 0};
    uint8_t hour = g_now.hour;
    uint8_t minute = g_now.minute;
    uint8_t second = g_now.second;
    uint32_t value;
    uint8_t pair_mode = 0;

    if ((count >= 2) &&
        (MatchToken(tokens[0], "HOUR") || MatchToken(tokens[0], "MINute") || MatchToken(tokens[0], "SECond")) &&
        ParseUnsigned(tokens[1], &value))
    {
        pair_mode = 1;
    }

    if (pair_mode != 0)
    {
        while (index < count)
        {
            if ((index + 1) >= count)
            {
                UARTReplyError("SYNTAX");
                return;
            }
            if (MatchToken(tokens[index], "HOUR"))
            {
                if ((used[0] != 0) || !ParseUnsigned(tokens[index + 1], &value))
                {
                    UARTReplyError((used[0] != 0) ? "PARAM" : "SYNTAX");
                    return;
                }
                if (value > 23)
                {
                    UARTReplyError("RANGE");
                    return;
                }
                hour = (uint8_t)value;
                used[0] = 1;
                index += 2;
                continue;
            }
            if (MatchToken(tokens[index], "MINute"))
            {
                if ((used[1] != 0) || !ParseUnsigned(tokens[index + 1], &value))
                {
                    UARTReplyError((used[1] != 0) ? "PARAM" : "SYNTAX");
                    return;
                }
                if (value > 59)
                {
                    UARTReplyError("RANGE");
                    return;
                }
                minute = (uint8_t)value;
                used[1] = 1;
                index += 2;
                continue;
            }
            if (MatchToken(tokens[index], "SECond"))
            {
                if ((used[2] != 0) || !ParseUnsigned(tokens[index + 1], &value))
                {
                    UARTReplyError((used[2] != 0) ? "PARAM" : "SYNTAX");
                    return;
                }
                if (value > 59)
                {
                    UARTReplyError("RANGE");
                    return;
                }
                second = (uint8_t)value;
                used[2] = 1;
                index += 2;
                continue;
            }
            UARTReplyError("SYNTAX");
            return;
        }
    }
    else
    {
        while (index < count)
        {
            if (MatchToken(tokens[index], "HOUR"))
            {
                if (used[0] != 0)
                {
                    UARTReplyError("PARAM");
                    return;
                }
                used[0] = 1;
                field_ids[field_count++] = 0;
                index++;
                continue;
            }
            if (MatchToken(tokens[index], "MINute"))
            {
                if (used[1] != 0)
                {
                    UARTReplyError("PARAM");
                    return;
                }
                used[1] = 1;
                field_ids[field_count++] = 1;
                index++;
                continue;
            }
            if (MatchToken(tokens[index], "SECond"))
            {
                if (used[2] != 0)
                {
                    UARTReplyError("PARAM");
                    return;
                }
                used[2] = 1;
                field_ids[field_count++] = 2;
                index++;
                continue;
            }
            break;
        }

        if ((field_count == 0) || ((count - index) != field_count))
        {
            UARTReplyError("SYNTAX");
            return;
        }

        while (index < count)
        {
            if (!ParseUnsigned(tokens[index], &value))
            {
                UARTReplyError("PARAM");
                return;
            }

            switch (field_ids[index - field_count])
            {
            case 0:
                if (value > 23)
                {
                    UARTReplyError("RANGE");
                    return;
                }
                hour = (uint8_t)value;
                break;
            case 1:
                if (value > 59)
                {
                    UARTReplyError("RANGE");
                    return;
                }
                minute = (uint8_t)value;
                break;
            default:
                if (value > 59)
                {
                    UARTReplyError("RANGE");
                    return;
                }
                second = (uint8_t)value;
                break;
            }
            index++;
        }
    }

    g_now.hour = hour;
    g_now.minute = minute;
    g_now.second = second;
    g_display_dirty = 1;
    UARTReplyOK(0);
}

static void HandleSetAlarm(char *tokens[], uint8_t count)
{
    uint8_t field_ids[3];
    uint8_t field_count = 0;
    uint8_t index = 0;
    uint8_t used[3] = {0, 0, 0};
    uint8_t hour = g_alarm_hour;
    uint8_t minute = g_alarm_minute;
    uint8_t second = g_alarm_second;
    uint32_t value;
    uint8_t pair_mode = 0;

    if ((count == 1) && MatchToken(tokens[0], "OFF"))
    {
        g_alarm_enabled = 0;
        UARTReplyOK(0);
        return;
    }

    if ((count >= 2) &&
        (MatchToken(tokens[0], "HOUR") || MatchToken(tokens[0], "MINute") || MatchToken(tokens[0], "SECond")) &&
        ParseUnsigned(tokens[1], &value))
    {
        pair_mode = 1;
    }

    if (pair_mode != 0)
    {
        while (index < count)
        {
            if ((index + 1) >= count)
            {
                UARTReplyError("SYNTAX");
                return;
            }
            if (MatchToken(tokens[index], "HOUR"))
            {
                if ((used[0] != 0) || !ParseUnsigned(tokens[index + 1], &value))
                {
                    UARTReplyError((used[0] != 0) ? "PARAM" : "SYNTAX");
                    return;
                }
                if (value > 23)
                {
                    UARTReplyError("RANGE");
                    return;
                }
                hour = (uint8_t)value;
                used[0] = 1;
                index += 2;
                continue;
            }
            if (MatchToken(tokens[index], "MINute"))
            {
                if ((used[1] != 0) || !ParseUnsigned(tokens[index + 1], &value))
                {
                    UARTReplyError((used[1] != 0) ? "PARAM" : "SYNTAX");
                    return;
                }
                if (value > 59)
                {
                    UARTReplyError("RANGE");
                    return;
                }
                minute = (uint8_t)value;
                used[1] = 1;
                index += 2;
                continue;
            }
            if (MatchToken(tokens[index], "SECond"))
            {
                if ((used[2] != 0) || !ParseUnsigned(tokens[index + 1], &value))
                {
                    UARTReplyError((used[2] != 0) ? "PARAM" : "SYNTAX");
                    return;
                }
                if (value > 59)
                {
                    UARTReplyError("RANGE");
                    return;
                }
                second = (uint8_t)value;
                used[2] = 1;
                index += 2;
                continue;
            }
            UARTReplyError("SYNTAX");
            return;
        }
    }
    else
    {
        while (index < count)
        {
            if (MatchToken(tokens[index], "HOUR"))
            {
                if (used[0] != 0)
                {
                    UARTReplyError("PARAM");
                    return;
                }
                used[0] = 1;
                field_ids[field_count++] = 0;
                index++;
                continue;
            }
            if (MatchToken(tokens[index], "MINute"))
            {
                if (used[1] != 0)
                {
                    UARTReplyError("PARAM");
                    return;
                }
                used[1] = 1;
                field_ids[field_count++] = 1;
                index++;
                continue;
            }
            if (MatchToken(tokens[index], "SECond"))
            {
                if (used[2] != 0)
                {
                    UARTReplyError("PARAM");
                    return;
                }
                used[2] = 1;
                field_ids[field_count++] = 2;
                index++;
                continue;
            }
            break;
        }

        if ((field_count == 0) || ((count - index) != field_count))
        {
            UARTReplyError("SYNTAX");
            return;
        }

        while (index < count)
        {
            if (!ParseUnsigned(tokens[index], &value))
            {
                UARTReplyError("PARAM");
                return;
            }

            switch (field_ids[index - field_count])
            {
            case 0:
                if (value > 23)
                {
                    UARTReplyError("RANGE");
                    return;
                }
                hour = (uint8_t)value;
                break;
            case 1:
                if (value > 59)
                {
                    UARTReplyError("RANGE");
                    return;
                }
                minute = (uint8_t)value;
                break;
            default:
                if (value > 59)
                {
                    UARTReplyError("RANGE");
                    return;
                }
                second = (uint8_t)value;
                break;
            }
            index++;
        }
    }

    g_alarm_hour = hour;
    g_alarm_minute = minute;
    g_alarm_second = second;
    g_alarm_enabled = 1;
    UARTReplyOK(0);
}

static void HandleGetDate(char *tokens[], uint8_t count)
{
    bool selected[3];
    char response[48];
    uint8_t offset = 0;
    DateTime now;

    if (!BuildDateFieldList(tokens, count, selected))
    {
        UARTReplyError("PARAM");
        return;
    }

    now.year = g_now.year;
    now.month = g_now.month;
    now.date = g_now.date;

    if (count == 0)
    {
        snprintf(response, sizeof(response), "%04u.%02u.%02u", now.year, now.month, now.date);
        if (g_format_left == 0)
        {
            ReverseText(response);
        }
        UARTReplyOK(response);
        return;
    }

    if (selected[0])
    {
        offset += (uint8_t)snprintf(response + offset, sizeof(response) - offset, "YEAR %04u", now.year);
    }
    if (selected[1])
    {
        offset += (uint8_t)snprintf(response + offset, sizeof(response) - offset, "%sMONTH %02u", (offset == 0) ? "" : " ", now.month);
    }
    if (selected[2])
    {
        offset += (uint8_t)snprintf(response + offset, sizeof(response) - offset, "%sDATE %02u", (offset == 0) ? "" : " ", now.date);
    }

    UARTReplyOK(response);
}

static void HandleGetTime(char *tokens[], uint8_t count)
{
    bool selected[3];
    char response[48];
    uint8_t offset = 0;
    DateTime now;

    if (!BuildTimeFieldList(tokens, count, selected))
    {
        UARTReplyError("PARAM");
        return;
    }

    now.hour = g_now.hour;
    now.minute = g_now.minute;
    now.second = g_now.second;

    if (count == 0)
    {
        BuildCompactTimeText(response, sizeof(response));
        UARTReplyOK(response);
        return;
    }

    if (selected[0])
    {
        offset += (uint8_t)snprintf(response + offset, sizeof(response) - offset, "HOUR %02u", now.hour);
    }
    if (selected[1])
    {
        offset += (uint8_t)snprintf(response + offset, sizeof(response) - offset, "%sMINUTE %02u", (offset == 0) ? "" : " ", now.minute);
    }
    if (selected[2])
    {
        offset += (uint8_t)snprintf(response + offset, sizeof(response) - offset, "%sSECOND %02u", (offset == 0) ? "" : " ", now.second);
    }

    UARTReplyOK(response);
}

static void HandleGetAlarm(char *tokens[], uint8_t count)
{
    bool selected[3];
    char response[48];
    uint8_t offset = 0;

    if ((count == 0) && (g_alarm_enabled == 0))
    {
        UARTReplyOK("OFF");
        return;
    }

    if (!BuildTimeFieldList(tokens, count, selected))
    {
        UARTReplyError("PARAM");
        return;
    }

    if (g_alarm_enabled == 0)
    {
        UARTReplyOK("OFF");
        return;
    }

    if (count == 0)
    {
        BuildCompactAlarmText(response, sizeof(response));
        UARTReplyOK(response);
        return;
    }

    if (selected[0])
    {
        offset += (uint8_t)snprintf(response + offset, sizeof(response) - offset, "HOUR %02u", g_alarm_hour);
    }
    if (selected[1])
    {
        offset += (uint8_t)snprintf(response + offset, sizeof(response) - offset, "%sMINUTE %02u", (offset == 0) ? "" : " ", g_alarm_minute);
    }
    if (selected[2])
    {
        offset += (uint8_t)snprintf(response + offset, sizeof(response) - offset, "%sSECOND %02u", (offset == 0) ? "" : " ", g_alarm_second);
    }

    UARTReplyOK(response);
}

static void HandleGetKey(char *tokens[], uint8_t count)
{
    char response[48];
    uint8_t key_value;
    uint8_t temp;

    if (count != 0)
    {
        UARTReplyError("PARAM");
        return;
    }

    key_value = I2C0_ReadByte(TCA6424_I2CADDR, TCA6424_INPUT_PORT0);
    temp = I2C0_ReadByte(TCA6424_I2CADDR, TCA6424_INPUT_PORT0);
    if (key_value != temp)
    {
        key_value = temp;
    }

    snprintf(response, sizeof(response), "RAW %02X STB %02X ACT %02X", key_value, g_board_key_stable_value, (uint8_t)(~g_board_key_stable_value));
    UARTReplyOK(response);
}

static void HandleSetDisplay(char *tokens[], uint8_t count)
{
    if (count != 1)
    {
        UARTReplyError("SYNTAX");
        return;
    }

    if (MatchToken(tokens[0], "ON"))
    {
        g_display_on = 1;
        g_display_dirty = 1;
        ApplyDisplayOutput();
        UARTReplyOK(0);
        return;
    }
    if (MatchToken(tokens[0], "OFF"))
    {
        g_display_on = 0;
        g_display_dirty = 1;
        ApplyDisplayOutput();
        UARTReplyOK(0);
        return;
    }

    UARTReplyError("PARAM");
}

static void HandleSetFormat(char *tokens[], uint8_t count)
{
    if (count != 1)
    {
        UARTReplyError("SYNTAX");
        return;
    }

    if (MatchToken(tokens[0], "LEFT"))
    {
        g_format_left = 1;
        g_scroll_offset = 0;
        g_scroll_elapsed_ms = 0;
        g_display_dirty = 1;
        UARTReplyOK(0);
        return;
    }
    if (MatchToken(tokens[0], "RIGHT"))
    {
        g_format_left = 0;
        g_scroll_offset = 0;
        g_scroll_elapsed_ms = 0;
        g_display_dirty = 1;
        UARTReplyOK(0);
        return;
    }

    UARTReplyError("PARAM");
}

static void HandleSetMode(char *tokens[], uint8_t count)
{
    if (count != 1)
    {
        UARTReplyError("SYNTAX");
        return;
    }

    if (MatchToken(tokens[0], "DAY"))
    {
        g_mode_day = 1;
        g_scroll_speed_ms = SCROLL_FAST_MS;
        g_display_dirty = 1;
        UARTReplyOK(0);
        return;
    }
    if (MatchToken(tokens[0], "NIGHT"))
    {
        g_mode_day = 0;
        g_scroll_speed_ms = SCROLL_SLOW_MS;
        g_display_dirty = 1;
        UARTReplyOK(0);
        return;
    }

    UARTReplyError("PARAM");
}

static void HandleGetDisplay(char *tokens[], uint8_t count)
{
    (void)tokens;
    if (count != 0)
    {
        UARTReplyError("PARAM");
        return;
    }
    UARTReplyOK((g_display_on != 0) ? "ON" : "OFF");
}

static void HandleGetFormat(char *tokens[], uint8_t count)
{
    (void)tokens;
    if (count != 0)
    {
        UARTReplyError("PARAM");
        return;
    }
    UARTReplyOK((g_format_left != 0) ? "LEFT" : "RIGHT");
}

static void HandleSetLed(char *tokens[], uint8_t count)
{
    uint8_t value;

    if (count != 1)
    {
        UARTReplyError("SYNTAX");
        return;
    }

    if (!ParseHexByte(tokens[0], &value))
    {
        UARTReplyError("PARAM");
        return;
    }

    g_led_value = value;
    ApplyLedOutput();
    UARTReplyOK(0);
}

static void HandleSetBeep(char *tokens[], uint8_t count)
{
    uint32_t value;

    if (count != 1)
    {
        UARTReplyError("SYNTAX");
        return;
    }

    if (!ParseUnsigned(tokens[0], &value))
    {
        UARTReplyError("PARAM");
        return;
    }
    if ((value < 10) || (value > 5000))
    {
        UARTReplyError("RANGE");
        return;
    }

    g_beep_remaining_ms = (uint16_t)value;
    g_beep_output_state = 1;
    SetBeepOutput(1);
    UARTReplyOK(0);
}

static void HandleSetKey(char *tokens[], uint8_t count)
{
    size_t length;

    if (count != 1)
    {
        UARTReplyError("SYNTAX");
        return;
    }

    length = strlen(tokens[0]);
    if ((length == 0) || (length > KEY_NAME_MAX_LEN))
    {
        UARTReplyError("PARAM");
        return;
    }

    memcpy(g_last_key, tokens[0], length + 1);
    ApplyVirtualKey(tokens[0]);
    UARTReplyOK(0);
}

static void HandleSetMsg(char *cursor)
{
    size_t length;

    SkipSpaces(&cursor);
    length = strlen(cursor);
    if (length == 0)
    {
        UARTReplyError("SYNTAX");
        return;
    }
    if (length > MSG_MAX_LEN)
    {
        UARTReplyError("LEN");
        return;
    }

    memcpy(g_message, cursor, length + 1);
    g_scroll_offset = 0;
    g_scroll_elapsed_ms = 0;
    g_display_dirty = 1;
    UARTReplyOK(0);
}

static void ApplyLedOutput(void)
{
    I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_OUTPUT, (uint8_t)(~g_led_value));
}

static void ApplyDisplayOutput(void)
{
    if (g_display_on == 0)
    {
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, 0x00);
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 0x00);
        return;
    }

    g_display_dirty = 1;
    g_display_refresh_pending = 1;
}

static void AdvanceBootSequence(void)
{
    switch (g_boot_phase)
    {
    case BOOT_PHASE_ALL_ON:
        g_boot_phase = BOOT_PHASE_ALL_OFF;
        g_boot_splash_ms = BOOT_OFF_MS;
        g_led_value = 0x00;
        break;
    case BOOT_PHASE_ALL_OFF:
        g_boot_phase = BOOT_PHASE_ID_ON;
        g_boot_splash_ms = BOOT_ON_MS;
        g_led_value = 0xff;
        break;
    case BOOT_PHASE_ID_ON:
        g_boot_phase = BOOT_PHASE_ID_OFF;
        g_boot_splash_ms = BOOT_OFF_MS;
        g_led_value = 0x00;
        break;
    case BOOT_PHASE_ID_OFF:
        g_boot_phase = BOOT_PHASE_NAME_ON;
        g_boot_splash_ms = BOOT_ON_MS;
        g_led_value = 0xff;
        break;
    case BOOT_PHASE_NAME_ON:
        g_boot_phase = BOOT_PHASE_NAME_OFF;
        g_boot_splash_ms = BOOT_OFF_MS;
        g_led_value = 0x00;
        break;
    case BOOT_PHASE_NAME_OFF:
        g_boot_phase = BOOT_PHASE_VERSION;
        g_boot_splash_ms = BOOT_VERSION_MS;
        g_led_value = 0x00;
        break;
    case BOOT_PHASE_VERSION:
        g_boot_phase = BOOT_PHASE_DONE;
        g_boot_splash_ms = 0;
        g_led_value = 0x00;
        break;
    default:
        g_boot_phase = BOOT_PHASE_DONE;
        g_boot_splash_ms = 0;
        g_led_value = 0x00;
        break;
    }

    ApplyLedOutput();
    g_display_dirty = 1;
}

static void ProcessDisplayTask(void)
{
    if ((SW_DEBUG_DISABLE_DISPLAY_AFTER_BOOT != 0) && (g_boot_phase == BOOT_PHASE_DONE))
    {
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, 0x00);
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 0x00);
        g_display_refresh_pending = 0;
        return;
    }

    if ((g_display_on != 0) && (g_display_page == 2) && (strlen(g_message) > DISPLAY_DIGITS) &&
        (g_scroll_elapsed_ms >= g_scroll_speed_ms))
    {
        uint8_t scroll_limit = (uint8_t)(strlen(g_message) + DISPLAY_DIGITS);
        g_scroll_elapsed_ms = 0;
        g_scroll_offset++;
        if (g_scroll_offset >= scroll_limit)
        {
            g_scroll_offset = 0;
        }
        g_display_dirty = 1;
    }

    if (g_display_dirty != 0)
    {
        UpdateDisplayBuffer();
        g_display_dirty = 0;
    }

    if (g_display_refresh_pending != 0)
    {
        g_display_refresh_pending = 0;
        RefreshDisplayDigit();
    }
}

static void ProcessKeyTask(void)
{
    uint8_t event_code;

    if (g_key_scan_pending != 0)
    {
        g_key_scan_pending = 0;
        SampleBoardKeys();
    }

    event_code = g_pending_key_event;
    if (event_code == KEY_EVENT_NONE)
    {
        return;
    }

    g_pending_key_event = KEY_EVENT_NONE;
    if (event_code == KEY_EVENT_PANEL_DISP)
    {
        ApplyVirtualKey("DISP");
        EmitKeyEvent("DISP");
        return;
    }
    if (event_code == KEY_EVENT_PANEL_FORMAT)
    {
        ApplyVirtualKey("FORMAT");
        EmitKeyEvent("FORMAT");
        return;
    }
    if (event_code == KEY_EVENT_FUNC)
    {
        ApplyVirtualKey("FUNC");
        EmitKeyEvent("FUNC");
        return;
    }
    if (event_code == KEY_EVENT_SAVE)
    {
        ApplyVirtualKey("SAVE");
        EmitKeyEvent("SAVE");
        return;
    }
    if (event_code == KEY_EVENT_DISP)
    {
        ApplyVirtualKey("DISP");
        EmitKeyEvent("DISP");
        return;
    }
    if (event_code == KEY_EVENT_SPEED)
    {
        ApplyVirtualKey("SPEED");
        EmitKeyEvent("SPEED");
        return;
    }
    if (event_code == KEY_EVENT_FORMAT)
    {
        ApplyVirtualKey("FORMAT");
        EmitKeyEvent("FORMAT");
        return;
    }
    if (event_code == KEY_EVENT_EXT)
    {
        ApplyVirtualKey("EXT");
        EmitKeyEvent("EXT");
        return;
    }
    if (event_code == KEY_EVENT_SHIFT)
    {
        ApplyVirtualKey("SHIFT");
        EmitKeyEvent("SHIFT");
        return;
    }
    if (event_code == KEY_EVENT_ADD)
    {
        ApplyVirtualKey("ADD");
        EmitKeyEvent("ADD");
    }
}

static void UpdateDisplayBuffer(void)
{
    char text[DISPLAY_DIGITS + 1];

    g_display_dot_mask = 0;

    if (g_display_on == 0)
    {
        FillDisplayText("");
        return;
    }

    if (g_boot_phase != BOOT_PHASE_DONE)
    {
        switch (g_boot_phase)
        {
        case BOOT_PHASE_ALL_ON:
            FillDisplayText("88888888");
            g_display_dot_mask = 0xff;
            return;
        case BOOT_PHASE_ALL_OFF:
            FillDisplayText("");
            return;
        case BOOT_PHASE_ID_ON:
            FillDisplayText(BOOT_ID_TEXT);
            return;
        case BOOT_PHASE_ID_OFF:
            FillDisplayText("");
            return;
        case BOOT_PHASE_NAME_ON:
            FillDisplayText(BOOT_NAME_TEXT);
            return;
        case BOOT_PHASE_NAME_OFF:
            FillDisplayText("");
            return;
        case BOOT_PHASE_VERSION:
            FillDisplayText(BOOT_VERSION_TEXT);
            return;
        default:
            break;
        }
    }

    if ((g_display_page == 2) && (g_message[0] != '\0'))
    {
        size_t length = strlen(g_message);
        if (length <= DISPLAY_DIGITS)
        {
            FillDisplayText(g_message);
        }
        else
        {
            int base;
            uint8_t index;
            uint8_t text_index = 0;
            uint8_t pending_next_dot = 0;
            memset(text, ' ', DISPLAY_DIGITS);
            text[DISPLAY_DIGITS] = '\0';
            if (g_format_left != 0)
            {
                base = (int)g_scroll_offset;
            }
            else
            {
                base = (int)length - DISPLAY_DIGITS - (int)g_scroll_offset;
            }
            for (index = 0; index < DISPLAY_DIGITS; index++)
            {
                int source = base + index;
                char c;
                if ((source < 0) || (source >= (int)length))
                {
                    continue;
                }
                c = g_message[source];
                if (c == '.')
                {
                    if (g_format_left != 0)
                    {
                        if (text_index != 0)
                        {
                            g_display_dot_mask |= (uint8_t)(1u << (text_index - 1));
                        }
                    }
                    else
                    {
                        pending_next_dot = 1;
                    }
                    continue;
                }
                if (text_index >= DISPLAY_DIGITS)
                {
                    break;
                }
                text[text_index] = c;
                if (pending_next_dot != 0)
                {
                    g_display_dot_mask |= (uint8_t)(1u << text_index);
                    pending_next_dot = 0;
                }
                text_index++;
            }
            memcpy(g_display_chars, text, DISPLAY_DIGITS + 1);
        }
        return;
    }

    if (g_display_page == 1)
    {
        BuildCompactDateText(text, sizeof(text));
        FillDisplayText(text);
        return;
    }

    BuildCompactTimeText(text, sizeof(text));
    FillDisplayText(text);
}

static void FillDisplayText(const char *text)
{
    size_t length;
    size_t copy_len;
    size_t start;

    memset(g_display_chars, ' ', DISPLAY_DIGITS);
    g_display_chars[DISPLAY_DIGITS] = '\0';

    length = strlen(text);
    copy_len = (length > DISPLAY_DIGITS) ? DISPLAY_DIGITS : length;
    start = 0;
    if ((g_format_left == 0) && (copy_len < DISPLAY_DIGITS))
    {
        start = DISPLAY_DIGITS - copy_len;
    }

    if (copy_len != 0)
    {
        memcpy(&g_display_chars[start], text, copy_len);
    }
}

static void ReverseText(char *text)
{
    size_t left = 0;
    size_t right = strlen(text);

    if (right == 0)
    {
        return;
    }

    right--;
    while (left < right)
    {
        char temp = text[left];
        text[left] = text[right];
        text[right] = temp;
        left++;
        right--;
    }
}

static void BuildCompactDateText(char *text, size_t size)
{
    snprintf(text, size, "%02u.%02u.%02u", g_now.year % 100, g_now.month, g_now.date);
    if (g_format_left == 0)
    {
        ReverseText(text);
    }
}

static void BuildCompactTimeText(char *text, size_t size)
{
    snprintf(text, size, "%02u.%02u.%02u", g_now.hour, g_now.minute, g_now.second);
    if (g_format_left == 0)
    {
        ReverseText(text);
    }
}

static void BuildCompactAlarmText(char *text, size_t size)
{
    if (g_alarm_enabled == 0)
    {
        snprintf(text, size, "OFF");
        return;
    }

    snprintf(text, size, "%02u.%02u.%02u", g_alarm_hour, g_alarm_minute, g_alarm_second);
    if (g_format_left == 0)
    {
        ReverseText(text);
    }
}

static void RefreshDisplayDigit(void)
{
    uint8_t index;
    uint8_t select_mask;
    uint8_t seg_value;

    if (g_display_on == 0)
    {
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, 0x00);
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 0x00);
        return;
    }

    index = g_display_digit_index;
    if (index >= DISPLAY_DIGITS)
    {
        index = 0;
    }

    select_mask = (uint8_t)(1u << index);
    seg_value = EncodeDisplayChar(g_display_chars[index]);
    if ((g_display_dot_mask & (uint8_t)(1u << index)) != 0)
    {
        seg_value |= 0x80;
    }
    I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 0x00);
    I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, 0x00);
    I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, seg_value);
    I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, select_mask);

    index++;
    if (index >= DISPLAY_DIGITS)
    {
        index = 0;
    }
    g_display_digit_index = index;
}

static uint8_t EncodeDisplayChar(char c)
{
    if ((c >= '0') && (c <= '9'))
    {
        return g_seg7_digits[c - '0'];
    }

    c = (char)toupper((unsigned char)c);
    switch (c)
    {
    case 'A':
        return g_seg7_digits[10];
    case 'B':
        return g_seg7_digits[11];
    case 'C':
        return g_seg7_digits[12];
    case 'D':
        return g_seg7_digits[13];
    case 'E':
        return g_seg7_digits[14];
    case 'F':
        return g_seg7_digits[15];
    case 'H':
        return 0x76;
    case 'L':
        return 0x38;
    case 'N':
        return 0x54;
    case 'O':
        return 0x3f;
    case 'P':
        return 0x73;
    case 'I':
        return 0x30;
    case 'R':
        return 0x50;
    case 'S':
        return 0x6d;
    case 'T':
        return 0x78;
    case 'U':
        return 0x3e;
    case 'W':
        return 0x3e;
    case 'Y':
        return 0x6e;
    case 'Z':
        return 0x5b;
    case '-':
        return 0x40;
    case '.':
        return 0x80;
    default:
        return 0x00;
    }
}

static void SampleBoardKeys(void)
{
    static const uint8_t event_map[8] = {
        KEY_EVENT_FUNC, KEY_EVENT_SAVE, KEY_EVENT_DISP, KEY_EVENT_SPEED,
        KEY_EVENT_FORMAT, KEY_EVENT_EXT, KEY_EVENT_SHIFT, KEY_EVENT_ADD
    };
    uint8_t key_value;
    uint8_t temp;
    uint8_t old_active_mask;
    uint8_t active_mask;
    uint8_t key_mask;
    uint8_t index;

    if (GPIOPinRead(GPIO_PORTJ_BASE, GPIO_PIN_0) == 0)
    {
        if (g_key_disp_count < KEY_DEBOUNCE_TICKS)
        {
            g_key_disp_count++;
        }
        else if (g_key_disp_state == 0)
        {
            g_key_disp_state = 1;
            QueueKeyEvent(KEY_EVENT_PANEL_DISP);
        }
    }
    else
    {
        g_key_disp_count = 0;
        g_key_disp_state = 0;
    }

    if (GPIOPinRead(GPIO_PORTJ_BASE, GPIO_PIN_1) == 0)
    {
        if (g_key_format_count < KEY_DEBOUNCE_TICKS)
        {
            g_key_format_count++;
        }
        else if (g_key_format_state == 0)
        {
            g_key_format_state = 1;
            QueueKeyEvent(KEY_EVENT_PANEL_FORMAT);
        }
    }
    else
    {
        g_key_format_count = 0;
        g_key_format_state = 0;
    }

    if (g_boot_phase != BOOT_PHASE_DONE)
    {
        g_board_key_raw_value = 0xff;
        g_board_key_stable_value = 0xff;
        g_board_key_debounce_count = 0;
        g_board_key_state_mask = 0;
        return;
    }

    key_value = I2C0_ReadByte(TCA6424_I2CADDR, TCA6424_INPUT_PORT0);
    temp = I2C0_ReadByte(TCA6424_I2CADDR, TCA6424_INPUT_PORT0);
    if (key_value != temp)
    {
        key_value = temp;
    }
    g_board_key_raw_value = key_value;

    if (key_value != g_board_key_stable_value)
    {
        g_board_key_debounce_count++;
        if (g_board_key_debounce_count >= 2)
        {
            old_active_mask = g_board_key_state_mask;
            g_board_key_stable_value = key_value;
            g_board_key_debounce_count = 0;
            active_mask = (uint8_t)(~g_board_key_stable_value);
            g_board_key_state_mask = active_mask;
            for (index = 0; index < 8; index++)
            {
                key_mask = (uint8_t)(1u << index);
                if (((active_mask & key_mask) != 0) && ((old_active_mask & key_mask) == 0))
                {
                    QueueKeyEvent(event_map[index]);
                    break;
                }
            }
        }
    }
    else
    {
        g_board_key_debounce_count = 0;
    }
}

static void QueueKeyEvent(uint8_t event_code)
{
    if (g_pending_key_event == KEY_EVENT_NONE)
    {
        g_pending_key_event = event_code;
    }
}

static void EmitKeyEvent(const char *name)
{
    UARTStringPut("*EVT:KEY ");
    UARTStringPut(name);
    UARTStringPut("\r\n");
}

static void ApplyVirtualKey(const char *name)
{
    uint8_t max_page;

    if (MatchToken(name, "DISP") || MatchToken(name, "DISPLAY"))
    {
        max_page = (g_message[0] != '\0') ? 2 : 1;
        if (g_display_page >= max_page)
        {
            g_display_page = 0;
        }
        else
        {
            g_display_page++;
        }
        g_scroll_offset = 0;
        g_scroll_elapsed_ms = 0;
        g_display_dirty = 1;
        return;
    }

    if (MatchToken(name, "FORMAT"))
    {
        g_format_left = (uint8_t)(g_format_left == 0);
        g_scroll_offset = 0;
        g_scroll_elapsed_ms = 0;
        g_display_dirty = 1;
        return;
    }

    if (MatchToken(name, "LEFT"))
    {
        g_format_left = 1;
        g_scroll_offset = 0;
        g_scroll_elapsed_ms = 0;
        g_display_dirty = 1;
        return;
    }

    if (MatchToken(name, "RIGHT"))
    {
        g_format_left = 0;
        g_scroll_offset = 0;
        g_scroll_elapsed_ms = 0;
        g_display_dirty = 1;
        return;
    }

    if (MatchToken(name, "TIME") || MatchToken(name, "USER1") || MatchToken(name, "FUNC"))
    {
        g_display_page = 0;
        g_scroll_offset = 0;
        g_scroll_elapsed_ms = 0;
        g_display_dirty = 1;
        return;
    }

    if (MatchToken(name, "DATE") || MatchToken(name, "USER2") || MatchToken(name, "SAVE"))
    {
        g_display_page = 1;
        g_scroll_offset = 0;
        g_scroll_elapsed_ms = 0;
        g_display_dirty = 1;
        return;
    }

    if (MatchToken(name, "MSG") || MatchToken(name, "EXT"))
    {
        if (g_message[0] != '\0')
        {
            g_display_page = 2;
            g_scroll_offset = 0;
            g_scroll_elapsed_ms = 0;
            g_display_dirty = 1;
        }
        return;
    }

    if (MatchToken(name, "SPEED"))
    {
        if (g_scroll_speed_ms == SCROLL_SLOW_MS)
        {
            g_scroll_speed_ms = SCROLL_MEDIUM_MS;
        }
        else if (g_scroll_speed_ms == SCROLL_MEDIUM_MS)
        {
            g_scroll_speed_ms = SCROLL_FAST_MS;
        }
        else
        {
            g_scroll_speed_ms = SCROLL_SLOW_MS;
        }
        g_scroll_elapsed_ms = 0;
        return;
    }

    if (MatchToken(name, "SHIFT"))
    {
        g_display_dirty = 1;
        return;
    }

    if (MatchToken(name, "ADD"))
    {
        g_beep_remaining_ms = 200;
        g_beep_output_state = 1;
        SetBeepOutput(1);
        return;
    }
}

static bool MatchToken(const char *token, const char *pattern)
{
    size_t token_len;
    size_t pattern_len;
    size_t min_len;
    size_t index;

    if ((token == 0) || (pattern == 0))
    {
        return false;
    }

    token_len = strlen(token);
    pattern_len = strlen(pattern);
    min_len = pattern_len;
    for (index = 0; index < pattern_len; index++)
    {
        if (islower((unsigned char)pattern[index]))
        {
            min_len = index;
            break;
        }
    }

    if ((token_len < min_len) || (token_len > pattern_len))
    {
        return false;
    }

    for (index = 0; index < token_len; index++)
    {
        if (toupper((unsigned char)token[index]) != toupper((unsigned char)pattern[index]))
        {
            return false;
        }
    }

    return true;
}

static bool ParseUnsigned(const char *token, uint32_t *value)
{
    uint32_t result = 0;
    size_t index;

    if ((token == 0) || (*token == '\0'))
    {
        return false;
    }

    for (index = 0; token[index] != '\0'; index++)
    {
        if (!isdigit((unsigned char)token[index]))
        {
            return false;
        }
        result = result * 10 + (uint32_t)(token[index] - '0');
    }

    *value = result;
    return true;
}

static bool ParseHexByte(const char *token, uint8_t *value)
{
    uint8_t result = 0;
    uint8_t index;
    char c;
    uint8_t nibble;

    if ((token == 0) || (strlen(token) != 2))
    {
        return false;
    }

    for (index = 0; index < 2; index++)
    {
        c = token[index];
        if ((c >= '0') && (c <= '9'))
        {
            nibble = (uint8_t)(c - '0');
        }
        else if ((c >= 'A') && (c <= 'F'))
        {
            nibble = (uint8_t)(c - 'A' + 10);
        }
        else if ((c >= 'a') && (c <= 'f'))
        {
            nibble = (uint8_t)(c - 'a' + 10);
        }
        else
        {
            return false;
        }
        result = (uint8_t)((result << 4) | nibble);
    }

    *value = result;
    return true;
}

static char *NextToken(char **cursor)
{
    char *start;

    SkipSpaces(cursor);
    if (**cursor == '\0')
    {
        return 0;
    }

    start = *cursor;
    while ((**cursor != '\0') && (**cursor != ' ') && (**cursor != '\t'))
    {
        (*cursor)++;
    }

    if (**cursor != '\0')
    {
        **cursor = '\0';
        (*cursor)++;
    }

    return start;
}

static void SkipSpaces(char **cursor)
{
    while ((**cursor == ' ') || (**cursor == '\t'))
    {
        (*cursor)++;
    }
}

static uint8_t CollectTokens(char *cursor, char *tokens[], uint8_t max_tokens)
{
    uint8_t count = 0;
    char *token;

    while (count < max_tokens)
    {
        token = NextToken(&cursor);
        if (token == 0)
        {
            break;
        }
        tokens[count++] = token;
    }

    return count;
}

static bool BuildDateFieldList(char *tokens[], uint8_t count, bool selected[3])
{
    uint8_t index;
    uint8_t any = 0;

    selected[0] = false;
    selected[1] = false;
    selected[2] = false;

    if (count == 0)
    {
        selected[0] = true;
        selected[1] = true;
        selected[2] = true;
        return true;
    }

    for (index = 0; index < count; index++)
    {
        if (MatchToken(tokens[index], "YEAR"))
        {
            selected[0] = true;
            any = 1;
            continue;
        }
        if (MatchToken(tokens[index], "MONTH"))
        {
            selected[1] = true;
            any = 1;
            continue;
        }
        if (MatchToken(tokens[index], "DATE"))
        {
            selected[2] = true;
            any = 1;
            continue;
        }
        return false;
    }

    return any != 0;
}

static bool BuildTimeFieldList(char *tokens[], uint8_t count, bool selected[3])
{
    uint8_t index;
    uint8_t any = 0;

    selected[0] = false;
    selected[1] = false;
    selected[2] = false;

    if (count == 0)
    {
        selected[0] = true;
        selected[1] = true;
        selected[2] = true;
        return true;
    }

    for (index = 0; index < count; index++)
    {
        if (MatchToken(tokens[index], "HOUR"))
        {
            selected[0] = true;
            any = 1;
            continue;
        }
        if (MatchToken(tokens[index], "MINute"))
        {
            selected[1] = true;
            any = 1;
            continue;
        }
        if (MatchToken(tokens[index], "SECond"))
        {
            selected[2] = true;
            any = 1;
            continue;
        }
        return false;
    }

    return any != 0;
}

static bool IsLeapYear(uint16_t year)
{
    return (((year % 4) == 0) && (((year % 100) != 0) || ((year % 400) == 0)));
}

static uint8_t DaysInMonth(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if ((month < 1) || (month > 12))
    {
        return 0;
    }
    if ((month == 2) && IsLeapYear(year))
    {
        return 29;
    }
    return days[month - 1];
}

static bool IsValidDate(uint16_t year, uint8_t month, uint8_t date)
{
    uint8_t max_day = DaysInMonth(year, month);
    return (max_day != 0) && (date >= 1) && (date <= max_day);
}

static void AdvanceClockOneSecond(void)
{
    g_now.second++;
    if (g_now.second < 60)
    {
        g_display_dirty = 1;
        CheckAlarmTrigger();
        return;
    }

    g_now.second = 0;
    g_now.minute++;
    if (g_now.minute < 60)
    {
        g_display_dirty = 1;
        CheckAlarmTrigger();
        return;
    }

    g_now.minute = 0;
    g_now.hour++;
    if (g_now.hour < 24)
    {
        g_display_dirty = 1;
        CheckAlarmTrigger();
        return;
    }

    g_now.hour = 0;
    g_now.date++;
    if (g_now.date <= DaysInMonth(g_now.year, g_now.month))
    {
        g_display_dirty = 1;
        CheckAlarmTrigger();
        return;
    }

    g_now.date = 1;
    g_now.month++;
    if (g_now.month <= 12)
    {
        g_display_dirty = 1;
        CheckAlarmTrigger();
        return;
    }

    g_now.month = 1;
    g_now.year++;
    g_display_dirty = 1;
    CheckAlarmTrigger();
}

static void CheckAlarmTrigger(void)
{
    if ((g_alarm_enabled != 0) &&
        (g_now.hour == g_alarm_hour) &&
        (g_now.minute == g_alarm_minute) &&
        (g_now.second == g_alarm_second))
    {
        g_beep_remaining_ms = DEFAULT_ALARM_BEEP_MS;
        g_beep_output_state = 1;
        SetBeepOutput(1);
    }
}

void S800_PWM_Init(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOK);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOK))
    {
    }
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_PWM0))
    {
    }

    GPIOPinConfigure(GPIO_PK5_M0PWM7);
    GPIOPinTypePWM(GPIO_PORTK_BASE, GPIO_PIN_5);
    PWMClockSet(PWM0_BASE, PWM_SYSCLK_DIV_1);
    PWMGenConfigure(PWM0_BASE, PWM_GEN_3, PWM_GEN_MODE_DOWN | PWM_GEN_MODE_NO_SYNC);
    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_3, BEEP_PWM_PERIOD);
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_7, BEEP_PWM_PERIOD / 4);
    PWMGenEnable(PWM0_BASE, PWM_GEN_3);
    SetBeepOutput(0);
}

void S800_UART_Init(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOA))
    {
    }

    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTConfigSetExpClk(UART0_BASE, ui32SysClock, 115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
    UARTFIFOLevelSet(UART0_BASE, UART_FIFO_TX2_8, UART_FIFO_RX7_8);
}

void S800_GPIO_Init(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF))
    {
    }
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOJ);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOJ))
    {
    }
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPION))
    {
    }

    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_0);
    GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_0);
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 0);
    GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, 0);

    GPIOPinTypeGPIOInput(GPIO_PORTJ_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    GPIOPadConfigSet(GPIO_PORTJ_BASE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
}

void S800_I2C0_Init(void)
{
    uint8_t result;

    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    GPIOPinConfigure(GPIO_PB2_I2C0SCL);
    GPIOPinConfigure(GPIO_PB3_I2C0SDA);
    GPIOPinTypeI2CSCL(GPIO_PORTB_BASE, GPIO_PIN_2);
    GPIOPinTypeI2C(GPIO_PORTB_BASE, GPIO_PIN_3);

    I2CMasterInitExpClk(I2C0_BASE, ui32SysClock, true);
    I2CMasterEnable(I2C0_BASE);

    result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_CONFIG_PORT0, 0xff);
    result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_POLINVERT_PORT0, 0x00);
    result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_CONFIG_PORT1, 0x00);
    result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_CONFIG_PORT2, 0x00);
    result = I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_CONFIG, 0x00);
    result = I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_OUTPUT, 0xff);
    (void)result;
}

uint8_t I2C0_WriteByte(uint8_t DevAddr, uint8_t RegAddr, uint8_t WriteData)
{
    uint8_t rop;

    while (I2CMasterBusy(I2C0_BASE))
    {
    }
    I2CMasterSlaveAddrSet(I2C0_BASE, DevAddr, false);
    I2CMasterDataPut(I2C0_BASE, RegAddr);
    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_START);
    while (I2CMasterBusy(I2C0_BASE))
    {
    }
    rop = (uint8_t)I2CMasterErr(I2C0_BASE);

    I2CMasterDataPut(I2C0_BASE, WriteData);
    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);
    while (I2CMasterBusy(I2C0_BASE))
    {
    }

    rop = (uint8_t)I2CMasterErr(I2C0_BASE);
    return rop;
}

uint8_t I2C0_ReadByte(uint8_t DevAddr, uint8_t RegAddr)
{
    uint8_t value;

    while (I2CMasterBusy(I2C0_BASE))
    {
    }
    I2CMasterSlaveAddrSet(I2C0_BASE, DevAddr, false);
    I2CMasterDataPut(I2C0_BASE, RegAddr);
    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_SINGLE_SEND);
    while (I2CMasterBusBusy(I2C0_BASE))
    {
    }
    Delay(1);
    I2CMasterSlaveAddrSet(I2C0_BASE, DevAddr, true);
    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_SINGLE_RECEIVE);
    while (I2CMasterBusBusy(I2C0_BASE))
    {
    }
    value = I2CMasterDataGet(I2C0_BASE);
    Delay(1);
    return value;
}

void SysTick_Handler(void)
{
    static uint8_t display_tick = 0;
    static uint8_t key_tick = 0;

    if (g_beep_remaining_ms != 0)
    {
        g_beep_remaining_ms--;
        if (g_beep_remaining_ms == 0)
        {
            g_beep_output_state = 0;
            SetBeepOutput(0);
        }
    }

    display_tick++;
    if (display_tick >= DISPLAY_REFRESH_MS)
    {
        display_tick = 0;
        g_display_refresh_pending = 1;
    }

    key_tick++;
    if (key_tick >= KEY_SCAN_PERIOD_MS)
    {
        key_tick = 0;
        g_key_scan_pending = 1;
    }

    if ((g_display_on != 0) && (g_display_page == 2) && (strlen(g_message) > DISPLAY_DIGITS))
    {
        g_scroll_elapsed_ms++;
    }

    if (g_boot_splash_ms != 0)
    {
        g_boot_splash_ms--;
        if (g_boot_splash_ms == 0)
        {
            AdvanceBootSequence();
        }
    }

    g_tick_1s++;
    if (g_tick_1s >= 1000)
    {
        g_tick_1s = 0;
        g_uptime_s++;
        AdvanceClockOneSecond();
    }
}

void UART0_Handler(void)
{
    int32_t uart0_int_status;

    uart0_int_status = UARTIntStatus(UART0_BASE, true);
    UARTIntClear(UART0_BASE, uart0_int_status);

    while (UARTCharsAvail(UART0_BASE))
    {
        char ch = (char)(UARTCharGetNonBlocking(UART0_BASE) & 0xff);

        if ((ch == '\r') || (ch == '\n'))
        {
            if (g_uart_line_overflow != 0)
            {
                g_uart_line_overflow = 0;
                g_uart_line_length = 0;
                g_uart_error_len = 1;
                continue;
            }

            if (g_uart_line_length == 0)
            {
                continue;
            }

            if (g_uart_ready_index >= 0)
            {
                g_uart_line_length = 0;
                g_uart_error_busy = 1;
                continue;
            }

            g_uart_lines[g_uart_write_index][g_uart_line_length] = '\0';
            g_uart_ready_index = (int8_t)g_uart_write_index;
            g_uart_write_index ^= 1u;
            g_uart_line_length = 0;
            g_uart_lines[g_uart_write_index][0] = '\0';
            continue;
        }

        if (g_uart_line_overflow != 0)
        {
            continue;
        }

        if (g_uart_line_length >= UART_FRAME_MAX_LEN)
        {
            g_uart_line_overflow = 1;
            continue;
        }

        g_uart_lines[g_uart_write_index][g_uart_line_length++] = ch;
    }
}
