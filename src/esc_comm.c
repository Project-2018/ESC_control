#include <stdio.h>
#include <string.h>

#include "ch.h"
#include "hal.h"

#include "chprintf.h"

#include "esc_comm.h"
#include "comm_uart.h"
#include "console.h"
#include "esc_led.h"

#define DEBOUNCE_DELAY_TIME     4      /* 40ms */
#define ESC_RPM                 4800   /* 600 RPM */
#define RPM_STEP                50     /* RPM step */

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

/* Callback function for the received data. */
void val_received(mc_values *val) {
    mc_val = val;
}

static THD_WORKING_AREA(ESCControl_wa, 128);
static THD_FUNCTION(ESCControl, p) {

  (void)p;
  chRegSetThreadName("ESC Control");
  while (TRUE) {
    systime_t time;
    time = 5;




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



    
    /* Setting the button LEDs */
    debounce.btnUp_currently   ? setUpLedState(LED_PUSHED)   : setUpLedState(LED_FULL);
    debounce.btnDown_currently ? setDownLedState(LED_PUSHED) : setDownLedState(LED_FULL);




    /* Down button is pressed */
    if (debounce.btnDown_state && !debounce.btnUp_state)
    {
        currently_set_rpm = (currently_set_rpm - RPM_STEP) > -ESC_RPM ? currently_set_rpm - RPM_STEP : -ESC_RPM;
        bldc_interface_set_rpm(currently_set_rpm);
        
        if (currently_set_rpm < (-ESC_RPM * 0.5))
        {
            if (escGetERPM() > (int32_t)(currently_set_rpm * 0.50))
            {
                currently_set_rpm = 0;
            }
        }
    }




    /* Up button is pressed */
    else if (debounce.btnUp_state && !debounce.btnDown_state)
    {   
        currently_set_rpm = (currently_set_rpm + RPM_STEP) < ESC_RPM ? currently_set_rpm + RPM_STEP : ESC_RPM;
        bldc_interface_set_rpm(currently_set_rpm);
        
        if (currently_set_rpm > (ESC_RPM * 0.5))
        {
            if (escGetERPM() < (int32_t)(currently_set_rpm * 0.50))
            {
                currently_set_rpm = 0;
            }
        }
    }
    



    /* Both buttons are released */
    else
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




    bldc_interface_get_values();
    chThdSleepMilliseconds(time);
  }
}

void ESC_ControlInit(void){

  debounce.btnUp_state       = 0;
  debounce.btnUp_currently   = 0;
  debounce.btnUp_last        = 0;
  debounce.btnDown_state     = 0;
  debounce.btnDown_currently = 0;
  debounce.btnDown_last      = 0;

  comm_uart_init();

  ESC_LedInit();

  bldc_interface_set_rx_value_func(val_received);

  chThdCreateStatic(ESCControl_wa, sizeof(ESCControl_wa), NORMALPRIO + 1, ESCControl, NULL);
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

/*
 * Shell function
 */
void cmd_escCommBtnValues(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  
  while (chnGetTimeout((BaseChannel *)chp, TIME_IMMEDIATE) == Q_TIMEOUT) {

    chprintf(chp, "\x1B\x63");
    chprintf(chp, "\x1B[2J");
    chprintf(chp, "\x1B[%d;%dH", 0, 0);

    chprintf(chp, "btnUp_state       : %d\r\n", debounce.btnUp_state       );
    chprintf(chp, "btnUp_currently   : %d\r\n", debounce.btnUp_currently   );
    chprintf(chp, "btnUp_last        : %d\r\n", debounce.btnUp_last        );
    chprintf(chp, "btnDown_state     : %d\r\n", debounce.btnDown_state     );
    chprintf(chp, "btnDown_currently : %d\r\n", debounce.btnDown_currently );
    chprintf(chp, "btnDown_last      : %d\r\n", debounce.btnDown_last      );
    chprintf(chp, "---------------------------------------------------\r\n");
    chprintf(chp, "currently_set_rpm : %d\r\n", currently_set_rpm          );
        
    chThdSleepMilliseconds(100);
  }
}