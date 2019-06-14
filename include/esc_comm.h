#ifndef ESC_COMM_H_
#define ESC_COMM_H_

#include "ch.h"
#include "hal.h"

#include "bldc_interface.h"

#define RPM_GUARD_OVERFLOW	400
#define TASK_PERIOD_MS		5
#define MAX_LIFTED_WEIGHT	600

typedef struct{
	bool (*IsDschAllowedByBMS)();
	bool (*IsRollingDetected)();
} EscControlConf_t;

typedef enum {
	UP_AND_DOWN,
	ONLY_UP,
	ONLY_DOWN,
	MAINTENENCE,
	NO_LIFT_ALLOWED,
	RPM_BLOCK
} EscControlStates_t;

typedef enum {
	BEEP_1S,
	BEEP_MAINTENANCE,
	BEEP_END
} BeepTypes_t;

typedef enum {
	ESC_CONNECTED,
	ESC_DISCONNECTED
} ESCConnectionType_t;

static mc_values *mc_val;

void val_received(mc_values *val);

void ESC_ControlInit(EscControlConf_t* conf);

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

void SetLiftingSpeed(uint8_t val);

uint16_t GetLiftedWeightKg(void);

bool IsEscInOverTemperature(void);

bool IsInOverWeightState(void);

uint8_t GetLiftingSpeed(void);

void DoBeep(void);

void SetEscMaintenence(void);

#endif