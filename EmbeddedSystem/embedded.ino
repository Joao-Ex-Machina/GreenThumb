
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

#define 0Humidure
#define 1Humidure

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 18000;   //Replace with your GMT offset (seconds)
const int   daylightOffset_sec = 0;
char verboseWaterTime[128];
time_t waterTime;
// Define the Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


void setup() {
    pinMode(LED, OUTPUT);
    pinMode(pumpWater, OUTPUT);
    pinMode(pumpRemove, OUTPUT);
    pinMode(pumpAdd, OUTPUT);
    pinMode(0Humidure, INPUT);
    pinMode(1Humidure, INPUT);
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
}

void loop() {
    
	readFromRTDB();
	
	 if(manualWater)
        water();
    		

            
    delay(1000);
}

void water(){
    digitalWrite(pumpWater, HIGH);
    delay();
	digitalWrite(pumpWater, LOW);
	Firebase.RTDB.setBool(&fbdo,"WaterManual", false);
	waterTime=0;
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
	bool Level = digitalRead(waterLevel)
	if(waterLevel){
		Firebase.RTDB.setFloat(&fbdo,"TankTurbidity", Turbidity);
		verboseTurbidity(Turbidity);
		
		if (vTurbi < turbiThreshold){
			CorrectTank(DIRTY_WATER);
			return;
		}
			
	}
	else {
		CorrectTank(NO_WATER);
		return;
	}
}

void CorrectTank(int code){

}

void readFromLevels(){}

void verboseTurbidity(double Turbidity){
	String vTurbi;
	if()
		vTurbi="Clear";
	if()
		vTurbi="Slightly Murky";
	if()
		vTurbi="Dirty";
	if()
		vTurbi="Very Dirty";

Firebase.RTDB.setString(&fbdo,"TankTurbidityVerbose", vTurbi);

}

void generateThresholds(){
	
	if(WaterSaving){
		tempThreshold=;
		humThreshold=;
		turbiThreshold=;
    }

	else{
		tempThreshold=;
		humThreshold=;
		turbiThreshold=;
    }
	
}

void updateTime()
{
  time_t rawtime;
  struct tm * timeinfo;
  time (&rawtime);
  timeinfo = localtime (&rawtime);
  strftime(verboseWaterTime, sizeof(verboseWaterTime), "%d %b %Y %H:%M", tm); 
  delay(1000);
}
