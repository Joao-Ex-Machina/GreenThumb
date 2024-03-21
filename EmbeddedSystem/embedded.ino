
/*+----------------+----------------------------------------------+
  | embedded.ino   | Connects ESP8266 to Firebase RT Database     |
  |                | Actuates over the Hydroponic System          |
  |                |______________________________________________|
  | Authored by:    João Barreiros C. Rodrigues                   |
  |                 Guilherme Eric Granqvist C. de Freitas        |
  +---------------------------------------------------------------+*/

#include <ESP8266WiFi.h>
#include <FirebaseFS.h>
#include <FB_Utils.h>
#include <Firebase_ESP_Client.h>
#include <FB_Network.h>
#include <Firebase.h>
#include <FB_Error.h>
#include <FB_Const.h>
#include <time.h>
#include "DHT.h"

#define DB_URL "iot-alarm-app-gr15-default-rtdb.europe-west1.firebasedatabase.app"
#define API_KEY "AIzaSyACS5i0U2R7gXSRt2J6UDkOCidJQ7m12Ws"
#define WIFI_SSID "Labs-LSD"
#define WIFI_PASSWORD "aulaslsd"

#define LED 16

#define pumpWater 12 
#define pumpRemove 13
#define pumpAdd 14

#define waterTemperature 5
#define waterTurbidity 4
#define waterLevel 0

#define NUM_LEVEL 2
#define 0HumidurePin
#define 1HumidurePin

typedef enum TankCode {NO_WATER, DIRTY_WATER, HOT_WATER};

DHT humidureSensors[NUM_LEVEL]{DHT(0HumidurePin, DHT11),DHT(1HumidurePin, DHT11)};

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 18000;   //Replace with your GMT offset (seconds)
const int   daylightOffset_sec = 0;
char verboseWaterTime[128];
time_t waterTime;
time_t refTime;
// Define the Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


void setup() {
    pinMode(LED, OUTPUT);
    pinMode(pumpWater, OUTPUT);
    pinMode(pumpRemove, OUTPUT);
    pinMode(pumpAdd, OUTPUT);
    pinMode(waterLevel, INPUT);
    pinMode(waterTurbidity, INPUT);
    pinMode(WaterTemperature,INPUT);

    delay(500);
    tone(buzzer,440,500);
    Serial.begin(9600);
    Serial.println("               ");
    digitalWrite(LED, HIGH);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to WiFi");
    while(WiFi.status() != WL_CONNECTED){
      Serial.print(".");
      delay(1000);  
    }
    Serial.println("");
    Serial.println("Connected to WiFi!");
    tone(buzzer,240,500);
    
    config.api_key =API_KEY;
    
    config.database_url = DB_URL;
    
    if (Firebase.signUp(&config, &auth, "", "")){
      Serial.println("Successfully signed-up");
    }
    else
      Serial.println("FATAL! Error at sign-up, please reboot!");
    
    
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    Serial.print("Connecting to Firebase");
    
    while(!(Firebase.ready())){
      Serial.print(".");
      delay(1000);  
    }

    Serial.println("");
    Serial.println("Connected to Firebase!");
    tone(buzzer,440,500);
    
    //waterTime=readTimefromRTDB();
    readFromRTDB();
    for(i=0; i<NUM_LEVEL; i++)
        humidureSensors[i].begin();
}

void loop() {
    
	readFromRTDB();
	generateThresholds();
	 if(manualWater)
        water();
    readFromTank();
    readFromLevels();
    if((refTime-waterTime>timeThreshold) && enAutoWater)
        water();
    		
    delay(30000);
}

void water(){
    digitalWrite(pumpWater, HIGH);
    delay();
	digitalWrite(pumpWater, LOW);
	Firebase.RTDB.setBool(&fbdo,"WaterManual", false);
    updateTime();
	
    
}


void readFromRTDB(){
	Firebase.RTDB.getBool(&fbdo,"WaterManual");
    manualWater=fbdo.boolData();
     
    Firebase.RTDB.getBool(&fbdo,"WaterAuto");
    enAutoWater=fbdo.boolData();
    
    Firebase.RTDB.getBool(&fbdo,"WaterSaving");
    enWaterSaving=fbdo.boolData();


}

void readFromTank(){
	double Turbidity = analogRead(waterTurbidity);
	double Temperature = analogRead(waterTemperature);
	bool Level = digitalRead(waterLevel);
	if(Level){
		Firebase.RTDB.setFloat(&fbdo,"TankTurbidity", Turbidity);
		
        verboseTurbidity(Turbidity);
		
		if (Turbidity < turbiThreshold){
			CorrectTank(DIRTY_WATER);
			return;
		}

        if (Temperature < tempThreshold){
            CorrectTank(HOT_WATER);
            return;
        }
			
	}
	else {
		CorrectTank(NO_WATER);
		return;
	}
}

void CorrectTank(TankCode code){
    if(code==NO_WATER){
        digitalWrite(pumpAdd, HIGH);
        delay(60000);
        digitalWrite(pumpAdd, LOW);
            
        return;
    }
    if(code==DIRTY_WATER){
        digitalWrite(pumpRemove, HIGH);
        digitalWrite(pumpAdd, HIGH);
        while(analogRead(waterTurbidity)<turbiThreshold)
                delay(10000);
        digitalWrite(pumpRemove, LOW);
        digitalWrite(pumpAdd, LOW);
    }
}

void readFromLevels(){
    float humi[NUM_LEVEL];
    float temp[NUM_LEVEL];
    for (int i=0; i<NUM_LEVEL; i++){
        humi[i]=humidureSensors[i].readHumidity();
        temp[i]=humidureSensors[i].readTemperature();
    }
    
    /*Change for number of levels*/
    Firebase.RTDB.setFloat(&fbdo,"Level0Humidity", humi[0]);
    Firebase.RTDB.setFloat(&fbdo,"Level0Temp", temp[0]);

    for (int i=0; i<NUM_LEVEL; i++){
        if(humi[i] < humiThreshold[i] || temp[i] > tempThreshold[i]){
            water();
            return;
        }
    }

    
}

void verboseTurbidity(double Turbidity){
	String vTurbi;
	if(Turbidity >= 4.10)
		vTurbi="Clear";
	if(Turbidity < 4.10 && Turbidity >= 4.0)
		vTurbi="Slightly Dirty";
	if(Turbidity < 4.0 && Turbity >= 3.5 )
		vTurbi="Dirty";
	if(Turbidity < 3.5)
		vTurbi="Very Dirty";

Firebase.RTDB.setString(&fbdo,"TankTurbidityVerbose", vTurbi);

}

void generateThresholds(){
	
	if(WaterSaving){
		tempThreshold=0.0;
        waterTempThreshold=0.0;
		humThreshold=0.0;
		turbiThreshold=3.5;
    }

	else{
		tempThreshold=0.0;
        waterTempThreshold=0.0;
		humThreshold=0.0;
		turbiThreshold= 4.0;
    }
	
}

void updateTime()
{
    time_t rawtime;
    struct tm * timeinfo;
    time (&rawtime);
    timeinfo = localtime (&rawtime);
    waterTime=time(&refTime);
    strftime(verboseWaterTime, sizeof(verboseWaterTime), "%d %b %Y %H:%M", tm); 
    delay(1000);
}
