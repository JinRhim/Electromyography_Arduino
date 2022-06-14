#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "hal/adc_types.h"


// ADC1: GPIO32 - GPIO39 
// ADC2: cannot be used with wifi / bluetooth 

// Vref = reference voltage for comparison 
// ADC_ATTEN_DB_11 = 150mv ~ 2450mv 

// ADC1 channel 5 is GPIO33. 

// https://techtutorialsx.com/2017/10/07/esp32-arduino-timer-interrupts/

// https://espressif-docs.readthedocs-hosted.com/projects/arduino-esp32/en/latest/api/timer.html

// https://www.youtube.com/watch?v=RWgyCcnUxPY

// default clock = 80 mHz 

// spinlock = a thread is staying in loop (spin) to check whether the lock is available. 




const int ledPin = 23;

uint16_t adcRawVal;

uint16_t adcMax; 
uint16_t adcMin; 

const int lower_threshold = 1000; 
const int upper_threshold = 1500; 


volatile int interruptCounter;
int totalInterruptCounter;

hw_timer_t * timer = NULL;

//synchronization between the main code and interrupt. 
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer() {     //compiler pace IRAM_ATTR to compile in Internal RAM.
  //enter internal service routine for timer interrupt
  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter++;

  //exit internal service routine 
  portEXIT_CRITICAL_ISR(&timerMux);
 
}

void setup() {
  Serial.begin(115200); 
  pinMode(ledPin, OUTPUT);
  
  timer = timerBegin(0, 80, true);  // every 1 seconds timer is triggered 

  //timer = global varaiable for time 
  // &onTimer = pointer to the initialized timer 
  // edge trigger = true. level trigger = false.
  timerAttachInterrupt(timer, &onTimer, true); 

  // 80 mHz / 80  = 1 million. 
  // 1 million microseconds = 1 seconds 
  // 1000000 / 1000 = 1000 us = 1 ms. 
  // ADC is triggered every 1ms to keep 1000 Hz sampling rate. 
  timerAlarmWrite(timer, 100000, true);  //divide by 1000.
  timerAlarmEnable(timer);
}

void TriggerADC() {
  adcRawVal = adc1_get_raw(ADC1_CHANNEL_5); 

  adcRawVal = 4095 - adcRawVal; //invert the value
 
  Serial.print("ADC Raw Value = "); 

  Serial.printf("%d %d %d\n", adcRawVal, lower_threshold, upper_threshold);
}

void loop() {
  if (interruptCounter > 0) {
 
    portENTER_CRITICAL(&timerMux);
    interruptCounter--;
    portEXIT_CRITICAL(&timerMux);
 
    totalInterruptCounter++;
    TriggerADC(); 
    Serial.print(totalInterruptCounter);
  }

  if (adcRawVal > lower_threshold && adcRawVal > upper_threshold) {
      //passed threshold, do something 
      digitalWrite(ledPin, HIGH); 
  }
  else {
      digitalWrite(ledPin, LOW);
  }


  
 
}
