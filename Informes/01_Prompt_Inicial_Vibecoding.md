# Prompt Inicial de Desarrollo - Sistema IoT NexoRiego

Actuá como arquitecto de sistemas embebidos, Edge-Cloud y desarrollador Android Senior. Diseñá e implementá desde cero un MVP funcional de un sistema de riego automatizado IoT llamado **NexoRiego** para la cátedra de Sistemas Operativos Avanzados (UNLaM), compuesto por un nodo embebido **ESP32 (FreeRTOS + PlatformIO + Wokwi)**, una capa **Edge-Cloud (Docker + Node-RED + Mosquitto)** y una aplicación móvil nativa **Android (Java)**, comunicados vía WiFi mediante **MQTT** (`tcp://broker.emqx.io:1883`).

## 1. Arquitectura del Sistema Embebido (`Embebido/Proyecto_con_Vibecoding`)

* **Estructura Modular:** Separar configuración (`include/Config.h`), tipos/eventos (`include/Types.h`) y lógica principal (`src/main.cpp`).
* **Mapeo de Hardware (compatible con circuito Wokwi):**
  * Humedad de suelo (Potenciómetro analógico): `GPIO 34`.
  * Luminosidad ambiente (LDR analógico): `GPIO 35`.
  * Temperatura ambiente (DHT22): `GPIO 14`.
  * Pulsador de restablecimiento de tanque (Reset con `INPUT_PULLDOWN`): `GPIO 26`.
  * Actuador bomba de agua (Relé + LED): `GPIO 19`.
  * LED RGB indicador de estado: Rojo `GPIO 2`, Verde `GPIO 4`, Azul `GPIO 5`.
  * Buzzer activo de alarma: `GPIO 15`.
* **Concurrencia y Máquina de Estados (FreeRTOS):**
  * Implementar 4 estados: `MONITOREO` (Amarillo), `RIEGO` (Azul, Bomba ON 10s), `ABSORCION` (Verde, Bomba OFF 10s) y `FALTA_AGUA` (Rojo, Buzzer ON al alcanzar 2 ciclos).
  * Utilizar **Software Timers de FreeRTOS** (`xTimerCreate`) para las ventanas de riego y absorción en lugar de tareas dedicadas bloqueadas.
  * Utilizar una **Cola de Eventos tipados (`EventMessage`)** que transporte tanto el tipo de evento como la cadena de origen (`origin`), garantizando trazabilidad completa de qué disparó cada transición.
  * Proteger el acceso al estado y contador de ciclos mediante un Mutex (`xSemaphoreCreateMutex`).
* **Contrato MQTT:**
  * Suscribirse a `soa/grupo05/nexoriego/comandos` aceptando `"regar"`, `"reset"`, `"luz:<valor>"` (reemplazo del LDR físico por la lectura de Android) y `"luz:-1"` (restauración del LDR local).
  * Publicar cada 3 segundos en `soa/grupo05/nexoriego/sensores` un JSON con: `estado`, `motivo`, `humedad`, `temperatura`, `luz`, `fuente_luz`, `regadas` y `max_regadas`.

## 2. Arquitectura Edge-Cloud (`Edge-Cloud/Proyecto_con_Vibecoding`)

* **Contenedorización (`docker-compose.yml`):** Desplegar **Node-RED** (puerto `1880`) con `node-red-dashboard` preinstalado y un broker **Eclipse Mosquitto** local (puertos `1883` / `9001`).
* **Flujo Edge (`flows.json`):**
  * Suscripción a `soa/grupo05/nexoriego/sensores`, enriquecimiento con timestamp ISO-8601 y detección de alertas (`Falta de agua`, humedad `< 25%`, temperatura `> 32 °C`).
  * Persistencia de histórico en `/data/logs/telemetry_history.jsonl`.
  * Dashboard Web (`/ui`) con gauges en tiempo real, gráficos históricos y botones de control remoto.
  * API REST: `GET /api/status`, `GET /api/history` y `POST /api/command`.

## 3. Arquitectura de la Aplicación Android (`Android/Proyecto_con_Vibecoding`)

* **Patrón Reactivo:** Implementar un repositorio Singleton (`IoTRepository`) que encapsule Eclipse Paho (`org.eclipse.paho.client.mqttv3:1.2.5`) sobre un `ExecutorService` y exponga el estado hacia las vistas mediante `LiveData` (`MutableLiveData`), utilizando `ViewBinding` y componentes Material 3.
* **Activities:**
  1. `DashboardActivity`: Visualiza el estado de conexión, una tarjeta dinámica con el estado de la FSM y el último evento de disparo (`motivo`), métricas de humedad, temperatura, luz activa y su fuente (`ESP32` o `ANDROID`), botones de acción (`Regar Ahora`, `Reset Tanque`), envío de comandos personalizados y navegación explícita.
  2. `LightOverrideActivity`: Implementa `SensorEventListener` sobre `Sensor.TYPE_LIGHT`. Registra el sensor en `onResume()` enviando `luz:<lux>` cada 800 ms, y en `onPause()` desregistra el listener y envía `luz:-1` para devolver el control al embebido.

## 4. Entorno Contenedorizado

* Incluir `.devcontainer` configurado con Python 3.11, PlatformIO CLI, plataforma `espressif32` y extensiones de VS Code para PlatformIO y Wokwi.
