#include "app_rtos.h"

void Heure_Incrementer(Heure *h){
	h->sec++;
	if(h->sec >= 60){
		h->sec = 0;
		h->min++;
	}
	if(h->min >= 60){
		h->min = 0;
		h->h++;
	}
	if(h->h >= 24){
		h->h++;
	}
}

bool Heure_Egales(Heure a, Heure b){
	if( (a.h == b.h) && (a.min == b.min) && (a.sec == b.sec)){
		return true;
	}
	return false;
}

void Aff_ecran(Heure h, uint16_t sel, uint16_t valeur, bool retro){
	Affichages aff;
	aff.ecran_heure = h;
	aff.ecran_sel = sel;
	aff.ecran_valide = true;
	aff.ecran_retro = retro;
	aff.ecran_valeur = valeur;
	xQueueSend(AffHandle, &aff, pdMS_TO_TICKS(10));   // si pleine, la MAJ de la seconde suivante rattrapera
}

void task_gestion_horloge(void *pvParameters){
	TickType_t dernier = xTaskGetTickCount();
	Infos infos;

	while(1){
		vTaskDelayUntil(&dernier, pdMS_TO_TICKS(PERIODE_HORLOGE));

		// Incrementation heure
		Heure h;
		xSemaphoreTake(mtx_heureHandle, portMAX_DELAY);
		Heure_Incrementer(&heure);
		h = heure; // Copie locale pour la comparaison avec l'alarme
		xSemaphoreGive(mtx_heureHandle);

		bool mesure_ok = (xQueuePeek(InfosHandle, &infos, 0) == pdPASS); // dernières mesures (Peek, comme pour le ventilateur)

		// Alarme
		xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
		bool declencher = Heure_Egales(h, consignes.alarme) && !consignes.activer_alarme;
		if(declencher){
			consignes.activer_alarme = true; // Pour la gestion de l'aube
		}

		// Retroeclairage
		if(mesure_ok){
			consignes.activer_retroeclairage = (infos.luminosite > consignes.seuil_luminosite);
		}
		bool retro = consignes.activer_retroeclairage;
		uint8_t sel = consignes.sel_affichage;
		xSemaphoreGive(mtx_consignesHandle);

		if(declencher){
			xSemaphoreGive(sem_alarmeHandle); // reveil de la task de gestion d'alarme
		}
		// Selection affichage
		uint16_t valeur =0;
		switch(sel){
			case 1:
				valeur = infos.temperature;
				break;
			case 2:
				valeur = infos.qualite_air;
				break;
			case 3:
				valeur = infos.qualite_sommeil;
				break;
			default:
				valeur = 0 ;
				break;
		}
		// Actualisation ecran
		Aff_ecran(h, sel, valeur, retro);
	}
}
