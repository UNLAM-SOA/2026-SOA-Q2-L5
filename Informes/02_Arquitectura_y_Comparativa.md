# Arquitectura, Decisiones Técnicas y Matriz Comparativa

## 1. Funcionalidades Implementadas

* **Control Agronómico Autónomo:** Dispara ciclos de riego ante sequía crítica (`humedad < 40%`) o estrés térmico-lumínico (`humedad < 50%`, `temperatura > 30°C` y `luz > 715`).
* **Trazabilidad End-to-End:** Cada evento en el embebido viaja acompañado de su descriptor de origen (`APP_BOTON_MANUAL`, `SUELO_SECO_CRITICO`, `CLIMA_Y_LUZ_ANDROID`, `CLIMA_Y_LUZ_LDR`, `BOTON_FISICO_RESET`), reflejándose en el Monitor Serie, en el Dashboard Edge-Cloud y en la interfaz Android.
* **Delegación Sensorial Dinámica:** Al ingresar a `LightOverrideActivity` en Android, el teléfono transmite su sensor `TYPE_LIGHT` en lux al ESP32, sustituyendo al fotoresistor físico hasta que el usuario abandona la pantalla.
* **Supervisión, Persistencia y API Edge-Cloud:** Node-RED procesa la telemetría entrante, genera alertas automáticas, persiste el histórico en JSONL, ofrece un Dashboard Web en `/ui` y expone una API REST (`/api/status`, `/api/history`, `/api/command`).

---

## 2. Comparativa: `Proyecto_sin_Vibecoding` vs. `Proyecto_con_Vibecoding`

| Dimensión Técnica | Versión Manual (`sin_Vibecoding`) | Versión Asistida (`con_Vibecoding`) | Impacto / Mejora |
| --- | --- | --- | --- |
| **Estructura Embebido** | Archivo monolítico `.ino` con variables globales y flags booleanos (`forzar_riego`). | Modular en C++ (`Config.h`, `Types.h`, `main.cpp`) con structs tipados. | Elimina condiciones de carrera por flags globales y mejora legibilidad. |
| **Temporización FreeRTOS** | 2 tareas dedicadas (`2048` bytes c/u) suspendidas con `ulTaskNotifyTake` + `vTaskDelay`. | **Software Timers** de FreeRTOS (`xTimerCreate`) compartiendo la daemon task. | Ahorro de ~4 KB de memoria RAM en pila (stack) del ESP32. |
| **Trazabilidad de Eventos** | La cola solo transporta un `enum` genérico (`Necesita_riego`); se pierde el origen. | La cola transporta `EventMessage { type, param, origin }`. | Auditoría exacta en consola, Edge-Cloud y app móvil sobre qué causó cada transición. |
| **Capa Edge-Cloud** | Sin flujo Node-RED ni persistencia configurada. | Stack Docker (`Node-RED` + `Mosquitto`), Dashboard `/ui`, alertas Edge, log JSONL y API REST. | Observabilidad histórica, control web independiente de Android y soporte on-premise. |
| **Concurrencia en Android** | `StrictMode.permitAll()` + `Thread.sleep(1000)` en el Hilo Principal. | `ExecutorService` dedicado en background; cero bloqueos en UI Thread. | Elimina riesgo de cierres por *Application Not Responding* (ANR). |
| **Comunicación UI Android** | `BroadcastReceiver` + `IntentFilter` + parsing JSON acoplado en la Activity. | Patrón **Repository + `LiveData` + `ViewBinding`** con modelo `TelemetryState`. | Desacoplamiento total, actualización reactiva y consciente del ciclo de vida. |
| **Sensor Móvil (`TYPE_LIGHT`)** | Activity vacía sin implementación. | `LightOverrideActivity` con barra visual, debounce de 800 ms y restauración automática (`luz:-1`). | Integración bidireccional real entre el hardware del smartphone y el ESP32. |
