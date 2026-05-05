HardwareSerial Serial1(PA10, PA9); 

bool headerSent = false;

float K = 2.5; // коэффициент датчика

float readPressure(int pin) {
  int adc = analogRead(pin);
  float voltage = adc * 3.3 / 4095.0;
  float pressure_kPa = (voltage - 1.45) / K;
  float pressure_Pa = pressure_kPa * 1000.0; // 👈 перевод в Паскали
  return pressure_Pa;
}

void setup() {
  Serial1.begin(115200); // UART к ESP32-S3
  analogReadResolution(12); // 12 бит
}

void loop() {
  float pressureValues[6];

  // PC0, PC2
  pressureValues[0] = readPressure(8);  
  pressureValues[1] = readPressure(10); 

  // PA0, PA2, PA4, PA6
  pressureValues[2] = readPressure(0);  
  pressureValues[3] = readPressure(2);  
  pressureValues[4] = readPressure(4);  
  pressureValues[5] = readPressure(6);  

  // Заголовок
  if (!headerSent) {
    Serial1.println("PC0,PC2,PA0,PA2,PA4,PA6");
    headerSent = true;
  }

  // Отправка CSV (давление в Па)
  for (int i = 0; i < 6; i++) {
    Serial1.print(pressureValues[i], 1); // можно 1 знак, т.к. Па
    if (i < 5) Serial1.print(",");
  }
  Serial1.println();

  delay(100);
}