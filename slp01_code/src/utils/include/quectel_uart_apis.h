/******************************************************************************

            quectel_uart_apis.h

******************************************************************************/

/******************************************************************************/
#ifndef __QUECTEL_UART_APIS_H__
#define __QUECTEL_UART_APIS_H__

/**************************************************************************
*                                 DEFINE
***************************************************************************/
typedef struct{
	qapi_UART_Handle_t  hdlr;
	qapi_UART_Port_Id_e port_id;
	char *tx_buff;
	uint32_t tx_len;
	char *rx_buff;
	uint32_t rx_len;
	uint32_t baudrate;
}QT_UART_CONF_PARA;

#define QT_UART_PORT_01		QAPI_UART_PORT_001_E
#define QT_UART_PORT_02		QAPI_UART_PORT_002_E
#define QT_UART_PORT_03		QAPI_UART_PORT_003_E

#define QT_UART_ECHO_ENABLE


/**************************************************************************
*                           FUNCTION DECLARATION
***************************************************************************/
void uart_init(void);
void uart_print(char *buff, uint16_t len);
void uart_recv(void);
void qt_uart_dbg(const char* fmt, ...);

#endif /* __QUECTEL_UART_APIS_H__ */

