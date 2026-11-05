#include "app_rtos.h"

void Act_ventilateur(bool rafraichir, bool balayer){
	Actions action = {0};
	action.ventilo_valide = true;
	action.act_rafraichissement = rafraichir;
	action.act_balayage = balayer;
	xQueueSend(ActionsHandle, &action, pdMS_TO_TICKS(100));
}

void task_gestion_ventilateur(void *pvParameters){
	Infos info;
	uint16_t seuil_temp, seuil_qa;

	while(1){
		// Attente de la commande manuel ou 5 minutes
		bool manuel = (xSemaphoreTake(sem_refreshHandle,pdMS_TO_TICKS(PERIODE_VENT_MS)) == pdTRUE);

		// Dernières mesures (Peek : ne vide pas la file, les autres tâches la lisent aussi)
		if(xQueuePeek(InfosHandle, &info, 0) != pdPASS){
			continue;   // pas encore de mesure
		}

		xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
		seuil_temp = consignes.seuil_temperature;
		seuil_qa = consignes.seuil_qa;
		xSemaphoreGive(mtx_consignesHandle);

		bool rafraichir = manuel || (info.temperature > seuil_temp);
		bool aerer = info.qualite_air > seuil_qa;
		Act_ventilateur(rafraichir,aerer);
	}
}
