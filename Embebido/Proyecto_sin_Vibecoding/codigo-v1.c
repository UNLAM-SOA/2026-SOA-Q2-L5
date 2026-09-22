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
#define TIEMPO_TIMER_RIEGO 10000
#define TIEMPO_TIMER_ABSORCION 300000  
#define PRIORIDAD 1
#define PRIORIDAD 2
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
Estado estadoActual = Monintoreando;
Evento eventoActual = CONTINUE; 

//creo la cola 
QueueHandle_t colaEventos = xQueueCreate(SIZE_QUEUE, sizeof(Evento));

int contador_regada=0;


//Handler del timer por HW
void tareaTiempoRiego(void *parameter){
	Evento eventoActual = Timeout1; 
	while(1){
		//realiza la accion cuando se cumple el tiempo determinado
		vTaskDelay(pdMS_TO_TICKS(TIEMPO_TIMER_RIEGO));
		//envio el evento a la cola 
		xQueueSend(colaEventos, &eventoActual, 10);
	}
}

void tareaTiempoAborcion(void *parameter){
	Evento eventoActual = Timeout2;
	while(1){
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

void tareaGetEvent(void *parameter) 
{
	Evento eventoActual; 
	while(1){
		if(verficarRegadas()){
		eventoActual = Falta_agua;
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
		
		vTaskDelay(pdMS_TO_TICKS(TIEMPO_TIMER_RIEGO));

	}	
}

bool verificarRegadas()
{
	//dentro de del getevetn deben generar verificar el contador 
	//y generar el evento Poca_agua
	if (contador_regada==10)
	{	
		eventoActual = Poca_agua;
		return true;
	}
	return false;
	
}

bool verificarSensoresSuelotemp()
{
	while(1){
		if(analogRead(PIN_HUMEDAD) < HUMEDAD_MIN){
		return true;
		}
		if(analogRead(PIN_HUMEDAD) < HUMEDAD_MED && dht.readTemperature() > TEMPERATURA_ALTA &&analogRead(PIN_LUZ) > 715){
			return true;
		}
	}
	
	return false; 

}

void encenderBomba() {
  digitalWrite(PIN_BOMBA, HIGH);
}

void detenerBomba() {
  digitalWrite(PIN_BOMBA, LOW);
}


void FSM(Evento eventoActual)
{	
	switch(estadoActual):
	{
		case Monitoreo:
			switch(eventoActual){
				case Poca_agua:
					estadoActual = Falta_agua;
					break; 

				case Necesita_riego: 
					encenderBomba();
					estadoActual = Riego;
					tareaTiempoRiego();
					break;

				case CONTINUE:
					break;
			}
			break; 
		
		case Falta_agua:
			switch (eventoActual){
				case Reset_presionado:
					estadoActual = Monitoreo; 
					break;
				
				case CONTINUE:
					break;
				}
		
		case Riego:	
			switch(eventoActual)
			{
				case(Timeout1):
					detenerBomba();
					contador_regada++;
					estadoActual = Absorcion; 
					break;
				
				case(CONTINUE):
					break;
			}
		
		case Absorcion:
			switch(eventoActual){
				case(Timeout2):
					estadoActual = Monitoreo;
					break;

				case(CONTINUE):
					break;
			}

	}
}


void setup() {
  Serial.begin(115200);
  dht.begin(); 
  xTaskCreate(tareaTiempoRiego, "Tarea_Tiempo_Riego", TAM_PILA, NULL, PRIORIDAD, NULL);
  xTaskCreate(tareaTiempoAborcion, "Tarea_Tiempo_Absorcion", TAM_PILA, NULL, PRIORIDAD, NULL);
  xTaskCreate(tareaGetEvent, "GetEvent", 2048, NULL, 1, NULL);
  xTaskCreate(tareaFSM, "FSM", 2048, NULL, 1, NULL);

}
		

