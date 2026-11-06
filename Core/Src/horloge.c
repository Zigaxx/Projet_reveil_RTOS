/**
 * @file    horloge.c
 * @brief   Horloge temps réel (1 Hz), comparaison avec l'alarme, mise à jour de l'écran.
 * @note    Projet Réveil simulateur d'aube - Mines Saint-Étienne (M. Diallo / A. Stachnik)
 *          Code issu du main.c d'origine : les fonctions sont reprises à l'identique,
 *          seuls des commentaires ont été ajoutés.
 */

#include "app_rtos.h"

/* ----------------------------------------------------------------------
 * Fonctions pour horloge et alarme
 * ---------------------------------------------------------------------- */

/**
 * Incrémente l'heure d'une seconde (avec retenue sec -> min -> h, retour à 0 à 24 h).
 * @param h Pointeur sur l'heure à incrémenter
 */
void Heure_Incrementer(Heure *h)
{
	h->sec++;
	if(h->sec >= 60)
	{
		h->sec = 0;
		h->min++;
	}
	if(h->min >= 60)
	{
		h->min = 0;
		h->h++;
	}
	if(h->h >= 24)
	{
		h->h = 0;
	}
}

/**
 * Compare deux heures (h, min, sec).
 * @return true si les deux heures sont identiques
 */
bool Heure_Egales(Heure a, Heure b){
	if( (a.h == b.h) && (a.min == b.min) && (a.sec == b.sec)){
		return true;
	}
	return false;
}

/**
 * Envoie une demande d'affichage écran à la tâche task_interface_aff via la queue AffHandle.
 * @param h Heure courante
 * @param sel Sélection (0 alarme, 1 temp, 2 QA, 3 QS)
 * @param valeur Valeur à afficher selon sel
 * @param retro Rétroéclairage (true = fond blanc)
 */
void Aff_ecran(Heure h, uint16_t sel, uint16_t valeur, bool retro){
	Affichages aff;
	aff.ecran_heure = h;
	aff.ecran_sel = sel;
	aff.ecran_valide = true;
	aff.ecran_retro = retro;
	aff.ecran_valeur = valeur;
	xQueueSend(AffHandle, &aff, pdMS_TO_TICKS(10));   // si pleine, la MAJ de la seconde suivante rattrapera
}

/* ----------------------------------------------------------------------
 * Tâche horloge (priorité la plus haute, période 1 s)
 * ---------------------------------------------------------------------- */

/**
 * Tâche horloge : incrémente l'heure chaque seconde (vTaskDelayUntil), déclenche l'alarme
 * quand l'heure courante égale l'heure d'alarme (donne sem_alarmeHandle), met à jour le
 * rétroéclairage selon la luminosité (Peek sur InfosHandle) et rafraîchit l'écran.
 */
void task_gestion_horloge(void *pvParameters){
	TickType_t dernier = xTaskGetTickCount();
	Infos infos = {0} ;

	while(1){
		vTaskDelayUntil(&dernier, pdMS_TO_TICKS(PERIODE_HORLOGE));

		// Incrementation heure
		Heure h;
		xSemaphoreTake(HeureMutexSemaphHandle, portMAX_DELAY);
		Heure_Incrementer(&heure);
		h = heure; // Copie locale pour la comparaison avec l'alarme
		xSemaphoreGive(HeureMutexSemaphHandle);

		bool mesure_ok = (xQueuePeek(InfosHandle, &infos, 0) == pdPASS); // dernières mesures (Peek, comme pour le ventilateur)

		// Alarme
		xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
		bool declencher = Heure_Egales(h, alarme) && !consignes.activer_alarme;
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
		uint8_t valeur = 0;
		switch(sel){
		case 1: valeur = infos.temperature; break;
		case 2: valeur = infos.qualite_air; break;
		case 3: valeur = infos.qualite_sommeil; break;
		default: break;
		}
		// Actualisation ecran
		Aff_ecran(h, sel, valeur, retro);
	}
}

