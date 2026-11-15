# Capa Edge / Cloud - NexoRiego (Grupo 05)

Esta carpeta contiene la infraestructura **Edge-Cloud** del proyecto `Proyecto_con_Vibecoding`, encargada de centralizar la telemetría MQTT, evaluar reglas de alerta en el borde, persistir el histórico de lecturas y exponer tanto un **Dashboard Web** como una **API REST** para supervisión y control remoto.

## Componentes (`Proyecto_con_Vibecoding/`)

- **`docker-compose.yml`**: Orquesta el gateway **Node-RED** (puerto `1880`) y un broker **Eclipse Mosquitto** local (puertos `1883` / `9001`) para pruebas en laboratorio sin salida a Internet.
- **`node-red/Dockerfile` y `node-red/flows.json`**: Imagen preconfigurada con `node-red-dashboard` y el flujo completo de NexoRiego:
  - Suscripción MQTT a `soa/grupo05/nexoriego/sensores` (`tcp://broker.emqx.io:1883` por defecto).
  - Enriquecimiento de telemetría con timestamp ISO-8601 y detección de alertas Edge (`Falta de agua`, sequía `< 25%`, estrés térmico `> 32 °C`).
  - Persistencia en disco en `/data/telemetry_history.jsonl`.
  - Dashboard Web en `http://localhost:1880/ui`.
  - API REST:
    - `GET /api/status`: Última telemetría y alertas activas.
    - `GET /api/history`: Últimas 100 muestras almacenadas en memoria.
    - `POST /api/command`: Envío de comandos (`{"cmd": "regar"}`, `{"cmd": "reset"}`, `{"cmd": "luz:850"}`, `{"cmd": "luz:-1"}`) al tópico `soa/grupo05/nexoriego/comandos`.
- **`mosquitto/config/mosquitto.conf`**: Configuración del broker local para despliegue on-premise.

## Contrato MQTT

### 1. Telemetría (`soa/grupo05/nexoriego/sensores`)
Publicado por el ESP32 cada 3 segundos:
```json
{
  "estado": "Riego",
  "motivo": "APP_BOTON_MANUAL",
  "humedad": 42,
  "temperatura": 31.5,
  "luz": 840,
  "fuente_luz": "ANDROID",
  "regadas": 1,
  "max_regadas": 2
}
```

### 2. Comandos (`soa/grupo05/nexoriego/comandos`)
Publicado por Android o Node-RED y consumido por el ESP32:
- `regar`: Dispara riego manual inmediato.
- `reset`: Restablece el tanque cuando el sistema está en `Falta de agua`.
- `luz:<entero>`: Reemplaza el LDR físico del ESP32 con el valor de luz del smartphone (`>= 0`).
- `luz:-1`: Devuelve el control sensorial al LDR físico del ESP32.

## Ejecución Rápida

```bash
cd Edge-Cloud/Proyecto_con_Vibecoding
docker compose up -d --build
```

- Si el puerto `1880` está ocupado en el host (por ejemplo por port-forwarding de VS Code), podés usar otro puerto: `NODERED_PORT=1881 docker compose up -d --build`.
- El broker **Mosquitto** local es opcional (perfil `local-broker`) para pruebas sin Internet: `docker compose --profile local-broker up -d --build`.

Luego abrir `http://localhost:1880/ui` (o el puerto elegido) para el Dashboard o `http://localhost:1880` para inspeccionar el flujo en Node-RED.
