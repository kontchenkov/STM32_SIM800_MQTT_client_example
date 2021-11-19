#ifndef __LEDS_and_button
#define __LEDS_and_button
//#include <stdio.h>
//#include <inttypes.h>
//#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
//#include "semphr.h"
#include <stm32f10x_gpio.h>
void initGPIO(void);
void SetGreenLED(void);
void ResetGreenLED(void);
void SetOrangeLED(void);
void ResetOrangeLED(void);
void SetRedLED(void);
void ResetRedLED(void);
extern TaskHandle_t taskLEDBlinkFrequentlyHandler;
extern TaskHandle_t taskLEDBlinkRarelyHandler;
void taskLEDBlinkRarely(void* params);
void taskLEDBlinkFrequently(void* params);
#endif
