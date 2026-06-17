#include <Wire.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>


Adafruit_BME280 bme;

volatile bool requested = false;
volatile bool connected = false;

uint8_t receiverMac[] = {0xe0, 0x72, 0xa1, 0xd7, 0x36, 0x64};

typedef struct {
  uint8_t id;
  float Sensor1_Data;
  float Sensor2_Data;
  float Temp;
  float Atm_Pres;
  float Hum;
} __attribute__((packed)) RecieveData;

float K = 2.5;
float Offset;

RecieveData data;

void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  //Serial.println(data[0]);
  if (!connected){
    uint8_t masterChannel = data[0];
    esp_wifi_set_channel(masterChannel, WIFI_SECOND_CHAN_NONE);
    connected = true;
    Serial.print("Найден канал: ");
    Serial.println(masterChannel);
    return;
  }
  requested = true;
    //Serial.println("Request!");
}

// На слейве — в setup()
void scanForMaster() {  
  for (int ch = 0; ch <= 13; ch++) {
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    Serial.print("Пробую канал: ");
    Serial.println(ch);
    
    // Шлём пинг мастеру
    uint8_t ping = 0xFF;
    esp_now_send(receiverMac, &ping, 1);
    
    delay(200); // ждём ответ
    
    if (connected) break; // мастер ответил — нашли канал
  }
}

void setup() {

  Serial.begin(115200);
  delay(2000);

  Wire.begin(6, 5); // SDA, SCL

  if (!bme.begin(0x76)) { 
    Serial.println("BME280 не найден");
    while (1);
  }
  Initialize();

  Serial.println("BME280 готов");
  //Serial.println(WiFi.macAddress());

  WiFi.mode(WIFI_STA);
  delay(100);

  esp_wifi_set_channel(13, WIFI_SECOND_CHAN_NONE);
  esp_now_init();
  esp_now_register_recv_cb(onReceive);


  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, receiverMac, 6);
  esp_now_add_peer(&peer);
  Serial.println(WiFi.macAddress());
  scanForMaster();
}

void Initialize() {
    float resOffset = 0;
    for (int i = 0; i < 5; i++) {
        int raw1 = analogRead(2);
        int raw2 = analogRead(3);
        resOffset += (raw1 + raw2) / 2.0;
        delay(50);
    }
    Offset = resOffset / 5.0;  // среднее в единицах ADC
}

void loop() {
  //Serial.println(bme.readTemperature());
  int raw1 = analogRead(2);
  int raw2 = analogRead(3);

  float v1 = raw1 * (3.3 / 4095.0);
  float v2 = raw2 * (3.3 / 4095.0);
  float v_offset = Offset * (3.3 / 4095.0);

  if (requested) {
        requested = false;
        
        data.id = 1;
        data.Sensor1_Data = ((v1 - v_offset) / 2.3) * 0.5 * 1000;  // результат в кПа
        data.Sensor2_Data = ((v2 - v_offset) / 2.3) * 2.5 * 1000;  // результат в кПа

        data.Temp = bme.readTemperature();
        data.Atm_Pres = bme.readPressure() / 1000.0F;
        data.Hum = bme.readHumidity();
        
        esp_now_send(receiverMac, (uint8_t *)&data, sizeof(data));
    }
}
