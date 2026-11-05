#include "app_rtos.h"

void Act_alarme(bool on){
	Actions action;
	action.alarme_valide = true;
	action.act_alarme = on;
	xQueueSend(ActionsHandle, &action, pdMS_TO_TICKS(100));
}

Heure Set_alarme(Heure base,int delta){
	Heure alarme = base;
	int total = base.h*60 + base.min + delta;
	alarme.h = total / 60;
	alarme.min = total % 60;
	alarme.sec = base.sec;
	return alarme;
}

// TACHES //

void task_gestion_alarme(void *pvParameters){
	Heure alarme_base;
	bool snooze_actif = false;
	while(1){
		xSemaphoreTake(sem_alarmeHandle, portMAX_DELAY); // On bloque jusqu'au signla activer_alarme

		xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY); // Sauvegarde alarme de base
		if(snooze_actif == false){
			alarme_base = consignes.alarme;
		}

		consignes.stop_alarme = false;
		consignes.snooze_alarme = false;
		xSemaphoreGive(mtx_consignesHandle);

		Act_alarme(true);

		// Attente stop ou snooze
		bool fini = false;
		while(fini != true){
			bool stop,snooze;
			xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
			stop = consignes.stop_alarme;
			snooze = consignes.snooze_alarme;
			xSemaphoreGive(mtx_consignesHandle);

			if(stop == true){
				xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
				if(snooze_actif == true){// Reconduite du reveil au lendemain
					consignes.alarme = alarme_base;
				}
				consignes.activer_alarme = false;
				consignes.stop_alarme = false;
				xSemaphoreGive(mtx_consignesHandle);
				snooze_actif = false;
				fini = true;
			}
			else if(snooze == true){
				xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
				consignes.alarme = Set_alarme(alarme_base,SNOOZE_MIN);
				consignes.activer_alarme = false;
				consignes.stop_alarme = false;
				xSemaphoreGive(mtx_consignesHandle);
				snooze_actif = true;
				fini = true;
			}
			else{
				vTaskDelay(pdMS_TO_TICKS(50)); // Contuniation de la mélodie
			}
		}
		Act_alarme(false);
	}
}
