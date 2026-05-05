#include <Wire.h>
#include <esp_now.h>
#include <WiFi.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>


Adafruit_BME280 bme;

volatile bool requested = false;


uint8_t receiverMac[] = {0x14, 0x33, 0x5C, 0x2D, 0xE9, 0x6C};

typedef struct {
  uint8_t id;
  float Sensor1_Data;
  float Sensor2_Data;
  float Temp;
  float Atm_Pres;
  float Hum;
} RecieveData;

float K;
float Offset;

RecieveData data;

void onReceive(const esp_now_recv_info_t *info, const uint8_t *call, int len) {
    requested = true;
}

void setup() {

  Serial.begin(115200);
  delay(2000);

  Wire.begin(6, 5); // SDA, SCL

  if (!bme.begin(0x76)) { 
    Serial.println("BME280 не найден");
    while (1);
  }

  Serial.println("BME280 готов");
  K = 0;
  //Serial.println(WiFi.macAddress());

  WiFi.mode(WIFI_STA);
  delay(100);

  esp_now_init();
  esp_now_register_recv_cb(onReceive);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, receiverMac, 6);
  esp_now_add_peer(&peer);
}

void Initialize(){
  float resOffset;
  for (int i = 0; i < 5; i++){
    int raw1 = analogRead(2);
    int raw2 = analogRead(3);
    resOffset += (raw1 + raw2) / 2;
  }
  resOffset = resOffset / 5.0;
  Offset = resOffset;
}

void loop() {
  int raw1 = analogRead(2);
  int raw2 = analogRead(3);

  float v1 = raw1 * (3.3 / 4095.0);
  float v2 = raw2 * (3.3 / 4095.0);

  if (requested) {
        requested = false;
        
        data.id = 1;
        data.Sensor1_Data = v1;
        data.Sensor2_Data = v2;

        data.Temp = bme.readTemperature();
        data.Atm_Pres = bme.readPressure() / 1000.0F;
        data.Hum = bme.readHumidity();
        
        esp_now_send(receiverMac, (uint8_t *)&data, sizeof(data));
    }
}