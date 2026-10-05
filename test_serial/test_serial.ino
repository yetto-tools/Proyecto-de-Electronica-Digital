// Prueba mínima del terminal serial (Virtual Terminal de Proteus, 9600 baud, RXD -> D1/TX).
// Imprime un contador cada segundo. No usa LCD, I2C ni timers.
// Además parpadea el LED de D13 y escribe lo mismo en D1 a mano si hace falta aislar el pin.

unsigned long n = 0;

void setup() {
  Serial.begin(9600);
  pinMode(13, OUTPUT);
  Serial.println(F("=== test_serial: inicio ==="));
}

void loop() {
  Serial.print(F("Hola desde el Arduino, n = "));
  Serial.println(n++);
  digitalWrite(13, !digitalRead(13));
  delay(1000);
}
