#include "LEDs_and_buttons.h"
void initGPIO(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
	GPIO_InitTypeDef gpioInit;
	gpioInit.GPIO_Mode = GPIO_Mode_Out_PP;
	gpioInit.GPIO_Speed = GPIO_Speed_50MHz;
	gpioInit.GPIO_Pin = GPIO_Pin_13;
	GPIO_Init(GPIOC, &gpioInit);
	GPIOC->ODR &= ~GPIO_Pin_13;
	
	// PA7 - для перезагрузки SIM800
	gpioInit.GPIO_Pin = GPIO_Pin_7;
	GPIO_Init(GPIOA, &gpioInit);
	
	// PB0 - кнопка
	gpioInit.GPIO_Mode = GPIO_Mode_IPU;
	gpioInit.GPIO_Pin = GPIO_Pin_0;
	GPIO_Init(GPIOB, &gpioInit);
	
	// PA4 - красный светодиод
  // PA5 - оранжевый светодиод
  // PA6 - зеленый светодиод
	gpioInit.GPIO_Mode = GPIO_Mode_Out_PP;
	gpioInit.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6;
	GPIO_Init(GPIOA, &gpioInit);
	
	
}
void ResetGreenLED(void)
{
	GPIO_WriteBit(GPIOA, GPIO_Pin_4, Bit_RESET);
};

void SetGreenLED(void)
{
	GPIO_WriteBit(GPIOA, GPIO_Pin_4, Bit_SET);
};

void ResetOrangeLED(void)
{
	GPIO_WriteBit(GPIOA, GPIO_Pin_5, Bit_RESET);
};

void SetOrangeLED(void)
{
	GPIO_WriteBit(GPIOA, GPIO_Pin_5, Bit_SET);
};

void ResetRedLED(void)
{
	GPIO_WriteBit(GPIOA, GPIO_Pin_6, Bit_RESET);
};

void SetRedLED(void)
{
	GPIO_WriteBit(GPIOA, GPIO_Pin_6, Bit_SET);
};

void taskLEDBlinkFrequently(void* params)
{
	while (1)
	{
		GPIOC->ODR ^= GPIO_Pin_13;
		vTaskDelay(100);		
	}
}

void taskLEDBlinkRarely(void* params)
{
	while (1)
	{
		GPIOC->ODR ^= GPIO_Pin_13;
		vTaskDelay(1000);		
	}
}
