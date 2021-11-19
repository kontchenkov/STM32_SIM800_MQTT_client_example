#ifndef __UART_DRIVER_HEADER
#define __UART_DRIVER_HEADER
// размер очереди usartSendQueue и usartSendQueueDouble
#define QUEUE_SIZE 100

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <stm32f10x_gpio.h>
#include <stm32f10x_rcc.h>
#include <stm32f10x_usart.h>

extern QueueHandle_t usartSendQueue; // очередь, в которую помещаются байты, полученные в обработчике прерывания от USART1 (SIM800)
																		 // размер очереди определяется значением QUEUE_SIZE, заданным в uart_driver.h
																		 
typedef enum string_send_status_e { 
	ERROR_SEND_STRING_STATUS,
	OK_SEND_STRING_STATUS,
} string_send_status_t;

void USART_init(void);
string_send_status_t SIM800_SendStrWithLen(char * str, uint8_t len);
string_send_status_t SIM800_SendStr(char * str);
string_send_status_t USB_UART_SendStrWithLen(char *buffer, uint16_t length);


#endif
