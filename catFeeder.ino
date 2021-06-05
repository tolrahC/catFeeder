#include <TimeLib.h>
#include <Time.h>
#include <Timezone.h>
#include <NTPClient.h>
#include <NewPingESP8266.h>

// Look for all "REPLACEME" before uploading the code.
#include <Stepper.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <ArduinoJson.h>

// wifi
const char* ssid = "YourSSID"; //type your WIFI information inside the quotes
const char* password = "WIFIPassword";
WiFiClient espClient;

// wifi UDP for NTP, we dont have real time and we dont trust http headers :)
WiFiUDP ntpUDP;
//#define NTP_OFFSET   60 * 60      // In seconds
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS  "pool.ntp.org"
//We get the UTC Time
NTPClient timeClient(ntpUDP, NTP_ADDRESS, 0, NTP_INTERVAL);

// OTA
#define SENSORNAME "CatFeeder" //change this to whatever you want to call your device
#define OTApassword "OTAPassword" //the password you will need to enter to upload remotely via the ArduinoIDE yourOTApassword
int OTAport = 8266;

// MQTT
const char* mqtt_server = "10.90.0.12"; // IP address or dns of the mqtt
const char* mqtt_username = "mqtt"; //
const char* mqtt_password = "MQTTPassword";
const int mqtt_port = 1883; //REPLACEME, usually not?
PubSubClient client(espClient);
// MQTT TOPICS (change these topics as you wish) 
const char* lastfed_topic = "home/catfeeder/lastfed"; // UTF date
const char* remaining_topic = "home/catfeeder/remaining"; //Remain % fix distance above
const char* feed_topic = "home/catfeeder/feed";  // command topic
const char* willTopic = "home/catfeeder/LWT";  // Last Will and Testiment topic

// stepper
const int steps = 200; //REPLACEME this is the number of steps of the motor for a 360° rotation.
const int baseSpeed = 55; //REPLACEME 
Stepper myStepper(steps, D1, D2, D3, D4); // you may want to REPLACEME this based on how you cabled the motor.
int enA = D5;
int enB = D6;
//int motorPower = 990; // legacy.. for using pwm

// ultrasonic
int trigger = D8;
int echo = D7;
float distance;
int percentageFood;
#define MAX_DISTANCE 200 //Max distance for the sensor to retrieve, in cm.
float max_food = 22;  // REPLACEME in cm? seems to be "about" right
NewPingESP8266 sonar(trigger, echo, MAX_DISTANCE);

// Button
const int buttonPin = 3;     // number of the pushbutton pin (RX, cause no other IO was available)

//Set the timezone
TimeChangeRule EDT = { "EDT", Second, Sun, Mar, 2, -240 };  // Eastern Daylight Time = UTC - 4 hours
TimeChangeRule EST = { "EST", First, Sun, Nov, 2, -300 };   // Eastern Standard Time = UTC - 5 hours
Timezone ET(EDT, EST);

/**
 * Input time in epoch format and return tm time format
 * by Renzo Mischianti <www.mischianti.org>
 */
static tm getDateTimeByParams(long time) 
{
    struct tm* newtime;
    const time_t tim = time;
    newtime = localtime(&tim);
    return *newtime;
}
/**
 * Input tm time format and return String with format pattern
 * by Renzo Mischianti <www.mischianti.org>
 */
static String getDateTimeStringByParams(tm* newtime, char* pattern = (char*)"%Y-%m-%d %H:%M:%S") 
{
    char buffer[30];
    strftime(buffer, 30, pattern, newtime);
    return buffer;
}

/**
 * Input time in epoch format format and return String with format pattern
 * by Renzo Mischianti <www.mischianti.org>
 */
static String getEpochStringByParams(long time, char* pattern = (char*)"%Y-%m-%d %H:%M:%S") 
{
    //    struct tm *newtime;
    tm newtime;
    newtime = getDateTimeByParams(time);
    return getDateTimeStringByParams(&newtime, pattern);
}

void setup()
{
    // Serial setup
    Serial.begin(115200);

    // pins setup
    pinMode(enA, OUTPUT);
    pinMode(enB, OUTPUT);
    pinMode(trigger, OUTPUT);
    pinMode(echo, INPUT);
    pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LEDs  pin as an output
    pinMode(2, OUTPUT); // ^ other led

    pinMode(buttonPin, INPUT_PULLUP);  // initialize the pushbutton pin as an input:

    // Wifi connection setup
    setup_wifi();
    timeClient.begin();
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);

    // Turn OFF builtin leds
    digitalWrite(BUILTIN_LED, HIGH);
    digitalWrite(2, HIGH);

    // stepper speed
    myStepper.setSpeed(baseSpeed);

    // OTA setup
    ArduinoOTA.setHostname(SENSORNAME);
    ArduinoOTA.setPort(OTAport);
    ArduinoOTA.setPassword((const char*)OTApassword);
    ArduinoOTA.begin();
}

void setup_wifi()
{

    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.setSleepMode(WIFI_NONE_SLEEP); // bit more power hungry, but seems stable.
    WiFi.hostname("CatFeeder"); // This will (probably) happear on your router somewhere.
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}
/********************************** START CALLBACK*****************************************/
void callback(char* topic, byte* payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");

    char message[length];
    for (int i = 0; i < length; i++) {
        message[i] = ((char)payload[i]);

    }
    message[length] = '\0';
    Serial.print("[");
    Serial.print(message);
    Serial.print("]");
    Serial.println();

    StaticJsonDocument<16> payloadJson;
    DeserializationError error = deserializeJson(payloadJson, message);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str()); //Don't know why, should use f_str, but doe,s not compile....
        return;
    }

    if (payloadJson.containsKey("steps")) {
        if (payloadJson.containsKey("speed")) {
            myStepper.setSpeed(payloadJson["speed"]);
        }
        Serial.print("Feeding cats...");
        Serial.println();
        feedCats(payloadJson["steps"]);
        myStepper.setSpeed(baseSpeed);
    }
    else {
        Serial.print("Payload not in the expected format");
        Serial.println();
    }
}

//Enable/Disable the motor
void setMotor(uint8_t mode)
{
    Serial.print("Motor mode: ");
    Serial.print(mode);
    Serial.println();
    digitalWrite(enA, mode);
    digitalWrite(enB, mode);
}

// feeds cats
void feedCats(int steps)
{
    digitalWrite(2, LOW); // Turn on onboard LED
    // Enable motors, i dont see the point in pwm with a stepper?
    setMotor(HIGH);

    myStepper.step(steps);

    //Disable the motor
    setMotor(LOW);
    delay(2000); // you may wanna change this based on how many times you press te button continously 
    digitalWrite(2, HIGH); // Turn off onboard LED

    publishInformationValues();
}

//Called when board button pushed
void buttonFeedCats()
{
    digitalWrite(2, LOW); // Turn on onboard LED

    //Until the button is released
    while (digitalRead(buttonPin) == LOW) {
        setMotor(HIGH);
        myStepper.step(50);
        setMotor(LOW);
        delay(100); // To prevent the driver to heat
    }

    digitalWrite(2, HIGH); // Turn off onboard LED
 
    publishInformationValues();
}

//Final step after food distribution
void publishInformationValues()
{
    timeClient.update();   // could this fail?
    String formattedTime = getEpochStringByParams(ET.toLocal(timeClient.getEpochTime()));
    char charBuf[20];
    formattedTime.toCharArray(charBuf, 20);
    client.publish(lastfed_topic, charBuf, true); // Publishing time of feeding to MQTT Sensor retain true
    Serial.print("Fed at: ");
    Serial.print(charBuf);
    Serial.println();
    calcRemainingFood();
}

// calc remaining food in %
void calcRemainingFood() 
{  
    distance = sonar.ping_cm();
    Serial.println(distance);
    //Serial.println(t);

    percentageFood = roundToMultiple((int)(100 - ((100 / max_food) * distance)), 5);
    if (percentageFood < 0) {
        percentageFood = 0;
    }
    Serial.print("Remaining food:\t");
    Serial.print(percentageFood);
    Serial.println(" %");
    char charBuf[6];
    int ret = snprintf(charBuf, sizeof charBuf, "%i", percentageFood);  // Translate float to char before publishing...
    client.publish(remaining_topic, charBuf, true); // Publishing remaining food to MQTT Sensor retain true
    delay(500);
}


//Round to nearest multiple
int roundToMultiple(int toRound, int multiple)
{
    return (toRound + (multiple / 2)) / multiple * multiple;
}

void reconnect()
{
    // Try to connect only once, then go back to loop
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(SENSORNAME, mqtt_username, mqtt_password, willTopic, 1, true, "Offline")) {
        client.publish(willTopic, "Online", true);
        Serial.println("connected");
        // ... and resubscribe
        client.subscribe(feed_topic);
    }
    else {
        Serial.print("MQTT failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        // Wait 5 seconds before retrying
    }
}

void loop()
{
    ArduinoOTA.handle();
    // Check for buttonpin push
    if (digitalRead(buttonPin) == LOW) {
        Serial.println("Button pushed, feeding cats...");
        buttonFeedCats();
    }
    if (!client.connected()) {
        reconnect();
    }
    client.loop();
    delay(100);
}
