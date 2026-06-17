#include <esp_now.h>
#include <WiFi.h>
#include <TinyGPSPlus.h>
#include <WiFiUdp.h>


#define SLAVE_COUNT 3
#define RECIEVE_CALL 0xFF
#define STM_RX 16
#define STM_TX 15

WiFiUDP udp;
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);
HardwareSerial stmSerial(2);

const char* ssid = "Lololo";
const char* password = "12345678";
const uint16_t serverPort = 11111;


volatile bool SendingData = false;
volatile bool SensorReady = false;
volatile bool BmeReady    = false;
volatile bool DistReady   = false;
volatile bool GpsReady    = false;
volatile bool StmReady    = false;

uint8_t slaveMacs[SLAVE_COUNT][6] = {
    {0xA4, 0xCB, 0x8F, 0x21, 0x37, 0x08},
    {0xe8, 0x3d, 0xc1, 0x8c, 0x17, 0x54},
    {0xA4, 0xCB, 0x8F, 0x20, 0xD8, 0x08},
};

typedef struct {
  uint8_t id;
  int16_t ax, ay, az;
  int16_t gx, gy, gz;
} __attribute__((packed)) SensorData;

typedef struct {
  uint8_t id;
  float Sensor1_Data;
  float Sensor2_Data;
  float Temp;
  float Atm_Pres;
  float Hum;
} __attribute__((packed)) BmeData;

typedef struct {
  uint8_t id;
  int16_t distance;
} __attribute__((packed)) DistanceData;

typedef struct {
  double  lat;
  double  lng;
  float   alt;
  float   speed;
  uint8_t satellites;
} __attribute__((packed)) GpsData;

typedef struct {
  uint8_t id;
  float   p[6]; // PC0, PC2, PA0, PA2, PA4, PA6
} __attribute__((packed)) PressureData;

typedef struct {
  SensorData   Sensor;
  BmeData      Bme;
  DistanceData Distance;
  GpsData      Gps;
  PressureData Pressure;
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
    Serial.print("s1: "); Serial.println(sendData.Bme.Sensor1_Data);
    Serial.print("s2: "); Serial.println(sendData.Bme.Sensor2_Data);
    BmeReady = true;
  }
  else if (data[0] == 2) {
    memcpy(&sendData.Distance, data, sizeof(DistanceData));
    //Serial.print("Dist: "); Serial.println(sendData.Distance.distance);
    DistReady = true;
  }
  else if (data[0] == 0xFF) { // это пинг от слейва
    uint8_t myChannel = WiFi.channel();
    esp_now_send(info->src_addr, &myChannel, 1); // отвечаем своим каналом
  }
}

void readGps() {
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
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

void readStm() {
  if (stmSerial.available() >= 4) {
    uint32_t h;
    stmSerial.readBytes((uint8_t*)&h, 4);

    if (h == 0xDEADBEEF) {
      while (stmSerial.available() < sizeof(PressureData));
      stmSerial.readBytes((uint8_t*)&sendData.Pressure, sizeof(PressureData));

      String labels[] = {"PC0", "PC2", "PA0", "PA2", "PA4", "PA6"};
      for (int i = 0; i < 2; i++) {
        Serial.print(labels[i]);
        Serial.print(": ");
        Serial.print(sendData.Pressure.p[i], 1);
        Serial.println(" Pa");
      }
      StmReady = true;
    }
  }
}


void setup() {
  Serial.begin(115200);
  delay(2000);

  gpsSerial.begin(9600, SERIAL_8N1, 18, 17); // GPS
  stmSerial.begin(115200, SERIAL_8N1, STM_RX, STM_TX); // STM32

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
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED){
    Serial.println("Connecting...");
    delay(500);
  }
  Serial.println("Connected!");
  Serial.println(WiFi.gatewayIP());
  Serial.println(WiFi.channel());

}

void SendDataToServer() {
  //Serial.print("Sending to: ");
  //Serial.println(WiFi.gatewayIP());
  udp.beginPacket(WiFi.gatewayIP(), serverPort);
  udp.write((uint8_t*)&sendData, sizeof(SendData));
  udp.endPacket();
  Serial.println("Send to mobile!");
  SendingData = false;
}
void onwificonnect(){
  
}

void loop() {
  //Serial.println(SendingData);
  //readGps();
  //readStm();

  uint8_t call = WiFi.channel();
  if (!SendingData) {
    for (int i = 0; i < SLAVE_COUNT; i++) {
      esp_now_send(slaveMacs[i], &call, sizeof(uint8_t));
      //Serial.println("Sended request!");
      delay(3);
    }
  }
  if (BmeReady){
    SendDataToServer();
    //StmReady = false;
    BmeReady = false;
  }
  delay(100);
}
