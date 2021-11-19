#include "uart_driver.h"

QueueHandle_t usartSendQueue; // очередь, в которую помещаются байты, полученные в обработчике прерывания от USART1

void USART_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
  USART_InitTypeDef USART_InitStructure;
  // Конфигурируем USART1 для обмена данными с модулем SIM800
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);
	// Настройка портов ввода-вывода
	// RX 
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
	// TX
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
	// Базовые настройки модуля USART1
  USART_InitStructure.USART_BaudRate = 9600;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  USART_Init(USART1, &USART_InitStructure);  
	// Разрешаем прерывание USART1 по приходу данных (от SIM800) 
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	USART_Cmd(USART1, ENABLE);
	
	/*****************************************************************/	
	// Конфигурируем USART2, используемый в отладочных целях	
	// для вывода в консоль данных, передаваемых по USART1
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2,ENABLE);
	// Настройка портов ввода-вывода
	// TX
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);	
	// RX 
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	// Базовые настройки модуля USART2
  USART_InitStructure.USART_BaudRate = 115200;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Tx;
  USART_Init(USART2, &USART_InitStructure);
  USART_Cmd(USART2, ENABLE);
	/*****************************************************************/

  // Настраиваем обмен данными с использованием прямого доступа к памяти:
	// USART1 использует канал 4 модуля DMA1 для передачи данных,
	// USART2 использует канал 7 модуля DMA1 для передачи данных
	
	// Подаем тактирование
	RCC_AHBPeriphClockCmd (RCC_AHBPeriph_DMA1, ENABLE) ;
	DMA_InitTypeDef DMA_InitStructure;
	// Сбрасываем настройки используемых каналов DMA
	DMA_DeInit(DMA1_Channel4);
  DMA_DeInit(DMA1_Channel7);
	
	// Настраиваем канал 4 для передачи данных по USART1
	DMA_InitStructure.DMA_BufferSize = 0;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST; // направление передачи данных - к периферийному устройству
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable; // запрещаем передачу данных типа память-память
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte; // устанавливаем размер одной порции передаваемых из памяти данных равным 1 байту
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable; // разрешаем инкрементирование указателя на память, содержащую массив передаваемых данных
	DMA_InitStructure.DMA_Mode = DMA_Mode_Normal; // устанавливаем режим работы
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR; // устанавливаем адрес приемника данных - регистр данных модуля USART1
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte; // устанавливаем размер одной порции передаваемых периферийному устройству данных равным 1 байту
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable; // запрещаем инкрементирование адреса приемника данных
	DMA_InitStructure.DMA_Priority = DMA_Priority_High; // устанавливаем приоритет работы контроллера DMA
	// Инициализируем 4 канал DMA1 для передачи данных на USART1
	DMA_Init(DMA1_Channel4, &DMA_InitStructure);
	USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
	
	// Инициализируем 7 канал DMA1 для передачи данных на USART2
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART2->DR;
	DMA_Init(DMA1_Channel7, &DMA_InitStructure);
	USART_DMACmd(USART2, USART_DMAReq_Tx, ENABLE);
	
	// Устанавливаем приоритеты прерывания 
	// Поскольку используем FreeRTOS, приоритеты маскируемых прерываний должны устанавливаться
	// от 11 до 15 
  // В файле FreeRTOSConfig.h значение параметра
	// configMAX_SYSCALL_INTERRUPT_PRIORITY = 191 = 0xBF, старшая тетрада этого числа равна 11 - 
	// прерывания с приоритетом ниже 11 немаскируемые, то есть будут обрабатываться независимо
	// от работы FreeRTOS, поэтому для обеспечения предсказуемого исполнения кода
	// в обработчиках немаскируемых прерываний нельзя применять FreeRTOS API.
	// В нашем случае как раз необходимо использовать FreeRTOS API (именно, функции для записи
	// в очередь), поэтому необходимо на уровне NVIC назначать приоритеты, соответствующие
	// маскируемым прерываниям
	NVIC_InitTypeDef NVIC_InitStructure;
  NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 11;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
	
	/************************************************************/
	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 12;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);	
	/************************************************************/
}

string_send_status_t SIM800_SendStrWithLen(char * str, uint8_t len)
{	
	DMA1_Channel4->CNDTR = len; // устранавливаем значение поля NDT (Number of data to transfer) регистра CNDTR - заносим длину строки
	DMA1_Channel4->CMAR = (uint32_t) str; // устанавливаем значение регистра CMAR (Channel x memory access register) - заносим адрес данных, которые должны быть переданы
	DMA_Cmd(DMA1_Channel4, ENABLE); // разрешаем работу выбранного канала DMA (DMA1 Channel4)
	int32_t timeout = 40;
	string_send_status_t status = ERROR_SEND_STRING_STATUS;
	uint8_t flag = 0;	
	while(!flag && timeout > 0) // проверяем флаг завершения передачи данных из памяти на USART1 по 4 каналу DMA1 
	{
		if(DMA_GetFlagStatus(DMA1_FLAG_TC4) == SET)
			flag = 1;
		timeout--;
		vTaskDelay(10);
	}	
  if(flag && timeout > 0)
		status = OK_SEND_STRING_STATUS;
	else
		status = ERROR_SEND_STRING_STATUS;	

	DMA_Cmd(DMA1_Channel4, DISABLE); // запрещаем работу выбранного канала DMA (DMA1 Channel4)

	DMA_ClearFlag(DMA1_FLAG_TC4); // самостоятельно снимаем флаг завершения передачи данных по выбранному каналу DMA 
	return status;
}

string_send_status_t SIM800_SendStr(char* str)
{
	uint16_t len = strlen((const char*)str);
	return SIM800_SendStrWithLen(str, len);
}

string_send_status_t USB_UART_SendStrWithLen(char *buffer, uint16_t length)
{
	DMA1_Channel7->CNDTR = length; // устанавливаем значение поля NDT (Number of data to transfer) регистра CNDTR - заносим длину строки
	DMA1_Channel7->CMAR = (uint32_t) buffer; // устанавливаем значение регистра CMAR (Channel x memory access register) - заносим адрес данных, которые должны быть переданы
	DMA_Cmd(DMA1_Channel7, ENABLE); // разрешаем работу выбранного канала DMA (DMA1 Channel7)
	int32_t timeout = 40;
	string_send_status_t status = ERROR_SEND_STRING_STATUS;
	uint8_t flag = 0;	
	while(!flag && timeout > 0) // проверяем флаг завершения передачи данных из памяти на USART2 по 7 каналу DMA1 
	{
		if(DMA_GetFlagStatus(DMA1_FLAG_TC7) == SET)
			flag = 1;
		timeout--;
		vTaskDelay(10);
	}	
  if(flag && timeout > 0)
		status = OK_SEND_STRING_STATUS;
	else
		status = ERROR_SEND_STRING_STATUS;
	DMA_Cmd(DMA1_Channel7, DISABLE); // запрещаем работу выбранного канала DMA (DMA1 Channel7)
	DMA_ClearFlag(DMA1_FLAG_TC7); // самостоятельно снимаем флаг завершения передачи данных по выбранному каналу DMA
	return status;
}

void USART1_IRQHandler(void)
{
	uint8_t byte;
	BaseType_t pxHigherPriorityTaskWoken;
	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
	{
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
		byte = USART_ReceiveData(USART1);
		if (xQueueSendFromISR(usartSendQueue, &(byte), &pxHigherPriorityTaskWoken) != pdPASS)
		{
			// TODO: Catch error
		}
		if (pxHigherPriorityTaskWoken == pdTRUE)
		{
			portEND_SWITCHING_ISR(pxHigherPriorityTaskWoken);
		}
	}
}
