#include "main.h"
void initRCC(void)
{
	RCC_DeInit();
	RCC_HSICmd(DISABLE);
	RCC_HSEConfig(RCC_HSE_ON); // разрешаем тактирование от внешнего генератора
	// ожидаем, пока внешний генератор будет готов к работе
	ErrorStatus HSEStartUpStatus = RCC_WaitForHSEStartUp();
	// если запуск внешнего генератора прошел успешно
	if (HSEStartUpStatus == SUCCESS)
	{
		/*
		Частота кварца - 8Мгц
		HSE(8 Mhz) -> PLL(x9 = 72 Mhz) -> SYSCLOCK_MUX -> SYSCLOCK(72 Mhz)
		AHB Prescaler = /1
		ABP1 Prescaler = /2
		APB2 Prescaler = /4
		*/

		//PLL
		RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
		RCC_PLLCmd(ENABLE);

		//Выбираем источником PLL
		RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

		//AHB Prescaler
		RCC_HCLKConfig(RCC_SYSCLK_Div1);

		//Настройка предделителей APBx
		RCC_PCLK1Config(RCC_HCLK_Div2);
		RCC_PCLK2Config(RCC_HCLK_Div4);

		// ожидаем, пока HSE не установится в качестве источника SYSCLOCK
		while (RCC_GetSYSCLKSource() != 0x08) {}
	}
	else
	{   //Если HSE не смог запуститься, тактирование настроено некорректно
		//Здесь следует поместить код обработчика этой ошибки
		while (1) {} // бесконечный цикл
	}
}


int main()
{
	initRCC();
	initGPIO();	
	
	// настройка группы приоритетов обработки прерываний на уровне NVIC
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);	
	
	// задача для проверки, что микроконтроллер не завис - 
	// мигание встроенным в Blue Pill светодиодом 5 раз в секунду
	xTaskCreate(taskLEDBlinkFrequently, "taskLEDBlinkFrequently", configMINIMAL_STACK_SIZE, NULL, 2, &taskLEDBlinkFrequentlyHandler);
		
	// инициализация работы с модулями USART
	USART_init();
  
	// инициализация очередей и семафоров
	InitRTOSStructuresToWorkWithMQTT();
	
	// задача персылки получаемых с USART1 байтов в очередь для обработки 
	// и на USART2 для отображения в окне терминала
	xTaskCreate(taskUart, "taskUart", configMINIMAL_STACK_SIZE, NULL, 5, &taskUartHandler);	
	
	// запуск задачи инициализации модуля SIM800
	xTaskCreate(taskInitSIM800, "taskInitSIM800", configMINIMAL_STACK_SIZE*3, NULL, 2, &taskInitSIM800Handler);
	vTaskStartScheduler();
	while(1){}
	return 0;
}


