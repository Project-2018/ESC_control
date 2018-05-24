#ifndef ESC_LED_H_
#define ESC_LED_H_

#include "ch.h"
#include "hal.h"

typedef enum{
	LED_OFF,
	LED_FULL,
	LED_IDLE,
	LED_ERROR
}ledState;

void ESC_LedInit(void);

void setUpLedState(ledState tmp);
void setDownLedState(ledState tmp);

#endif