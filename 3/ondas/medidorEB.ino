// ============================================================
//  Medidor de Campo Eléctrico y Magnético v7
//  Hardware: Pro Micro + ADS1115 + SS49E + OLED SSD1306 I2C
//
//  ADS1115:
//    AIN0  → SS49E (Hall)      → Campo magnético B
//    AIN1  → Salida TL082 /2   → Campo eléctrico E
//
//  Layout OLED 128×64:
//    y 0– 8  : texto B (Gauss + mT)
//    y 9–15  : barra B (solo borde, se llena siempre)
//    y16     : separador
//    y17–25  : texto E (voltaje + nivel)
//    y26–33  : barra E (borde vacío / llena según persistencia)
//    y34     : separador
//    y35–63  : osciloscopio E (blanco) + B (punteado/gris)
// ============================================================

#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---- Hardware ----
#define OLED_WIDTH   128
#define OLED_HEIGHT   64
#define OLED_ADDR    0x3C
#define ADS_ADDR     0x48

// ---- SS49E ----
#define SS49E_SENS   5.0f      // mV/Gauss
#define HALL_MAX    40.0f      // Gauss — fondo de escala barra

// ---- Suavizado barras ----
#define VENTANA      6

// ---- Umbrales campo E (V post-divisor en ADC) ----
#define E_BAJO       1.1f
#define E_MEDIO      1.7f
#define E_ALTO       2.5f
#define E_MAX        3.8f

// ---- Persistencia barra E ----
// A 250SPS con VENTANA=6, cada loop ≈ 25-30ms
// 35 ciclos ≈ ~1 segundo
#define PERSIST_CICLOS  35     // ciclos sin cambio → barra vacía
#define CAMBIO_UMBRAL   0.03f  // V — delta mínimo que cuenta como "cambio"

// ---- Osciloscopio ----
#define OSC_X        0         // inicio x
#define OSC_Y       35         // inicio y
#define OSC_W      128         // ancho (puntos de historia)
#define OSC_H       28         // alto zona osciloscópio (35..62)

// ---- Botón re-calibración ----
// Conectar entre este pin y GND (usa pull-up interno)
// Al presionar durante ~1s → recalibra offset Hall
#define PIN_CAL      9
#define CAL_HOLD_MS  800       // ms que hay que mantener pulsado

// ---- Objetos ----
Adafruit_ADS1115 ads;
Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// ---- Buffers suavizado ----
float bufHall[VENTANA] = {0};
float bufE[VENTANA]    = {0};
int   bufIdx = 0;

// ---- Calibración ----
float offsetHall_mV = 0;

// ---- Estado previo barras ----
int barHall_prev = -1;
int barE_prev    = -1;

// ---- Persistencia barra E ----
float eAnterior      = 0;
int   ciclosSinCambio = 0;
bool  barraELlena    = false;

// ---- Osciloscopio: historiales circulares ----
int8_t oscE[OSC_W];   // valor Y escalado para E  (0..OSC_H-1)
int8_t oscB[OSC_W];   // valor Y escalado para B  (0..OSC_H-1)
int    oscIdx = 0;
bool   oscLleno = false;

// ============================================================
//  Nivel texto campo E
// ============================================================
const char* nivelE(float v) {
  if      (v < E_BAJO)  return "BAJO";
  else if (v < E_MEDIO) return "MED ";
  else if (v < E_ALTO)  return "ALTO";
  else                  return "MUY!";
}

// ============================================================
//  LECTURA
// ============================================================
float leerHall_mV() {
  ads.setGain(GAIN_ONE);
  return ads.computeVolts(ads.readADC_SingleEnded(0)) * 1000.0f;
}

float leerE_V() {
  ads.setGain(GAIN_ONE);
  return ads.computeVolts(ads.readADC_SingleEnded(1));
}

// ============================================================
//  Media móvil
// ============================================================
float suavizar(float* buf, float nuevo) {
  buf[bufIdx] = nuevo;
  float s = 0;
  for (int i = 0; i < VENTANA; i++) s += buf[i];
  return s / VENTANA;
}

// ============================================================
//  CALIBRACIÓN
// ============================================================
void calibrar() {
  oled.clearDisplay();
  oled.setTextSize(2);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(10, 20);
  oled.print("CAL...");
  oled.display();

  // Promedia 50 muestras para offset Hall robusto ante cualquier Vcc
  float sH = 0;
  for (int i = 0; i < 50; i++) {
    sH += leerHall_mV();
    delay(10);
  }
  offsetHall_mV = sH / 50.0f;

  // BUG FIX: inicializar buffer con 0 relativo, NO con 0 absoluto.
  // Si se llena con 0, las primeras VENTANA lecturas suavizadas
  // arrastran el offset crudo y el resultado no parte en 0G.
  // La señal relativa correcta en reposo es 0.0f.
  for (int i = 0; i < VENTANA; i++) {
    bufHall[i] = 0.0f;   // 0 relativo = sin campo (ya descontado offset)
    bufE[i]    = 0.0f;
  }
  bufIdx = 0;

  // Reset osciloscopio
  memset(oscE, 0, sizeof(oscE));
  memset(oscB, 0, sizeof(oscB));
  oscIdx   = 0;
  oscLleno = false;

  // Reset estado barras
  barHall_prev    = -1;
  barE_prev       = -1;
  eAnterior       = 0;
  ciclosSinCambio = 0;
  barraELlena     = false;

  oled.clearDisplay();
  oled.setTextSize(2);
  oled.setCursor(20, 20);
  oled.print("OK!");
  oled.display();
  delay(400);

  // ---- Estructura fija de UI ----
  dibujarEstructura();
}

void dibujarEstructura() {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  oled.setCursor(0, 0);   oled.print("B:");
  oled.drawRect(0,  9, 128, 6, SSD1306_WHITE);   // marco barra B

  oled.drawFastHLine(0, 16, 128, SSD1306_WHITE);

  oled.setCursor(0, 17);  oled.print("E:");
  oled.drawRect(0, 26, 128, 6, SSD1306_WHITE);   // marco barra E (siempre visible)

  oled.drawFastHLine(0, 33, 128, SSD1306_WHITE);

  // Borde del osciloscopio (opcional, quita si quieres más espacio)
  // oled.drawRect(OSC_X, OSC_Y, OSC_W, OSC_H, SSD1306_WHITE);

  oled.display();
}

// ============================================================
//  OSCILOSCOPIO — dibuja toda la zona inferior
// ============================================================
void dibujarOsciloscopio() {
  // Borra zona
  oled.fillRect(OSC_X, OSC_Y, OSC_W, OSC_H, SSD1306_BLACK);

  // Línea central de referencia (tenue — solo píxeles pares)
  int yMid = OSC_Y + OSC_H / 2;
  for (int x = 0; x < OSC_W; x += 4) {
    oled.drawPixel(x, yMid, SSD1306_WHITE);
  }

  int total = oscLleno ? OSC_W : oscIdx;
  if (total < 2) return;

  for (int i = 0; i < total - 1; i++) {
    // Índice real en buffer circular
    int ia = oscLleno ? (oscIdx + i)     % OSC_W : i;
    int ib = oscLleno ? (oscIdx + i + 1) % OSC_W : i + 1;

    int x1 = i;
    int x2 = i + 1;

    // ---- Canal E: línea sólida ----
    int yE1 = OSC_Y + (OSC_H - 1 - oscE[ia]);
    int yE2 = OSC_Y + (OSC_H - 1 - oscE[ib]);
    oled.drawLine(x1, yE1, x2, yE2, SSD1306_WHITE);

    // ---- Canal B: punteado (cada 2px) para diferenciar ----
    if (i % 2 == 0) {
      int yB1 = OSC_Y + (OSC_H - 1 - oscB[ia]);
      oled.drawPixel(x1, yB1, SSD1306_WHITE);
    }
  }
}

// ============================================================
//  PANTALLA PRINCIPAL
// ============================================================
void mostrar(float gauss, float tesla, float e_v) {

  // ---- Texto B ----
  oled.fillRect(14, 0, 114, 8, SSD1306_BLACK);
  oled.setCursor(14, 0);
  oled.print(gauss, 1);
  oled.print("G  ");
  oled.print(tesla * 1000.0f, 2);
  oled.print("mT");

  // ---- Barra B (siempre llena, proporcional) ----
  int bH = constrain((int)(abs(gauss) / HALL_MAX * 124), 0, 124);
  if (bH != barHall_prev) {
    if (bH > barHall_prev) {
      oled.fillRect(1 + barHall_prev, 10, bH - barHall_prev, 4, SSD1306_WHITE);
    } else {
      oled.fillRect(1 + bH, 10, barHall_prev - bH, 4, SSD1306_BLACK);
    }
    barHall_prev = bH;
  }

  // ---- Texto E ----
  oled.fillRect(14, 17, 114, 8, SSD1306_BLACK);
  oled.setCursor(14, 17);
  oled.print(e_v, 2);
  oled.print("V ");
  oled.print(nivelE(e_v));

  // ---- Lógica de persistencia barra E ----
  float delta = abs(e_v - eAnterior);
  if (delta >= CAMBIO_UMBRAL) {
    ciclosSinCambio = 0;
    barraELlena = true;
  } else {
    ciclosSinCambio++;
    if (ciclosSinCambio >= PERSIST_CICLOS) {
      barraELlena = false;
    }
  }
  eAnterior = e_v;

  // ---- Barra E (llena o vacía según persistencia) ----
  int bE = constrain((int)(e_v / E_MAX * 124), 0, 124);

  if (barraELlena) {
    // Señal activa/continua → barra llena
    if (bE != barE_prev) {
      if (bE > barE_prev) {
        oled.fillRect(1 + barE_prev, 27, bE - barE_prev, 4, SSD1306_WHITE);
      } else {
        oled.fillRect(1 + bE, 27, barE_prev - bE, 4, SSD1306_BLACK);
      }
      barE_prev = bE;
    }
  } else {
    // Sin actividad sostenida → vaciar barra (solo queda el borde del marco)
    if (barE_prev != 0) {
      oled.fillRect(1, 27, 126, 4, SSD1306_BLACK);
      barE_prev = 0;
    }
  }

  // ---- Actualiza historial osciloscopio ----
  // E: escala 0..E_MAX → 0..OSC_H-1
  oscE[oscIdx] = (int8_t)constrain((int)(e_v / E_MAX * (OSC_H - 1)), 0, OSC_H - 1);

  // B: escala -HALL_MAX..+HALL_MAX → 0..OSC_H-1 (centrado)
  float gaussClamp = constrain(gauss, -HALL_MAX, HALL_MAX);
  oscB[oscIdx] = (int8_t)constrain(
    (int)((gaussClamp + HALL_MAX) / (2.0f * HALL_MAX) * (OSC_H - 1)),
    0, OSC_H - 1
  );

  oscIdx++;
  if (oscIdx >= OSC_W) {
    oscIdx = 0;
    oscLleno = true;
  }

  // ---- Dibuja osciloscopio ----
  dibujarOsciloscopio();

  oled.display();
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);

  Wire.begin();
  Wire.setClock(400000);

  if (!ads.begin(ADS_ADDR)) {
    oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0, 20);
    oled.print("ERROR: ADS1115");
    oled.display();
    while (1);
  }

  ads.setDataRate(RATE_ADS1115_250SPS);

  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (1);
  }

  calibrar();

  pinMode(PIN_CAL, INPUT_PULLUP);   // botón re-calibración

  Serial.println("Gauss,mTesla,E_V,Nivel,BarraActiva");
}

// ============================================================
//  LOOP
// ============================================================
void loop() {

  // ---- Re-calibración por botón (mantener ~1s) ----
  // El OLED muestra cuenta regresiva para feedback visual
  static uint32_t tBoton = 0;
  if (digitalRead(PIN_CAL) == LOW) {
    if (tBoton == 0) tBoton = millis();

    uint32_t held = millis() - tBoton;

    // Feedback: muestra barra de progreso en zona B mientras espera
    if (held < CAL_HOLD_MS) {
      int progreso = (int)(held * 126 / CAL_HOLD_MS);
      oled.fillRect(1, 10, progreso, 4, SSD1306_WHITE);
      oled.fillRect(1 + progreso, 10, 126 - progreso, 4, SSD1306_BLACK);
      oled.display();
      return;   // no leer sensores mientras se mantiene pulsado
    }

    // Tiempo cumplido → recalibrar
    tBoton = 0;
    calibrar();
    return;

  } else {
    // Botón suelto antes de tiempo → restaura barra B
    if (tBoton != 0) {
      tBoton = 0;
      oled.fillRect(1, 10, 126, 4, SSD1306_BLACK);
      barHall_prev = -1;   // fuerza redibujo barra B
    }
  }

  float hallRel   = leerHall_mV() - offsetHall_mV;
  float eAbsoluto = leerE_V();

  float hallSuav = suavizar(bufHall, hallRel);
  float eSuav    = suavizar(bufE, eAbsoluto);
  bufIdx = (bufIdx + 1) % VENTANA;

  float gauss = hallSuav / SS49E_SENS;
  float tesla = gauss * 1e-4f;

  mostrar(gauss, tesla, eSuav);

  Serial.print(gauss, 2);             Serial.print(",");
  Serial.print(tesla * 1000.0f, 3);   Serial.print(",");
  Serial.print(eSuav, 3);             Serial.print(",");
  Serial.print(nivelE(eSuav));        Serial.print(",");
  Serial.println(barraELlena ? "SI" : "NO");
}
