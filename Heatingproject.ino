// Application for Running the Heating system at Long Ridge
// 2012-Dec-22 Jon Bartlett, various parts borrowed from other example sketches
//Arduino IDE v1.0.3 and librarys from Dec-2012

#include <EtherCard.h>
#include "NanodeMAC.h"
#include <Time.h> 
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
//#include <avr/pgmspace.h>

//Set Applications debug options - 1=on, 0= off.
#define  DEBUG 1
#define  DEBUG_MEM 0

// These settings are to upload the real time data to Pachube (COSM)
#define FEED    "yourfeed_id"
#define APIKEY  "yjR4WMsdfggAhgfdKxghjkghjkkcDlpaz0g"
#define SERIAL_BAUD_RATE 115200

//Define where the DS18S20's are connected to
//Data wire from DS18S20 Temperature chip is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 3
//Define the Digital Pin outs for the three relays
//we could do this using the Analogue ports (as they're free).
#define PUMP_ON 4
#define CALL_HW 7
#define CALL_HEATING 10
//The time between readings (in secs)
#define READINGINTERVAL 10
//Temp reading varibility allowed so we dont flip-flop on temp noise values
#define VARIABILITY 0.1

//Define which Bytes of EEPROM memory to use for the target temp data
#define TARGETROOMADDRESS 00
#define TARGETWATERADDRESS 02
#define TARGETPUMPADDRESS 04

//Variables to hold the sensor and target readings
//Could put this in the main loop, but less efficient to keep creating and destroying heap
float currentRoomTemp;
float currentWaterTemp;
float targetRoomTemp;
float targetWaterTemp;
//These variable are for convenience in main loop.
boolean pump;
boolean room;
boolean hw;

// ethernet interface mac address, must be unique on the LAN
static uint8_t mymac[6] = { 0,0,0,0,0,0 };
NanodeMAC mac( mymac );        //Use Nanodes MAC address chip

char website[] PROGMEM = "api.pachube.com";

byte Ethernet::buffer[700];
Stash stash;
static BufferFiller bfill;  // used as cursor while filling the buffer

//Time stuff
unsigned long timer;
unsigned long timesyncs=0;
static time_t datetime=0;
//Number of seconds for timezone 
//    0=UTC (London)
//19800=5.5hours Chennai, Kolkata
//36000=Brisbane (UTC+10hrs)
#define timeZoneOffset 0*60*60
static uint8_t ntpMyPort = 123;
char sntpserver[] PROGMEM = "uk.pool.ntp.org"; //was "3.pool.ntp.org";
#if DEBUG
const unsigned long SyncInterval=5*60; //Set the SyncInterval to every 5 minutes when in Debug mode
#else
const unsigned long SyncInterval=5*3600; //Otherwise, set to every five hours
#endif
const unsigned long seventy_years = 2208988800UL;

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device addresses (one thermocouple on the room air temp, the other on the Hot Water cylinder
DeviceAddress RoomThermocouple;
DeviceAddress HWThermocouple;


void setup () {

  //Define the Relay outputs
  pinMode(PUMP_ON, OUTPUT);
  pinMode(CALL_HW, OUTPUT);
  pinMode(CALL_HEATING, OUTPUT);
  
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println("[webClient]");
  
  if (ether.begin(sizeof Ethernet::buffer, mymac) == 0) 
    Serial.println( "Failed to access Ethernet controller");
  if (!ether.dhcpSetup())
    Serial.println("DHCP failed");  
  #ifdef DEBUG    
  //Show IP data if we're debgging...
  ether.printIp("IP:  ", ether.myip);
  ether.printIp("GW:  ", ether.gwip);  
  ether.printIp("DNS: ", ether.dnsip);  
  #endif

  if (!ether.dnsLookup(website))
    Serial.println("DNS failed");
  ether.printIp("SRV: ", ether.hisip);
  
  //Now setup the temperature
  // locate Temperature devices on the (one-wire) bus
  sensors.begin();
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices");
  if (sensors.getDeviceCount() != 2 )
    Serial.println( "Sensors not found");

  //Report parasite power requirements
  //Serial.print("Parasite power is: "); 
  //if (sensors.isParasitePowerMode()) Serial.println("ON");
  //else Serial.println("OFF");
  
  //Do this the hardway - should be a loop with device address array...
  //Get the first Thermometer
  if (!sensors.getAddress(RoomThermocouple, 0)) Serial.println("No Room Thermo");   
//  Serial.print("Room Address: ");
//  printAddress(RoomThermocouple);
//  Serial.println();
  //Get the second thermometer
  if (!sensors.getAddress(HWThermocouple, 1)) Serial.println("No HotWater Thermo");   


  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(RoomThermocouple, 9);
  sensors.setResolution(HWThermocouple, 9);

  // show the addresses we found on the bus
//  Serial.print("Device 0 Resolution: ");
//  Serial.println(sensors.getResolution(RoomThermocouple), DEC); 
  
  //Initiate the first sensor reading (so we dont get bogus value on first time round)
  sensors.requestTemperatures(); // Send the command to get temperatures
  
  //setup the time from Internet stuff
  setSyncInterval(SyncInterval);  // only works if the now() function is referenced.
  setSyncProvider(getNtpTime);  //This also fires off a refresh of the time immediately

  //Code adapted from using millis to 'now' + interval - which is in  secs rather than thousanths.
  timer = now() + READINGINTERVAL;
  
}

void loop () {
   
  //Look out for incoming data to us via Ethernet, and respond if it does.
  checkInputStream();
    
  //if its time to take a reading, then do so  
  if (now() > timer) {
    //Code needed to refresh the stash due to memory leak in the Ethercard library.
    #if DEBUG_MEM   
    Serial.print("Time-Now:");
    digitalClockDisplay(now());
    Serial.print(" Timer: ");
    digitalClockDisplay(timer);
    Serial.print("Stash Free:");
    Serial.println(Stash::freeCount());
    #endif
    if(Stash::freeCount()<45){
  	Stash::initMap(56);
                Serial.println("Stash reset");
      }
    //Get the current readings from the temp sensors (they should have settled by now
    currentRoomTemp = sensors.getTempC(RoomThermocouple);
    currentWaterTemp = sensors.getTempC(HWThermocouple);
    //Get the current target temps
    //Note: Water is whole integer degrees only
    targetRoomTemp = EEPROM.read(TARGETROOMADDRESS)/10.0;
    targetWaterTemp = EEPROM.read(TARGETWATERADDRESS);
    //This determines if we're on or not.
    //Ultimately, we could simply leave on all the time, and adjust target temps at the set time
    pump = EEPROM.read(TARGETPUMPADDRESS)>0;

    #ifdef DEBUG
    //output the targets and current temps
    Serial.print("Target-R: ");
    Serial.print( targetRoomTemp);
    Serial.print(",W: ");
    Serial.println(targetWaterTemp);
    Serial.print("Actual-R: ");
    Serial.print( currentRoomTemp);
    Serial.print(",W: ");
    Serial.println(currentWaterTemp);
    Serial.print("NTPSyncs: ");
    Serial.println(timesyncs);    
    #endif
    
    //take action based upon the readings:
    //If heating is ON, then we can change the state of the HW or Heater
    if (pump) {
      //Use 0.05 either side of deesired temp so, we dont switch on and then off from the noise of the sensors
      //Note, this means that when the when the temp is within the VARIABILITY, then the pin isn't set at all...
      if (currentRoomTemp < targetRoomTemp - VARIABILITY)
      {
        room = true;
      }
      if (currentRoomTemp > targetRoomTemp + VARIABILITY)
      {
        room = false;
      }  
      //Switch Hot Water based on temp
      if (currentWaterTemp < targetWaterTemp - VARIABILITY)
      {
        hw = true;
      }
      if (currentWaterTemp > targetWaterTemp + VARIABILITY)
      {
        hw = false;
      }
    }  
    else {
      //Turn the other relays off to be safe
      hw= false;
      room = false;      
    }

    #if DEBUG
    Serial.print(F("States- Pump: ")); Serial.print(pump);
    Serial.print(F(" Room: ")); Serial.print(room);
    Serial.print(F(" Water: "));Serial.println(hw);
    #endif   

    //Now set the relays (all of them based on state).
    switchRelay(PUMP_ON, pump);
    switchRelay(CALL_HEATING, room);
    switchRelay(CALL_HW, hw);  
    
    //Send data to Pachube/COSM
    //Need to reget the IP address of Pachube, as we're also doing NTP lookups....
    if (!ether.dnsLookup(website)){
    #if DEBUG      
      Serial.println("DNS fail");
    #endif      
      //reset Nanode
      asm volatile ("  jmp 0"); 
    } 
    
    byte sd = stash.create();
    stash.print(F("temperature1,"));
    stash.println(currentRoomTemp);
    stash.print(F("temperature2,"));
    stash.println(currentWaterTemp);
    stash.print(F("heating,"));
    stash.println(room);
    stash.print(F("water,"));
    stash.println(hw);
    stash.print(F("pump,"));
    stash.println(pump);
    //Send the target temps up to COSM too.  This is a bit lazy, as they wont change that often.
    stash.print(F("SPRoom,"));
    stash.println(targetRoomTemp);
    stash.print(F("SPWater,"));
    stash.println(targetWaterTemp);
    //Send the number of NTP syncs we've done too
    stash.print(F("NTPSyncs,"));
    stash.println(timesyncs);
    stash.save();    
    // generate the header with payload - note that the stash size is used,
    // and that a "stash descriptor" is passed in as argument using "$H"
    Stash::prepare(PSTR("PUT http://$F/v2/feeds/$F.csv HTTP/1.0" "\r\n"
                        "Host: $F" "\r\n"
                        "X-PachubeApiKey: $F" "\r\n"
                        "Content-Length: $D" "\r\n"
                        "\r\n"
                        "$H"),
            website, PSTR(FEED), website, PSTR(APIKEY), stash.size(), sd);

    // send the packet - this also releases all stash buffers once done
    ether.tcpSend();
    
    //Set the time for the next reading (do retreival of Ethernet packets while not)
    timer= now() + READINGINTERVAL;
    sensors.requestTemperatures(); // Send the command to get temperatures again, let them settle whilst we're waiting
#if DEBUG_MEM
    Serial.print("Mem free:");
    Serial.println(freeRam());
#endif    
  }
}

 // function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}


void switchRelay(int pin, boolean setting)
  {
    digitalWrite(pin, setting);
  } 

unsigned long getNtpTime() {
  unsigned long timeFromNTP;
  unsigned long ReplyTries=0;

  if (!ether.dnsLookup( sntpserver )) {
    Serial.println("DNS fail");
    //reset Nanode
    asm volatile ("  jmp 0"); 
  } 
  else {
    //ether.printIp("SRV: ", ether.hisip);
    ether.ntpRequest(ether.hisip, ntpMyPort);
    Serial.print("NTP request ");
    //If this doesn't return a valid time from NTP, then it gets stuck, so put an arbitary number of loops 10000?
    while(ReplyTries++ < 10000) {
      word length = ether.packetReceive();
      ether.packetLoop(length);
      if(length > 0 && ether.ntpProcessAnswer(&timeFromNTP,ntpMyPort)) {
#if DEBUG        
        Serial.print("Tries :"); Serial.print(ReplyTries);
#endif        
        Serial.print(" Reply :");
        timesyncs++;;
        timeFromNTP=timeFromNTP - seventy_years + timeZoneOffset;
        digitalClockDisplay(timeFromNTP);
        return timeFromNTP;
      }
    }
#if DEBUG
    Serial.println(" Fail"); 
#endif
  }
  return 0;
}
