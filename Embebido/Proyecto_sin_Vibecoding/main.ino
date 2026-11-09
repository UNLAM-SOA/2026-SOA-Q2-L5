#include <WiFi.h>
#include <DHT.h>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <PubSubClient.h>


// Definición de pines
#define PIN_HUMEDAD 34  // Potenciómetro (Humedad del suelo)
#define PIN_LUZ     35  // LDR (Luz ambiental)
#define PIN_DHT     14  // Sensor DHT22 (Temperatura)
#define PIN_RESET   26  // Botón de Reset
#define PIN_BOMBA   19  // Salida/LED para la bomba de agua
#define PIN_RED 2
#define PIN_GREEN 4
#define PIN_BLUE 5
#define PIN_BUZZER 15
#define DHTTYPE DHT22

#define HUMEDAD_MIN 4096 - (4096 * 0.4) //40
#define HUMEDAD_MED 4096 * 0.5 // 50
#define TEMPERATURA_ALTA 30
#define LUZ_ALTA 715
#define TIEMPO_TIMER_RIEGO 10000
#define TIEMPO_TIMER_ABSORCION 10000  
#define MAX_REGADAS 2
#define PRIORIDAD 1
#define SIZE_QUEUE 10


enum Estado {
  Monitoreo,
  Falta_agua,
  Riego,
  Absorcion
};

enum Evento {
  Poca_agua,
  Reset_presionado,
  Necesita_riego,
  Timeout1,
  Timeout2,
  CONTINUE
};

DHT dht(PIN_DHT, DHTTYPE);
Estado estadoActual = Monitoreo;
Evento eventoActual = CONTINUE; 

// Credenciales WiFi (Wokwi) y MQTT (EMQX)
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.emqx.io";

WiFiClient espClient;
PubSubClient client(espClient);

// Nombres de los temas (topics)
const char* topic_sensores = "soa/grupo05/nexoriego/sensores";
const char* topic_comandos = "soa/grupo05/nexoriego/comandos";


// Creo los handles para luego apuntar a las tareas
TaskHandle_t handleTiempoRiego = NULL;
TaskHandle_t handleTiempoAbsorcion = NULL; 
QueueHandle_t colaEventos;
SemaphoreHandle_t mutexContador = NULL;

// Prototipos de funciones 
void FSM(Evento eventoActual);
bool verificarRegadas();
bool verificarSensoresSueloTemp();
void encenderBomba();
void detenerBomba();
void prenderAzul();
void prenderRojo();
void prenderVerde();
void prenderBuzzer();
void apagarBuzzer();
void setup_wifi();
void callback(char* toic, byte* , unsigned int );
void reconnect();


int contador_regada=0;
bool forzar_riego = false; 

void setup_wifi() {
  delay(10);
  Serial.print("Conectando a WiFi ");
  
  // 1. Forzar modo estación (cliente)
  WiFi.mode(WIFI_STA);
  // 2. Desconectar y limpiar configuraciones previas
  WiFi.disconnect(true);
  delay(100);

  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado");
}

void callback(char* topic, byte* payload, unsigned int length) {
  String mensaje = "";
  for (int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }
  Serial.print("Mensaje recibido ["); Serial.print(topic); Serial.print("]: ");
  Serial.println(mensaje);

  // Si recibo la orden desde MyMQTT, disparamos el riego en la FSM 
  if (mensaje == "regar") {
    forzar_riego = true; 
    Serial.println("Comando remoto: forzando riego.......");
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Intentando conexión MQTT...");
    String clientId = "ESP32-Riego-";
    clientId += String(random(0xffff), HEX); // ID aleatorio
    
    if (client.connect(clientId.c_str())) {
      Serial.println("Conectado al broker!");
      // Nos suscribimos al tema de comandos
      client.subscribe(topic_comandos);
    } else {
      Serial.print("Fallo, rc=");
      Serial.print(client.state());
      Serial.println(" reintentando en 5 segundos");
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }
}

void tareaTiempoRiego(void *parameter){
	Evento eventoActual = Timeout1; 
	while(1){
		//aca se frena la tarea hasta recibir el give
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		//realiza la accion cuando se cumple el tiempo determinado
		vTaskDelay(pdMS_TO_TICKS(TIEMPO_TIMER_RIEGO));
		//envio el evento a la cola 
		xQueueSend(colaEventos, &eventoActual, 10);
	}
}

void tareaTiempoAborcion(void *parameter){
	Evento eventoActual = Timeout2;
	while(1){
		//aca se frena la tarea hasta recibir el give
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		//realiza la accion cuando se cumple el tiempo determinado
		vTaskDelay(pdMS_TO_TICKS(TIEMPO_TIMER_ABSORCION));
		eventoActual = Timeout2;
		xQueueSend(colaEventos, &eventoActual, 10);
	}
}

void tareaFSM(void *parameter){
	Evento evento_recibido; 
	while(1){
		xQueueReceive(colaEventos,&evento_recibido, portMAX_DELAY);
		FSM(evento_recibido);
	}
}

void tareaGetEvent(void *parameter) {
	Evento eventoActual; 
	while(1){

		if(digitalRead(PIN_RESET) == HIGH){
			Evento evento_reset = Reset_presionado; 
			xQueueSend(colaEventos, &evento_reset,10);
			vTaskDelay(pdMS_TO_TICKS(500));
		}

		if(verificarRegadas()){
			eventoActual = Poca_agua;
		}
		else if(verificarSensoresSueloTemp()){
			eventoActual = Necesita_riego;
		}

		else{
			eventoActual = CONTINUE; 
		}		
		if(eventoActual != CONTINUE){
			xQueueSend(colaEventos, &eventoActual, 10);
		}
	
		vTaskDelay(pdMS_TO_TICKS(500));

	}	
}

bool verificarRegadas()
{
	xSemaphoreTake(mutexContador, portMAX_DELAY);
	if (contador_regada == MAX_REGADAS){	
		eventoActual = Poca_agua;
		xSemaphoreGive(mutexContador);
			return true;
	}
	xSemaphoreGive(mutexContador);
	return false;
	
}

bool verificarSensoresSueloTemp(){
  int humedadSuelo = analogRead(PIN_HUMEDAD);
  float temp = dht.readTemperature();
  int luz = analogRead(PIN_LUZ);

  // Imprimir por monitor serie para debug
  Serial.print("Humedad: "); Serial.print(humedadSuelo);
  Serial.print(" | Temp: "); Serial.print(temp);
  Serial.print("C | Luz: "); Serial.println(luz);

  if(forzar_riego == true){
    forzar_riego = false; 
    return true; 
  }

  if(humedadSuelo < HUMEDAD_MIN){
    return true;
  }
  if(humedadSuelo < HUMEDAD_MED && temp > TEMPERATURA_ALTA && luz > LUZ_ALTA){
    return true;
  }
  return false; 
}

void encenderBomba() {
  digitalWrite(PIN_BOMBA, HIGH);
}

void detenerBomba() {
  digitalWrite(PIN_BOMBA, LOW);
}

void prenderBuzzer(){
	digitalWrite(PIN_BUZZER, LOW); 
}

void apagarBuzzer(){
	digitalWrite(PIN_BUZZER, HIGH);
}

void prenderAzul() {
	    digitalWrite(PIN_RED, LOW);
      digitalWrite(PIN_GREEN, LOW);
      digitalWrite(PIN_BLUE, HIGH);
}

void prenderVerde() {
	  digitalWrite(PIN_RED, LOW);
    digitalWrite(PIN_GREEN, HIGH);
    digitalWrite(PIN_BLUE, LOW);
}

void prenderRojo() {
	  digitalWrite(PIN_RED, HIGH);
    digitalWrite(PIN_GREEN, LOW);
    digitalWrite(PIN_BLUE, LOW);
}

void prenderAmarillo(){
  digitalWrite(PIN_RED, HIGH);
  digitalWrite(PIN_GREEN, HIGH);
  digitalWrite(PIN_BLUE, LOW);
}

void FSM(Evento eventoActual){	

	switch(estadoActual){
		case Monitoreo:
			switch(eventoActual){
				case Poca_agua:
					prenderRojo();
					Serial.println(">>> TRANSICION: Se quedo sin agua. Presionar el boton de RESET");
					prenderBuzzer();
					estadoActual = Falta_agua;
					break; 

				case Necesita_riego: 
					encenderBomba();
					prenderAzul();
					estadoActual = Riego;
					Serial.println(">>> TRANSICIÓN: Entrando a RIEGO. Bomba ENCENDIDA.");
					//mando la señal al handle de la tarea para que avance 
					
					if(handleTiempoRiego != NULL){
						xTaskNotifyGive(handleTiempoRiego);
					}
					break;

				case CONTINUE:
					break;
			}
			break; 
		
		case Falta_agua:
			switch (eventoActual){
				case Reset_presionado:
					apagarBuzzer();
					xSemaphoreTake(mutexContador,portMAX_DELAY);
					contador_regada = 0;
					xSemaphoreGive(mutexContador);
					estadoActual = Monitoreo; 
					break;
				
				case CONTINUE:
					break;
				}
				break;
		
		case Riego:	
			switch(eventoActual)
			{
				case(Timeout1):
					detenerBomba();
					xSemaphoreTake(mutexContador,portMAX_DELAY);
					contador_regada++;
					xSemaphoreGive(mutexContador);
					prenderVerde();
					estadoActual = Absorcion; 
					Serial.println(">>> TRANSICIÓN: Entrando a ABSORCIÓN. Bomba APAGADA.");
					//mando la señal al handle de la tarea para que avance 
					if(handleTiempoAbsorcion != NULL){
						xTaskNotifyGive(handleTiempoAbsorcion);
					}
					break;
				
				case(CONTINUE):
					break;
			}
			break; 
		
		case Absorcion:
			switch(eventoActual){
				case(Timeout2):
					prenderAmarillo();
					estadoActual = Monitoreo;
					break;

				case(CONTINUE):
					break;
			}
			break; 

	}
}

String estadoToString(Estado estado) {
  switch(estado) {
    case Monitoreo:   return "Monitoreo";
    case Falta_agua:  return "Falta de agua";
    case Riego:       return "Riego";
    case Absorcion:   return "Absorcion";
    default:          return "Desconocido";
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin(); 

	pinMode(PIN_BOMBA, OUTPUT);
	pinMode(PIN_RESET, INPUT);
	pinMode(PIN_GREEN, OUTPUT);
	pinMode(PIN_RED, OUTPUT);
	pinMode(PIN_BLUE, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
	apagarBuzzer();
	detenerBomba();

	prenderAmarillo();

	setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

	colaEventos = xQueueCreate(SIZE_QUEUE, sizeof(Evento));

	mutexContador = xSemaphoreCreateMutex();
  xTaskCreate(tareaTiempoRiego, "Tarea_Tiempo_Riego", 2048, NULL, PRIORIDAD, &handleTiempoRiego);
  xTaskCreate(tareaTiempoAborcion, "Tarea_Tiempo_Absorcion", 2048, NULL, PRIORIDAD, &handleTiempoAbsorcion);
  
	xTaskCreate(tareaGetEvent, "GetEvent", 2048, NULL, 1, NULL);
  xTaskCreate(tareaFSM, "FSM", 2048, NULL, 1, NULL);

}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // Mantiene la conexión MQTT viva y revisa mensajes entrantes

  // Publicar el estado de los sensores cada 5 segundos
  static unsigned long lastMsg = 0;
  unsigned long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;
    
    // preparo el mensaje
    String payload = "{\"temperatura\":\"" + String(dht.readTemperature()) + "\"," +
                 "\"humedad\":\"" + String(analogRead(PIN_HUMEDAD)) + "\"," +
                 "\"luz\":\"" + String(analogRead(PIN_LUZ)) + "\"," +
                 "\"estado\":\"" + estadoToString(estadoActual) + "\"}";
                     
    client.publish(topic_sensores, payload.c_str());
  }

  // Ceder tiempo a otras tareas
  vTaskDelay(pdMS_TO_TICKS(50)); 
}
		

