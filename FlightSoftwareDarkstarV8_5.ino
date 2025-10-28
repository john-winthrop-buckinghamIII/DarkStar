
//-----------------------
//Importing libraries
//-----------------------
#include <Wire.h>                   // I2C communication library
#include <SD.h>                     // SD card library
#include <Servo.h>                  // Servo control library
#include <FastLED.h>                // LED control library
#include <Adafruit_BMP3XX.h>        // BMP388 barometric pressure sensor library
#include <Adafruit_GPS.h>           // PA1010D GPS library
#include <LSM6.h>                     // Accelerometer and gyroscope
#include <LIS3MDL.h>                // Magnetometer library
#include <LPS.h>                   //alt and temp

//-----------------------
//Pin Assignments
//-----------------------
const int SDA_PIN=4;      // I2C data pin
const int SCL_PIN=5;      // I2C clock pin
const int SD_PIN=17;       // SD card chip select pin (Some pin between 18-22, cant tell :/)
const int CAMERA=12;       // Camera control pin
const int AEROBRAKES=21;    // Aerobrake servo control pin
const int BUZZER=0;      // Buzzer pin *What is this used for???*
const int ADC_PIN=26;         // Battery voltage analog input pin
const int LED=16;         // LED pin


//-----------------------
//Library Renaming
//-----------------------
LPS ps;
LSM6 imu;
LIS3MDL mag;
Adafruit_GPS gps(&Wire);
#define GPSECHO false
Adafruit_BMP3XX bmp;
File telemetryFile;
Servo aerobrakes;

//-----------------------
//Variable Definition
//-----------------------
//Telemetry variables
float alt, temp, vel, ax, ay, az, heading, lat, lon, voltage, roll, pitch, yaw, missionTime;
enum FlightState { PRELAUNCH, LAUNCH, BOOST, COAST, APOGEE, DESCENT, LANDED };
FlightState State = PRELAUNCH;
#define SEND_INTERVAL 500
#define SEALEVELPRESSURE_HPA (1013.25)

//Sensor reading variables
float pressure;
int ADC;
float vOut;
float r1=8200;
float r2=10000;



//Aerobrake Storing and Deployment variables and Boolean
const int aeroDeploy=1400;
const int aeroStow=400;
const int deployAlt=1600;
bool aerobrakesDeployed=false;

//LED Definitions
#define NUM_LEDS 2                     // Number of LEDs in the array
CRGB leds[NUM_LEDS];                  // LED array for FastLED
#define FLIGHT_LED 0                   // Index for flight status LED
#define ERROR_LED 1                    // Index for error LED
unsigned long flightLedLastToggle = 0;// Timestamp of last flight LED toggle
bool flightLedState = false;          // Current state of flight LED (on/off)
unsigned long errorLedLastToggle = 0; // Timestamp of last error LED toggle
bool errorLedState = false;   

//Setup Variables
unsigned long lastTime;
float initPressure;
float initAlt=0.0;
bool telemetryStopped=false;

//Loop Variables
unsigned long now;
unsigned long dt;
float lastAlt=0;
unsigned long lastSend=0;
float lastVel=0;
unsigned long launchTime;

//-----------------------
// Function Definition
//-----------------------
void senddTelemetry();
void writTelemetry();
void readSensors();
float headingCalculation();
void parseCommands();
void blinkLED();
void updateFlightLED();
void updateErrorLED();
const char* fsString();
String conv2Str(float v, int prec = 2) {
    if(isnan(v)) return "NaN";        // Return "NaN" if value is not a number
    return String(v, prec);           // Otherwise return float as string
}

//-----------------------
//Setup
//-----------------------
void setup() {
  // Opening the Serials and bedinning the I2C
  Serial.begin(9600);                 // Debug
  Serial1.begin(115200);                  // LTE serial
  Wire.begin();                          // Start I2C

  //Initializing the SD card
  if (SD.begin(SD_PIN)){
    telemetryFile = SD.open("telemetry.csv", FAPPEND); //If FAPPEND doesn't work, try FILE_READ

      if(!telemetryFile) 
        telemetryFile = SD.open("telemetry.csv", FILE_WRITE); 
      if(telemetryFile){ 
        telemetryFile.println("TeamID,State,Altitude,Temperature,Velocity,AccelX,AccelY,AccelZ,Compass,GPSLat,GPSLon,Battery,Roll,Pitch,Yaw,MissionTime"); 
        }
  }

  //Servo Initialization
  aerobrakes.attach(AEROBRAKES,aeroStow,aeroDeploy);
  aerobrakes.write(aeroStow); //Making sure the aerobrake is stowed at the beginning

  //Turning on the camera
  pinMode(CAMERA, OUTPUT);
  digitalWrite(CAMERA,HIGH);

  //Sensor Initialization

  //BMP Initialization
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
  bmp.setOutputDataRate(BMP3_ODR_50_HZ);
  bmp.performReading();


  //PA-1010D Initialization
  gps.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  gps.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);

  //Alt IMU Initialization
  imu.enableDefault();
  imu.writeReg(LSM6::CTRL3_C, 0x44);
  imu.writeReg(LSM6::CTRL1_XL, 0x64);
  mag.enableDefault();
  ps.enableDefault();

  //Setting initial altitude and initial last time
  lastTime=0;
}

//-----------------------
//Loop
//-----------------------
void loop() {
  //Setting up the time variables
  if (State==PRELAUNCH){
    missionTime=0;
  }else{
    missionTime=(millis()-launchTime)/1000.0f;    
  }
  now=millis();

  //Checking for ground station commands
  parseCommands();
  //Updating Sensor values
  readSensors();

  //Velocity calculations
  dt=(now-lastTime)/1000.0f;
  vel=(alt-lastAlt)/dt;

  //State Determination
  switch (State){
    case PRELAUNCH:
      if (ay>=5.0f){
        State=LAUNCH;
        Serial.println("STATE->LAUNCH, camera ON");
      }
      break;
    case LAUNCH:
      launchTime=now;
      sendTelemetry();
      State = BOOST;
      Serial.println("STATE->BOOST");
      break;
    case BOOST:
      if (ay<=0){
        State = COAST;
        Serial.println("STATE->COAST");
      }
      break;
    case COAST:
      if (alt>=deployAlt){
        aerobrakes.writeMicroseconds(aeroDeploy);
        aerobrakesDeployed = true;
      }
      if (vel<=0.0f && ay<=0.0f){
        //sendTelemetry();
        State = APOGEE;
        Serial.println("STATE->APOGEE");
      }
      break;
    case APOGEE:
      aerobrakes.writeMicroseconds(aeroStow);
      aerobrakesDeployed = false;
      State = DESCENT;
      break;
    case DESCENT: //Test after FRR
      if (abs(vel) < 0.5 && alt<150.0f){ 
          State = LANDED;
        }
      break;
    case LANDED:
      break;
  }

  //Write Telemetry to SD
  writeTelemetry();

  //Send telemetry packet to ground
  if (now-lastSend>=SEND_INTERVAL){
    lastSend=now;
    sendTelemetry();
    Serial.println(pressure);
  }

  // Update LEDs
  updateFlightLED(State);                 // Update flight LED
  updateErrorLED();                              // Update error LED

  //Update Variables
  lastVel = vel;
  lastAlt = alt; 
  lastTime = now;

  //Define initial altitude
  if (initAlt==0.0){
    initAlt=bmp.readAltitude(SEALEVELPRESSURE_HPA);
  }
}

//-----------------------
//Function Creation
//-----------------------
//Telemetry communication
void sendTelemetry(){
  if (telemetryStopped){
    return;
  }
  //Sending to ground
  Serial1.print(conv2Str(1001, 0)+
    "," + fsString(State) + "," + conv2Str(alt)+
    "," + conv2Str(temp) + "," + conv2Str(vel)+
    "," + conv2Str(ax) + "," + conv2Str(ay)+
    "," + conv2Str(az) + "," + conv2Str(heading)+
    "," + conv2Str(lat, 5) + ","+conv2Str(lon, 5)+
    "," + conv2Str(voltage) + ","+conv2Str(roll)+
    "," + conv2Str(pitch) + "," + conv2Str(yaw)+
    "," + conv2Str(missionTime) + ",," + conv2Str(aerobrakesDeployed ? 1:0)+
    "," + conv2Str(launchTime/1000.0f));

  //Writing to the Debug Screen
  Serial.print(1001); Serial.print(",");
  Serial.print(fsString(State)); Serial.print(",");
  Serial.print(alt, 1); Serial.print(",");
  Serial.print(temp); Serial.print(",");
  Serial.print(vel, 2); Serial.print(",");
  Serial.print(ax, 2); Serial.print(",");
  Serial.print(ay, 2); Serial.print(",");
  Serial.print(az, 2); Serial.print(",");
  Serial.print(heading, 2); Serial.print(",");
  Serial.print(lat, 5); Serial.print(",");
  Serial.print(lon, 5); Serial.print(",");
  Serial.print(voltage, 2); Serial.print(",");
  Serial.print(roll, 2); Serial.print(",");
  Serial.print(pitch, 2); Serial.print(",");
  Serial.print(yaw, 2); Serial.print(",");
  Serial.print(missionTime); Serial.print(",,");
  Serial.print(aerobrakesDeployed); Serial.println(",");

}

//Recording Telemetry on SD card
void writeTelemetry(){
  telemetryFile.println(1001); telemetryFile.println(",");
  telemetryFile.println(fsString(State)); telemetryFile.println(",");
  telemetryFile.println(alt, 1); telemetryFile.println(",");
  telemetryFile.println(temp); telemetryFile.println(",");
  telemetryFile.println(vel, 2); telemetryFile.println(",");
  telemetryFile.println(ax, 2); telemetryFile.println(",");
  telemetryFile.println(ay, 2); telemetryFile.println(",");
  telemetryFile.println(az, 2); telemetryFile.println(",");
  telemetryFile.println(heading, 2); telemetryFile.println(",");
  telemetryFile.println(lat, 5); telemetryFile.println(",");
  telemetryFile.println(lon, 5); telemetryFile.println(",");
  telemetryFile.println(voltage); telemetryFile.println(",");
  telemetryFile.println(roll, 2); telemetryFile.println(",");
  telemetryFile.println(pitch, 2); telemetryFile.println(",");
  telemetryFile.println(yaw, 2); telemetryFile.println(",");
  telemetryFile.println(missionTime); telemetryFile.println(",,");
  telemetryFile.println(aerobrakesDeployed); telemetryFile.println(",");
}
//Reading all of the sensors
void readSensors(){
  //Sensor Measuring
  imu.read();
  mag.read();
  gps.read();
  gps.newNMEAreceived();
  bmp.performReading();


  //Read Altitude
  alt=bmp.readAltitude(SEALEVELPRESSURE_HPA)-initAlt;

  //Read Temperature
  temp=bmp.temperature;

  //Read Acceleration
  ax= imu.a.x * 0.000488f; ay= imu.a.y * 0.000488f; az= imu.a.z * 0.000488f;

  //Read Compass
  heading=headingCalculation(mag.m.x,mag.m.y,mag.m.z,pitch,roll);

  //Read GPS
  lat=gps.latitude;
  lon=gps.longitude;

  //Battery Voltage Calculation
  ADC=analogRead(ADC_PIN);
  vOut=ADC*3.3/1023.0f;
  voltage=vOut*(r1+r2)/r2;
  
  //Read Roll, Pitch, and Yaw
  roll= imu.g.x; pitch= imu.g.y; yaw=imu.g.z;
}

//Adjusting the magnometer to compute heading
float headingCalculation(float mx, float my, float mz, float pitch, float roll) {
  float pr = pitch * PI / 180.0;
  float rr = roll  * PI / 180.0;

  float xh = mx * cos(pr) + mz * sin(pr);
  float yh = mx * sin(rr) * sin(pr) + my * cos(rr) - mz * sin(rr) * cos(pr);

  float heading = atan2(-yh, xh) * 180.0 / PI;
  if (heading < 0) heading += 360.0;
  return heading;
}

void parseCommands(){
  
  //Radio
  while(Serial1.available()){               // Check for incoming data
        String cmd = Serial1.readStringUntil('\n'); 
        cmd.trim(); // Read command
        if(cmd.length() == 0) 
            break;        // Skip empty commands
        Serial.print("CMD RX: "); 
        Serial.println(cmd);
        if(cmd=="DEPLOY_AEROBRAKES" && ay<=0){
            if(!aerobrakesDeployed){ 
                aerobrakes.writeMicroseconds(aeroDeploy); 
                aerobrakesDeployed = true;  
            } 
            else if (aerobrakesDeployed){
                aerobrakes.writeMicroseconds(aeroStow);
                aerobrakesDeployed = false;
            }
        }

        else if(cmd=="STOP_TELEMETRY"){ 

            if (!telemetryStopped){

                telemetryFile.close(); 
                telemetryStopped = true; 
                digitalWrite(CAMERA,LOW);
            }
        }
    }

  //Software
  while(Serial.available()){               // Check for incoming data
        String cmd = Serial.readStringUntil('\n'); 
        cmd.trim(); // Read command
        if(cmd.length() == 0) 
            break;        // Skip empty commands
        Serial.print("CMD RX: "); 
        Serial.println(cmd);
        if(cmd=="DEPLOY_AEROBRAKES" && ay<=0){
            if(!aerobrakesDeployed){ 
                aerobrakes.writeMicroseconds(aeroDeploy); 
                aerobrakesDeployed = true;  
            } 
            else if (aerobrakesDeployed){
                aerobrakes.writeMicroseconds(aeroStow);
                aerobrakesDeployed = false;
            }
        }

        else if(cmd=="STOP_TELEMETRY"){ 

            if (!telemetryStopped){

                telemetryFile.close(); 
                telemetryStopped = true; 
                digitalWrite(CAMERA,LOW);
            }
        }

        else if(cmd=="LAUNCH_ROCKET"){
          State=LAUNCH;
        }

    }
}

void blinkLED(int index, CRGB color, bool &state, unsigned long &lastToggle, int interval){
    unsigned long now = millis();                 // Get current time
    if(now - lastToggle >= interval){             // Check if enough time passed 
        leds[index] = state ? color : CRGB::Black; // Set LED ON/OFF
        FastLED.show();                           // Update LED strip
        lastToggle = now;                         // Update last toggle time
    }
}

void updateFlightLED(FlightState state){
    switch(state){
        case PRELAUNCH:
            blinkLED(FLIGHT_LED, CRGB::Blue, flightLedState, flightLedLastToggle, 500); 
            break;

        case LAUNCH:
            leds[FLIGHT_LED] = CRGB::Green; 
            FastLED.show(); 
            break;

        case BOOST:         
            blinkLED(FLIGHT_LED, CRGB::Green, flightLedState, flightLedLastToggle, 300); 
            break;

        case COAST:                 
            leds[FLIGHT_LED] = CRGB::Blue; 
            FastLED.show(); 
            break;

        case APOGEE:               
            leds[FLIGHT_LED] = CRGB::Yellow; 
            FastLED.show(); 
            break;

        case DESCENT:             
            blinkLED(FLIGHT_LED, CRGB::Yellow, flightLedState, flightLedLastToggle, 300); 
            break;

        case LANDED:              
            leds[FLIGHT_LED] = CRGB::Red; 
            FastLED.show(); 
            break;

    }
}

void updateErrorLED(){
  if (!SD.begin(SD_PIN) || !bmp.begin_I2C() || !imu.init() || !mag.init() || !ps.init() || !gps.begin(0x10)){
    blinkLED(FLIGHT_LED, CRGB::Red, errorLedState, errorLedLastToggle, 500);
    Serial.println("Sensor Error")
  }
  else{
    leds[ERROR_LED] = CRGB::Black; 
    FastLED.show(); 
    return; // No errors -> LED off
  }

}

const char* fsString(FlightState s) {
    switch(s){
        case PRELAUNCH: return "PRELAUNCH";
        case LAUNCH:    return "LAUNCH";
        case BOOST:     return "BOOST";
        case COAST:     return "COAST";
        case APOGEE:    return "APOGEE";
        case DESCENT:   return "DESCENT";
        case LANDED:    return "LANDED";
    }
}




