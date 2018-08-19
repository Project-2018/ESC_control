#ifndef ESC_COMM_H_
#define ESC_COMM_H_

#include "ch.h"
#include "hal.h"

#include "bldc_interface.h"

static mc_values *mc_val;

void val_received(mc_values *val);

void ESC_ControlInit(void);

double escGetVoltage(void);
double escGetESCTemp(void);
double escGetAcCurrent(void);
double escGetDcCurrent(void);
int32_t escGetERPM(void);
double escGetDutyCycle(void);
double escGetAmpHours(void);
double escGetAmpHoursCharged(void);
double escGetWattHours(void);
double escGetWattHoursCharged(void);
int16_t escGetTachometer(void);
int16_t escGetTachometerAbs(void);
char* escGetFaultcode(void);

/*
 * Shell function
 */
void cmd_escCommBtnValues(BaseSequentialStream *chp, int argc, char *argv[]);

#endif