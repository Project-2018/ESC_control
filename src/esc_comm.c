#include <stdio.h>
#include <string.h>

#include "ch.h"
#include "hal.h"

#include "chprintf.h"

#include "esc_comm.h"
#include "comm_uart.h"
#include "console.h"
#include "esc_led.h"
#include "calc.h"
#include "syslog.h"

#include "memorymap.h"
#include "eeprom.h"

#define VESC_FW_MINOR           1
#define VESC_FW_MAJOR           1

#define DEBOUNCE_DELAY_TIME     4      /* 40ms */
#define ESC_RPM_0               4000   /* 500 RPM */
#define ESC_RPM_1               4400   /* 550 RPM */
#define ESC_RPM_2               4800   /* 600 RPM */
#define RPM_STEP                50     /* RPM step */

#define ESC_MAX_TEMP            75
#define ESC_TEMP_HYS            10

#define TOTAL_BRAKE_TIME        5000
#define ACTIVE_BRAKE_TIME       3500

#define SPEAKER_ACTIVE_PWM      500

#define WEIGHT_LIMIT_KG         380

#define PERIODIC_POSTSCALER     20

static int32_t currently_set_rpm;

static struct Debounce{
  bool btnUp_state;
  bool btnUp_currently;
  bool btnUp_last;
  bool btnDown_state;
  bool btnDown_currently;
  bool btnDown_last;
  uint8_t btnUpCnt;
  uint8_t btnDownCnt
}debounce;

EscControlConf_t* EscControlConf;

EscControlStates_t EscCtrlStatus = NO_LIFT_ALLOWED;

uint16_t LiftingSpeed = ESC_RPM_1;

uint16_t RPMGuardCounter = 0;

float LiftedWeight = 0.0f;

bool EscTempAlarm = false;

uint32_t BrakeCounter = 0;

BeepTypes_t BeepState = BEEP_END;

uint32_t BeepCounter = 0;

bool IsOverWeightPresent = false;

ESCConnectionType_t EscConnection = ESC_DISCONNECTED;

virtual_timer_t EscConnTimeout;

uint16_t UptimeCounter = 0;

uint16_t PeriodicDataPostscaler = 0;

uint8_t BeepForRollingDone = 0;

static PWMConfig pwmcfg = {
  250000,                                    /* 10kHz PWM clock frequency.   */
  100,                                    /* Initial PWM period 1S.       */
  NULL,
  {
   {PWM_OUTPUT_ACTIVE_HIGH, NULL},
   {PWM_OUTPUT_DISABLED, NULL},
   {PWM_OUTPUT_DISABLED, NULL},
   {PWM_OUTPUT_DISABLED, NULL}
  },
  0,
  0
};

void EscConnectionTimeout_fb(void){
  EscConnection = ESC_DISCONNECTED;
}

/* Callback function for the received data. */
void val_received(mc_values *val) {
    mc_val = val;
    EscConnection = ESC_CONNECTED;
    chVTSet(&EscConnTimeout, TIME_MS2I(1000), EscConnectionTimeout_fb, NULL);
}

void SetLiftingSpeed(uint8_t val){
  switch(val){
    case 0:
      LiftingSpeed = ESC_RPM_0;
    break;
    case 1:
      LiftingSpeed = ESC_RPM_1;
    break;
    case 2:
      LiftingSpeed = ESC_RPM_2;
    break;
    default:
      LiftingSpeed = ESC_RPM_1;
    break;
  }
}

uint8_t GetLiftingSpeed(void){
  switch(LiftingSpeed){
    case ESC_RPM_0:
      return 0;
    break;
    case ESC_RPM_1:
      return 1;
    break;
    case ESC_RPM_2:
      return 2;
    break;
    default:
      return 1;
    break;
  }  
}

bool GetTopSwitchState(void){
  if(!palReadLine(LINE_SW1) == PAL_LOW)
    return true;

  return false;
}

bool GetBottomSwitchState(void){
  if(!palReadLine(LINE_SW3) == PAL_LOW)
    return true;

  return false;
}

bool GetMiddleSwitchState(void){
  //return true;
  if(palReadLine(LINE_SW2) == PAL_LOW)
    return true;

  return false;
}

void ReleaseBrake(void){
  BrakeCounter++;
}

void CalcLiftedWeight(void){
  if(GetMiddleSwitchState() == false){
      if(escGetERPM() >= (int32_t)(currently_set_rpm * 0.95)){

      switch(LiftingSpeed){
        case ESC_RPM_0:
          LiftedWeight = GetLiftedWeightSpd0();
        break;
        case ESC_RPM_1:
          LiftedWeight = GetLiftedWeightSpd1();
        break;
        case ESC_RPM_2:
          LiftedWeight = GetLiftedWeightSpd2();
        break;
      }    

    }  
  }
}

void ResetLiftedWeight(void){
  LiftedWeight = 0.0f;
  IsOverWeightPresent = false;
}

void DownButtonProcess(void){

  if(EscConnection != ESC_CONNECTED){
    setDownLedState(LED_OFF);
    return;
  }

  debounce.btnDown_currently ? setDownLedState(LED_PUSHED) : setDownLedState(LED_FULL);

  /* Down button is pressed */
  if (debounce.btnDown_state && !debounce.btnUp_state)
  {
      currently_set_rpm = (currently_set_rpm - RPM_STEP) > -LiftingSpeed ? currently_set_rpm - RPM_STEP : -LiftingSpeed;
      bldc_interface_set_rpm(currently_set_rpm);
      
      /*
      if (currently_set_rpm < (-LiftingSpeed * 0.5))
      {
          if (escGetERPM() > (int32_t)(currently_set_rpm * 0.50))
          {
              currently_set_rpm = 0;
          }
      }*/
  }

}

void UpButtonProcess(void){
  if(EscConnection != ESC_CONNECTED){
    setUpLedState(LED_OFF);
    return;
  }


  /* Setting the button LEDs */
  debounce.btnUp_currently   ? setUpLedState(LED_PUSHED)   : setUpLedState(LED_FULL);
  
  /* Up button is pressed */
  if (debounce.btnUp_state && !debounce.btnDown_state)
  {   
      CalcLiftedWeight();
      currently_set_rpm = (currently_set_rpm + RPM_STEP) < LiftingSpeed ? currently_set_rpm + RPM_STEP : LiftingSpeed;
      bldc_interface_set_rpm(currently_set_rpm);
      
      /*
      if (currently_set_rpm > (LiftingSpeed * 0.5))
      {
          if (escGetERPM() < (int32_t)(currently_set_rpm * 0.50))
          {
              currently_set_rpm = 0;
          }
      }*/
  }
}

void CheckWeight(void){

/*
  if(!debounce.btnUp_state){
    IsOverWeightPresent = false;
    return;
  }*/

  if(IsOverWeightPresent == true){
    return;
  }

  if(GetLiftedWeightKg() >= WEIGHT_LIMIT_KG && debounce.btnUp_state && !debounce.btnDown_state){
    IsOverWeightPresent = true;
    ADD_SYSLOG(SYSLOG_ERROR, "ESC", "Overweight detected: %d kg", GetLiftedWeightKg());
    DoBeep();
  }

}

void StopLifting(void){
  currently_set_rpm = 0;
  bldc_interface_set_rpm(currently_set_rpm);  
}

void RPMGuard(void){

  if(debounce.btnUp_state == true || debounce.btnDown_state == true){

    if(RPMGuardCounter > RPM_GUARD_OVERFLOW){
      StopLifting();
      EscCtrlStatus = RPM_BLOCK;
      ADD_SYSLOG(SYSLOG_ERROR, "ESC", "Motor blocked mechanically. RPM: %d", (uint16_t)escGetERPM());
    }else{

      if (abs(escGetERPM()) < abs((int32_t)(currently_set_rpm * 0.80)))
        RPMGuardCounter++;
      else
        RPMGuardCounter = 0;
    
    }

  }else{
    RPMGuardCounter = 0;
  }
}

void CheckRolling(void){
  if(EscControlConf->IsRollingDetected() == true && BeepForRollingDone == 0){
    DoBeep();
    BeepForRollingDone = 1;
  }
  if(EscControlConf->IsRollingDetected() == false)
    BeepForRollingDone = 0;
}

void DoBeep(void){
  BeepState = BEEP_1S;
}


static THD_WORKING_AREA(BrakeTask_wa, 256);
static THD_FUNCTION(BrakeTask, p) {

  (void)p;
  chRegSetThreadName("BrakeTHD");
  uint32_t LastCounter = 0;
  
  
  while (TRUE) {

    if(BrakeCounter != LastCounter){
      LastCounter = BrakeCounter;
      palSetLine(LINE_SOLENOID);
      chThdSleep(TIME_US2I(ACTIVE_BRAKE_TIME));
      palClearLine(LINE_SOLENOID);
      chThdSleep(TIME_US2I(TOTAL_BRAKE_TIME - ACTIVE_BRAKE_TIME));
    }else{

      LastCounter = BrakeCounter;
      palClearLine(LINE_SOLENOID);
      chThdSleep(TIME_US2I(TOTAL_BRAKE_TIME));
    }

    
  }

}


static THD_WORKING_AREA(UptimeCount_wa, 256);
static THD_FUNCTION(UptimeCount, p) {

  (void)p;
  chRegSetThreadName("UptimeCounter");
  
  while (TRUE) {

      if(EscControlConf->IsDschAllowedByBMS() == true){

        uint32_t* UptimeMinPtr = GetUptimeMin();

        UptimeCounter++;

        if(UptimeCounter >= 60){
          
          (*UptimeMinPtr)++;
          UptimeCounter = 0;
          StoreRecordToEeprom(UPTIMEMIN);
        }

      }

      chThdSleepMilliseconds(1000);

  }

}



static THD_WORKING_AREA(ESCControl_wa, 1024);
static THD_FUNCTION(ESCControl, p) {

  (void)p;
  chRegSetThreadName("ESC Control");
  while (TRUE) {

    /*
     * Up button debounce
     */
    debounce.btnUp_currently = !palReadPad(GPIOA, GPIOA_BTN_UP);
    if (debounce.btnUp_currently != debounce.btnUp_last)
    {
        debounce.btnUpCnt = 0;
    }
    else if(debounce.btnUp_currently != debounce.btnUp_state){
        debounce.btnUpCnt++;
    }

    if (debounce.btnUpCnt > DEBOUNCE_DELAY_TIME)
    {
        debounce.btnUp_state = debounce.btnUp_currently;
    }
    debounce.btnUp_last = debounce.btnUp_currently;


    /*
     * Down button debounce
     */
    debounce.btnDown_currently = !palReadPad(GPIOA, GPIOA_BTN_DOWN);
    if (debounce.btnDown_currently != debounce.btnDown_last)
    {
        debounce.btnDownCnt = 0;
    }
    else if(debounce.btnDown_currently != debounce.btnDown_state){
        debounce.btnDownCnt++;
    }

    if (debounce.btnDownCnt > DEBOUNCE_DELAY_TIME)
    {
        debounce.btnDown_state = debounce.btnDown_currently;
    }
    debounce.btnDown_last = debounce.btnDown_currently;



    switch(EscCtrlStatus){
      case UP_AND_DOWN:

        if(EscControlConf->IsDschAllowedByBMS() == false){
          StopLifting();
          EscCtrlStatus = NO_LIFT_ALLOWED;
          setUpLedState(LED_OFF);
          setDownLedState(LED_OFF);
          break;
        }

        if(EscTempAlarm == true){
          StopLifting();
          EscCtrlStatus = ONLY_DOWN;
          setUpLedState(LED_OFF);
          break;
        }

        if(GetTopSwitchState() == true){
          StopLifting();
          EscCtrlStatus = ONLY_DOWN;
          setUpLedState(LED_OFF);
          break;
        }

        if(GetBottomSwitchState() == true){
          StopLifting();
          EscCtrlStatus = ONLY_UP;
          ResetLiftedWeight();
          setDownLedState(LED_OFF);
          break;
        }

        if(EscControlConf->IsRollingDetected() == true && GetMiddleSwitchState() == true){
          StopLifting();
          EscCtrlStatus = ONLY_DOWN;
          setUpLedState(LED_OFF);
          ADD_SYSLOG(SYSLOG_WARN, "ESC", "Machine rolling detected. Lifting blocked.");
          DoBeep();
          break;
        }

        if(IsOverWeightPresent == true && GetMiddleSwitchState() == true){
          StopLifting();
          EscCtrlStatus = ONLY_DOWN;
          setUpLedState(LED_OFF);
          break;
        }


        RPMGuard();

        UpButtonProcess();
        DownButtonProcess();

      break;
      case ONLY_UP:

        if(EscControlConf->IsDschAllowedByBMS() == false){
          StopLifting();
          EscCtrlStatus = NO_LIFT_ALLOWED;
          setUpLedState(LED_OFF);
          setDownLedState(LED_OFF);
          break;
        }

        if(GetBottomSwitchState() == false && debounce.btnDown_state == false){
          EscCtrlStatus = UP_AND_DOWN;
          break;
        }

        RPMGuard();
        UpButtonProcess();

      break;
      case ONLY_DOWN:

        if(EscControlConf->IsDschAllowedByBMS() == false){
          StopLifting();
          EscCtrlStatus = NO_LIFT_ALLOWED;
          setUpLedState(LED_OFF);
          setDownLedState(LED_OFF);
          break;
        }

        if(GetBottomSwitchState() == true && EscTempAlarm == true){
          StopLifting();
          EscCtrlStatus = NO_LIFT_ALLOWED;
          setUpLedState(LED_OFF);
          setDownLedState(LED_OFF);
          break;
        }

        if(GetBottomSwitchState() == true){
          StopLifting();
          EscCtrlStatus = ONLY_UP;
          ResetLiftedWeight();
          setDownLedState(LED_OFF);
          break;
        }

        if((GetMiddleSwitchState() == false || EscControlConf->IsRollingDetected() == false) && (GetBottomSwitchState() == false && GetTopSwitchState() == false)  && debounce.btnUp_state == false && IsOverWeightPresent == false){
          EscCtrlStatus = UP_AND_DOWN;
          break;
        }

        RPMGuard();
        DownButtonProcess();

      break;
      case RPM_BLOCK:

        if(EscControlConf->IsDschAllowedByBMS() == false){
          StopLifting();
          EscCtrlStatus = NO_LIFT_ALLOWED;
          setUpLedState(LED_OFF);
          setDownLedState(LED_OFF);
          break;
        }

        if(debounce.btnUp_state == false && debounce.btnDown_state == false){
          EscCtrlStatus = UP_AND_DOWN;
        }


      break;
      case NO_LIFT_ALLOWED:

        if(EscControlConf->IsDschAllowedByBMS() == true && EscTempAlarm == false)
          EscCtrlStatus = UP_AND_DOWN;

      break;
      case MAINTENENCE:
        setUpLedState(LED_OFF);
        setDownLedState(LED_OFF);
      break;
    }


    CheckWeight();

    CheckRolling();

    if(currently_set_rpm != 0)
      ReleaseBrake();

    /* Both buttons are released or pressed or during ONLY_DOWN the up button is processed */
    if((!debounce.btnUp_state && !debounce.btnDown_state) || (debounce.btnUp_state && debounce.btnDown_state) || (EscCtrlStatus == ONLY_DOWN && debounce.btnUp_state))
    {
        if (currently_set_rpm > 0)
        {
            currently_set_rpm = (currently_set_rpm - RPM_STEP) > 0 ? currently_set_rpm - RPM_STEP : 0;
        }
        else if (currently_set_rpm < 0)
        {
            currently_set_rpm = (currently_set_rpm + RPM_STEP) < 0 ? currently_set_rpm + RPM_STEP : 0;
        }

        bldc_interface_set_rpm(currently_set_rpm);
    }


    switch(EscTempAlarm){
      case false:
        if((int16_t)escGetESCTemp() > ESC_MAX_TEMP){
          ADD_SYSLOG(SYSLOG_ERROR, "ESC", "ESC temperature reached %d c", ESC_MAX_TEMP);
          EscTempAlarm = true;
          DoBeep();
        }
      break;
      case true:
        if((int16_t)escGetESCTemp() < ESC_MAX_TEMP - ESC_TEMP_HYS){
          EscTempAlarm = false;
        }

      break;
    }


    switch(BeepState){
      case BEEP_1S:
        pwmEnableChannel(&PWMD8, 0, PWM_PERCENTAGE_TO_WIDTH(&PWMD8, SPEAKER_ACTIVE_PWM));
        if(BeepCounter > (1000.0f / (float)TASK_PERIOD_MS)){
          pwmDisableChannel(&PWMD8, 0);
          BeepState = BEEP_END;
          break;
        }
        BeepCounter++;

      break;
      case BEEP_MAINTENANCE:
        pwmEnableChannel(&PWMD8, 0, PWM_PERCENTAGE_TO_WIDTH(&PWMD8, SPEAKER_ACTIVE_PWM));
        if(BeepCounter > (2000.0f / (float)TASK_PERIOD_MS)){
          pwmDisableChannel(&PWMD8, 0);
          BeepState = BEEP_END;
          break;
        }
        BeepCounter++;

      break;
      case BEEP_END:
        pwmDisableChannel(&PWMD8, 0);
        BeepCounter = 0;
      break;
    }



    PeriodicDataPostscaler++;
    if(PeriodicDataPostscaler > PERIODIC_POSTSCALER){
      bldc_interface_get_values();
      CalcFilteredACcurrent();
      PeriodicDataPostscaler = 0;
    }

    
    chThdSleepMilliseconds(TASK_PERIOD_MS);
  }
}


void ESC_ControlInit(EscControlConf_t* conf){

  if(conf->IsDschAllowedByBMS == NULL || conf->IsRollingDetected == NULL)
    chSysHalt("ESC control conf NULL pointer");

  EscControlConf = conf;

  debounce.btnUp_state       = 0;
  debounce.btnUp_currently   = 0;
  debounce.btnUp_last        = 0;
  debounce.btnDown_state     = 0;
  debounce.btnDown_currently = 0;
  debounce.btnDown_last      = 0;

  ESC_LedInit();

  comm_uart_init();

  bldc_interface_set_rx_value_func(val_received);

  bldc_interface_get_values();

  pwmStart(&PWMD8, &pwmcfg);

  SetLiftingSpeed(1);

  InitAcCurrentFilter(10, 2);

  chThdCreateStatic(ESCControl_wa, sizeof(ESCControl_wa), NORMALPRIO + 1, ESCControl, NULL);
  chThdCreateStatic(BrakeTask_wa, sizeof(BrakeTask_wa), NORMALPRIO + 1, BrakeTask, NULL);
  chThdCreateStatic(UptimeCount_wa, sizeof(UptimeCount_wa), NORMALPRIO, UptimeCount, NULL);
}

double  escGetVoltage(void)          { return mc_val->v_in; }
double  escGetESCTemp(void)          { return mc_val->temp_pcb; }
double  escGetAcCurrent(void)        { return mc_val->current_motor; }
double  escGetDcCurrent(void)        { return mc_val->current_in; }
int32_t escGetERPM(void)             { return mc_val->rpm; }
double  escGetDutyCycle(void)        { return mc_val->duty_now * 100.0; }
double  escGetAmpHours(void)         { return mc_val->amp_hours; }
double  escGetAmpHoursCharged(void)  { return mc_val->amp_hours_charged; }
double  escGetWattHours(void)        { return mc_val->watt_hours; }
double  escGetWattHoursCharged(void) { return mc_val->watt_hours_charged; }
int16_t escGetTachometer(void)       { return mc_val->tachometer; }
int16_t escGetTachometerAbs(void)    { return mc_val->tachometer_abs; }
char*   escGetFaultcode(void)        { return bldc_interface_fault_to_string(mc_val->fault_code); }

uint16_t GetLiftedWeightKg(void){
  return LiftedWeight;
}

bool IsEscInOverTemperature(void){
  return EscTempAlarm;
}

bool IsInOverWeightState(void){
  return IsOverWeightPresent;
}

void SetEscMaintenence(void){
  EscCtrlStatus = MAINTENENCE;
}

/*
 * Shell function
 */
void cmd_escCommBtnValues(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  
  while (chnGetTimeout((BaseChannel *)chp, TIME_IMMEDIATE) == Q_TIMEOUT) {

    chprintf(chp, "\x1B\x63");
    chprintf(chp, "\x1B[2J");
    chprintf(chp, "\x1B[%d;%dH", 0, 0);

    switch(EscCtrlStatus){
      case UP_AND_DOWN:
        chprintf(chp, "EscCtrlStatus     :UP_AND_DOWN\r\n");

      break;
      case ONLY_UP:
        chprintf(chp, "EscCtrlStatus     :ONLY_UP\r\n");
      break;

      case ONLY_DOWN:
        chprintf(chp, "EscCtrlStatus     :ONLY_DOWN\r\n");
      break;
      case MAINTENENCE:
        chprintf(chp, "EscCtrlStatus     :MAINTENENCE\r\n");
      break;
      case NO_LIFT_ALLOWED:
        chprintf(chp, "EscCtrlStatus     :NO_LIFT_ALLOWED\r\n");
      break;
      case RPM_BLOCK:
        chprintf(chp, "EscCtrlStatus     :RPM_BLOCK\r\n");
      break;
    }

    switch(EscConnection){
      case ESC_CONNECTED:
        chprintf(chp, "ESC status        :ESC_CONNECTED\r\n");

      break;
      case ESC_DISCONNECTED:
        chprintf(chp, "ESC status        :ESC_DISCONNECTED\r\n");
      break;
    }

    chprintf(chp, "btnUp_state       : %d\r\n", debounce.btnUp_state       );
    chprintf(chp, "btnUp_currently   : %d\r\n", debounce.btnUp_currently   );
    chprintf(chp, "btnUp_last        : %d\r\n", debounce.btnUp_last        );
    chprintf(chp, "btnDown_state     : %d\r\n", debounce.btnDown_state     );
    chprintf(chp, "btnDown_currently : %d\r\n", debounce.btnDown_currently );
    chprintf(chp, "btnDown_last      : %d\r\n", debounce.btnDown_last      );
    chprintf(chp, "---------------------------------------------------\r\n");
    chprintf(chp, "currently_set_rpm : %d\r\n", currently_set_rpm          );
    chprintf(chp, "RPMguard : %d\r\n", RPMGuardCounter          );
    chprintf(chp, "Switches : %d, %d, %d\r\n", GetTopSwitchState(), GetBottomSwitchState(), GetMiddleSwitchState());
    chprintf(chp, "LiftedWeight : %.2f %d %d\r\n", LiftedWeight, GetLiftedWeightKg());
    chprintf(chp, "Esc overtemp: %d\r\n", EscTempAlarm);
    chprintf(chp, "Brakecounter: %d\r\n", BrakeCounter);
    chprintf(chp, "Uptimecounter: %d %d\r\n", UptimeCounter, *GetUptimeMin());

        
    chThdSleepMilliseconds(100);
  }
}