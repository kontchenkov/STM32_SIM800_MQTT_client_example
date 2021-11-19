#include "main.h"
#include "mqtt_gprs.h"
#include "string.h"
#include "semphr.h"

/// RTOS
QueueHandle_t usartSendQueueDouble;					// очередь, в которую копируются байты из usartSendQueue 
																						//(нужна для осуществления эха на USART2 от GSM-модуля SIM800)
																						// размер очереди определяется значением QUEUE_SIZE, заданным в uart_driver.h
static xSemaphoreHandle sem_null_terminal_string_given= NULL; // семафор, выдаваемый каждый раз после прихода строки,
																										          // заканчивающейся символом '\n'
static xSemaphoreHandle sem_invitation_to_enter_TCP_string_given= NULL; // семафор, выдаваемый каждый раз после получения,
																										// от модуля SIM800 символа '>' 
																										// (приглашения ввода строки, передаваемой по протоколу TCP;
																										// это приглашение ввода SIM800 выдает после успешного выполнения
																										// команды AT+CIPSEND

static xSemaphoreHandle sem_MQTT_message_given = NULL; // семафор, выдаваемый при условии, что пришло сообщение вида ";xxxx<"
                                                    // (пока реализована реакция только на ";LEDS ON<" и ";LEDS OFF<")

static xSemaphoreHandle sem_PINGRESP_given = NULL; // семафор, выдаваемый при условии, что получено сообщение PINGRESP

static xSemaphoreHandle mutex_MQTT = NULL; // мьютекс для разграничения доступа к пересылке сообщений по протоколу MQTT

static xSemaphoreHandle sem_CONNACK_given = NULL; // семафор, выдаваемый при получения сообщения CONNACK

static xSemaphoreHandle sem_SUBACK_given = NULL; // семафор, выдаваемый при получении сообщения SUBACK

uint8_t AcknowledgeFlag = 0; // флаг, указывающий, что нужно ждать Acknowledge (CONNACK, SUBACK) по MQTT
static xSemaphoreHandle mutex_AcknowledgeFlag = NULL; // мьютекс, защищающий флаг AcknowledgeFlag

TaskHandle_t taskInitSIM800Handler = NULL; // хэндлер задачи инициализации модуля SIM800, служит для снятия этой задачи 
																					 // после выполнения инициализации
TaskHandle_t taskMessageFromSIM800TakeHandler = NULL; // хэндлер задачи обработки данных от модуля SIM800 как сообщений
TaskHandle_t taskButtonPressSendingHandler = NULL; // хэндлер задачи оповещения о нажатии кнопки
TaskHandle_t taskMQTTMessageProcessingHandler = NULL; // хэндлер задачи обработки MQTT сообщений, получаемых от сервера
TaskHandle_t taskSendPINGREQHandler = NULL; //хэндлер задачи keep alive
TaskHandle_t taskUartHandler = NULL; // хэндлер задачи эхо от модуля SIM800
TaskHandle_t taskLEDBlinkRarelyHandler = NULL; //хэндлер задачи мигания светодиодом 1 раз в 2 секундs
TaskHandle_t taskLEDBlinkFrequentlyHandler = NULL; //хэндлер задачи мигания светодиодом 5 раз в секунду
/// RTOS

// эта задача служит для пересылки всех данных, передаваемых с USART1 (SIM800), на USART2
void taskUart(void* params)
{
	uint8_t usartData;

	while (1)
	{
		if(xQueueReceive(usartSendQueue, &usartData, portMAX_DELAY))
		{
			xQueueSend(usartSendQueueDouble, ( void* ) &usartData, portMAX_DELAY);
			USART_SendData(USART2, usartData);
			while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET) {};
		}		
	}
}

// задача, обрабатывающая получаемые с модуля SIM800 данные как сообщения
uint8_t rxBufferDouble[RX_ATCOMMAND_BUFFER_LENGTH] = { '\0' }; // в этот буфер копируется содержимое буфера rxBuffer после того как пришел символ '\n'
uint8_t rxBufferMQTTDouble[RX_MQTTMESSAGE_BUFFER_LENGTH] = { '\0' }; // в этот буфер копируется содержимое буфера rxBufferMQTT после того как пришел символ '\n'
void taskMessageFromSIM800Take(void* params)
{
	uint8_t MessageByte;
	
	uint8_t PINGRESP_IsReceiving = 0;
	
  uint8_t rxBuffer[RX_ATCOMMAND_BUFFER_LENGTH] = { '\0' }; // в этот буфер по одному кладутся байты, принимаемые по USART1
  uint8_t rxBufferIndex = 0;
  uint8_t rxBufferLength;
	
	uint8_t rxBufferMQTT[RX_MQTTMESSAGE_BUFFER_LENGTH] = {'\0'}; // в этот буфер по одному кладутся байты, принимаемые по USART1, относящиеся к MQTT-сообщению
	uint8_t rxBufferMQTTIndex = 0;
  uint8_t rxBufferMQTTLength;	
  uint8_t MQTT_MessageIsReceiving = 0;		

  // CONNACK
	char CONNACK_string[4] = {0x20, 0x02, 0x00, 0x00};
	uint8_t CONNACK_string_length = 4;
	uint8_t rxBufferCONNACK[10];
	uint8_t rxBufferCONNACKIndex = 0;
	uint8_t CONNACK_IsReceiving = 0;
	
  // SUBACK	
  char SUBACK_string[5] = {
	                         0x90,
                           0x03,
                           (char)(((uint16_t)SUBSCRIBE_PACKET_IDENTIFIER)>>8),
                           (char)((((uint16_t)SUBSCRIBE_PACKET_IDENTIFIER)<<8)>>8),
                           0x00}; 
	uint8_t SUBACK_string_length = 5;
	uint8_t rxBufferSUBACK[10];
	uint8_t rxBufferSUBACKIndex = 0;
	uint8_t SUBACK_IsReceiving = 0;	
	
  while (1)
	{
		if(xQueueReceive(usartSendQueueDouble, &MessageByte, ( TickType_t )7000) == pdPASS)
		{
			if(MessageByte == '>') // если получено приглашение для ввода сообщения, передаваемого по TCP
			{
				rxBufferIndex = 0;
				xSemaphoreGive(sem_invitation_to_enter_TCP_string_given);
			}
			else if(MessageByte == 0x3B) // если получен символ начала MQTT-сообщения
			{
				rxBufferMQTTIndex = 0;
				MQTT_MessageIsReceiving = 1; // флаг остается установленным в течение всего времени приема MQTT-сообщения
			}
			else if(MessageByte == 0x3C) // если получен символ конца MQTT-сообщения
			{
				rxBufferMQTTLength = rxBufferMQTTIndex;
				memcpy(rxBufferMQTTDouble, rxBufferMQTT, rxBufferMQTTLength); // копируем из rxBufferMQTT
				                                                              // rxBufferMQTTLength байтов в массив rxBufferDouble
				rxBufferMQTTIndex = 0;
				MQTT_MessageIsReceiving = 0; 
				xSemaphoreGive(sem_MQTT_message_given); // выдаем семафор, что принято MQTT-сообщение			
			}
      else if (MessageByte == '\n') // если получен символ конца строки
			{				        
				if(!MQTT_MessageIsReceiving && !CONNACK_IsReceiving && !CONNACK_IsReceiving && !PINGRESP_IsReceiving)
				{
					rxBufferLength = rxBufferIndex;
					memcpy(rxBufferDouble, rxBuffer, rxBufferLength); // копируем из rxBuffer 
					                                                  // rxBufferLength байтов в массив rxBufferDouble (вместе с символом '\n')
					rxBufferIndex = 0;
					memset(rxBuffer, 0, rxBufferLength); // обнуляем все элементы в rxBuffer
					xSemaphoreGive(sem_null_terminal_string_given); // выдаем семафор	
				}								
			}
      else if(MessageByte == 0xD0)
			{
				PINGRESP_IsReceiving = 1;
			}
      else if(MessageByte == 0x00 && PINGRESP_IsReceiving == 1)
      {
				xSemaphoreGive(sem_PINGRESP_given);
				PINGRESP_IsReceiving = 0;
			}
			else if(AcknowledgeFlag == 1)
			{
			  if(MessageByte == 32)
				{
					CONNACK_IsReceiving = 1;
					rxBufferCONNACKIndex = 0;
				}
			  if(MessageByte == 144)
				{
					SUBACK_IsReceiving = 1;
					rxBufferSUBACKIndex = 0;
				}
				if(CONNACK_IsReceiving)
				{
					rxBufferCONNACK[rxBufferCONNACKIndex] = MessageByte;
					rxBufferCONNACKIndex++;
					if(rxBufferCONNACKIndex == CONNACK_string_length)
					{
						if(strstr((char*)rxBufferCONNACK, CONNACK_string))
						{
							xSemaphoreGive(sem_CONNACK_given);
							rxBufferCONNACKIndex = 0;
							CONNACK_IsReceiving = 0;
							if(xSemaphoreTake(mutex_AcknowledgeFlag, (( TickType_t )100) == pdTRUE))
							{
								AcknowledgeFlag = 0;
								xSemaphoreGive(mutex_AcknowledgeFlag);
							}
						}						
					}
				}
				if(SUBACK_IsReceiving)
				{
					rxBufferSUBACK[rxBufferSUBACKIndex] = MessageByte;
					rxBufferSUBACKIndex++;
					if(rxBufferSUBACKIndex == SUBACK_string_length)
					{
						if(strstr((char*)rxBufferSUBACK, SUBACK_string))
						{
							xSemaphoreGive(sem_SUBACK_given);
							rxBufferSUBACKIndex = 0;
							SUBACK_IsReceiving = 0;
							if(xSemaphoreTake(mutex_AcknowledgeFlag, (( TickType_t )100) == pdTRUE))
							{
								AcknowledgeFlag = 0;
								xSemaphoreGive(mutex_AcknowledgeFlag);
							}
						}						
					}
				}		
			}				
			else // еcли получен любой байт, кроме вышеперечисленных
			{
				if(MQTT_MessageIsReceiving) // если принимаем MQTT-сообщение
				{
					rxBufferMQTT[rxBufferMQTTIndex] = MessageByte;
					rxBufferMQTTIndex++;
				}
				else // если принимаем отклик на команду
				{
					rxBuffer[rxBufferIndex] = MessageByte;
					rxBufferIndex++;
				}				
			}
		}
    else 
    {
			// TODO: если слишком долго нет байтов от SIM800, перезагружаем соединение
		}			
    vTaskDelay(1);		
	}
}

command_status_t ExecuteATCommand(char * cmd, const char * mask, uint32_t period)
{
	uint8_t num_attempt_to_get_correct_answer = 3;
	uint8_t num_attempt_to_send_command = 3;
	uint8_t flag = 0;
	string_send_status_t status = ERROR_SEND_STRING_STATUS;
	while(!flag && num_attempt_to_send_command > 0)
	{
		vTaskDelay(period);
		status = ERROR_SEND_STRING_STATUS;
   	status = SIM800_SendStr(cmd); // посылаем строку
		num_attempt_to_get_correct_answer = 10;
		flag = 0;
		while(!flag && status == OK_SEND_STRING_STATUS && num_attempt_to_get_correct_answer > 0)
		{
			if( xSemaphoreTake(sem_null_terminal_string_given, ( TickType_t ) 1000 ) == pdTRUE )
			{
				if(strstr((char*)rxBufferDouble,mask))
					flag = 1;
				else if(strstr((char*)rxBufferDouble,"ERROR"))
					return ERROR_COMMAND_STATUS;
			}			
			num_attempt_to_get_correct_answer--;
		};
		num_attempt_to_send_command--;
	}
	if(!flag || status == ERROR_SEND_STRING_STATUS)
		return ERROR_COMMAND_STATUS;
	else
			return OK_COMMAND_STATUS;
}


// отправка сообщения по TCP
command_status_t SendMessageByTCP(char * message, uint8_t length_of_message, uint32_t period)
{
	uint8_t num_attempt_to_get_correct_answer_ALL = 3;
	uint8_t num_attempt_to_get_correct_answer_CIPSEND = 5;
	uint8_t num_attempt_to_get_correct_answer_SEND_OK = 5;
	uint8_t flag_CIPSEND = 0;
	uint8_t flag_message = 0;
	string_send_status_t status = ERROR_SEND_STRING_STATUS;
	while(!(flag_CIPSEND && flag_message) && num_attempt_to_get_correct_answer_ALL > 0)
	{
		vTaskDelay(period);
		/*---------------------------------*/
		status = ERROR_SEND_STRING_STATUS;
		status = SIM800_SendStr("AT+CIPSEND\r\n"); // посылаем строку
		num_attempt_to_get_correct_answer_CIPSEND = 10;
		flag_CIPSEND = 0;
    while(!flag_CIPSEND && status == OK_SEND_STRING_STATUS && num_attempt_to_get_correct_answer_CIPSEND > 0)
    {
			if(xSemaphoreTake(sem_invitation_to_enter_TCP_string_given, ( TickType_t ) 1000) == pdTRUE) // ждем, пока не будет принят символ '>' (приглашение ввода строки, передаваемой по TCP)
			{
				flag_CIPSEND = 1;
			}
			else if(xSemaphoreTake(sem_null_terminal_string_given, ( TickType_t ) 1000) == pdTRUE)
				if(strstr((char*)rxBufferDouble,"ERROR"))
					return ERROR_COMMAND_STATUS;
      num_attempt_to_get_correct_answer_CIPSEND--;				
		}
		if(flag_CIPSEND == 1)
		{
			status = ERROR_SEND_STRING_STATUS;
			status = SIM800_SendStrWithLen(message, length_of_message);	// посылаем строку
			num_attempt_to_get_correct_answer_SEND_OK = 10;
			flag_message = 0;
			while(!flag_message && status == OK_SEND_STRING_STATUS && num_attempt_to_get_correct_answer_SEND_OK > 0)
			{
				if(xSemaphoreTake(sem_null_terminal_string_given, (TickType_t)1000) == pdTRUE)
				{
					if(strstr((char*)rxBufferDouble,"SEND OK"))
						flag_message = 1;			
				}
				num_attempt_to_get_correct_answer_SEND_OK--;
			}
		}
		num_attempt_to_get_correct_answer_ALL--;
	}
	if(flag_CIPSEND == 1 && flag_message == 1)
		return OK_COMMAND_STATUS;
  else
		return ERROR_COMMAND_STATUS;
}


command_status_t SendMessageByTCP_With_Wait_Acknowledge(char * message,
																												uint8_t length_of_message,
																												xSemaphoreHandle *ACK_given,
																												uint32_t period)
{
	uint8_t num_attempt_to_get_correct_answer_ALL = 3;
	
	string_send_status_t status = ERROR_SEND_STRING_STATUS;
  	
	uint8_t flag_CIPSEND = 0;
	
	uint8_t num_attempt_to_get_correct_answer_SEND_OK = 5;	
	uint8_t flag_message = 0;
	
	uint8_t num_attempt_to_get_acknowledge = 3;
	uint8_t flag_acknowledge = 0;	
	
	while(!(flag_CIPSEND && flag_message && flag_acknowledge) && num_attempt_to_get_correct_answer_ALL > 0)
	{
		vTaskDelay(period);
		status = ERROR_SEND_STRING_STATUS;
		status = SIM800_SendStr("AT+CIPSEND\r\n"); // посылаем строку
   	flag_CIPSEND = 0;
    if(xSemaphoreTake(sem_invitation_to_enter_TCP_string_given, ( TickType_t ) 3000) == pdTRUE) // ждем, пока не будет принят символ '>' (приглашение ввода строки, передаваемой по TCP)
		{
			flag_CIPSEND = 1;
		}
		else if(xSemaphoreTake(sem_null_terminal_string_given, ( TickType_t ) 3000) == pdTRUE)
			if(strstr((char*)rxBufferDouble,"ERROR"))
				return ERROR_COMMAND_STATUS;
    
		if(flag_CIPSEND == 1)
		{			
			num_attempt_to_get_correct_answer_SEND_OK = 10;
			flag_message = 0;
			status = ERROR_SEND_STRING_STATUS;
			status = SIM800_SendStrWithLen(message, length_of_message);	// посылаем строку			
			while(!flag_message && status==OK_SEND_STRING_STATUS && num_attempt_to_get_correct_answer_SEND_OK > 0)
			{
				if(xSemaphoreTake(sem_null_terminal_string_given, ( TickType_t ) 1000) == pdTRUE)
				{
					if(strstr((char*)rxBufferDouble,"SEND OK"))
						flag_message = 1;			
				}
				num_attempt_to_get_correct_answer_SEND_OK--;
			}
			if(flag_message)
			{
				if(xSemaphoreTake(mutex_AcknowledgeFlag, ( TickType_t ) 1000) == pdTRUE)
				{
					AcknowledgeFlag = 1; // флаг, указывающей задаче taskMessageFromSIM800Take,
					// что нужно ожидать прихода сообщения CONNACK или SUBACK
					xSemaphoreGive(mutex_AcknowledgeFlag);
				}
				num_attempt_to_get_acknowledge = 3;
				flag_acknowledge = 0;
				while(!flag_acknowledge && num_attempt_to_get_acknowledge > 0)
				{
					if(xSemaphoreTake(*ACK_given, ( TickType_t ) 1000) == pdTRUE)
					{
						flag_acknowledge = 1;			
					}
					num_attempt_to_get_acknowledge--;
				}
			}
		}
		num_attempt_to_get_correct_answer_ALL--;
	}
	if(flag_CIPSEND == 1 && flag_message == 1 && flag_acknowledge == 1)
		return OK_COMMAND_STATUS;
  else
		return ERROR_COMMAND_STATUS;
}

void taskInitSIM800(void *params)
{
	command_status_t status;
	uint8_t flag = 0;
	while(1)
	{
		while(!flag)
		{
			// перезагружаем SIM800 физически
			GPIO_WriteBit(GPIOA, GPIO_Pin_7, Bit_RESET);
			vTaskDelay(10000);
			GPIO_WriteBit(GPIOA, GPIO_Pin_7, Bit_SET);
			USB_UART_SendStrWithLen("Hello\r\n", 7);
			vTaskDelay(7000);
			
			// запуск задачи приема сообщений с модуля SIM800
	    xTaskCreate(taskMessageFromSIM800Take, "taskMessageFromSIM800Take", configMINIMAL_STACK_SIZE, NULL, 3, &taskMessageFromSIM800TakeHandler);
			
			status = ExecuteATCommand("ATZ\r\n", "OK", 3000);
			if(status == ERROR_COMMAND_STATUS)
				break;
						
			// AT+CSQ
			status = ExecuteATCommand("AT+CSQ\r\n", "OK", 3000); // Signal Quality Report; +CSQ: 23,0 - нормальный сигнал (18-23)
			if(status == ERROR_COMMAND_STATUS)
				break;
			
			// AT+CPIN?		
			status = ExecuteATCommand("AT+CPIN?\r\n", "+CPIN: READY", 3000); // Enter PIN; +CPIN: READY - не требуется вводить PIN
			if(status == ERROR_COMMAND_STATUS)
				break;
			
			// AT+CREG?
			status = ExecuteATCommand("AT+CREG?\r\n", "OK", 3000); //"AT+CREG?", // Network Registartion; +CREG: 0,1 - 
			if(status == ERROR_COMMAND_STATUS)													// 0 - Disable network registarion unsolicited result code
				break;																									 // (отключить регистрацию сети незапрашиваемого кода результата)
																																	// 1 -  Registered, home network
			
			// AT+CGATT?
			status = ExecuteATCommand("AT+CGATT?\r\n", "OK", 3000); //"AT+CGATT?", // Attach or Detach from GPRS Service; +CGATT: 1 - Attached
			if(status == ERROR_COMMAND_STATUS)
				break;
			
			// AT+CIPMODE=0
			status = ExecuteATCommand("AT+CIPMODE=0\r\n", "OK", 3000); // "AT+CIPMODE=0", // Select TCPIP Application Mode; 0 - NORMAL MODE
			if(status == ERROR_COMMAND_STATUS)
				break;
			
			// AT+CIPMUX=0
			status = ExecuteATCommand("AT+CIPMUX=0\r\n", "OK", 3000); // "AT+CIPMUX=0", // Start Up Multi-IP Connection; 0 Single IP connection
			if(status == ERROR_COMMAND_STATUS)
				break;
			
			// AT+CIPSTATUS
			status = ExecuteATCommand("AT+CIPSTATUS\r\n", "STATE: IP INITIAL", 3000); //  "AT+CIPSTATUS", // Query Current Connection Status; OK
			if(status == ERROR_COMMAND_STATUS)																							//  Ожидаем прихода строки STATE: IP INITIAL
				break;
			
			// AT+CIPRXGET?
			status = ExecuteATCommand("AT+CIPRXGET?\r\n", "OK", 3000); //    "AT+CIPRXGET?", // Get Data from Network Manually; OK +CIPRXGET: 0
			if(status == ERROR_COMMAND_STATUS)															//0 - Disable getting data from network manually, the module is								
				break;																											 // set to normal mode, data will be pushed to TE directly
																														 
			// AT+CIPRXGET=1
			status = ExecuteATCommand("AT+CIPRXGET=0\r\n", "OK", 3000); //"AT+CIPRXGET=1", // 1 - Enable getting data from network manually
			if(status == ERROR_COMMAND_STATUS)																						
				break;
			
			/* Настройка контекста и открытие соединения */
			// AT+CSTT=\"internet.beeline.ru\",\"beeline\",\"beeline\"
			status = ExecuteATCommand("AT+CSTT=\"internet.beeline.ru\",\"beeline\",\"beeline\"\r\n", "OK", 3000);
			if(status == ERROR_COMMAND_STATUS)					//  "AT+CSTT=\"internet.beeline.ru\",\"beeline\",\"beeline\"",																	
				break;																		// Start Task and Set APN, USER NAME, PASSWORD; OK
			
			
			
			// AT+CIPSTATUS
			status = ExecuteATCommand("AT+CIPSTATUS\r\n", "STATE: IP START", 3000); // "AT+CIPSTATUS", //; OK STATE: IP START
			if(status == ERROR_COMMAND_STATUS)																						
				break;
			
			// AT+CIICR
			status = ExecuteATCommand("AT+CIICR\r\n\n", "OK", 3000); //    "AT+CIICR", // Bring Up Wireless Connection with GPRS or CSD; OK
			if(status == ERROR_COMMAND_STATUS)																						
				break;
			
			// AT+CIPSTATUS
			status = ExecuteATCommand("AT+CIPSTATUS\r\n", "STATE: IP GPRSACT", 3000); //    "AT+CIPSTATUS", //; OK STATE: GPRSACT
			if(status == ERROR_COMMAND_STATUS)																						
				break;
			
			// AT+CIFSR
			status = ExecuteATCommand("AT+CIFSR\r\n", ".", 3000); //    "AT+CIFSR", // Get Local IP Address; 10.119.54.182
			if(status == ERROR_COMMAND_STATUS)																						
				break;
			
			// AT+CIPSTATUS
			status = ExecuteATCommand("AT+CIPSTATUS\r\n", "STATE: IP STATUS", 3000); // AT+CIPSTATUS", // OK STATE: IP STATUS
			if(status == ERROR_COMMAND_STATUS)																						
				break;

      status = CreateTasksAndSetMQTTConnection();
			if(status == ERROR_COMMAND_STATUS)																						
				break;
			vTaskDelete(taskInitSIM800Handler);
			vTaskDelay(3000);      			
		}		
	}
}

command_status_t CreateTasksAndSetMQTTConnection(void)
{
	command_status_t status;
	uint8_t flag_connect_OK = 0;
	uint8_t flag_SUBSCRIBE_OK = 0;
	uint8_t flag_finish_MQTT_preparation = 0;
	char tempstr[100];
	uint8_t len;
	uint8_t num_of_attempt_to_set_MQTT_connection = 3;
	uint8_t num_of_attempt_to_SUBSCRIBE_MQTT_connection = 3;
	
	char  CIPSTART_TCP_string[100];
	while(!flag_finish_MQTT_preparation)
	{
		flag_connect_OK = 0;
		while(!flag_connect_OK && num_of_attempt_to_set_MQTT_connection > 0)
		{
			// Устанавливаем TCP-соединение
			sprintf(CIPSTART_TCP_string, "%s,\"%s\",\"%s\"\r\n", "AT+CIPSTART=\"TCP\"", BROKER_IP, BROKER_PORT);
			status = ExecuteATCommand(CIPSTART_TCP_string, "CONNECT OK", 3000);
			
			if(status == ERROR_COMMAND_STATUS)																						
				break;
			vTaskDelay(1000);
					
			// Посылаем пакет CONNECT
			uint8_t len;		
			len = MQTT_CONNECT_string(tempstr, 11, CLIENT_NAME);
			status = SendMessageByTCP_With_Wait_Acknowledge(tempstr, len, &sem_CONNACK_given, 3000);
			if(status == OK_COMMAND_STATUS)
				flag_connect_OK = 1;
			num_of_attempt_to_set_MQTT_connection--;
		}
		if(!flag_connect_OK)
			break;		
		vTaskDelay(500);
					
		// Посылаем пакет SUBSCRIBE
		flag_SUBSCRIBE_OK = 0;
		len = MQTT_SUBSCRIBE_string(tempstr, TOPIC_NAME, SUBSCRIBE_PACKET_IDENTIFIER);
		num_of_attempt_to_SUBSCRIBE_MQTT_connection = 3;
		while(!flag_SUBSCRIBE_OK && num_of_attempt_to_SUBSCRIBE_MQTT_connection>0)
		{
			status = SendMessageByTCP_With_Wait_Acknowledge(tempstr, len, &sem_SUBACK_given, 3000);
			if(status == OK_COMMAND_STATUS)
				flag_SUBSCRIBE_OK = 1;
			num_of_attempt_to_SUBSCRIBE_MQTT_connection--;
		}		
		if(!flag_SUBSCRIBE_OK)																						
			break;	
				
			
		// задачи - пересылка значения, считанного с карточки
		xTaskCreate(taskButtonPressSending, "taskButtonPressSending", configMINIMAL_STACK_SIZE, NULL, 2, &taskButtonPressSendingHandler);
				
		// задача - обработка полученного по MQTT сообщения    				
		xTaskCreate(taskMQTTMessageProcessing, "taskMQTTMessageProcessing", configMINIMAL_STACK_SIZE*3, NULL, 2, &taskMQTTMessageProcessingHandler);			
		
		// задача - KeepAlive
		xTaskCreate(taskSendPINGREQ, "taskSendPINGREQ", configMINIMAL_STACK_SIZE, NULL, 2, &taskSendPINGREQHandler); // 
		
		//если дошли до этого момента, отключаем быстрое мигание зеленого светодиода на BluePill и включаем медленное мигание этого светодиода
		vTaskDelete(taskLEDBlinkFrequentlyHandler);
		xTaskCreate(taskLEDBlinkRarely, "taskLEDBlinkRarely", configMINIMAL_STACK_SIZE, NULL, 1, &taskLEDBlinkRarelyHandler);
		
		GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);
		xSemaphoreGive(sem_PINGRESP_given);
    flag_finish_MQTT_preparation = 1;
	}
	if(flag_finish_MQTT_preparation && flag_connect_OK && flag_SUBSCRIBE_OK)
		return OK_COMMAND_STATUS;
	else
		return ERROR_COMMAND_STATUS;	
}

void taskSendPINGREQ(void* params)
{
	char tempstr[10];
	uint8_t len = MQTT_PINGREQ_string(tempstr);
	command_status_t status;
	uint8_t num_of_attempt_get_PING_RESP = 10;
	uint8_t flag_get_PING_RESP = 0;
	while(1)
	{
		num_of_attempt_get_PING_RESP = 10;
	  flag_get_PING_RESP = 0;
		// ждем, когда придет PINGRESP
		while(!flag_get_PING_RESP && num_of_attempt_get_PING_RESP > 0) 
		{
			if(xSemaphoreTake(sem_PINGRESP_given, ( TickType_t ) 1000) == pdPASS)
			{
				flag_get_PING_RESP = 1;
				ResetOrangeLED();
				num_of_attempt_get_PING_RESP--;      			
			}
		}			
		if(flag_get_PING_RESP == 1) // если PINGRESP пришел
		{
			if(xSemaphoreTake(mutex_MQTT, ( TickType_t ) 2000) == pdTRUE )
			{
				status = SendMessageByTCP(tempstr, len, 2000); // посылаем PINGREQ
        SetOrangeLED();				
				if(status == ERROR_COMMAND_STATUS) // если отослать PINGREQ не удалось, перезапускаем инициализацию
				{
					USB_UART_SendStrWithLen("SEND PINGREQ error\r\n",20);
					// TODO: подать звуковой сигнал и запустить задачу восстановления подключения					
				};
				xSemaphoreGive(mutex_MQTT); // освобождаем мьютекс на передачу MQTT-сообщения
			}
			else // если мьютекс на передачу MQTT-сообщения захватить не удалось, перезапускаем инициализацию
			{
				USB_UART_SendStrWithLen("MQTT mutex error\r\n",18);
				// TODO: подать звуковой сигнал и запустить задачу восстановления подключения
			}
		}
		else // если PINGRESP не пришел, перезапускаем инициализацию
		{
			USB_UART_SendStrWithLen("GET PINGRESP error\r\n",20);
			// TODO: подать звуковой сигнал и запустить задачу восстановления подключения
		}
    vTaskDelay(2000);		
	}	
}

void taskMQTTMessageProcessing(void* params)
{
	while(1)
	{		
		if(xSemaphoreTake(sem_MQTT_message_given, portMAX_DELAY) == pdPASS)
		{	
			if(strstr((char*)rxBufferMQTTDouble,"LEDS ON"))
			{
				USB_UART_SendStrWithLen("LEDS ON accepted\r\n", 12);
        SetGreenLED();
				SetOrangeLED();
				SetRedLED();				
			}
      else if(strstr((char*)rxBufferMQTTDouble,"LEDS OFF"))
			{
				USB_UART_SendStrWithLen("LEDS OFF accepted\r\n", 12);
        ResetGreenLED();
				ResetOrangeLED();
				ResetRedLED();
			}
			vTaskDelay(100);
		}		
	}
}


void taskButtonPressSending(void *params)
{
  char tempstr[100];
	uint8_t len = MQTT_PUBLISH_string(tempstr, "BTN PRESS", TOPIC_NAME);
  while(1)
	{
		if(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0) == 0)
    {
			if( xSemaphoreTake( mutex_MQTT, ( TickType_t ) 2000 ) == pdTRUE )
			{
				SendMessageByTCP(tempstr, len, 2000);
				ResetGreenLED();
				xSemaphoreGive(mutex_MQTT);				
			}
		}
		vTaskDelay(100);
	}	
}


void InitRTOSStructuresToWorkWithMQTT(void)
{
	if ((usartSendQueue = xQueueCreate(QUEUE_SIZE, sizeof(uint8_t))) == NULL){};
	if ((usartSendQueueDouble = xQueueCreate(QUEUE_SIZE, sizeof(uint8_t))) == NULL){};
	sem_null_terminal_string_given = xSemaphoreCreateBinary();
	sem_invitation_to_enter_TCP_string_given = xSemaphoreCreateBinary();
	
	sem_MQTT_message_given = xSemaphoreCreateBinary();			
	sem_PINGRESP_given = xSemaphoreCreateBinary();
	mutex_MQTT = xSemaphoreCreateBinary();
	
	sem_CONNACK_given = xSemaphoreCreateBinary();
	sem_SUBACK_given = xSemaphoreCreateBinary();
	
	mutex_AcknowledgeFlag = xSemaphoreCreateBinary();
				
	xSemaphoreGive(mutex_MQTT);
  xSemaphoreGive(mutex_AcknowledgeFlag);	
}


