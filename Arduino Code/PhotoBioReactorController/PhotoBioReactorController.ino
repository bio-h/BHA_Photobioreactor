/*
	This file is part of Waag Society's Open Wetlab code repository

	This code is free software: you can 
	redistribute it and/or modify it under the terms of the GNU 
	General Public License as published by the Free Software 
	Foundation, either version 3 of the License, or (at your option) 
	any later version.

	This code is distributed in the hope 
	that it will be useful, but WITHOUT ANY WARRANTY; without even 
	the implied warranty of MERCHANTABILITY or FITNESS FOR A 
	PARTICULAR PURPOSE. See the GNU General Public License for more 
	details.

	You should have received a copy of the GNU General Public License
	along with this code. If not, see 
	<http://www.gnu.org/licenses/>.


# Instructions

The code includes a section on keeping track of time.
To synchronize the clock on Mac / Linux: you can use this command 
"date +T%s\n > /dev/ttyACM0" (UTC time zone)

You can modify the following settings:
- boolean lightSensorToggle = true;
  Choose between Adafruit (true) or BH1750 (false) light sensor
- int minhour = 8;
  int maxhour = 10;
  Set the hours of the day in between which the lights will be on
*/

/* *******************************************************
/  Libraries
*/
#include <Wire.h>
#include "LiquidCrystal_I2C.h" // Needed for operating the LCD screen
#include <math.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include <TimeLib.h>
#include <BH1750.h>
/* *******************************************************
*/

/* *******************************************************
/  Temperature calculation function
*/
double Thermistor(int RawADC) {
 double Temp;
 Temp = log(10000.0*((1024.0/RawADC-1))); 
 Temp = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * Temp * Temp ))* Temp );
 Temp = Temp - 273.15;            // Convert Kelvin to Celcius
 return Temp;
}

/* *******************************************************
/  pH sensor variables
*/
#define SensorPin 0          //pH meter Analog output to Arduino Analog Input 0
unsigned long int avgValue;  //Store the average value of the sensor feedback
float b;
int buf[10],temp;

/* *******************************************************
/  LCD
*/
// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27,16,2);
/* *******************************************************
*/

/* *******************************************************
/  Light sensor setup
*/
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);
BH1750 lightMeter;
boolean lightSensorToggle = true; // true = TSL2561, false = BH1750

/* *******************************************************
/  Clock variables
*/
#define TIME_HEADER  "T"   // Header tag for serial time sync message
#define TIME_REQUEST  7    // ASCII bell character requests a time sync message 
boolean LEDtoggle = true;
int minhour = 8;
int maxhour = 10;

/* *******************************************************
/  Configures the gain and integration time for the TSL2561
*/
void configureSensor(void)
{
  /* You can also manually set the gain or enable auto-gain support */
  // tsl.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright light to avoid sensor saturation */
  // tsl.setGain(TSL2561_GAIN_16X);     /* 16x gain ... use in low light to boost sensitivity */
  tsl.enableAutoRange(true);            /* Auto-gain ... switches automatically between 1x and 16x */
  
  /* Changing the integration time gives you better sensor resolution (402ms = 16-bit data) */
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);      /* fast but low resolution */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium resolution and speed   */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit data but slowest conversions */

  /* Update these values depending on what you've set above! */  
  Serial.println("------------------------------------");
  Serial.print  ("Gain:         "); Serial.println("Auto");
  Serial.print  ("Timing:       "); Serial.println("13 ms");
  Serial.println("------------------------------------");
}

/* *******************************************************
/  Setup function, this code only runs once
*/
void setup()
{
  /* Open Serial connection */
  Serial.begin(9600);  
  Serial.println("Ready");    //Test the serial monitor

  /* Set LED pin to output */
  pinMode(13,OUTPUT); 
  pinMode(6,OUTPUT);

  if(lightSensorToggle) {
    /* Initialise the Adafruit light sensor */
    if(!tsl.begin())
    {
      /* There was a problem detecting the ADXL345 ... check your connections */
      Serial.print("Ooops, no TSL2561 detected ... Check your wiring or I2C ADDR!");
      while(1);
    }
    
    /* Setup the sensor gain and integration time of the light sensor */
    configureSensor();
  }
  else {
    /* Initialise the BH1750 ligth sensor */
    lightMeter.begin();
  }
  
  /* Setup the clock */
  setSyncProvider( requestSync);  //set function to call when sync required

  // Initialize the LCD and print a message
  lcd.init(); // start the LCD
  lcd.backlight(); // enable the backlight
  lcd.clear(); // Clear the screen from any character
  lcd.setCursor(0,0); // Start writing from position 0 row 0, so top left
  lcd.print("Photobioreactor");
  delay(1000);
  lcd.clear();

}

/* *******************************************************
/* Loop, this code is continiously repeated
*/
void loop()
{
  /* Process clock sync message */
  if (Serial.available()) {
    processSyncMessage();
  }
  if (timeStatus()!= timeNotSet) {
    digitalClockDisplay();  
  }
  if(hour()>minhour && hour()<maxhour) {
    digitalWrite(6, HIGH);
    LEDtoggle = true;
  }
  else {
    digitalWrite(6, LOW);
    LEDtoggle = false;
  }
  
  /* pH sensor reading */
  for(int i=0;i<10;i++)       //Get 10 sample value from the sensor for smooth the value
  { 
    buf[i]=analogRead(SensorPin);
    delay(10);
  }
  for(int i=0;i<9;i++)        //sort the analog from small to large
  {
    for(int j=i+1;j<10;j++)
    {
      if(buf[i]>buf[j])
      {
        temp=buf[i];
        buf[i]=buf[j];
        buf[j]=temp;
      }
    }
  }
  avgValue=0;
  for(int i=2;i<8;i++)                      //take the average value of 6 center sample
    avgValue+=buf[i];
  float phValue=(float)avgValue*5.0/1024/6; //convert the analog into millivolt
  phValue=3.5*phValue;                      //convert the millivolt into pH value
  Serial.print("    pH:");  
  Serial.print(phValue,2);
  Serial.println(" ");

  sensors_event_t event;

  if(lightSensorToggle) {
    /* Adafruit light sensor reading */
    
    tsl.getEvent(&event);
   
    /* Display the Adafruit light sensor results (light is measured in lux) */
    if (event.light)
    {
      Serial.print(event.light); Serial.println(" lux");
    }
  }
  else {
    /* BH1750 light sensor */
    uint16_t lux = lightMeter.readLightLevel();
    Serial.print("Light: ");
    Serial.print(lux);
    Serial.println(" lux");
  }
  
  /* Temperature sensor reading */
  Serial.println(int(Thermistor(analogRead(1))));
  
  /* LCD display output */
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Temp:");
  lcd.print(int(Thermistor(analogRead(1))));
  lcd.print(" ");
  lcd.print(hour());
  printLCDDigits(minute());
  lcd.print(" ");
  if(LEDtoggle) {
    lcd.print("*");
  } else {
    lcd.print("-");
  }
  lcd.setCursor(0,1);
  lcd.print("pH:");  
  lcd.print(int(phValue));
  lcd.print(" Lux:");
  if(lightSensorToggle) { 
    lcd.print(int(event.light)); // Adafruit
  }
  else {
    uint16_t lux = lightMeter.readLightLevel();
    lcd.print(int(lux)); // BH1750
  }
  
  /* Blink LED as status indicator */
  digitalWrite(13, HIGH);       
  delay(800);
  digitalWrite(13, LOW); 
}






/* Functions for setting the clock */
void digitalClockDisplay(){
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year()); 
  Serial.println(); 
}

void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

void printLCDDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  lcd.print(":");
  if(digits < 10)
    lcd.print('0');
  lcd.print(digits);
}

void processSyncMessage() {
  unsigned long pctime;
  const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013

  if(Serial.find(TIME_HEADER)) {
     pctime = Serial.parseInt();
     if( pctime >= DEFAULT_TIME) { // check the integer is a valid time (greater than Jan 1 2013)
       setTime(pctime); // Sync Arduino clock to the time received on the serial port
     }
  }
}

time_t requestSync()
{
  Serial.write(TIME_REQUEST);  
  return 0; // the time will be sent later in response to serial mesg
}
