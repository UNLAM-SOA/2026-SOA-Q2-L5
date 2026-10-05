#include <DHT.h>
#include <Arduino.h>

// Definición de pines
#define PIN_HUMEDAD 34  // Potenciómetro (Humedad del suelo)
#define PIN_LUZ     35  // LDR (Luz ambiental)
#define PIN_DHT     14  // Sensor DHT22 (Temperatura)
#define PIN_RESET   26  // Botón de Reset
#define PIN_BOMBA   27  // Salida/LED para la bomba de agua
#define DHTTYPE DHT22

#define HUMEDAD_MIN 40
#define HUMEDAD_MED 50
#define TEMPERATURA_ALTA 30
#define LUZ_ALTA 715
#define TIEMPO_TIMER_RIEGO 1000
#define TIEMPO_TIMER_ABSORCION 30000  
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

// Prototipos de funciones 
void FSM(Evento eventoActual);
bool verificarRegadas();
bool verificarSensoresSueloTemp();
void encenderBomba();
void detenerBomba();


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
	//dentro de del getevetn deben generar verificar el contador 
	//y generar el evento Poca_agua
	if (contador_regada == 10){	
		eventoActual = Poca_agua;
			return true;
	}
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


void FSM(Evento eventoActual){	

	switch(estadoActual){
		case Monitoreo:
			switch(eventoActual){
				case Poca_agua:
					estadoActual = Falta_agua;
					break; 

				case Necesita_riego: 
					encenderBomba();
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
				contador_regada = 0;
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
					contador_regada++;
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
	detenerBomba();

	colaEventos = xQueueCreate(SIZE_QUEUE, sizeof(Evento));

  xTaskCreate(tareaTiempoRiego, "Tarea_Tiempo_Riego", 2048, NULL, PRIORIDAD, &handleTiempoRiego);
  xTaskCreate(tareaTiempoAborcion, "Tarea_Tiempo_Absorcion", 2048, NULL, PRIORIDAD, &handleTiempoAbsorcion);
  xTaskCreate(tareaGetEvent, "GetEvent", 2048, NULL, 1, NULL);
  xTaskCreate(tareaFSM, "FSM", 2048, NULL, 1, NULL);

}

void loop(){
	if(digitalRead(PIN_RESET) == LOW){
		Evento evento_reset = Reset_presionado; 
		xQueueSend(colaEventos, &evento_reset,10);
		vTaskDelay(pdMS_TO_TICKS(500));
	}
	vTaskDelay(pdMS_TO_TICKS(100));
}
		

