/**
 * @file    if_sorties.c
 * @brief   Interfaces de sortie : buzzer (mélodies), LEDs WS2812 (DMA), écran OLED, actions ventilateur/alarme.
 * @note    Projet Réveil simulateur d'aube - Mines Saint-Étienne (M. Diallo / A. Stachnik)
 *          Code issu du main.c d'origine : les fonctions sont reprises à l'identique,
 *          seuls des commentaires ont été ajoutés.
 */

#include "app_rtos.h"

/* ----------------------------------------------------------------------
 * Mélodies
 * ---------------------------------------------------------------------- */

const Note mel_beep[] = { {2000,200}, {0,200} };
const Note mel_1[]    = { {523,200}, {659,200}, {784,200}, {1047,400}, {0,300} };

const Note mel_2[]    = {
		// Marche impériale - première phrase
		{392, 500}, {0, 150},  // Sol
		{392, 500}, {0, 150},  // Sol
		{392, 500}, {0, 150},  // Sol
		{311, 350}, {0, 150},  // Mib
		{466, 150}, {0, 75},   // Sib rapide
		{392, 500}, {0, 150},  // Sol
		{311, 350}, {0, 150},  // Mib
		{466, 150}, {0, 75},   // Sib rapide
		{392, 900}, {0, 150},  // Sol long

		// Deuxième phrase
		{587, 500}, {0, 150},  // Ré
		{587, 500}, {0, 150},  // Ré
		{587, 500}, {0, 150},  // Ré
		{622, 350}, {0, 150},  // Mib
		{466, 150}, {0, 75},   // Sib rapide
		{370, 500}, {0, 150},  // Fa#
		{311, 350}, {0, 150},  // Mib
		{466, 150}, {0, 75},   // Sib rapide
		{392, 900},            // Sol final

		{0, 500}               // Silence final
};

const Note mel_3[] = {
		{523, 300}, {587, 300}, {659, 300}, {523, 300}, // Frère Jacques, frère Jacques
		{523, 300}, {587, 300}, {659, 300}, {523, 300}, // Dormez-vous ? dormez-vous ?
		{659, 300}, {698, 300}, {784, 600},             // Sonnez les matines, sonnez les matines
		{659, 300}, {698, 300}, {784, 600},             // Sonnez les matines, sonnez les matines
		{523, 300}, {392, 300}, {523, 600},             // Din, dan, don, din, dan, don
		{0, 400} };

const Melodie melodies[] = {
		{ mel_beep, sizeof(mel_beep)/sizeof(Note) },    // 0 = beep
		{ mel_1,    sizeof(mel_1)/sizeof(Note)    },
		{ mel_2,    sizeof(mel_2)/sizeof(Note)    },
		{ mel_3,    sizeof(mel_3)/sizeof(Note)    },
};

uint16_t led_buffer[LED_BITS + LED_RESET_SLOTS];

/* ----------------------------------------------------------------------
 * Fonctions Buzzer
 * ---------------------------------------------------------------------- */

/**
 * Règle la fréquence du buzzer (TIM8_CH4, horloge 1 MHz). freq = 0 -> silence. Volume 50 %.
 */
void Buzzer_Note(uint16_t freq){
	if(freq == 0){
		__HAL_TIM_SET_COMPARE(&htim8,TIM_CHANNEL_4,0);
	}
	else{
		uint32_t arr = 1000000/freq -1;
		__HAL_TIM_SET_AUTORELOAD(&htim8,arr);
		__HAL_TIM_SET_COMPARE(&htim8,TIM_CHANNEL_4,arr/2); // On mets le volume à 50 %
	}
}

/**
 * Retourne le délai restant avant echeance (0 si dépassée), en ticks.
 */
TickType_t Delai(TickType_t echeance, TickType_t now){
	int d = echeance - now;
	if(d > 0){
		return (TickType_t)d;
	}
	return 0;
}

/* ----------------------------------------------------------------------
 * Fonctions Leds
 * ---------------------------------------------------------------------- */

/**
 * Génère la trame WS2812 (couleur ambre, courbe quadratique) et l'envoie en DMA sur TIM3_CH4.
 */
void Leds_Envoyer(uint8_t niveau){
	uint32_t n = ((uint32_t)niveau * niveau) / 255;
	uint32_t r = n;
	uint32_t g = (n * 150) / 255;
	uint32_t b = (n * 30) / 255;
	uint32_t grb = (g << 16) | (r << 8) | b;

	for (int i = 0; i < NB_LEDS; i++) {
		for (int j = 0; j < 24; ++j) {
			led_buffer[i*24 + j] = (grb & (1UL << (23 - j))) ? LED_T1 : LED_T0;
		}
	}

	for (int i = 0; i < LED_RESET_SLOTS; ++i) {
		led_buffer[LED_BITS + i] = 0;
	}

	HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_4, (uint32_t*)led_buffer, LED_BITS + LED_RESET_SLOTS);
	xSemaphoreTake(sem_ledHandle, pdMS_TO_TICKS(20));
	HAL_TIM_PWM_Stop_DMA(&htim3, TIM_CHANNEL_4);
}

/* ----------------------------------------------------------------------
 * Fonctions Affichage
 * ---------------------------------------------------------------------- */

/**
 * Affiche l'heure (ligne 1) et la valeur sélectionnée (ligne 2) sur l'OLED SSD1306.
 */
void Ecran_Afficher(const Affichages *a){
	char l1[16], l2[20];

	snprintf(l1, sizeof(l1), "%02u:%02u:%02u",(unsigned)a->ecran_heure.h, (unsigned)a->ecran_heure.min, (unsigned)a->ecran_heure.sec);

	switch (a->ecran_sel) {
	case 0:
		snprintf(l2, sizeof(l2), "%02u:%02u:%02u",(unsigned)alarme.h, (unsigned)alarme.min, (unsigned)alarme.sec);
		break;
	case 1:
		snprintf(l2, sizeof(l2), "Temp: %u C", (unsigned)a->ecran_valeur);
		break;
	case 2:
		snprintf(l2, sizeof(l2), "QA: %u ppm", (unsigned)a->ecran_valeur);
		break;
	case 3:
		snprintf(l2, sizeof(l2), "Sommeil: %u", (unsigned)a->ecran_valeur);
		break;
	default:
		l2[0] = '\0';
		break;
	}

	SSD1306_COLOR fond  = a->ecran_retro ? White : Black;
	SSD1306_COLOR texte = a->ecran_retro ? Black : White;

	ssd1306_Fill(fond);
	ssd1306_SetCursor(0, 0);
	ssd1306_WriteString(l1, Font_11x18, texte);
	ssd1306_SetCursor(0, 30);
	ssd1306_WriteString(l2, Font_11x18, texte);
	xSemaphoreTake(mtx_i2cHandle, portMAX_DELAY);
	ssd1306_UpdateScreen();
	xSemaphoreGive(mtx_i2cHandle);
}

/* ----------------------------------------------------------------------
 * Callback fin de trame DMA des LEDs
 * ---------------------------------------------------------------------- */

/**
 * Callback fin d'impulsion PWM TIM3 : donne sem_ledHandle (fin de trame DMA).
 */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim){
	if(htim->Instance == TIM3){
		BaseType_t woken = pdFALSE;
		xSemaphoreGiveFromISR(sem_ledHandle,&woken);
		portYIELD_FROM_ISR(woken);
	}
}

/* ----------------------------------------------------------------------
 * Tâches
 * ---------------------------------------------------------------------- */

/**
 * Tâche interface affichage : initialise l'OLED puis traite les ordres LEDs/écran de AffHandle.
 */
void task_interface_aff(void *pvParameters){
	Affichages aff;
	xSemaphoreTake(mtx_i2cHandle, portMAX_DELAY);
	ssd1306_Init();
	xSemaphoreGive(mtx_i2cHandle);

	while(1){
		if(xQueueReceive(AffHandle, &aff, portMAX_DELAY) == pdPASS){
			if(aff.led_valide){
				Leds_Envoyer(aff.led_niveau);
			}

			if(aff.ecran_valide){
				Ecran_Afficher(&aff);
			}
		}
	}
}

/**
 * Tâche interface actions : exécute les ordres de ActionsHandle (mélodie du buzzer
 * note par note, ventilateur et balayage avec l'encodeur TIM5).
 */
void task_interface_actions(void *pvParameters){
	Actions action;
	bool sonne = false, balaye = false;
	uint8_t mel, idx;
	int8_t sens = 1;
	TickType_t t_note, t_bal, t_inversion;
	uint32_t ref = 0; //reference pour le calcul du balayage

	while(1){
		// Attente prochaine action
		TickType_t now = xTaskGetTickCount();
		TickType_t wait = portMAX_DELAY;

		if(sonne){
			wait = Delai(t_note,now);
		}

		if(balaye){
			TickType_t d = Delai(t_bal,now);
			if(d < wait){
				wait = d;
			}
		}
		if(xQueueReceive(ActionsHandle, &action, wait) == pdPASS){
			// Reception ordre
			if(action.ventilo_valide){
				if (action.act_rafraichissement || action.act_balayage){
					Vent_Marche();
					balaye = action.act_balayage;
					if(balaye){
						ref = __HAL_TIM_SET_COUNTER(&htim5,0);
						sens = 1;
						HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, GPIO_PIN_RESET);
						t_bal = t_inversion = xTaskGetTickCount();
					}
				}
				else{
					balaye = false;
					Vent_Arret();
				}
			}

			if(action.alarme_valide){
				if(action.act_alarme && !sonne){
					xSemaphoreTake(mtx_consignesHandle,portMAX_DELAY);
					if(consignes.sel_melodie < NB_MELODIES){
						mel = consignes.sel_melodie;
					}
					else{
						mel = 0;
					}
					xSemaphoreGive(mtx_consignesHandle);
					idx = 0;
					sonne = true;
					HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_4);
					t_note = xTaskGetTickCount();
				}
				else if(!action.act_alarme){
					sonne = false;
					HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_4);
				}
			}
		}
		// Action
		now = xTaskGetTickCount();

		if(sonne && (int32_t)(now - t_note) >= 0)
		{
			const Note* n= &melodies[mel].notes[idx];
			Buzzer_Note(n->feq);
			t_note = now + pdMS_TO_TICKS(n->duree);
			idx = (idx +1) % melodies[mel].nb;
		}

		if (balaye && (int32_t)(now - t_bal) >= 0)
		{
			uint32_t cnt   = __HAL_TIM_GET_COUNTER(&htim5);
			int32_t  delta = (int32_t)(cnt - ref);   // la soustraction non signée gère le wrap
			if (delta < 0)
				delta = -delta;

			bool inverser = (delta >= BALAYAGE_BORNE) || ((now - t_inversion) > pdMS_TO_TICKS(BALAYAGE_MAX));

			if (inverser)
			{
				sens = -sens;
				HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, (sens > 0) ? GPIO_PIN_RESET : GPIO_PIN_SET);
				ref = cnt;                // nouvelle référence pour le sens suivant
				t_inversion = now;
			}
			t_bal = now + pdMS_TO_TICKS(PERIODE_BALAYAGE_MS);
		}

	}
}

