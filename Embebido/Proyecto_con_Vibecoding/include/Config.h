#pragma once

// Cambiar a false al desplegar en hardware fisico con hotspot WiFi
#define MODO_WOKWI true

#if MODO_WOKWI
  constexpr const char* WIFI_SSID     = "Wokwi-GUEST";
  constexpr const char* WIFI_PASSWORD = "";
#else
  constexpr const char* WIFI_SSID     = "TU_WIFI_LAB";
  constexpr const char* WIFI_PASSWORD = "TU_CLAVE_WIFI";
#endif

// Broker MQTT y Contrato de Topicos
constexpr const char* MQTT_BROKER     = "broker.emqx.io";
constexpr int         MQTT_PORT       = 1883;
constexpr const char* TOPIC_SENSORES  = "soa/grupo05/nexoriego/sensores";
constexpr const char* TOPIC_COMANDOS  = "soa/grupo05/nexoriego/comandos";

// Mapeo de Pines GPIO (Identico a diagrama Wokwi)
constexpr int PIN_HUMEDAD = 34;
constexpr int PIN_LUZ     = 35;
constexpr int PIN_DHT     = 14;
constexpr int PIN_RESET   = 26;
constexpr int PIN_BOMBA   = 19;
constexpr int PIN_RED     = 2;
constexpr int PIN_GREEN   = 4;
constexpr int PIN_BLUE    = 5;
constexpr int PIN_BUZZER  = 15;

// Parametros de Control Agronomico y Temporizacion
constexpr int   HUMEDAD_CRITICA_ADC   = 1638;  // < 40%
constexpr int   HUMEDAD_MEDIA_ADC     = 2048;  // < 50%
constexpr float TEMPERATURA_ALTA_C    = 30.0f;
constexpr int   LUZ_ALTA_UMBRAL       = 715;

constexpr uint32_t RIEGO_MS           = 10000; // 10 segundos
constexpr uint32_t ABSORCION_MS       = 10000; // 10 segundos
constexpr uint32_t TELEMETRIA_MS      = 3000;  // 3 segundos
constexpr uint32_t POLLING_SENSORES_MS = 500;  // 500 ms

constexpr int MAX_CICLOS_AGUA         = 2;
constexpr int EVENT_QUEUE_LEN         = 16;
