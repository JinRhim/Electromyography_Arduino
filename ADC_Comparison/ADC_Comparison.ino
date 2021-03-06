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
// ADC2_Channel_ 7 = GPIO27 

// default clock = 80 mHz 


const int ledPin = 23;

uint16_t adcRawVal;
uint16_t adcRawVal_2;
int read_raw; 

uint16_t adcMax; 
uint16_t adcMin; 

const int lower_threshold = 50; 
const int upper_threshold = 100; 

const int lower_bound = 000;
const int upper_bound = 4095;


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

  adc_power_acquire(); 
  adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_11);
  adc2_config_channel_atten(ADC2_CHANNEL_7, ADC_ATTEN_DB_11); 
  
  timer = timerBegin(0, 80, true);  // every 1 seconds timer is triggered 

  //timer = global varaiable for time 
  // &onTimer = pointer to the initialized timer 
  // edge trigger = true. level trigger = false.
  timerAttachInterrupt(timer, &onTimer, true); 

  // 80 mHz / 80  = 1 million. 
  // 1 million microseconds = 1 seconds 
  // 1000000 / 1000 = 1000 us = 1 ms.
  // ADC is triggered every 1ms to keep 1000 Hz sampling rate. 
  timerAlarmWrite(timer, 5000, true);  //divide by 1000.
  timerAlarmEnable(timer);
}

void TriggerADC() {
  adcRawVal = adc1_get_raw(ADC1_CHANNEL_5); 
  esp_err_t r = adc2_get_raw( ADC2_CHANNEL_7, ADC_WIDTH_12Bit, &read_raw);

  //Serial.printf("%d %d %d\n %d \n", adcRawVal, lower_threshold, upper_threshold, ADC_avg);
  //Serial.printf("%d %d %d\n ", adcRawVal, lower_bound, upper_bound);

  Serial.printf("%d %d %d %d\n ", adcRawVal, read_raw, lower_bound, upper_bound);

  //Serial.println(adcRawVal);
  Serial.print("\n");
  
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

}
 
