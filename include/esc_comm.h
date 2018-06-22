#ifndef ESC_COMM_H_
#define ESC_COMM_H_

#include "ch.h"
#include "hal.h"

#include "bldc_interface.h"

static mc_values *mc_val;

void val_received(mc_values *val);

void ESC_ControlInit(void);

double getESCTemp(void);
double getAcCurrent(void);
double getDcCurrent(void);
double getERPM(void);
double getDutyCycle(void);
double getAmpHours(void);
double getAmpHoursCharged(void);
double getWattHours(void);
double getWattHoursCharged(void);
int16_t getTachometer(void);
int16_t getTachometerAbs(void);

#endif