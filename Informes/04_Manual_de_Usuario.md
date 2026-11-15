# Manual de Usuario - NexoRiego IoT

## 1. Ejecución del Sistema Embebido en DevContainer (Wokwi)

1. Abrir la carpeta raíz `2026-SOA-Q2-L5` en Visual Studio Code.
2. Seleccionar **Reopen in Container** cuando VS Code lo solicite. El contenedor instalará PlatformIO y compilará automáticamente el proyecto (`pio run -d Embebido/Proyecto_con_Vibecoding`).
3. Abrir `Embebido/Proyecto_con_Vibecoding/diagram.json` y hacer clic en **Start Simulation (Play)**.
4. En el Monitor Serie se visualizará la conexión WiFi, la suscripción al broker MQTT y los logs periódicos de sensores.

### Despliegue en Placa Física ESP32

1. En `Embebido/Proyecto_con_Vibecoding/include/Config.h`, cambiar `#define MODO_WOKWI true` a `false` y completar `WIFI_SSID` y `WIFI_PASSWORD`.
2. Ejecutar `pio run -d Embebido/Proyecto_con_Vibecoding -t upload` desde la terminal del contenedor.

---

## 2. Despliegue de la Capa Edge-Cloud (Node-RED + Mosquitto)

1. Abrir una terminal en `Edge-Cloud/Proyecto_con_Vibecoding` y ejecutar:
   ```bash
   docker compose up -d --build
   ```
2. Acceder al **Dashboard Web** en `http://localhost:1880/ui` para visualizar métricas en vivo, alertas activas y accionar botones de control remoto.
3. Acceder al **Editor Node-RED** en `http://localhost:1880` para inspeccionar el flujo o cambiar el broker a `Mosquitto Local (Docker)` si se opera en una red aislada.
4. Consultar la **API REST** del Edge:
   - `GET http://localhost:1880/api/status`
   - `GET http://localhost:1880/api/history`
   - `POST http://localhost:1880/api/command` con body `{"cmd": "regar"}`

---

## 3. Operación de la Aplicación Android (`NexoRiego IoT`)

1. Abrir `Android/Proyecto_con_Vibecoding` en Android Studio y ejecutar la aplicación en un dispositivo físico Android.
2. **Panel Principal (`DashboardActivity`):**
   * El indicador superior mostrará **MQTT Online** en verde al conectarse al broker.
   * La tarjeta superior indica el **Estado de la FSM** (cambiando de color según el estado) y la línea **Último evento**, permitiendo auditar si el riego fue automático o manual.
   * **Regar Ahora:** Envía `"regar"` y activa la bomba durante 10 segundos.
   * **Reset Tanque:** Envía `"reset"` para silenciar la alarma y restablecer los ciclos cuando el sistema entra en `Falta de agua`.
   * **Comando MQTT:** Campo libre para enviar órdenes manuales (`regar`, `reset`, `luz:950`).
3. **Delegación del Sensor de Luz (`LightOverrideActivity`):**
   * Presionar **Delegar Sensor de Luz al Smartphone**.
   * El teléfono leerá su sensor ambiental (`Sensor.TYPE_LIGHT`) y enviará el valor en lux al ESP32. En el panel principal (o monitor serie) la fuente de luz cambiará de `ESP32` a `ANDROID`.
   * Si la luminosidad supera `715 lx` con humedad `< 50%` y temperatura `> 30 °C`, el ESP32 iniciará el riego automáticamente.
   * Al presionar **Finalizar Override y Volver**, la aplicación envía `"luz:-1"` y el ESP32 vuelve a utilizar su sensor LDR físico.
