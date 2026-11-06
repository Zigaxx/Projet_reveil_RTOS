/**
 * @file    alarme.c
 * @brief   Gestion de l'alarme : déclenchement, snooze, arrêt.
 * @note    Projet Réveil simulateur d'aube - Mines Saint-Étienne (M. Diallo / A. Stachnik)
 *          Code issu du main.c d'origine : les fonctions sont reprises à l'identique,
 *          seuls des commentaires ont été ajoutés.
 */

#include "app_rtos.h"

/* ----------------------------------------------------------------------
 * Fonctions Alarme
 * ---------------------------------------------------------------------- */

/**
 * Demande à task_interface_actions de lancer (on = true) ou couper (on = false) la mélodie.
 */
void Act_alarme(bool on){
	Actions action;
	action.alarme_valide = true;
	action.act_alarme = on;
	xQueueSend(ActionsHandle, &action, pdMS_TO_TICKS(100));
}

/**
 * Calcule une nouvelle heure d'alarme décalée de delta minutes (utilisé pour le snooze).
 */
Heure Set_alarme(Heure base,int delta){
	Heure alarme = base;
	int total = base.h*60 + base.min + delta;
	alarme.h = total / 60;
	alarme.min = total % 60;
	alarme.sec = base.sec;
	return alarme;
}

/* ----------------------------------------------------------------------
 * Tâche pour la gestion de l'alarme
 * ---------------------------------------------------------------------- */

/**
 * Tâche alarme : attend sem_alarmeHandle, lance la mélodie puis attend stop ou snooze.
 * Snooze = alarme décalée de SNOOZE_MIN ; stop = retour à l'heure d'alarme de base.
 */
void task_gestion_alarme(void *pvParameters){
	Heure alarme_base;
	bool snooze_actif = false;
	while(1)
	{
		xSemaphoreTake(sem_alarmeHandle, portMAX_DELAY); // On bloque jusqu'au signla activer_alarme

		xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY); // Sauvegarde alarme de base
		if(snooze_actif == false)
		{
			alarme_base = alarme;
		}

		consignes.stop_alarme = false;
		consignes.snooze_alarme = false;
		xSemaphoreGive(mtx_consignesHandle);

		Act_alarme(true);

		// Attente stop ou snooze
		bool fini = false;
		while(fini != true)
		{
			bool stop,snooze;
			xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
			stop = consignes.stop_alarme;
			snooze = consignes.snooze_alarme;
			xSemaphoreGive(mtx_consignesHandle);

			if(stop == true)
			{
				if(snooze_actif == true){// Reconduite du reveil au lendemain
					xSemaphoreTake(AlarmeMutexSemaphHandle,portMAX_DELAY);
					alarme = alarme_base;
					xSemaphoreGive(AlarmeMutexSemaphHandle);
				}
				xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
				consignes.activer_alarme = false;
				consignes.stop_alarme = false;
				xSemaphoreGive(mtx_consignesHandle);
				snooze_actif = false;
				fini = true;
			}
			else if(snooze == true)
			{
				xSemaphoreTake(AlarmeMutexSemaphHandle,portMAX_DELAY);
				alarme = Set_alarme(alarme,SNOOZE_MIN);
				xSemaphoreGive(AlarmeMutexSemaphHandle);
				xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
				consignes.activer_alarme = false;
				consignes.stop_alarme = false;
				xSemaphoreGive(mtx_consignesHandle);
				snooze_actif = true;
				fini = true;
			}
			else
			{
				vTaskDelay(pdMS_TO_TICKS(50)); // Contuniation de la mélodie
			}
		}
		Act_alarme(false);
	}
}

