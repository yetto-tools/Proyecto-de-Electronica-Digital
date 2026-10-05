// Test simple del LCD JHD-2X16-I2C (Proteus). Solo Wire + LiquidCrystal_I2C.
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C *lcd;
uint8_t contador = 0;

void setup() {
  Serial.begin(9600);
  Wire.begin();

  // Escáner I2C: imprime todas las direcciones que responden
  uint8_t lcdDir = 0;
  for (uint8_t d = 1; d < 127; d++) {
    Wire.beginTransmission(d);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("Dispositivo I2C en 0x"));
      Serial.println(d, HEX);
      if (d == 0x27 || d == 0x20 || d == 0x3F) lcdDir = d;
    }
  }
  if (lcdDir == 0) {
    Serial.println(F("ERROR: no se encontro el LCD"));
    while (true) { }
  }

  lcd = new LiquidCrystal_I2C(lcdDir, 16, 2);
  lcd->init();
  lcd->backlight();
  lcd->setCursor(0, 0);
  lcd->print(F("Hello, World!"));
  Serial.print(F("LCD en 0x"));
  Serial.println(lcdDir, HEX);
}

void loop() {
  lcd->setCursor(0, 1);
  lcd->print(F("Segundos: "));
  lcd->print(contador++);
  lcd->print(F("   "));
  delay(1000);
}
