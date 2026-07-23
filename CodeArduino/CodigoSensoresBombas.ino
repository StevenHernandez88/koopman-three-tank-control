// ==============================
// Pines de los sensores ultrasónicos (Tanques 1, 2 y 3)
// ==============================
const int TRIG1 = 23, ECHO1 = 19;
const int TRIG2 = 5,  ECHO2 = 17;   // ← NUEVO: sensor tanque 2 (solo verificación)
const int TRIG3 = 22, ECHO3 = 18;

// ==============================
// Pines de las bombas (MOSFET IRLZ44N)
// ==============================
const int BOMBA1 = 4;
const int BOMBA2 = 14;

// ==============================
// ALTURA DEL SENSOR (cm) — AJUSTA ESTOS VALORES CON UNA REGLA
// ==============================
const float ALTURA_SENSOR_1 = 31.0;
const float ALTURA_SENSOR_2 = 31.0;  // ← NUEVO: mide la distancia real del sensor al fondo
const float ALTURA_SENSOR_3 = 31.0;

// ==============================
// Filtro de mediana (elimina picos por burbujas)
// ==============================
const int MUESTRAS = 7;

int cmp(const void *a, const void *b) {
  float diff = *(float*)a - *(float*)b;
  return (diff > 0) ? 1 : ((diff < 0) ? -1 : 0);
}

float medirDistanciaRobusta(int trig, int echo) {
  float muestras[MUESTRAS];
  for (int i = 0; i < MUESTRAS; i++) {
    digitalWrite(trig, LOW);
    delayMicroseconds(2);
    digitalWrite(trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig, LOW);
    long duracion = pulseIn(echo, HIGH, 30000);
    muestras[i] = duracion * 0.034 / 2.0;
    delay(30);
  }
  qsort(muestras, MUESTRAS, sizeof(float), cmp);
  return muestras[MUESTRAS / 2];
}

// ==============================
// Variables globales
// ==============================
unsigned long lastSend = 0;
const float MAX_PWM = 0.7;

void setup() {
  // Sensores tanque 1 y 3
  pinMode(TRIG1, OUTPUT); pinMode(ECHO1, INPUT);
  pinMode(TRIG3, OUTPUT); pinMode(ECHO3, INPUT);

  // Sensor tanque 2 — solo verificación, no entra al lazo de control
  pinMode(TRIG2, OUTPUT); pinMode(ECHO2, INPUT);  // ← NUEVO

  // Bombas (PWM por LEDC)
  ledcAttachChannel(BOMBA1, 5000, 8, 0);
  ledcAttachChannel(BOMBA2, 5000, 8, 1);
  ledcWriteChannel(0, 0);
  ledcWriteChannel(1, 0);

  Serial.begin(115200);
}

void loop() {
  // ==============================
  // 1. Leer distancias con filtro de mediana
  //    ORDEN IMPORTANTE: primero 1 y 3 (lazo de control),
  //    luego 2 (verificación). Así el tiempo de cómputo del
  //    sensor 2 no retrasa la medición de control.
  // ==============================
  float d1 = medirDistanciaRobusta(TRIG1, ECHO1);
  float d3 = medirDistanciaRobusta(TRIG3, ECHO3);
  float d2 = medirDistanciaRobusta(TRIG2, ECHO2);  // ← NUEVO

  // ==============================
  // 2. Calcular niveles de agua (metros)
  // ==============================
  float h1 = (ALTURA_SENSOR_1 - d1) / 100.0;
  float h2 = (ALTURA_SENSOR_2 - d2) / 100.0;  // ← NUEVO
  float h3 = (ALTURA_SENSOR_3 - d3) / 100.0;

  if (h1 < 0) h1 = 0;
  if (h2 < 0) h2 = 0;  // ← NUEVO
  if (h3 < 0) h3 = 0;

  // ==============================
  // 3. Enviar niveles cada 2 segundos
  //    Formato anterior: h1,0.0000,h3
  //    Formato nuevo:    h1,h2,h3
  //    Python recibe h2 pero NO lo inyecta al observador.
  // ==============================
  if (millis() - lastSend >= 2000) {
    lastSend = millis();
    Serial.print(h1, 4);
    Serial.print(",");
    Serial.print(h2, 4);  // ← antes era siempre 0.0000, ahora es la medición real
    Serial.print(",");
    Serial.println(h3, 4);
  }

  // ==============================
  // 4. Recibir comandos de control (u1,u2 entre 0 y 1)
  //    Sin cambios — el control sigue siendo solo función de h1 y h3
  // ==============================
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    int comma = input.indexOf(',');
    if (comma > 0) {
      float u1 = input.substring(0, comma).toFloat();
      float u2 = input.substring(comma + 1).toFloat();

      u1 = constrain(u1, 0.0f, 1.0f);
      u2 = constrain(u2, 0.0f, 1.0f);

      ledcWriteChannel(0, (int)(u1 * MAX_PWM * 255));
      ledcWriteChannel(1, (int)(u2 * MAX_PWM * 255));
    }
  }

  delay(10);
}