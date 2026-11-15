#pragma once
#include <Arduino.h>

enum class SystemState : uint8_t {
  MONITOREO = 0,
  RIEGO,
  ABSORCION,
  FALTA_AGUA
};

enum class EventType : uint8_t {
  EV_RIEGO_AUTOMATICO,
  EV_RIEGO_MANUAL_APP,
  EV_FIN_RIEGO,
  EV_FIN_ABSORCION,
  EV_TANQUE_VACIO,
  EV_RESET_SISTEMA,
  EV_OVERRIDE_LUZ
};

// Mensaje autocontenido que viaja por la cola FreeRTOS con trazabilidad completa
struct EventMessage {
  EventType type;
  int       param;
  char      origin[32];
};
