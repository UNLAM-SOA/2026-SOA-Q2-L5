#include <Arduino.h>
#include <WiFi.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>

#include "Config.h"
#include "Types.h"

// ============================================================================
// INSTANCIAS GLOBALES Y PRIMITIVAS FREERTOS
// ============================================================================
DHT dht(PIN_DHT, DHT22);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

QueueHandle_t     eventQueue    = nullptr;
SemaphoreHandle_t stateMutex    = nullptr;
TimerHandle_t     riegoTimer    = nullptr;
TimerHandle_t     absorcionTimer = nullptr;

volatile SystemState currentState  = SystemState::MONITOREO;
volatile int         ciclosRegados = 0;
volatile int         luzRemotaApp  = -1; // -1 = LDR local | >= 0 = sensor Android
volatile float       ultimaTempC   = 25.0f;

char ultimoMotivo[48] = "Sistema iniciado";

// ============================================================================
// CAPA DE ABSTRACCION DE HARDWARE (HAL)
// ============================================================================
void setRgbColor(bool r, bool g, bool b) {
  digitalWrite(PIN_RED,   r ? HIGH : LOW);
  digitalWrite(PIN_GREEN, g ? HIGH : LOW);
  digitalWrite(PIN_BLUE,  b ? HIGH : LOW);
}

void setBomba(bool active) {
  digitalWrite(PIN_BOMBA, active ? HIGH : LOW);
}

void setBuzzer(bool active) {
  // Buzzer activo en bajo segun conexion del circuito
  digitalWrite(PIN_BUZZER, active ? LOW : HIGH);
}

const char* stateToString(SystemState st) {
  switch (st) {
    case SystemState::MONITOREO:  return "Monitoreo";
    case SystemState::RIEGO:      return "Riego";
    case SystemState::ABSORCION:  return "Absorcion";
    case SystemState::FALTA_AGUA: return "Falta de agua";
    default:                      return "Desconocido";
  }
}

void postEvent(EventType type, const char* origin, int param = 0) {
  if (eventQueue == nullptr) return;
  EventMessage msg{};
  msg.type = type;
  msg.param = param;
  strncpy(msg.origin, origin, sizeof(msg.origin) - 1);
  msg.origin[sizeof(msg.origin) - 1] = '\0';
  xQueueSend(eventQueue, &msg, pdMS_TO_TICKS(20));
}

// ============================================================================
// CALLBACKS DE TIMERS SOFTWARE FREERTOS (Zero-Stack overhead vs Tasks)
// ============================================================================
void onRiegoTimerCallback(TimerHandle_t xTimer) {
  postEvent(EventType::EV_FIN_RIEGO, "TIMER_RIEGO_10S");
}

void onAbsorcionTimerCallback(TimerHandle_t xTimer) {
  postEvent(EventType::EV_FIN_ABSORCION, "TIMER_ABSORCION_10S");
}

// ============================================================================
// MAQUINA DE ESTADOS FINITA (FSM) DETERMINISTA CON TRAZABILIDAD
// ============================================================================
void processEvent(const EventMessage& ev) {
  // Evento transversal de delegacion sensorial
  if (ev.type == EventType::EV_OVERRIDE_LUZ) {
    luzRemotaApp = ev.param;
    if (luzRemotaApp < 0) {
      Serial.println("[SENSOR-MUX] Luz restaurada a LDR fisico (GPIO 35).");
    } else {
      Serial.printf("[SENSOR-MUX] Luz delegada a Android: %d lx\n", luzRemotaApp);
    }
    return;
  }

  xSemaphoreTake(stateMutex, portMAX_DELAY);

  switch (currentState) {
    case SystemState::MONITOREO:
      if (ev.type == EventType::EV_TANQUE_VACIO) {
        currentState = SystemState::FALTA_AGUA;
        strncpy(ultimoMotivo, ev.origin, sizeof(ultimoMotivo) - 1);
        setRgbColor(true, false, false); // Rojo
        setBuzzer(true);
        Serial.printf("[FSM] MONITOREO -> FALTA_AGUA | Origen: %s\n", ev.origin);
      }
      else if (ev.type == EventType::EV_RIEGO_AUTOMATICO || ev.type == EventType::EV_RIEGO_MANUAL_APP) {
        if (ciclosRegados >= MAX_CICLOS_AGUA) {
          currentState = SystemState::FALTA_AGUA;
          strncpy(ultimoMotivo, "TANQUE_AGOTADO", sizeof(ultimoMotivo) - 1);
          setRgbColor(true, false, false);
          setBuzzer(true);
          Serial.println("[FSM] Bloqueo de seguridad: Tanque sin agua antes de regar.");
        } else {
          currentState = SystemState::RIEGO;
          strncpy(ultimoMotivo, ev.origin, sizeof(ultimoMotivo) - 1);
          setBomba(true);
          setRgbColor(false, false, true); // Azul
          xTimerStart(riegoTimer, 0);
          Serial.printf("[FSM] MONITOREO -> RIEGO | Origen: %s\n", ev.origin);
        }
      }
      break;

    case SystemState::RIEGO:
      if (ev.type == EventType::EV_FIN_RIEGO) {
        setBomba(false);
        ciclosRegados++;
        currentState = SystemState::ABSORCION;
        strncpy(ultimoMotivo, ev.origin, sizeof(ultimoMotivo) - 1);
        setRgbColor(false, true, false); // Verde
        xTimerStart(absorcionTimer, 0);
        Serial.printf("[FSM] RIEGO -> ABSORCION (Ciclo %d/%d) | Origen: %s\n",
                      ciclosRegados, MAX_CICLOS_AGUA, ev.origin);
      }
      break;

    case SystemState::ABSORCION:
      if (ev.type == EventType::EV_FIN_ABSORCION) {
        currentState = SystemState::MONITOREO;
        strncpy(ultimoMotivo, "CICLO_COMPLETADO", sizeof(ultimoMotivo) - 1);
        setRgbColor(true, true, false); // Amarillo
        Serial.println("[FSM] ABSORCION -> MONITOREO | Listo para nueva evaluacion.");
      }
      break;

    case SystemState::FALTA_AGUA:
      if (ev.type == EventType::EV_RESET_SISTEMA) {
        ciclosRegados = 0;
        setBuzzer(false);
        currentState = SystemState::MONITOREO;
        strncpy(ultimoMotivo, ev.origin, sizeof(ultimoMotivo) - 1);
        setRgbColor(true, true, false); // Amarillo
        Serial.printf("[FSM] FALTA_AGUA -> MONITOREO | Origen: %s\n", ev.origin);
      }
      break;
  }

  xSemaphoreGive(stateMutex);
}

// ============================================================================
// TAREAS FREERTOS
// ============================================================================
void taskStateMachine(void* pvParameters) {
  EventMessage incoming{};
  while (true) {
    if (xQueueReceive(eventQueue, &incoming, portMAX_DELAY) == pdTRUE) {
      processEvent(incoming);
    }
  }
}

void taskSensorMonitor(void* pvParameters) {
  while (true) {
    // 1. Chequeo de pulsador fisico de Reset
    if (digitalRead(PIN_RESET) == HIGH && currentState == SystemState::FALTA_AGUA) {
      postEvent(EventType::EV_RESET_SISTEMA, "BOTON_FISICO_RESET");
      vTaskDelay(pdMS_TO_TICKS(600)); // Antirrebote
      continue;
    }

    // 2. Evaluacion agronomica solo en estado MONITOREO
    if (currentState == SystemState::MONITOREO) {
      int humedadAdc = analogRead(PIN_HUMEDAD);
      float tempC    = dht.readTemperature();
      if (isnan(tempC)) tempC = ultimaTempC;
      else ultimaTempC = tempC;

      int luzActual  = (luzRemotaApp >= 0) ? luzRemotaApp : analogRead(PIN_LUZ);
      const char* fuente = (luzRemotaApp >= 0) ? "ANDROID" : "LDR";

      Serial.printf("[SENSORES] Hum:%d | Temp:%.1fC | Luz(%s):%d | Estado:%s\n",
                    humedadAdc, tempC, fuente, luzActual, stateToString(currentState));

      if (ciclosRegados >= MAX_CICLOS_AGUA) {
        postEvent(EventType::EV_TANQUE_VACIO, "LIMITE_CICLOS_AGUA");
      } else if (humedadAdc < HUMEDAD_CRITICA_ADC) {
        postEvent(EventType::EV_RIEGO_AUTOMATICO, "SUELO_SECO_CRITICO");
      } else if (humedadAdc < HUMEDAD_MEDIA_ADC && tempC > TEMPERATURA_ALTA_C && luzActual > LUZ_ALTA_UMBRAL) {
        postEvent(EventType::EV_RIEGO_AUTOMATICO,
                  (luzRemotaApp >= 0) ? "CLIMA_Y_LUZ_ANDROID" : "CLIMA_Y_LUZ_LDR");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(POLLING_SENSORES_MS));
  }
}

// ============================================================================
// CONECTIVIDAD WIFI Y MQTT
// ============================================================================
void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);
  Serial.printf("Conectando a WiFi [%s]", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado! IP: " + WiFi.localIP().toString());
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String cmd;
  cmd.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) {
    cmd += static_cast<char>(payload[i]);
  }
  cmd.trim();

  Serial.printf("[MQTT RX] %s -> %s\n", topic, cmd.c_str());

  if (cmd == "regar") {
    postEvent(EventType::EV_RIEGO_MANUAL_APP, "APP_BOTON_MANUAL");
  } else if (cmd == "reset") {
    postEvent(EventType::EV_RESET_SISTEMA, "APP_RESET_REMOTO");
  } else if (cmd.startsWith("luz:")) {
    int lux = cmd.substring(4).toInt();
    postEvent(EventType::EV_OVERRIDE_LUZ, "APP_SENSOR_LUZ", lux);
  }
}

void ensureMqttConnected() {
  while (!mqttClient.connected()) {
    String clientId = "ESP32-NexoRiego-Vibe-" + String(random(0xffff), HEX);
    Serial.print("Conectando a Broker MQTT...");
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println(" OK!");
      mqttClient.subscribe(TOPIC_COMANDOS);
    } else {
      Serial.printf(" Error rc=%d. Reintentando en 3s...\n", mqttClient.state());
      vTaskDelay(pdMS_TO_TICKS(3000));
    }
  }
}

void publishTelemetry() {
  float tempC = ultimaTempC;

  int humedadRaw = analogRead(PIN_HUMEDAD);
  int humedadPct = map(constrain(humedadRaw, 0, 4095), 0, 4095, 0, 100);
  int luzActiva  = (luzRemotaApp >= 0) ? luzRemotaApp : analogRead(PIN_LUZ);
  const char* fuenteLuz = (luzRemotaApp >= 0) ? "ANDROID" : "ESP32";

  char jsonBuffer[280];
  snprintf(jsonBuffer, sizeof(jsonBuffer),
           "{\"estado\":\"%s\",\"motivo\":\"%s\",\"humedad\":%d,"
           "\"temperatura\":%.1f,\"luz\":%d,\"fuente_luz\":\"%s\","
           "\"regadas\":%d,\"max_regadas\":%d}",
           stateToString(currentState),
           ultimoMotivo,
           humedadPct,
           tempC,
           luzActiva,
           fuenteLuz,
           ciclosRegados,
           MAX_CICLOS_AGUA);

  bool ok = mqttClient.publish(TOPIC_SENSORES, jsonBuffer);
  Serial.printf("[MQTT TX] %s -> %s\n", ok ? "OK" : "FAIL", jsonBuffer);
}

// ============================================================================
// PUNTO DE ENTRADA
// ============================================================================
void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(PIN_BOMBA,  OUTPUT);
  pinMode(PIN_RESET,  INPUT_PULLDOWN);
  pinMode(PIN_RED,    OUTPUT);
  pinMode(PIN_GREEN,  OUTPUT);
  pinMode(PIN_BLUE,   OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  setBomba(false);
  setBuzzer(false);
  setRgbColor(true, true, false); // Inicia en Monitoreo (Amarillo)

  setupWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setBufferSize(512);
  mqttClient.setCallback(onMqttMessage);

  eventQueue     = xQueueCreate(EVENT_QUEUE_LEN, sizeof(EventMessage));
  stateMutex     = xSemaphoreCreateMutex();
  riegoTimer     = xTimerCreate("RiegoTimer", pdMS_TO_TICKS(RIEGO_MS), pdFALSE, nullptr, onRiegoTimerCallback);
  absorcionTimer = xTimerCreate("AbsorcionTimer", pdMS_TO_TICKS(ABSORCION_MS), pdFALSE, nullptr, onAbsorcionTimerCallback);

  xTaskCreate(taskStateMachine,  "FSM_Task",     3072, nullptr, 2, nullptr);
  xTaskCreate(taskSensorMonitor, "Sensor_Task",  3072, nullptr, 1, nullptr);
}

void loop() {
  if (!mqttClient.connected()) {
    ensureMqttConnected();
  }
  mqttClient.loop();

  static uint32_t lastTelemetry = 0;
  if (millis() - lastTelemetry >= TELEMETRIA_MS) {
    lastTelemetry = millis();
    publishTelemetry();
  }

  vTaskDelay(pdMS_TO_TICKS(25));
}
