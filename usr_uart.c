/*
  License: Following license.txt (Nordic Semiconductor)

  http://qiita.com/mt08/items/50683e3e135ba13aed21

  */

#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "ble_hids.h"
#include "bsp.h"

#define UART_TX_BUF_SIZE 256                                                            /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE 1                                                              /**< UART RX buffer size. */

#define INPUT_REPORT_KEYS_INDEX          0
#define INPUT_REPORT_KEYS_MAX_LEN        8
#define MODIFIER_KEY_POS                 0
#define SCAN_CODE_POS                    2

extern ble_hids_t m_hids;
extern bool m_in_boot_mode;
extern uint16_t  m_conn_handle;

/*
modifier
bit 0: left control
bit 1: left shift
bit 2: left alt
bit 3: left GUI (Win/Apple/Meta key)
bit 4: right control
bit 5: right shift
bit 6: right alt
bit 7: right GUI
*/
#define MODIFIER_LCTRL  (0x01)
#define MODIFIER_LSHIFT (0x02)

typedef struct {
    uint8_t asc;
    uint16_t scn;
} S_KEYTBL;


const S_KEYTBL sKeysTable[] = {
    { 0x1B, 0x0029 }, //ESC
    { 0x7F, 0x004C }, //DEL
    { '0', 0x0027 },
    { ' ', 0x002C },
    //
    { '-', 0x002D },
    { '=', 0x002E },
    { '[', 0x002F },
    { ']', 0x0030 },
    { '\\', 0x0031 },
    { '_', 0x022D },
    { '+', 0x022E },
    { '{', 0x022F },
    { '}', 0x0230 },
    { '|', 0x0231 },
    //
    { ';', 0x0033 },
    { '\'', 0x0034 },
    { '`', 0x0035 },
    { ',', 0x0036 },
    { '.', 0x0037 },
    { '/', 0x0038 },
    { ':', 0x0233 },
    { '\"', 0x0234 },
    { '~', 0x0235 },
    { '<', 0x0236 },
    { '>', 0x0237 },
    { '?', 0x0238 },
    //
    { '!', 0x021E },
    { '@', 0x021F },
    { '#', 0x0220 },
    { '$', 0x0221 },
    { '%', 0x0222 },
    { '^', 0x0223 },
    { '&', 0x0224 },
    { '*', 0x0225 },
    { '(', 0x0226 },
    { ')', 0x0227 },

    //
    { 0x00, 0x0000 },
};


static uint16_t convert_asc_to_scantbl(uint8_t ch)
{
    uint16_t ret = 0;
    if ((0x61 <= ch) && (ch <= 0x7a)) { ret = ch + 0x03 - 0x60; }
    else if ((0x41 <= ch) && (ch <= 0x5a)) { ret = (MODIFIER_LSHIFT << 8) + (ch + 0x03 - 0x40); }
    else if ((0x31 <= ch) && (ch <= 0x39)) { ret = ch + 0x1E - 0x31; }
    else if ((0x01 <= ch) && (ch <= 0x1a)) { ret = (MODIFIER_LCTRL << 8) + (ch + 0x03); }
    else {
        const S_KEYTBL *pkey;
        for (pkey = &sKeysTable[0]; (pkey->asc != 0x00); pkey++)
        {
            if (pkey->asc == ch) { ret = pkey->scn; break; }
        }
        //if (pkey->asc == 0x00) { ret = 0; }
    }
    return ret;
}


static uint32_t send_key_scan_press_release1(ble_hids_t *p_hids, uint8_t ch)
{
    uint32_t err_code;
    uint8_t  data[INPUT_REPORT_KEYS_MAX_LEN];

    // Reset the data buffer. 
    memset(data, 0, sizeof(data));

    uint16_t scancode = convert_asc_to_scantbl(ch);
    data[SCAN_CODE_POS] = (uint8_t)(scancode & 0xff);
    data[MODIFIER_KEY_POS] = (uint8_t)(scancode >> 8);

    if (!m_in_boot_mode)
    {
        err_code = ble_hids_inp_rep_send(p_hids,
            INPUT_REPORT_KEYS_INDEX,
            INPUT_REPORT_KEYS_MAX_LEN,
            data);
    }
    else
    {
        err_code = ble_hids_boot_kb_inp_rep_send(p_hids,
            INPUT_REPORT_KEYS_MAX_LEN,
            data);
    }

    return err_code;
}


void uart_error_handle(app_uart_evt_t * p_event)
{
    switch (p_event->evt_type)
    {
    case APP_UART_DATA_READY:
    case APP_UART_DATA:
    {
        //ECHO Back
        uint8_t ch;
        if (app_uart_get(&ch) == NRF_SUCCESS)
        {
            if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
            {
                //        while(app_uart_put(ch) != NRF_SUCCESS);
#if 1
                // print Character Code.
                char *str = "0123456789ABCDEF";
                while (app_uart_put('0') != NRF_SUCCESS);
                while (app_uart_put('x') != NRF_SUCCESS);
                while (app_uart_put(str[(ch >> 4) & 0x0f]) != NRF_SUCCESS);
                while (app_uart_put(str[(ch) & 0x0f]) != NRF_SUCCESS);
                while (app_uart_put(' ') != NRF_SUCCESS);
#endif
                send_key_scan_press_release1(&m_hids, ch);
                send_key_scan_press_release1(&m_hids, 0x00);
            }
        }

        break;
    }
    case APP_UART_COMMUNICATION_ERROR:
        APP_ERROR_HANDLER(p_event->data.error_communication);
        break;
    case APP_UART_FIFO_ERROR:
        APP_ERROR_HANDLER(p_event->data.error_code);
        break;
    case APP_UART_TX_EMPTY:
        break;
    default:
        break;
    }
}
void usr_uart_init(void)
{
    uint32_t err_code = NRF_SUCCESS;
    const app_uart_comm_params_t comm_params =
    {
        RX_PIN_NUMBER,
        TX_PIN_NUMBER,
        RTS_PIN_NUMBER,
        CTS_PIN_NUMBER,
        APP_UART_FLOW_CONTROL_DISABLED,
        false,
        UART_BAUDRATE_BAUDRATE_Baud38400
    };

    APP_UART_FIFO_INIT(&comm_params,
        UART_RX_BUF_SIZE,
        UART_TX_BUF_SIZE,
        uart_error_handle,
        APP_IRQ_PRIORITY_LOW,
        err_code);
    UNUSED_VARIABLE(err_code);
}
