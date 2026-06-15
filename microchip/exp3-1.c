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

#define SYSTICK_FREQUENCY           1000
#define UART_FRAME_MAX_LEN          64
#define MSG_MAX_LEN                 32
#define KEY_NAME_MAX_LEN            15
#define DEFAULT_YEAR                2026
#define DEFAULT_MONTH               1
#define DEFAULT_DATE                1
#define DEFAULT_HOUR                0
#define DEFAULT_MINUTE              0
#define DEFAULT_SECOND              0
#define DEFAULT_ALARM_BEEP_MS       1000

#define TCA6424_I2CADDR             0x22
#define PCA9557_I2CADDR             0x18

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
void SysTick_Handler(void);
void UART0_Handler(void);

static void UARTStringPut(const char *message);
static void UARTReplyOK(const char *data);
static void UARTReplyError(const char *reason);
static void ResetClockState(void);
static void ResetProtocolState(void);
static void ProcessPendingUart(void);
static void ProcessCommand(char *line);
static void HandleSetCommand(char *subcommand, char *cursor);
static void HandleGetCommand(char *subcommand, char *cursor);
static void HandleSetDate(char *tokens[], uint8_t count);
static void HandleSetTime(char *tokens[], uint8_t count);
static void HandleSetAlarm(char *tokens[], uint8_t count);
static void HandleGetDate(char *tokens[], uint8_t count);
static void HandleGetTime(char *tokens[], uint8_t count);
static void HandleGetAlarm(char *tokens[], uint8_t count);
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
static volatile char g_uart_lines[2][UART_FRAME_MAX_LEN + 1];
static char g_message[MSG_MAX_LEN + 1];
static char g_last_key[KEY_NAME_MAX_LEN + 1];

int main(void)
{
    ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_16MHZ | SYSCTL_OSC_INT | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 20000000);

    SysTickPeriodSet(ui32SysClock / SYSTICK_FREQUENCY);
    SysTickEnable();
    SysTickIntEnable();

    S800_GPIO_Init();
    S800_I2C0_Init();
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
    g_beep_remaining_ms = 0;
    GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, 0);
}

static void ResetProtocolState(void)
{
    memset((void *)g_uart_lines, 0, sizeof(g_uart_lines));
    memset(g_message, 0, sizeof(g_message));
    memset(g_last_key, 0, sizeof(g_last_key));
    g_uptime_s = 0;
    g_tick_1s = 0;
    g_uart_write_index = 0;
    g_uart_line_length = 0;
    g_uart_line_overflow = 0;
    g_uart_ready_index = -1;
    g_uart_error_len = 0;
    g_uart_error_busy = 0;
    g_led_value = 0x00;
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

    if (!IsValidDate(year, month, date))
    {
        UARTReplyError("RANGE");
        return;
    }

    g_now.year = year;
    g_now.month = month;
    g_now.date = date;
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

    g_now.hour = hour;
    g_now.minute = minute;
    g_now.second = second;
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

    if ((count == 1) && MatchToken(tokens[0], "OFF"))
    {
        g_alarm_enabled = 0;
        UARTReplyOK(0);
        return;
    }

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
        ApplyDisplayOutput();
        UARTReplyOK(0);
        return;
    }
    if (MatchToken(tokens[0], "OFF"))
    {
        g_display_on = 0;
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
        UARTReplyOK(0);
        return;
    }
    if (MatchToken(tokens[0], "RIGHT"))
    {
        g_format_left = 0;
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
        UARTReplyOK(0);
        return;
    }
    if (MatchToken(tokens[0], "NIGHT"))
    {
        g_mode_day = 0;
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
    GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, GPIO_PIN_1);
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
        CheckAlarmTrigger();
        return;
    }

    g_now.second = 0;
    g_now.minute++;
    if (g_now.minute < 60)
    {
        CheckAlarmTrigger();
        return;
    }

    g_now.minute = 0;
    g_now.hour++;
    if (g_now.hour < 24)
    {
        CheckAlarmTrigger();
        return;
    }

    g_now.hour = 0;
    g_now.date++;
    if (g_now.date <= DaysInMonth(g_now.year, g_now.month))
    {
        CheckAlarmTrigger();
        return;
    }

    g_now.date = 1;
    g_now.month++;
    if (g_now.month <= 12)
    {
        CheckAlarmTrigger();
        return;
    }

    g_now.month = 1;
    g_now.year++;
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
        GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, GPIO_PIN_1);
    }
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
    GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_1);
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 0);
    GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0 | GPIO_PIN_1, 0);

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
    if (g_beep_remaining_ms != 0)
    {
        g_beep_remaining_ms--;
        if (g_beep_remaining_ms == 0)
        {
            GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, 0);
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
