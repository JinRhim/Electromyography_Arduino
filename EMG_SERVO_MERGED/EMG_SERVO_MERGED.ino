// ADC_Timer_Template



#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "hal/adc_types.h"

#include <ESP32Servo.h>


uint16_t adcFiltered;
uint16_t adcRawVal;

volatile int interruptCounter;

hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;


Servo myservo_thumb;  // create servo object to control a servo
Servo myservo_index;
Servo myservo_pinky;

// twelve servo objects can be created on most boards

char pos_thumb = 0;    // variable to store the servo position
char pos_index = 0;
char pos_pinky = 0;



void IRAM_ATTR onTimer() {     //compiler pace IRAM_ATTR to compile in Internal RAM.
  //enter internal service routine for timer interrupt
  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter++;

  //exit internal service routine
  portEXIT_CRITICAL_ISR(&timerMux);

}

void setup() {
  Serial.begin(115200);
  adc_power_acquire();
  adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_11);


  timer = timerBegin(0, 80, true);  // every 1 seconds timer is triggered
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 10000, true);  //divide by 1000.
  timerAlarmEnable(timer);

  myservo_thumb.attach(4);  // attaches the servo on pin GPIO 15 to the servo object
  myservo_index.attach(15);
  myservo_pinky.attach(2);

}

void TriggerADC() {
  adcRawVal = adc1_get_raw(ADC1_CHANNEL_5);
  adcFiltered = adcRawVal;

  Serial.printf("%d %d\n ", adcRawVal, adcFiltered );
  Serial.print("\n");

}

void loop() {
  if (interruptCounter > 0) {

    portENTER_CRITICAL(&timerMux);
    interruptCounter--;
    portEXIT_CRITICAL(&timerMux);

    TriggerADC();
  }
  if (adcRawVal > 1000) {
    myservo_thumb.write(30);
    myservo_index.write(40);
    myservo_pinky.write(0);
  }
  else {
    myservo_thumb.write(140);
    myservo_index.write(140);
    myservo_pinky.write(120);


  }
}
