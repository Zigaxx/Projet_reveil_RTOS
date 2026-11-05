#include "app_rtos.h"

void aff_led(uint8_t niveau){
	Affichages aff;
	aff.led_valide = true;
	aff.led_niveau = niveau;
	xQueueSend(AffHandle, &aff, pdMS_TO_TICKS(100));
}

// Déclaration des fonctions

bool Alarme_active(){
	xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
	bool r = consignes.activer_alarme;
	xSemaphoreGive(mtx_consignesHandle);
	return r;
}

bool Crep_active(){
	xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
	bool r = consignes.activer_crepuscule;
	xSemaphoreGive(mtx_consignesHandle);
	return r;
}

void task_gestion_aff_crep_aube(void *pvParameters){

	while(1){
		if(Alarme_active()){
			uint32_t t = 0;

			while(Alarme_active()){
				if(t > DUREE_AUBE){
					t = DUREE_AUBE;
				}
				aff_led((uint8_t)((t * 255) / DUREE_AUBE));
				vTaskDelay(pdMS_TO_TICKS(PERIODE_MAJ_LED));
				t += PERIODE_MAJ_LED;
			}
			aff_led(0);
		}
		else if(Crep_active()){
			uint32_t t = 0;

			while(t <= DUREE_CREP && Crep_active() && !Alarme_active()){
				aff_led((uint8_t)(255 - (t * 255) / DUREE_CREP));
				vTaskDelay(pdMS_TO_TICKS(PERIODE_MAJ_LED));
				t += PERIODE_MAJ_LED;
			}
			aff_led(0);
			xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
			consignes.activer_crepuscule = false;
			xSemaphoreGive(mtx_consignesHandle);
		}
		else{
			vTaskDelay(pdMS_TO_TICKS(PERIODE_MAJ_LED)); // Contuniation de la mélodie
		}
	}
}
