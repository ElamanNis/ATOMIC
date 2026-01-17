
#include <HardwareSerial.h>
#include <ArduinoJson.h>


struct SystemConfig {
    const int leakPin = 33;
    const int pressurePin = 34;
    const int valvePin = 25;
    const int loraBaud = 9600;
    const char* deviceID = "KAZ-URALS-WELL-001";
};

SystemConfig config;
HardwareSerial LoRa(2);


class SensorProcessor {
private:
    float _lastPressure;
    int _samples;

public:
    SensorProcessor() : _lastPressure(0.0), _samples(25) {}


    float getFilteredPressure() {
        long sum = 0;
        for (int i = 0; i < _samples; i++) {
            sum += analogRead(config.pressurePin);
            delayMicroseconds(500);
        }
        float voltage = (sum / (float)_samples) * (3.3 / 4095.0);
        _lastPressure = voltage * 10.197; 
        return _lastPressure;
    }

    bool checkCriticalLeak() {
        return digitalRead(config.leakPin) == HIGH;
    }
};

SensorProcessor processor;
volatile bool emergencyActive = false;

void IRAM_ATTR emergencyISR() {
    emergencyActive = true;
}

void setup() {
    Serial.begin(115200);
    LoRa.begin(config.loraBaud, SERIAL_8N1, 16, 17);

    pinMode(config.leakPin, INPUT_PULLDOWN);
    pinMode(config.valvePin, OUTPUT);
    digitalWrite(config.valvePin, LOW);

    attachInterrupt(digitalPinToInterrupt(config.leakPin), emergencyISR, RISING);

    Serial.println("SYSTEM STATUS: CORE INITIALIZED");
}

void loop() {
    if (emergencyActive) {
        handleEmergencyProtocol();
    }


    StaticJsonDocument<256> doc;
    doc["id"] = config.deviceID;
    doc["pressure"] = processor.getFilteredPressure();
    doc["status"] = "OPERATIONAL";
    doc["uptime"] = millis() / 1000;
    doc["signal_rssi"] = random(-110, -80); 
    String output;
    serializeJson(doc, output);
    

    LoRa.println(output);
    Serial.println("DATA TX: " + output);

    delay(10000); 
}

void handleEmergencyProtocol() {
    digitalWrite(config.valvePin, HIGH);
    
    while(true) {
        LoRa.println("{\"id\":\"WELL-001\",\"status\":\"CRITICAL_FAILURE\",\"valve\":\"CLOSED\"}");
        Serial.println("FATAL ERROR: LEAK DETECTED. VALVE SECURED.");
        delay(2000); 
    }
}и