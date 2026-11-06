#include "app_rtos.h"

void Act_ventilateur(bool rafraichir, bool balayer){
	Actions action = {0};
	action.ventilo_valide = true;
	action.act_rafraichissement = rafraichir;
	action.act_balayage = balayer;
	xQueueSend(ActionsHandle, &action, pdMS_TO_TICKS(100));
}

void task_gestion_ventilateur(void *pvParameters){
	Infos info = {0};
	float    seuil_temp;
	uint16_t seuil_qa;
	bool manuel_actif = false;
	bool dernier_rafraichir = false, dernier_aerer = false;
	bool premier = true;                      // force le premier envoi

	while(1){
		uint16_t pot = ADC_Lire(1);
				if(!manuel_actif && pot > 300)  manuel_actif = true;
				if( manuel_actif && pot < 150) manuel_actif = false;

		bool mesure_ok = (xQueuePeek(InfosHandle, &info, 0) == pdPASS);

		if(mesure_ok){
			xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
			seuil_temp = consignes.seuil_temperature;
			seuil_qa   = consignes.seuil_qa;
			xSemaphoreGive(mtx_consignesHandle);

			bool rafraichir =  manuel_actif || (info.temperature > seuil_temp);
			bool aerer      = (info.qualite_air > seuil_qa);

			if(premier || rafraichir != dernier_rafraichir || aerer != dernier_aerer){
				premier = false;
				dernier_rafraichir = rafraichir;
				dernier_aerer = aerer;
				printf("VENT: rafraichir=%d aerer=%d\r\n", rafraichir, aerer);
				Act_ventilateur(rafraichir, aerer);
			}
		}
		vTaskDelay(pdMS_TO_TICKS(PERIODE_VENT_MS));
	}
}
