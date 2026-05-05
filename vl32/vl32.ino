#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <VL53L1X.h>

VL53L1X sensor;
volatile bool requested = false;
uint8_t receiverMac[] = {0x14, 0x33, 0x5C, 0x2D, 0xE9, 0x6C};

typedef struct {
  uint8_t id;
  int16_t distance; // мм
} SensorData;

SensorData data;

void onReceive(const esp_now_recv_info_t *info, const uint8_t *call, int len) {
  requested = true;
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  esp_now_init();
  esp_now_register_recv_cb(onReceive);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, receiverMac, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  Wire.begin(6, 5); // SDA=6, SCL=5
  sensor.setTimeout(500);
  sensor.init();
  sensor.setDistanceMode(VL53L1X::Long);
  sensor.setMeasurementTimingBudget(50000);
  sensor.startContinuous(50);

  Serial.println(WiFi.macAddress());
}

void loop() {
  if (requested) {
    requested = false;

    data.id = 2;
    data.distance = sensor.read(); // расстояние в мм

    esp_now_send(receiverMac, (uint8_t *)&data, sizeof(data));
  }
}