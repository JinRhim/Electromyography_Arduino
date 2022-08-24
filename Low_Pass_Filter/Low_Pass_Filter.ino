//https://www.youtube.com/watch?v=HJ-C4Incgpw



float xn1 = 0; 
float yn1 = 0; 
int k = 0; 

void setup() {
  Serial.begin(115200);
}

void loop() {
  float t = micros()/1.0e6; 
  //raw signal 
  float xn = sin(1* PI*2*t) + 0.2*sin(2* PI * 50* t); 


  //filtered signal 
  float yn = 0.969*yn1 + 0.0155*xn + 0.0155*xn1; 

  delay(1); 

  xn1 = xn; 
  yn1 = yn; 

  if (k % 3 == 0) {
    Serial.print(2*xn); //this prints original signal 
    Serial.print(" "); 
    Serial.println(2*xn - 2*yn);  
  }
  k++; 
}
