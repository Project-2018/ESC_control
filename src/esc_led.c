#include "esc_led.h"

#define ESC_PWM_FREQ			      1000000   /* 1MHz PWM clock frequency. */
#define ESC_PWM_PERIOD			    1000      /* Initial PWM period 1ms.   */
#define ESC_PWM_DRIVER          PWMD9
#define ESC_PWM_UP_CHANNEL      1
#define ESC_PWM_DOWN_CHANNEL    2
#define ESC_PWM_MAX_DUTY        5000
#define ESC_PWM_IDLE_DUTY       2000
#define ESC_PWM_MIN_DUTY        0

#define LED_TASK_PERIOD_MS      100
#define LED_ANIMATION_TIME      20

static ledState upLedState, downLedState;

uint16_t LedStateCounterUp = 0;
uint16_t CalcPwmUp = 0;

uint16_t LedStateCounterDown = 0;
uint16_t CalcPwmDown = 0;

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
static THD_WORKING_AREA(btnled_thd_wa, 256);
static THD_FUNCTION(btnled_thd, p) {

  (void)p;
  chRegSetThreadName("btn_led");
  while (TRUE) {

    switch(downLedState){
    	case LED_OFF:
    	    pwmEnableChannel(&ESC_PWM_DRIVER, ESC_PWM_DOWN_CHANNEL-1, PWM_PERCENTAGE_TO_WIDTH(&ESC_PWM_DRIVER, ESC_PWM_MIN_DUTY));
          LedStateCounterDown = 0;
          CalcPwmDown = 0;
    	    break;

    	case LED_FULL:
          if(LedStateCounterDown < LED_ANIMATION_TIME){
            CalcPwmDown += ((float)ESC_PWM_MAX_DUTY / (float)LED_ANIMATION_TIME);
            pwmEnableChannel(&ESC_PWM_DRIVER, ESC_PWM_DOWN_CHANNEL-1, PWM_PERCENTAGE_TO_WIDTH(&ESC_PWM_DRIVER, CalcPwmDown));
            LedStateCounterDown++;
          }else{
            pwmEnableChannel(&ESC_PWM_DRIVER, ESC_PWM_DOWN_CHANNEL-1, PWM_PERCENTAGE_TO_WIDTH(&ESC_PWM_DRIVER, ESC_PWM_MAX_DUTY));
          }
    	    
    	    break;

    	case LED_PUSHED:
    	    pwmEnableChannel(&ESC_PWM_DRIVER, ESC_PWM_DOWN_CHANNEL-1, PWM_PERCENTAGE_TO_WIDTH(&ESC_PWM_DRIVER, ESC_PWM_IDLE_DUTY));
    	    break;

    	case LED_ERROR:
    	    break;

    	default:
    	    break;
    }


    switch(upLedState){
      case LED_OFF:
          pwmEnableChannel(&ESC_PWM_DRIVER, ESC_PWM_UP_CHANNEL-1, PWM_PERCENTAGE_TO_WIDTH(&ESC_PWM_DRIVER, ESC_PWM_MIN_DUTY));
          LedStateCounterUp = 0;
          CalcPwmUp = 0;
          break;

      case LED_FULL:
          if(LedStateCounterUp < LED_ANIMATION_TIME){
            CalcPwmUp += ((float)ESC_PWM_MAX_DUTY / (float)LED_ANIMATION_TIME);
            pwmEnableChannel(&ESC_PWM_DRIVER, ESC_PWM_UP_CHANNEL-1, PWM_PERCENTAGE_TO_WIDTH(&ESC_PWM_DRIVER, CalcPwmUp));
            LedStateCounterUp++;
          }else{
            pwmEnableChannel(&ESC_PWM_DRIVER, ESC_PWM_UP_CHANNEL-1, PWM_PERCENTAGE_TO_WIDTH(&ESC_PWM_DRIVER, ESC_PWM_MAX_DUTY));
          }
          break;

      case LED_PUSHED:
          pwmEnableChannel(&ESC_PWM_DRIVER, ESC_PWM_UP_CHANNEL-1, PWM_PERCENTAGE_TO_WIDTH(&ESC_PWM_DRIVER, ESC_PWM_IDLE_DUTY));
          break;

      case LED_ERROR:
          break;

      default:
          break;
    }

    chThdSleepMilliseconds(LED_TASK_PERIOD_MS);
  }
}

void ESC_LedInit(void){

    upLedState   = LED_OFF;
    downLedState = LED_OFF;

    pwmStart(&ESC_PWM_DRIVER, &esc_pwmcfg);
    pwmDisableChannel(&ESC_PWM_DRIVER, ESC_PWM_UP_CHANNEL-1);
    pwmDisableChannel(&ESC_PWM_DRIVER, ESC_PWM_DOWN_CHANNEL-1);

    /*
     * Creates the up button PWM control.
     */
    chThdCreateStatic(btnled_thd_wa, sizeof(btnled_thd_wa), NORMALPRIO, btnled_thd, NULL);
}

void setUpLedState(ledState tmp){
    upLedState = tmp;
}

void setDownLedState(ledState tmp){
    downLedState = tmp;
}