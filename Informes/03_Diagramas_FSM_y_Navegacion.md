# Diagramas del Sistema - NexoRiego

## 1. Máquina de Estados Finita con Trazabilidad (ESP32)

```text
                        +---------------------------------------+
                        |               MONITOREO               |
         +------------->|          (LED RGB: Amarillo)          |<-------------+
         |              +---------------------------------------+              |
         |                  |                               |                  |
 [EV_RESET_SISTEMA]  [EV_TANQUE_VACIO]        [EV_RIEGO_AUTOMATICO /    [EV_FIN_ABSORCION]
 (Físico o MQTT)     (regadas >= 2)            EV_RIEGO_MANUAL_APP]     (Timer 10s)
         |                  |                               |                  |
         |                  v                               v                  |
+-----------------+  +---------------+             +-----------------+         |
| ciclosRegados=0 |  |  FALTA_AGUA   |             |      RIEGO      |         |
| Buzzer: OFF     |  | (LED: Rojo)   |             | (Bomba: ON)     |         |
+-----------------+  | (Buzzer: ON)  |             | (LED: Azul)     |         |
         ^           +---------------+             +-----------------+         |
         |                  |                               |                  |
         +------------------+                        [EV_FIN_RIEGO]            |
                                                       (Timer 10s)             |
                                                            |                  |
                                                            v                  |
                                                   +-----------------+         |
                                                   |    ABSORCION    |---------+
                                                   | (Bomba: OFF)    |
                                                   | (LED: Verde)    |
                                                   | ciclosRegados++ |
                                                   +-----------------+
```

---

## 2. Arquitectura Edge-Cloud y Flujo de Mensajería

```text
+--------------------+      MQTT (sensores)      +-------------------------------+
|  ESP32 NexoRiego   | ------------------------> |    Broker MQTT (EMQX:1883)    |
|  (Wokwi / Físico)  | <------------------------ |   o Mosquitto Local (1883)    |
+--------------------+      MQTT (comandos)      +-------------------------------+
                                                    ^                         ^
                                                    |                         |
                         +--------------------------+                         +--------------------------+
                         |                                                                               |
                         v                                                                               v
+-------------------------------------------------+                     +-------------------------------------------------+
|           Edge-Cloud: Node-RED (1880)           |                     |             Android App (Java)                  |
|                                                 |                     |                                                 |
|  - Reglas de Alerta (Falta agua / Sequía / T°)  |                     |  - IoTRepository (ExecutorService + LiveData)   |
|  - Persistencia: telemetry_history.jsonl        |                     |  - DashboardActivity (Estado, Motivo, Control)  |
|  - Dashboard Web: http://localhost:1880/ui      |                     |  - LightOverrideActivity (Sensor.TYPE_LIGHT)    |
|  - API REST: /api/status, /history, /command    |                     +-------------------------------------------------+
+-------------------------------------------------+
```

---

## 3. Arquitectura Reactiva y Navegación Android

```text
+-------------------------------------------------------------------+
|                        DashboardActivity                          |
|                    (activity_dashboard.xml)                       |
|                                                                   |
|  - Observa LiveData<Boolean> (Estado Conexión Broker)             |
|  - Observa LiveData<TelemetryState> (FSM, Motivo, Sensores)       |
|  - Acciones: [Regar Ahora] [Reset Tanque] [Enviar Comando]        |
|  - Navegación: [Delegar Sensor de Luz al Smartphone]              |
+-------------------------------------------------------------------+
                 |                                   ^
        Intent Explícito                             | Botón [Volver]
        startActivity()                              | finish() / onPause()
                 v                                   |
+-------------------------------------------------------------------+
|                      LightOverrideActivity                      |
|                  (activity_light_override.xml)                    |
|                                                                   |
|  - onResume(): registerListener(Sensor.TYPE_LIGHT)                |
|  - onSensorChanged(): publish("luz:<lux>") con debounce 800 ms    |
|  - onPause(): unregisterListener() + publish("luz:-1")            |
+-------------------------------------------------------------------+
                 |                                   ^
                 | sendCommand()                     | postValue()
                 v                                   |
+-------------------------------------------------------------------+
|                     IoTRepository (Singleton)                     |
|                                                                   |
|  - ExecutorService (Hilo secundario) <---> Eclipse Paho MQTT      |
|  - Broker: tcp://broker.emqx.io:1883                              |
+-------------------------------------------------------------------+
```
