static void digitalClockDisplay(time_t t){
  // digital clock display  
  printDigits(hour(t));
  Serial.print(':');
  printDigits(minute(t));
  Serial.print(':');
  printDigits(second(t));
  Serial.print(" Day: ");
  printDigits(day(t));
  Serial.print("/");
  printDigits(month(t)); 
  Serial.print("/");
  Serial.print(year(t)); 

 
  Serial.println();
}

static void printDigits(int digits){
  if(digits < 10) Serial.print('0');
  Serial.print(digits);
}

static void printHexDigits(int digits){
  if(digits <= 0xF) Serial.print('0');
  Serial.print(digits,HEX);
}


//Check the free RAM (debug only)
#if DEBUG_MEM
static int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
#endif 
