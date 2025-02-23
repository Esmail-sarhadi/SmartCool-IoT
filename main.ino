#include <WiFiManager.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <time.h>
#define TIMEZONE_OFFSET (3.5 * 3600)  // UTC+3:30 for Iran
#define DEBOUNCE_DELAY 300  


// MQTT Broker settings
const char* mqtt_broker = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* temperature_topic = "cooler/temperature";
const char* status_topic = "cooler/status";
const char* control_topic = "cooler/control";
const char* schedule_topic = "cooler/schedule";
const char* time_topic = "cooler/time";

// Relay Pins
const int RELAY_HIGH = 25;   // High speed relay
const int RELAY_LOW = 26;    // Low speed relay
const int RELAY_PUMP = 27;   // Water pump relay

// Touch Button Pins
const int TOUCH_POWER = 32;  // Power touch button
const int TOUCH_PUMP = 18;   // Pump control touch button
const int TOUCH_SPEED = 19;  // Speed control touch button
const int TOUCH_MODE = 21;   // Mode control touch button

// Touch control variables
bool touchLocked = false;
unsigned long powerPressStartTime = 0;
const unsigned long LOCK_PRESS_DURATION = 5000; // 5 seconds for lock
bool lastTouchStates[4] = {false, false, false, false};
unsigned long lastDebounceTime[4] = {0, 0, 0, 0};
const unsigned long debounceDelay = 50;

// DHT Sensor settings
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Iran TimeZone
TimeChangeRule iranSTD = {"IRST", Last, Fri, Mar, 2, +210};  // UTC+3:30
TimeChangeRule iranDST = {"IRDT", Last, Fri, Sep, 2, +270};  // UTC+4:30
Timezone iranTZ(iranSTD, iranDST);

// WiFi and MQTT clients
WiFiClient espClient;
PubSubClient client(espClient);

// Control state structure
struct CoolerState {
    bool isOn = false;
    bool highSpeed = false;
    bool lowSpeed = false;
    bool pump = false;
    float targetTemp = 25.0;
    bool tempControlEnabled = false;
} coolerState;

// Schedule structure
struct Schedule {
    bool enabled = false;
    int startHour = 0;
    int startMinute = 0;
    int endHour = 0;
    int endMinute = 0;
    bool days[7] = {false};  // Sunday to Saturday
} schedule;

// Timing variables
unsigned long lastMsg = 0;
unsigned long lastDHTRead = 0;
const unsigned long PUBLISH_INTERVAL = 500;
const unsigned long DHT_READ_DELAY = 100;

void publishSchedule() {
    Serial.println("Publishing schedule to MQTT...");
    StaticJsonDocument<400> doc;
    doc["enabled"] = schedule.enabled;
    doc["startHour"] = schedule.startHour;
    doc["startMinute"] = schedule.startMinute;
    doc["endHour"] = schedule.endHour;
    doc["endMinute"] = schedule.endMinute;
    
    JsonArray days = doc.createNestedArray("days");
    for (int i = 0; i < 7; i++) {
        days.add(schedule.days[i]);
    }
    
    char buffer[400];
    serializeJson(doc, buffer);
    client.publish(schedule_topic, buffer, true);
    
    Serial.println("Schedule published successfully");
}

void handleTemperatureControl() {
    if (!coolerState.tempControlEnabled || !coolerState.isOn) {
        return;
    }
    
    float currentTemp = dht.readTemperature();
    if (isnan(currentTemp)) {
        Serial.println("Failed to read temperature!");
        return;
    }
    
    Serial.print("Current temperature: ");
    Serial.print(currentTemp);
    Serial.print("°C, Target: ");
    Serial.print(coolerState.targetTemp);
    Serial.println("°C");
    
    if (currentTemp > coolerState.targetTemp + 1) {
        if (!coolerState.highSpeed) {
            Serial.println("Temperature too high - switching to HIGH speed");
            setSpeed("high");
        }
    } else if (currentTemp < coolerState.targetTemp - 1) {
        if (!coolerState.lowSpeed) {
            Serial.println("Temperature lower than target - switching to LOW speed");
            setSpeed("low");
        }
    }
}

unsigned long lastTouchTime[4] = {0, 0, 0, 0};
bool buttonEnabled[4] = {true, true, true, true};

bool debounceTouch(int pin, int index) {
    unsigned long currentTime = millis();
    bool currentState = digitalRead(pin) == HIGH;
    
    // اگر دکمه فشرده شده و زمان کافی از آخرین فشار گذشته است
    if (currentState && buttonEnabled[index] && 
        (currentTime - lastTouchTime[index] > DEBOUNCE_DELAY)) {
        
        Serial.print("Button ");
        Serial.print(index);
        Serial.println(" pressed successfully");
        
        lastTouchTime[index] = currentTime;
        buttonEnabled[index] = false;  // غیرفعال کردن دکمه تا رها شود
        return true;
    }
    
    // اگر دکمه رها شده، دوباره فعالش کن
    if (!currentState) {
        buttonEnabled[index] = true;
    }
    
    return false;
}

void setupRelays() {
    Serial.println("Setting up relays...");
    pinMode(RELAY_HIGH, OUTPUT);
    pinMode(RELAY_LOW, OUTPUT);
    pinMode(RELAY_PUMP, OUTPUT);
    
    digitalWrite(RELAY_HIGH, HIGH);
    digitalWrite(RELAY_LOW, HIGH);
    digitalWrite(RELAY_PUMP, HIGH);
    Serial.println("Relays initialized to OFF state");
}

void setupTouchButtons() {
    Serial.println("Setting up touch buttons...");
    
    
    pinMode(TOUCH_POWER, INPUT_PULLDOWN);
    pinMode(TOUCH_PUMP, INPUT_PULLDOWN);
    pinMode(TOUCH_SPEED, INPUT_PULLDOWN);
    pinMode(TOUCH_MODE, INPUT_PULLDOWN);
    
    
    for(int i = 0; i < 4; i++) {
        lastTouchTime[i] = 0;
        buttonEnabled[i] = true;
    }
    
    Serial.println("Touch buttons initialized with improved debounce");
}
void handleTouchButtons() {
    static unsigned long lockCheckTime = 0;
    bool powerTouch = digitalRead(TOUCH_POWER) == HIGH;
    unsigned long currentTime = millis();

    // اگر دکمه پاور فشرده شده
    if (powerTouch) {
        if (powerPressStartTime == 0) {
            powerPressStartTime = currentTime;
            Serial.println("Power button press started");
        }
        // اگر دکمه به مدت کافی نگه داشته شده
        else if ((currentTime - powerPressStartTime) >= LOCK_PRESS_DURATION) {
            if (lockCheckTime == 0) {  // فقط یکبار اجرا شود
                touchLocked = !touchLocked; // تغییر وضعیت قفل
                Serial.print("Touch buttons ");
                Serial.println(touchLocked ? "LOCKED" : "UNLOCKED");
                
                // ارسال وضعیت جدید به MQTT
                StaticJsonDocument<100> doc;
                doc["touchLocked"] = touchLocked;
                char buffer[100];
                serializeJson(doc, buffer);
                client.publish(status_topic, buffer, true);
                lockCheckTime = currentTime;
            }
            return;
        }
    }
    // وقتی دکمه رها می‌شود
    else if (!powerTouch && powerPressStartTime > 0) {
        if ((currentTime - powerPressStartTime) < LOCK_PRESS_DURATION) {
            // فقط اگر قفل نیست، عملیات روشن/خاموش انجام شود
            if (!touchLocked) {
                Serial.println("Power button short press");
                if (coolerState.isOn) {
                    turnOffCooler();
                } else {
                    turnOnCooler();
                }
            }
        }
        powerPressStartTime = 0;
        lockCheckTime = 0;
    }

    // اگر قفل است، بقیه دکمه‌ها را بررسی نکن
    if (touchLocked) {
        return;
    }

    // کنترل سایر دکمه‌ها با debounce
    if (debounceTouch(TOUCH_PUMP, 1)) {
        Serial.println("Pump control triggered");
        setPump(!coolerState.pump);
    }

    if (debounceTouch(TOUCH_SPEED, 2) && coolerState.isOn) {
        Serial.println("Speed control triggered");
        if (coolerState.highSpeed) {
            setSpeed("low");
        } else {
            setSpeed("high");
        }
    }

    if (debounceTouch(TOUCH_MODE, 3)) {
        Serial.println("Mode control triggered");
        if (!coolerState.isOn) {
            turnOnCooler();
        } else {
            if (coolerState.highSpeed) {
                setSpeed("low");
            } else {
                setSpeed("high");
            }
        }
    }
}

void turnOnCooler() {
    Serial.println("=== Turning ON Cooler ===");
    digitalWrite(RELAY_HIGH, LOW);  
    digitalWrite(RELAY_LOW, HIGH);  
    digitalWrite(RELAY_PUMP, LOW);  
    
    coolerState.isOn = true;
    coolerState.highSpeed = true;
    coolerState.lowSpeed = false;
    coolerState.pump = true;
    
    Serial.println("High Speed: ON");
    Serial.println("Low Speed: OFF");
    Serial.println("Pump: ON");
    
    publishState();
    Serial.println("=== Cooler ON Complete ===");
}

void turnOffCooler() {
    Serial.println("=== Turning OFF Cooler ===");
    digitalWrite(RELAY_HIGH, HIGH); 
    digitalWrite(RELAY_LOW, HIGH);
    digitalWrite(RELAY_PUMP, HIGH);
    
    coolerState.isOn = false;
    coolerState.highSpeed = false;
    coolerState.lowSpeed = false;
    coolerState.pump = false;
    
    Serial.println("High Speed: OFF");
    Serial.println("Low Speed: OFF");
    Serial.println("Pump: OFF");
    
    publishState();
    Serial.println("=== Cooler OFF Complete ===");
}

void setSpeed(const char* speed) {
    Serial.print("=== Setting Speed: ");
    Serial.print(speed);
    Serial.println(" ===");
    
    if (!coolerState.isOn) {
        Serial.println("Ignored - Cooler is OFF");
        return;
    }

    
    if (strcmp(speed, "high") == 0) {
        digitalWrite(RELAY_HIGH, LOW);   // روشن کردن دور تند
        digitalWrite(RELAY_LOW, HIGH);   // خاموش کردن دور کند
        coolerState.highSpeed = true;
        coolerState.lowSpeed = false;
    } 
    
    else if (strcmp(speed, "low") == 0) {
        digitalWrite(RELAY_HIGH, HIGH);  // خاموش کردن دور تند
        digitalWrite(RELAY_LOW, LOW);    // روشن کردن دور کند
        coolerState.highSpeed = false;
        coolerState.lowSpeed = true;
    }
    
    else if (strcmp(speed, "none") == 0) {
        digitalWrite(RELAY_HIGH, HIGH);  // خاموش کردن دور تند
        digitalWrite(RELAY_LOW, HIGH);   // خاموش کردن دور کند
        coolerState.highSpeed = false;
        coolerState.lowSpeed = false;
    }
    
    Serial.print("Final state - High Speed: ");
    Serial.print(coolerState.highSpeed ? "ON" : "OFF");
    Serial.print(", Low Speed: ");
    Serial.println(coolerState.lowSpeed ? "ON" : "OFF");
    
    publishState();
}

void setPump(bool state) {
    Serial.print("=== Setting Pump: ");
    Serial.print(state ? "ON" : "OFF");
    Serial.println(" ===");
    
    digitalWrite(RELAY_PUMP, !state);
    coolerState.pump = state;
    
    Serial.print("Pump relay state: ");
    Serial.println(!state ? "HIGH" : "LOW");
    
    publishState();
}

void publishState() {
    Serial.println("Publishing state to MQTT...");
    StaticJsonDocument<200> doc;
    doc["isOn"] = coolerState.isOn;
    doc["highSpeed"] = coolerState.highSpeed;
    doc["lowSpeed"] = coolerState.lowSpeed;
    doc["pump"] = coolerState.pump;
    doc["targetTemp"] = coolerState.targetTemp;
    doc["tempControlEnabled"] = coolerState.tempControlEnabled;
    doc["touchLocked"] = touchLocked;
    
    char buffer[200];
    serializeJson(doc, buffer);
    client.publish(status_topic, buffer, true);
    
    Serial.print("Current State: Power=");
    Serial.print(coolerState.isOn ? "ON" : "OFF");
    Serial.print(", High=");
    Serial.print(coolerState.highSpeed ? "ON" : "OFF");
    Serial.print(", Low=");
    Serial.print(coolerState.lowSpeed ? "ON" : "OFF");
    Serial.print(", Pump=");
    Serial.println(coolerState.pump ? "ON" : "OFF");
}

void handleSchedule() {
    if (!schedule.enabled) return;
    
    time_t now;
    time(&now);
    now += TIMEZONE_OFFSET;  // تبدیل به وقت ایران
    struct tm* timeinfo = localtime(&now);
    
    int currentHour = timeinfo->tm_hour;
    int currentMinute = timeinfo->tm_min;
    int currentDay = timeinfo->tm_wday;  // 0 = یکشنبه
    
    Serial.print("Current time: ");
    Serial.print(currentHour);
    Serial.print(":");
    Serial.print(currentMinute);
    Serial.print(" Day: ");
    Serial.println(currentDay);
    
    if (schedule.days[currentDay]) {
        int currentTime = currentHour * 60 + currentMinute;
        int startTime = schedule.startHour * 60 + schedule.startMinute;
        int endTime = schedule.endHour * 60 + schedule.endMinute;
        
        if (currentTime >= startTime && currentTime < endTime && !coolerState.isOn) {
            Serial.println("Schedule: Turning cooler ON");
            turnOnCooler();
        } else if (currentTime >= endTime && coolerState.isOn) {
            Serial.println("Schedule: Turning cooler OFF");
            turnOffCooler();
        }
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("MQTT Message received on topic: ");
    Serial.println(topic);
    
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    
    Serial.print("Message content: ");
    Serial.println(message);
    
    StaticJsonDocument<400> doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        return;
    }
    
    if (strcmp(topic, control_topic) == 0) {
        if (doc.containsKey("touchLock")) {
            touchLocked = doc["touchLock"];
            Serial.print("Touch lock set to: ");
            Serial.println(touchLocked ? "LOCKED" : "UNLOCKED");
        }
        
        if (doc.containsKey("power")) {
            bool power = doc["power"];
            Serial.print("Power command received: ");
            Serial.println(power ? "ON" : "OFF");
            if (power) turnOnCooler();
            else turnOffCooler();
        }
        
        if (doc.containsKey("speed")) {
            const char* speed = doc["speed"];
            Serial.print("Speed command received: ");
            Serial.println(speed);
            setSpeed(speed);
        }
        
        if (doc.containsKey("pump")) {
            bool pumpState = doc["pump"];
            Serial.print("Pump command received: ");
            Serial.println(pumpState ? "ON" : "OFF");
            setPump(pumpState);
        }
    }
}

void reconnect() {
    Serial.println("Attempting MQTT connection...");
    while (!client.connected()) {
        String clientId = "ESP32Cooler-" + String(random(0xffff), HEX);
        Serial.print("ClientId: ");
        Serial.println(clientId);
        
        if (client.connect(clientId.c_str())) {
            Serial.println("Connected to MQTT broker");
            client.subscribe(control_topic);
            client.subscribe(schedule_topic);
            client.subscribe(time_topic);
            
            publishState();
            publishSchedule();
        } else {
            Serial.print("MQTT connection failed, rc=");
            Serial.print(client.state());
            Serial.println(" retrying in 5 seconds");
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Cooler Control System Starting ===");
    
    setupRelays();
    setupTouchButtons();
    
    Serial.println("Initializing DHT sensor...");
    dht.begin();
    
    Serial.println("Setting up WiFi...");
    WiFiManager wm;
    bool res = wm.autoConnect("Cooler-Setup");
    
    if(!res) {
        Serial.println("Failed to connect to WiFi");
        ESP.restart();
    }
    
    Serial.println("WiFi connected");
    Serial.println("Setting up MQTT...");
    client.setServer(mqtt_broker, mqtt_port);
    client.setCallback(callback);
    
    Serial.println("Setup complete!");
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();
    
    handleTouchButtons();
    
    unsigned long now = millis();
    if (now - lastMsg > PUBLISH_INTERVAL) {
        lastMsg = now;
        
        float temperature = dht.readTemperature();
        if (!isnan(temperature)) {
            char tempStr[10];
            dtostrf(temperature, 2, 1, tempStr);
            Serial.print("Temperature: ");
            Serial.println(tempStr);
            client.publish(temperature_topic, tempStr);
        }
        
        handleSchedule();
        handleTemperatureControl();
    }
}
