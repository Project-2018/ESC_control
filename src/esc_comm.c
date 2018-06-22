#include <stdio.h>
#include <string.h>

#include "ch.h"
#include "hal.h"

#include "chprintf.h"

#include "esc_comm.h"
#include "comm_uart.h"
#include "console.h"
#include "esc_led.h"

#include "usbcfg.h"

#define DEBOUNCE_DELAY_TIME     4      /* 40ms */
#define ESC_RPM                 4800   /* 600 RPM */
#define RPM_STEP                200    /* RPM step */
#define PRESCALER_TIME          2      /* x * thd cycle */

static uint8_t prescaler;
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

    time = 10;
    

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


    
    /* Switching the LEDs */
    debounce.btnUp_currently   ? setUpLedState(LED_PUSHED)   : setUpLedState(LED_FULL);
    debounce.btnDown_currently ? setDownLedState(LED_PUSHED) : setDownLedState(LED_FULL);



    /* Up button is pressed */
    if (debounce.btnUp_state)
    {   
        if (PRESCALER_TIME < prescaler++)
        {
            currently_set_rpm = (currently_set_rpm + RPM_STEP) < ESC_RPM ? currently_set_rpm + RPM_STEP : ESC_RPM;
            prescaler = 0;
        }
        if (currently_set_rpm > 0)
        {
            bldc_interface_set_rpm(currently_set_rpm);
        }
        //bldc_interface_set_duty_cycle(0.5);
        //bldc_interface_set_current(2);

    }
    
    /* Down button is pressed */
    if (debounce.btnDown_state)
    {
        if (PRESCALER_TIME < prescaler++)
        {
            currently_set_rpm = (currently_set_rpm - RPM_STEP) > -ESC_RPM ? currently_set_rpm - RPM_STEP : -ESC_RPM;
            prescaler = 0;
        }
        
        if (currently_set_rpm < 0)
        {
            bldc_interface_set_rpm(currently_set_rpm);
        }
        //bldc_interface_set_current(2);
    }

    if ((debounce.btnUp_state == 0) && (debounce.btnDown_state == 0))
    {
        if (PRESCALER_TIME < prescaler++)
        {
            
            if (currently_set_rpm > 0)
            {
                currently_set_rpm = (currently_set_rpm - RPM_STEP) > 0 ? currently_set_rpm - RPM_STEP : 0;
            }
            else if (currently_set_rpm < 0)
            {
                currently_set_rpm = (currently_set_rpm + RPM_STEP) < 0 ? currently_set_rpm + RPM_STEP : 0;
            }
            prescaler = 0;
        }
        
        bldc_interface_set_rpm(currently_set_rpm);
        //bldc_interface_set_handbrake(5);
    }


    bldc_interface_get_values();
    //bldc_interface_get_fw_version();
    //bldc_interface_set_current ( 10.0 );

    chThdSleepMilliseconds(time);
  }
}

void ESC_ControlInit(void){

//  debounce.btnUp_state       = 0;
//  debounce.btnUp_currently   = 0;
//  debounce.btnUp_last        = 0;
//  debounce.btnDown_state     = 0;
//  debounce.btnDown_currently = 0;
//  debounce.btnDown_last      = 0;

//  comm_uart_init();

//  ESC_LedInit();

//  bldc_interface_set_rx_value_func(val_received);

//  chThdCreateStatic(ESCControl_wa, sizeof(ESCControl_wa), NORMALPRIO + 1, ESCControl, NULL);
}

double getESCTemp(void)          { return mc_val->temp_pcb; }
double getAcCurrent(void)        { return mc_val->current_motor; }
double getDcCurrent(void)        { return mc_val->current_in; }
double getERPM(void)             { return mc_val->rpm; }
double getDutyCycle(void)        { return mc_val->duty_now * 100.0; }
double getAmpHours(void)         { return mc_val->amp_hours; }
double getAmpHoursCharged(void)  { return mc_val->amp_hours_charged; }
double getWattHours(void)        { return mc_val->watt_hours; }
double getWattHoursCharged(void) { return mc_val->watt_hours_charged; }
int16_t getTachometer(void)      { return mc_val->tachometer; }
int16_t getTachometerAbs(void)   { return mc_val->tachometer_abs; }
