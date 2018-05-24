#include <stdio.h>
#include <string.h>

#include "ch.h"
#include "hal.h"

#include "chprintf.h"

#include "esc_comm.h"
#include "comm_uart.h"
#include "bldc_interface.h"
#include "console.h"
#include "esc_led.h"

#include "usbcfg.h"

#define DEBOUNCE_DELAY_TIME     4      /* 40ms */
#define ESC_RPM                 1000

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



    debounce.btnUp_currently   ? setUpLedState(LED_FULL)   : setUpLedState(LED_IDLE);
    debounce.btnDown_currently ? setDownLedState(LED_FULL) : setDownLedState(LED_IDLE);



    /* Up button is pressed */
    if (debounce.btnUp_state)
    {
        //bldc_interface_set_rpm(ESC_RPM);
    }
    else{
    }
    


    /* Down button is pressed */
    if (debounce.btnDown_state)
    {
        //bldc_interface_set_rpm(-ESC_RPM);
    }
    else{
    }

    if ((debounce.btnUp_state == 0) && (debounce.btnDown_state == 0))
    {
        //bldc_interface_set_rpm(0);
        //bldc_interface_set_handbrake(5);
    }


    //bldc_interface_get_values();
    //bldc_interface_get_fw_version();
    //bldc_interface_set_current ( 10.0 );

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

  chThdCreateStatic(ESCControl_wa, sizeof(ESCControl_wa), NORMALPRIO + 1, ESCControl, NULL);
}
