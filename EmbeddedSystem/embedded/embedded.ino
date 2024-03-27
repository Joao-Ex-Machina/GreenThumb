
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

/*Firebase keys*/
#define DB_URL "iot-alarm-app-gr15-default-rtdb.europe-west1.firebasedatabase.app"
#define API_KEY "AIzaSyACS5i0U2R7gXSRt2J6UDkOCidJQ7m12Ws"

/*WiFi credentials*/
#define WIFI_SSID "Labs-LSD"
#define WIFI_PASSWORD "aulaslsd"

/*->Define Pins<-*/
    #define LED 16
    
    /*Pump Relays*/
    #define pumpWater 14
    #define pumpRemove 13
    #define pumpAdd 12

    /*Water quality*/
    #define waterTemperature 5
    #define waterTurbidity 4
    #define waterLevel 2
    
    /*Level evaluation*/
    #define NUM_LEVEL 1
    #define HumidurePin0 15
    //#define HumidurePin1 

/*define for number of pumps*/
#define HAS_PUMP_REMOVE 0
#define HAS_PUMP_ADD 0

typedef enum TankCode {NO_WATER, DIRTY_WATER, HOT_WATER};

/*Have to manually init them*/
DHT humidureSensors[NUM_LEVEL]{DHT(HumidurePin0, DHT11)/*,DHT(HumidurePin1, DHT11)*/};

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;   //Replace with your GMT offset (seconds)

//const int   daylightOffset_sec = 3600;
const int   daylightOffset_sec = 0;

char verboseWaterTime[512];
time_t waterTime,timeThreshold;
bool manualWater, enAutoWater, enWaterSaving, enWaterSchedule;
float turbiThreshold, humiThreshold, tempThreshold, waterTempThreshold;
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
    pinMode(waterTemperature,INPUT);

    delay(500);
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
    
    config.api_key = API_KEY;
    
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
    
    for( int i=0; i<NUM_LEVEL; i++)
        humidureSensors[i].begin();

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    readFromRTDB();
}

void loop() {
    
	readFromRTDB();
	generateThresholds();
	 if(manualWater)
        water();
    readFromTank();
    
    //readFromLevels();

    if((getCurrenttime()-waterTime > timeThreshold) && enAutoWater && enWaterSchedule)
        water();
    		
    delay(60000 * 0.5); /*re-check in 0.5 minutes*/ 
}

void water(){
    digitalWrite(pumpWater, HIGH);
    delay(60*1000);
	digitalWrite(pumpWater, LOW);
	Firebase.RTDB.setBool(&fbdo,"waterManual", false); /*overide a Manual call to water*/
    updateTime();
	
    
}


void readFromRTDB(){
  Serial.println("Reading from RTDB");
	Firebase.RTDB.getBool(&fbdo,"waterManual");
    manualWater=fbdo.boolData();
     
    Firebase.RTDB.getBool(&fbdo,"waterAuto");
    enAutoWater=fbdo.boolData();
    
    Firebase.RTDB.getBool(&fbdo,"waterSaving");
    enWaterSaving=fbdo.boolData();

    Firebase.RTDB.getBool(&fbdo,"waterScheduleEnable");
    enWaterSchedule=fbdo.boolData();

    Firebase.RTDB.getFloat(&fbdo,"waterSchedule");
    timeThreshold=(time_t)(fbdo.floatData())*1000*3600;

    Firebase.RTDB.getFloat(&fbdo,"waterLastTimeUNIX");
    waterTime=(time_t)fbdo.floatData();


}

void readFromTank(){
	double Turbidity = analogRead(waterTurbidity);
	double Temperature = analogRead(waterTemperature);
	bool Level = digitalRead(waterLevel);
	if(Level){
		Firebase.RTDB.setFloat(&fbdo,"TankTurbidity", Turbidity);
		Firebase.RTDB.setBool(&fbdo,"TankVolumePing", false);

        verboseTurbidity(Turbidity);
		
		if (Turbidity < turbiThreshold){
			  Firebase.RTDB.setBool(&fbdo,"TankTurbidityPing", true );
        //CorrectTank(DIRTY_WATER);
			  return;
		}
        else{
           Firebase.RTDB.setBool(&fbdo,"TankTurbidityPing", false);
        }

        if (Temperature < tempThreshold){
            Firebase.RTDB.setBool(&fbdo,"TankTemperaturePing", true);
            //CorrectTank(HOT_WATER);
            return;
        }
        else{
           Firebase.RTDB.setBool(&fbdo,"TankTemperaturePing", false);
        }
			
	}
	else {
        Firebase.RTDB.setBool(&fbdo,"TankVolumePing", false);
		if(HAS_PUMP_ADD && HAS_PUMP_REMOVE)
            CorrectTank(NO_WATER);
		return;
	}
}

void CorrectTank(TankCode code){
    if(code==NO_WATER){
        digitalWrite(pumpAdd, HIGH);
        delay(60*1000);
        digitalWrite(pumpAdd, LOW);
        return;
    }
    
    if(code==DIRTY_WATER){
        if(HAS_PUMP_REMOVE)
          digitalWrite(pumpRemove, HIGH);
        digitalWrite(pumpAdd, HIGH);
        while(analogRead(waterTurbidity)<turbiThreshold)
            delay(10*1000);
        digitalWrite(pumpRemove, LOW);
        digitalWrite(pumpAdd, LOW);
        return;
    }
    
    if(code==HOT_WATER){
        if(HAS_PUMP_REMOVE)
            digitalWrite(pumpRemove, HIGH);
        digitalWrite(pumpAdd, HIGH);
        while(analogRead(waterTemperature)>tempThreshold)
        delay(10*1000);
        if(HAS_PUMP_REMOVE)
            digitalWrite(pumpRemove, LOW);
        digitalWrite(pumpAdd, LOW);
        return;
    }
}

void readFromLevels(){
    float humi[NUM_LEVEL];
    float temp[NUM_LEVEL];
    for (int i=0; i<NUM_LEVEL; i++){
        humi[i]=humidureSensors[i].readHumidity();
        temp[i]=humidureSensors[i].readTemperature();
    }
    
    /*Change for number of levels */
    Firebase.RTDB.setFloat(&fbdo,"Level0Humidity", humi[0]);
    Firebase.RTDB.setFloat(&fbdo,"Level0Temp", temp[0]);

    for (int i=0; i<NUM_LEVEL; i++){
        if(humi[i] < humiThreshold || temp[i] > tempThreshold){
            if(enAutoWater)
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
	if(Turbidity < 4.0 && Turbidity >= 3.5 )
		vTurbi="Dirty";
	if(Turbidity < 3.5)
		vTurbi="Very Dirty";

    Firebase.RTDB.setString(&fbdo,"TankTurbidityVerbose", vTurbi);

}

void generateThresholds(){
	
	if(enWaterSaving){
    tempThreshold=0.0;
    waterTempThreshold=0.0;
		humiThreshold=0.0;
		turbiThreshold=3.5;
    }

	else{
		tempThreshold=0.0;
    waterTempThreshold=0.0;
		humiThreshold=0.0;
		turbiThreshold= 4.0;
    }
	
}

void updateTime(){
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
        Serial.println("Failed to obtain time");
        return;
    }

    waterTime=mktime(&timeinfo);
    strftime(verboseWaterTime, sizeof(verboseWaterTime), "%d %b %H:%M", &timeinfo);
    Serial.println("Water time at:");
    Serial.println(verboseWaterTime);
    Serial.println("Which is UNIX time:");
    Serial.println(waterTime);
    Firebase.RTDB.setString(&fbdo,"waterLastTime", verboseWaterTime);
    Firebase.RTDB.setFloat(&fbdo, "waterLastTimeUNIX", waterTime);   

    delay(1000);
}

time_t getCurrenttime(){
    struct tm timeinfo;
    time_t currTime;    
    if(!getLocalTime(&timeinfo)){
        Serial.println("Failed to obtain time");
        return 0;
    }
    
    currTime=mktime(&timeinfo);
    return currTime;
}
