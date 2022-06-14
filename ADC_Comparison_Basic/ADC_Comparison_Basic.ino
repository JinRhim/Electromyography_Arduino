// this is basic ADC tutorial 


#define ADCPIN A0 

uint adcVal; 
float voltVal; 

void setup() {
  Serial.begin(115200); 
}

void loop() {
  adcVal = analogRead(ADCPIN); 
  voltVal = ((adcVal * 3.3)/ 4095); 
  Serial.print("ADC Value = "); 
  Serial.print(adcVal); 

  Serial.print(" "); 
  Serial.print("Voltage = "); 
  Serial.print(voltVal);
  Serial.println(" V"); 
  delay(100);

}
