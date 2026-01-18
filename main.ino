#include <Arduino.h>
#include <HardwareSerial.h>


#define PIN_LEAK 33      
#define PIN_PRESSURE 34  
#define PIN_FLOW 32      
#define PIN_VALVE 25     

// Настройки LoRa и сна 
HardwareSerial LoRa(2);
#define TIME_TO_SLEEP 600 // Сон 10 минут
#define uS_TO_S_FACTOR 1000000ULL 

// Переменные 
volatile int pulseCount = 0;
void IRAM_ATTR pulseCounter() { pulseCount++; }

void setup() {
  Serial.begin(115200);
  LoRa.begin(9600, SERIAL_8N1, 16, 17); // Инициализация LoRa 
  
  pinMode(PIN_LEAK, INPUT_PULLUP);
  pinMode(PIN_FLOW, INPUT_PULLUP);
  pinMode(PIN_VALVE, OUTPUT);
  
  
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();// Проверка причины пробуждения

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    // АВАРИЙНЫЙ РЕЖИМ: 
    digitalWrite(PIN_VALVE, HIGH); // Закрываем клапан за <50мс 
    sendData("{\"alert\":\"LEAK\", \"valve\":\"CLOSED\"}");
  } else {

    readAndSendSensors();
  }

  //  пробуждения по протечке (Пин 33, сигнал LOW)
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_LEAK, 0); 
  
  // Настройка таймера сна [cite: 64, 69]
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  
  Serial.println("Вход в режим Deep Sleep (100uA)..."); [cite: 64]
  esp_deep_sleep_start();
}

void readAndSendSensors() {
  // 1. Давление (HK2400) [cite: 23]
  int pRaw = analogRead(PIN_PRESSURE);
  float pressure = (pRaw / 4095.0) * 10.0; // Примерный перевод в Бары

  // 2. Поток (YF-S201) - замер за 1 секунду [cite: 23]
  attachInterrupt(digitalPinToInterrupt(PIN_FLOW), pulseCounter, FALLING);
  pulseCount = 0;
  delay(1000); 
  detachInterrupt(digitalPinToInterrupt(PIN_FLOW));
  float flowRate = pulseCount / 7.5; // л/мин для YF-S201

  // Формируем JSON пакет 
  String payload = "{\"P\":" + String(pressure) + ",\"Q\":" + String(flowRate) + ",\"L\":0}";
  sendData(payload);
}

void sendData(String data) {
  LoRa.println(data); // Отправка через LoRa модели [cite: 38]
  Serial.println("Данные отправлены: " + data);
}

void loop() {} // Не используется в режиме Deep Sleep
