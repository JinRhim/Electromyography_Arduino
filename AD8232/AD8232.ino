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


// ADC1 channel 0 is GPIO 36.
// ADC1 channel 5 is GPIO33. 


// https://techtutorialsx.com/2017/10/07/esp32-arduino-timer-interrupts/

// https://espressif-docs.readthedocs-hosted.com/projects/arduino-esp32/en/latest/api/timer.html

// https://www.youtube.com/watch?v=RWgyCcnUxPY

// default clock = 80 mHz 

// spinlock = a thread is staying in loop (spin) to check whether the lock is available. 


const int ledPin = 23;
const int ledPin2 = 22; 

const int LO_Minus = 35;
const int LO_Plus = 34; 

uint16_t adcRawVal;

uint16_t adcMax; 
uint16_t adcMin; 

const int lower_threshold = 50; 
const int upper_threshold = 100;
const int upper_threshold_2 = 000; 

uint16_t ADC_Buff[100]; 
int ADC_Buff_Index = 0; 
int sum = 0; 
int ADC_avg = 0; 



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
  pinMode(ledPin2, OUTPUT);
  pinMode(LO_Minus, INPUT); //setup for leads off detection LO-
  pinMode(LO_Plus, INPUT); //Setupt for leads off detection LO+


  adc_power_acquire(); 
  adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_11);
  
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

  //adcRawVal = 4095 - adcRawVal; //invert the value


  //save the adc value to 100 array
  ADC_Buff[ADC_Buff_Index] = adcRawVal; 
  //once the data is 100th index, it will overwrite itself
  ADC_Buff_Index = (ADC_Buff_Index + 1) % 100; 

  if (ADC_Buff_Index == 0) {
    sum = 0; 
    for (int i = 0; i < 100; i++) {
      sum += ADC_Buff[i]; 
    }
    ADC_avg = sum/100;  //calculate 100 datapoint average
  }
  
  Serial.printf("%d  %d  %d %d\n ", adcRawVal, lower_threshold, upper_threshold, upper_threshold_2);
}
 

void loop() {
  if (interruptCounter > 0) {
 
    portENTER_CRITICAL(&timerMux);
    interruptCounter--;
    portEXIT_CRITICAL(&timerMux);
 
    totalInterruptCounter++;
    TriggerADC(); 
    //Serial.print(totalInterruptCounter);
  } 

  if (adcRawVal > upper_threshold) {
      //passed threshold, do something 
      digitalWrite(ledPin, HIGH); 
  }
  else if (adcRawVal < lower_threshold){
      digitalWrite(ledPin, LOW);
  }
  if (adcRawVal > upper_threshold_2) {
      digitalWrite(ledPin2, HIGH);
  }
  else {
    digitalWrite(ledPin2, LOW);
  }
  

  
  if((digitalRead(LO_Minus) == 1)||(digitalRead(LO_Plus) == 1)){
    Serial.println('!');
  }


 
}
 
