#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <MPU6050.h>

MPU6050 mpu;

volatile bool requested = false;

uint8_t receiverMac[] = {0x14, 0x33, 0x5C, 0x2D, 0xE9, 0x6C};

typedef struct {
  uint8_t id;     
  int16_t ax, ay, az;
  int16_t gx, gy, gz;
} SensorData;

SensorData data;

void onReceive(const esp_now_recv_info_t *info, const uint8_t *call, int len) {
    requested = true; // только флаг, больше ничего
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

  Wire.begin(3, 4); // твои пины
  mpu.initialize();
  Serial.println(WiFi.macAddress());
}

void loop() {
    if (requested) {
        requested = false;
        
        SensorData data;
        data.id = 0;
        mpu.getMotion6(&data.ax, &data.ay, &data.az,
                       &data.gx, &data.gy, &data.gz);
        
        esp_now_send(receiverMac, (uint8_t *)&data, sizeof(data));
    }
}