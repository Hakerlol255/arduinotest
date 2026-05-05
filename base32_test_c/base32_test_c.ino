#include <esp_now.h>
#include <WiFi.h>
#include <TinyGPSPlus.h>

#define SLAVE_COUNT 3
#define RECIEVE_CALL 1

TinyGPSPlus gps;
HardwareSerial gpsSerial(1);

volatile bool SendingData = false;
volatile bool SensorReady = false;
volatile bool BmeReady = false;
volatile bool DistReady = false;
volatile bool GpsReady = false;

uint8_t slaveMacs[SLAVE_COUNT][6] = {
    {0xA4, 0xCB, 0x8F, 0x21, 0x37, 0x08}, 
    {0xe8, 0x3d, 0xc1, 0x8c, 0x17, 0x54},
    {0xA4, 0xCB, 0x8F, 0x20, 0xD8, 0x08},
};

typedef struct {
  uint8_t id;      
  int16_t ax, ay, az;
  int16_t gx, gy, gz;
} SensorData;

typedef struct {
  uint8_t id;
  float Sensor1_Data;
  float Sensor2_Data;
  float Temp;
  float Atm_Pres;
  float Hum;
} BmeData;

typedef struct {
  uint8_t id;
  int16_t distance;
} DistanceData;

typedef struct {
  double lat;
  double lng;
  float alt;
  float speed;
  uint8_t satellites;
} GpsData;

typedef struct {
  SensorData Sensor;
  BmeData Bme;
  DistanceData Distance;
  GpsData Gps;
} SendData;

SendData sendData;

void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (SendingData) return;
  if (data[0] == 0) {
    memcpy(&sendData.Sensor, data, sizeof(SensorData));
    Serial.print("AX: "); Serial.println(sendData.Sensor.ax);
    SensorReady = true;
  }
  else if (data[0] == 1) {
    memcpy(&sendData.Bme, data, sizeof(BmeData));
    Serial.print("Temp: "); Serial.println(sendData.Bme.Temp);
    BmeReady = true;
  }
  else if (data[0] == 2) {
    memcpy(&sendData.Distance, data, sizeof(DistanceData));
    Serial.print("Dist: "); Serial.println(sendData.Distance.distance);
    DistReady = true;
  }
}

void readGps() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
    char c = gpsSerial.read();
    Serial.print(c); // ← смотрим сырые NMEA строки
    gps.encode(c);
  }

  if (gps.location.isUpdated()) {
    sendData.Gps.lat        = gps.location.lat();
    sendData.Gps.lng        = gps.location.lng();
    sendData.Gps.alt        = gps.altitude.meters();
    sendData.Gps.speed      = gps.speed.kmph();
    sendData.Gps.satellites = gps.satellites.value();
    GpsReady = true;

    Serial.print("Lat: "); Serial.println(sendData.Gps.lat, 6);
    Serial.print("Lng: "); Serial.println(sendData.Gps.lng, 6);
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  gpsSerial.begin(4800 , SERIAL_8N1, 18, 17);

  WiFi.mode(WIFI_STA);
  delay(100);
  Serial.println(WiFi.macAddress());

  esp_now_init();
  esp_now_register_recv_cb(onReceive);

  esp_now_peer_info_t peer = {};
  peer.channel = 0;
  peer.encrypt = false;
  for (int i = 0; i < SLAVE_COUNT; i++) {
    memcpy(peer.peer_addr, slaveMacs[i], 6);
    esp_now_add_peer(&peer);
  }
  readGps();
}

void SendDataToServer() {}

void loop() {
  // читаем GPS постоянно
  readGps();

  uint8_t call = RECIEVE_CALL;
  if (!SendingData) {
    for (int i = 0; i < SLAVE_COUNT; i++) {
      esp_now_send(slaveMacs[i], &call, sizeof(uint8_t));
      delay(3);
    }
  }

  delay(20);

  if (SensorReady && BmeReady && DistReady) {
    SendingData = true;
    SendDataToServer();
    SensorReady = false;
    BmeReady    = false;
    DistReady   = false;
    SendingData = false;
  }
}