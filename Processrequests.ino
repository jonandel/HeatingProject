// The Heating project settings are controlled by a HTTP requests
// Commands :
//            <IP_of_Nanode>/setroom=XYZ
//            <IP_of_Nanode>/setwater=XYZ
//            <IP_of_Nanode>/setpump=XYZ
// Where :
//    XYZ is a three digit number.
//      for the heating and water temperature settings, this value is is tenths of a degree (e.g. 27.2 degrees is 272)
//      for the pump, 000 is OFF and all other values are on
//
// In addition, the status of the system is output as a webpage...
//            <IP_of_Nanode>/status
// However this needs work...currently broken.


//Use these for the param arguments
#define PUMP 1
#define HEATING 2
#define WATER 3

char t1[] PROGMEM = "Status" ;

static void showStatus(BufferFiller& buf) {
  //Reply to HTTP request, with 'OK'.
  //We could make this better, by stating the command done.
  byte ll = stash.create();
  stash.print("Temp");
  stash.print(targetRoomTemp);
  stash.save();
  buf.emit_p(PSTR("HTTP/1.0 200 OK" "\r\n"
          "Content-Type: text/html" "\r\n"
          "Content-Length: $D" "\r\n"          
          "\r\n"
          "<h1>Status" "</h1>"
          "<h2>$F" "</h2>"
          "<h3>$H" "</h3>\r\n"),
          stash.size(), t1, ll);
      Serial.println( targetRoomTemp);
//  Stash::prepare(PSTR("HTTP/1.0 200 OK" "\r\n"
//          "Content-Type: text/html" "\r\n"
//          "\r\n"
//          "<h1>Status Page</h1>"
//          "$H" "\r\n"
//          "<h3>$F" "</h3>" "\r\n")
//          , sd, sd);
 

// ether.tcpSend(); // send web page data
         
 
}  

static void setTargetemp(const char* data, int p, int arg) {
  //All target numbers are three digits - this will last one is tenths
  //p points to the first character of the three in the string
  const char zero = '0';
  
  int value;
  value = (data[p] - zero) * 100 + (data[p+1] - zero) * 10  + (data[p+2] - zero);  
       
  Serial.print("SET ");       
  if (arg == HEATING) {
    //Set the new target
    Serial.print("Room=");
    //Save this data in the EEPROM memory
    EEPROM.write(TARGETROOMADDRESS, value); 
  }
  else if (arg == WATER) {
    Serial.print("Water=");
    //Set the new target
    value = value / 10;
    //Save this data to the EEPROM memory
    //Note, only need integer value for
    EEPROM.write(TARGETWATERADDRESS, value);   
  }
  else if (arg == PUMP) {
    Serial.print("Pump=");
    //Pump is either on of off...
    //Save as EEPROM - value is either on XXX or off 000
    EEPROM.write(TARGETPUMPADDRESS, value > 0); 
  }
  Serial.println(value);
  
  //Reply to HTTP request, with 'OK'.
  //We could make this better, by stating the command done.
  bfill.emit_p(PSTR(
          "HTTP/1.0 200 OK\r\n"
          "Content-Type: text/html\r\n"
          "\r\n"
          "<h1>Set OK!</h1>"));    
}  

static void checkInputStream(){
  //Check the input Ethernet stream, and process if commands are sent to us 
  word len = ether.packetReceive();
  word pos = ether.packetLoop(len);
  // check if valid tcp data is received
  if (pos) {
      bfill = ether.tcpOffset();
      char* data = (char *) Ethernet::buffer + pos;
      //show the string we got sent

#if DEBUG
      Serial.println(data);
#endif      

      //Now process the commands found in the string.
      if (strncmp("GET /setroom=", data, 13) == 0) 
            setTargetemp(data, 13, HEATING);
      else if (strncmp("GET /setwater=", data, 14) == 0)
           setTargetemp(data, 14, WATER);
      else if (strncmp("GET /setpump=", data, 13) == 0)
           setTargetemp(data, 13, PUMP);
      else if (strncmp("GET /status", data, 11) == 0) 
           showStatus(bfill);
      else
            bfill.emit_p(PSTR(
                "HTTP/1.0 401 Unauthorized\r\n"
                "Content-Type: text/html\r\n"
                "\r\n"
                "<h1>401 Unknown command</h1>"));  
      ether.httpServerReply(bfill.position()); // send web page data
         
  }        
}




