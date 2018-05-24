#include "esc_led.h"

#define ESC_PWM_FREQ			1000000   /* 1MHz PWM clock frequency. */
#define ESC_PWM_PERIOD			1000      /* Initial PWM period 1ms.   */
#define ESC_PWM_DRIVER          PWMD9
#define ESC_PWM_UP_CHANNEL      1
#define ESC_PWM_DOWN_CHANNEL    2
#define ESC_PWM_MAX_DUTY        10000
#define ESC_PWM_IDLE_DUTY       2000
#define ESC_PWM_MIN_DUTY        0

static ledState upLedState, downLedState;

static PWMConfig esc_pwmcfg = {
  ESC_PWM_FREQ,
  ESC_PWM_PERIOD,
  NULL,
  {
   {PWM_OUTPUT_ACTIVE_HIGH, NULL},
   {PWM_OUTPUT_ACTIVE_HIGH, NULL},
   {PWM_OUTPUT_DISABLED, NULL},
   {PWM_OUTPUT_DISABLED, NULL}
  },
  0,
  0
};

/*
 * Blinker thread.
 */
static THD_WORKING_AREA(up_led_thd_wa, 128);
static THD_FUNCTION(up_led_thd, p) {

  (void)p;
  chRegSetThreadName("up_button_led");

  while (TRUE) {
    systime_t time;
    time = 5;

    switch(upLedState){
    	case LED_OFF:
    	    pwmEnableChannel(&ESC_PWM_DRIVER, ESC_PWM_UP_CHANNEL-1, PWM_PERCENTAGE_TO_WIDTH(&ESC_PWM_DRIVER, ESC_PWM_MIN_DUTY));
    	    break;

    	case LED_FULL:
    	    pwmEnableChannel(&ESC_PWM_DRIVER, ESC_PWM_UP_CHANNEL-1, PWM_PERCENTAGE_TO_WIDTH(&ESC_PWM_DRIVER, ESC_PWM_MAX_DUTY));
    	    break;

    	case LED_IDLE:
    	    pwmEnableChannel(&ESC_PWM_DRIVER, ESC_PWM_UP_CHANNEL-1, PWM_PERCENTAGE_TO_WIDTH(&ESC_PWM_DRIVER, ESC_PWM_IDLE_DUTY));
    	    break;

    	case LED_ERROR:
    	    break;

    	default:
    	    break;
    }

    chThdSleepMilliseconds(time);
  }
}

/*
 * Blinker thread.
 */
static THD_WORKING_AREA(down_led_thd_wa, 128);
static THD_FUNCTION(down_led_thd, p) {

  (void)p;
  chRegSetThreadName("down_button_led");
  while (TRUE) {
    systime_t time;
    time = 10;

    switch(downLedState){
    	case LED_OFF:
    	    pwmEnableChannel(&ESC_PWM_DRIVER, ESC_PWM_DOWN_CHANNEL-1, PWM_PERCENTAGE_TO_WIDTH(&ESC_PWM_DRIVER, ESC_PWM_MIN_DUTY));
    	    break;

    	case LED_FULL:
    	    pwmEnableChannel(&ESC_PWM_DRIVER, ESC_PWM_DOWN_CHANNEL-1, PWM_PERCENTAGE_TO_WIDTH(&ESC_PWM_DRIVER, ESC_PWM_MAX_DUTY));
    	    break;

    	case LED_IDLE:
    	    pwmEnableChannel(&ESC_PWM_DRIVER, ESC_PWM_DOWN_CHANNEL-1, PWM_PERCENTAGE_TO_WIDTH(&ESC_PWM_DRIVER, ESC_PWM_IDLE_DUTY));
    	    break;

    	case LED_ERROR:
    	    break;

    	default:
    	    break;
    }

    chThdSleepMilliseconds(time);
  }
}

void ESC_LedInit(void){

    upLedState   = LED_OFF;
    downLedState = LED_OFF;

    pwmStart(&ESC_PWM_DRIVER, &esc_pwmcfg);

    /*
     * Creates the up button PWM control.
     */
    chThdCreateStatic(up_led_thd_wa, sizeof(up_led_thd_wa), NORMALPRIO, up_led_thd, NULL);

    /*
     * Creates the down button PWM control.
     */
    chThdCreateStatic(down_led_thd_wa, sizeof(down_led_thd_wa), NORMALPRIO, down_led_thd, NULL);
}

void setUpLedState(ledState tmp){
    upLedState = tmp;
}

void setDownLedState(ledState tmp){
    downLedState = tmp;
}