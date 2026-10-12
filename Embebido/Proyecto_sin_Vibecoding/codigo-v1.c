#include <DHT.h>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Definición de pines
#define PIN_HUMEDAD 34  // Potenciómetro (Humedad del suelo)
#define PIN_LUZ     35  // LDR (Luz ambiental)
#define PIN_DHT     14  // Sensor DHT22 (Temperatura)
#define PIN_RESET   26  // Botón de Reset
#define PIN_BOMBA   19  // Salida/LED para la bomba de agua
#define PIN_RED 2
#define PIN_GREEN 4
#define PIN_BLUE 5
#define DHTTYPE DHT22

#define HUMEDAD_MIN 40
#define HUMEDAD_MED 50
#define TEMPERATURA_ALTA 30
#define LUZ_ALTA 715
#define TIEMPO_TIMER_RIEGO 10000
#define TIEMPO_TIMER_ABSORCION 10000  
#define MAX_REGADAS 3
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
void setColor(int, int, int);


int contador_regada=0;

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

void setColor(int r, int g, int b) {

  analogWrite(PIN_RED, 255 - r);   
  analogWrite(PIN_GREEN, 255 - g);
  analogWrite(PIN_BLUE, 255 - b);
}

void prenderAzul() {
	  setColor(0, 0, 255);
}

void prenderVerde() {
	  setColor(0, 255, 0);
}

void prenderRojo() {
	  setColor(255, 0, 0);
}

void prenderAmarillo(){
	setColor(255,255,0);
}

void FSM(Evento eventoActual){	

	switch(estadoActual){
		case Monitoreo:
			switch(eventoActual){
				case Poca_agua:
					prenderRojo();
					Serial.println(">>> TRANSICION: Se quedo sin agua. Presionar el boton de RESET");
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


void setup() {
  Serial.begin(115200);
  dht.begin(); 

	pinMode(PIN_BOMBA, OUTPUT);
	pinMode(PIN_RESET, INPUT_PULLUP);
	pinMode(PIN_GREEN, OUTPUT);
	pinMode(PIN_RED, OUTPUT);
	pinMode(PIN_BLUE, OUTPUT);
	detenerBomba();

	prenderAmarillo();
	colaEventos = xQueueCreate(SIZE_QUEUE, sizeof(Evento));

	mutexContador = xSemaphoreCreateMutex();
  xTaskCreate(tareaTiempoRiego, "Tarea_Tiempo_Riego", 2048, NULL, PRIORIDAD, &handleTiempoRiego);
  xTaskCreate(tareaTiempoAborcion, "Tarea_Tiempo_Absorcion", 2048, NULL, PRIORIDAD, &handleTiempoAbsorcion);
  
	xTaskCreate(tareaGetEvent, "GetEvent", 2048, NULL, 1, NULL);
  xTaskCreate(tareaFSM, "FSM", 2048, NULL, 1, NULL);

}

void loop(){

	vTaskDelay(pdMS_TO_TICKS(100));
}
		

