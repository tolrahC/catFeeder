/*

All variables that may need to be changed are here


*/


// Wifi
const char* ssid = "YourSSID";
const char* password = "WIFIPassword";

// OTA
#define SENSORNAME "CatFeeder" //change this to whatever you want to call your device
#define OTApassword "OTAPassword" //the password you will need to enter to upload remotely via the ArduinoIDE yourOTApassword
int OTAport = 8266;

// MQTT
const char* mqtt_server = "mqtt.acme.com"; // IP address or dns of the mqtt
const char* mqtt_username = "mqtt";
const char* mqtt_password = "MQTTPassword";
const int mqtt_port = 1883; //usually no need to change

// MQTT TOPICS (change these topics as you wish) 
const char* lastfed_topic = "home/catfeeder/lastfed"; // UTF date
const char* remaining_topic = "home/catfeeder/remaining"; //Remain % fix distance above
const char* feed_topic = "home/catfeeder/feed";  // command topic
const char* willTopic = "home/catfeeder/LWT";  // Last Will and Testiment topic

// stepper
const int steps = 200; //this is the number of steps of the motor for a 360� rotation.
const int baseSpeed = 55;
Stepper myStepper(steps, D1, D2, D3, D4); // you may want to change this based on how you cabled the motor.
int enA = D5;
int enB = D6;

// Ultrasonic
int trigger = D8;
int echo = D7;
float max_food = 22;  // in cm
#define MAX_DISTANCE 200 //Max distance for the sensor to retrieve, in cm.

//Set the timezone
TimeChangeRule EDT = { "EDT", Second, Sun, Mar, 2, -240 };  // Eastern Daylight Time = UTC - 4 hours
TimeChangeRule EST = { "EST", First, Sun, Nov, 2, -300 };   // Eastern Standard Time = UTC - 5 hours
Timezone ET(EDT, EST);
