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

extern ble_hids_t m_hids;
extern bool m_in_boot_mode;
extern uint16_t  m_conn_handle;

#define CODE_START (0xFE)
#define CODE_END   (0xFD)

void uart_error_handle(app_uart_evt_t * p_event)
{
    static int recv_data_index = 0;
    static uint8_t recv_data[10];

    switch (p_event->evt_type)
    {
    case APP_UART_DATA_READY:
    case APP_UART_DATA:
    {
        //ECHO Back
        uint8_t ch;
        if ((app_uart_get(&ch) == NRF_SUCCESS) &&
            (m_conn_handle != BLE_CONN_HANDLE_INVALID))
        {
            switch (ch)
            {
            case CODE_START:
                recv_data_index = 0;
                break;
            case CODE_END:
                if (recv_data_index == 8) {
                    //Send Data
                    if (!m_in_boot_mode)
                    {
                        ble_hids_inp_rep_send(&m_hids,
                            INPUT_REPORT_KEYS_INDEX,
                            INPUT_REPORT_KEYS_MAX_LEN,
                            recv_data);
                    }
                    else
                    {
                        ble_hids_boot_kb_inp_rep_send(&m_hids,
                            INPUT_REPORT_KEYS_MAX_LEN,
                            recv_data);
                    }
                }
                break;
            default:
                if (recv_data_index < 8)
                {
                    recv_data[recv_data_index] = ch;
                    recv_data_index++;
                }
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
