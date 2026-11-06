/**
 * @file    ventilateur.c
 * @brief   Ventilateur : commandes moteur (PWM TIM2), ordres de rafraîchissement/aération.
 * @note    Projet Réveil simulateur d'aube - Mines Saint-Étienne (M. Diallo / A. Stachnik)
 *          Code issu du main.c d'origine : les fonctions sont reprises à l'identique,
 *          seuls des commentaires ont été ajoutés.
 */

#include "app_rtos.h"

/* ----------------------------------------------------------------------
 * Fonctions Ventilateur
 * ---------------------------------------------------------------------- */

/**
 * Démarre le moteur (PWM TIM2_CH1, DIS à l'état bas).
 */
void Vent_Marche(){
	uint16_t pot = ADC_Lire(1);
	//__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_1,(uint32_t)pot * 3999 / 4095);
	__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_1,2999);
	HAL_GPIO_WritePin(DIS_GPIO_Port, DIS_Pin, GPIO_PIN_RESET); // activation du moteur
}

/**
 * Arrête le moteur (PWM à 0, DIS à l'état haut).
 */
void Vent_Arret(){
	__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_1,0);
	HAL_GPIO_WritePin(DIS_GPIO_Port, DIS_Pin, GPIO_PIN_SET); // désactivation du moteur
}

/**
 * Envoie un ordre de ventilation à task_interface_actions via la queue ActionsHandle.
 */
void Act_ventilateur(bool rafraichir, bool balayer){
	Actions action = {0};
	action.ventilo_valide = true;
	action.act_rafraichissement = rafraichir;
	action.act_balayage = balayer;
	xQueueSend(ActionsHandle, &action, pdMS_TO_TICKS(100));
}

/* ----------------------------------------------------------------------
 * Tâche gestion du ventilateur
 * ---------------------------------------------------------------------- */

/**
 * Tâche ventilateur : compare température/qualité d'air (Peek sur InfosHandle) aux seuils
 * et n'envoie un ordre que si l'état change.
 */
void task_gestion_ventilateur(void *pvParameters){
	Infos info = {0};
	float    seuil_temp;
	uint16_t seuil_qa;
	bool dernier_rafraichir = false, dernier_aerer = false;
	bool premier = true;                      // force le premier envoi

	while(1){
		bool mesure_ok = (xQueuePeek(InfosHandle, &info, 0) == pdPASS);

		if(mesure_ok){
			xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
			seuil_temp = consignes.seuil_temperature;
			seuil_qa   = consignes.seuil_qa;
			xSemaphoreGive(mtx_consignesHandle);

			bool rafraichir = (info.temperature > seuil_temp);
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

