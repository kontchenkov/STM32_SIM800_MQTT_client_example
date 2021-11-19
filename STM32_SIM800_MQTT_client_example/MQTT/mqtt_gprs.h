#ifndef __MQTT_GSM_HEADER
#define __MQTT_GSM_HEADER
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "uart_driver.h"
#include "mqtt_commands.h"
#include "settings.h"
#include "LEDs_and_buttons.h"


#define RX_ATCOMMAND_BUFFER_LENGTH 64 // длина буфера, в который помещаются данные,
																			// полученные от модуля SIM800, воспринимаемые
																			// как ответ на команду, адресованную самому
																			// модулю
#define RX_MQTTMESSAGE_BUFFER_LENGTH 64 // длина буфера, в который помещаются данные,
																			// полученные от модуля SIM800, воспринимаемые
																			// как ответ на сообщение, принятое по протоколу 
																			// MQTT
typedef enum command_status_e { 
	ERROR_COMMAND_STATUS,
	OK_COMMAND_STATUS,
	CLOSED_COMMAND_STATUS
} command_status_t;



extern TaskHandle_t taskInitSIM800Handler;  // хэндлер задачи инициализации модуля SIM800, служит для снятия этой задачи 
																						// после выполнения инициализации
extern TaskHandle_t taskUartHandler;

// задачи FreeRTOS
void taskInitSIM800(void* params); 
void taskUart(void* params);
void taskMessageFromSIM800Take(void* params);
void taskSendPINGREQ(void *params);
void taskButtonPressSending(void *params);
void taskMQTTMessageProcessing(void* params);

// 
void InitRTOSStructuresToWorkWithMQTT(void);
command_status_t SendMessageByTCP_With_Wait_Acknowledge(char * message,
																												uint8_t length_of_message,
																												xSemaphoreHandle *ACK_given,
																												uint32_t period);

command_status_t CreateTasksAndSetMQTTConnection(void);
#endif
