/*
 * if_sorties.c
 *
 *  Created on: Nov 5, 2026
 *      Author: azert
 */

#include "app_rtos.h"


const Note mel_beep[] = { {2000,200}, {0,200} };
const Note mel_1[]    = { {523,200}, {659,200}, {784,200}, {1047,400}, {0,300} };
const Note mel_2[]    = { {784,150}, {988,150}, {1175,150}, {1568,300}, {0,400} };

const Melodie melodies[] = {
	{ mel_beep, sizeof(mel_beep)/sizeof(Note) },    // 0 = beep
	{ mel_1,    sizeof(mel_1)/sizeof(Note)    },
	{ mel_2,    sizeof(mel_2)/sizeof(Note)    },
};

uint16_t led_buffer[LED_BITS + LED_RESET_SLOTS];

// Buzzer
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim){
	if(htim->Instance == TIM3){
		BaseType_t woken = pdFALSE;
		xSemaphoreGiveFromISR(sem_ledHandle,&woken);
		portYIELD_FROM_ISR(woken);
	}
}

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

TickType_t Delai(TickType_t echeance, TickType_t now){
	int d = echeance - now;
	if(d > 0){
		return (TickType_t)d;
	}
	return 0;
}

// Affichaes
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



void Ecran_Afficher(const Affichages *a){
	char l1[16], l2[20];

	snprintf(l1, sizeof(l1), "%02u:%02u:%02u",
			(unsigned)a->ecran_heure.h, (unsigned)a->ecran_heure.min, (unsigned)a->ecran_heure.sec);

	switch (a->ecran_sel) {
		case 1:
			snprintf(l2, sizeof(l2), "Temp: %f C", (unsigned)a->ecran_valeur);
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

	ssd1306_Fill(Black);
	ssd1306_SetCursor(0, 0);
	ssd1306_WriteString(l1, Font_11x18, White);
	ssd1306_SetCursor(0, 30);
	ssd1306_WriteString(l2, Font_11x18, White);
	ssd1306_SetContrast(a->ecran_retro ? 0xFF : 0x40);
	xSemaphoreTake(mtx_i2cHandle, portMAX_DELAY);
	ssd1306_UpdateScreen();
	xSemaphoreGive(mtx_i2cHandle);
}

// Ventilateur
void Vent_Marche(){
	uint16_t pot = ADC_Lire(ADC_CHANNEL_11);
	__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_1,(uint32_t)pot * 3999 / 4095);
	HAL_GPIO_WritePin(DIS_GPIO_Port, DIS_Pin, GPIO_PIN_RESET); // activation du moteur
}

void Vent_Arret(){
	__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_1,0);
	HAL_GPIO_WritePin(DIS_GPIO_Port, DIS_Pin, GPIO_PIN_SET); // désactivation du moteur
}

// Taches
void task_interface_actions(void *pvParameters){
	Actions action;
	bool sonne = false, balaye = false;
	uint8_t mel, idx;
	int8_t sens = 1;
	TickType_t t_note, t_bal, t_inversion;
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
						__HAL_TIM_SET_COUNTER(&htim5,0);
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

		if(sonne && (int32_t)(now - t_note) >= 0){
			const Note* n= &melodies[mel].notes[idx];
			Buzzer_Note(n->feq);
			t_note = now + pdMS_TO_TICKS(n->duree);
			idx = (idx +1) % melodies[mel].nb;
		}

		if(balaye && (int32_t)(now - t_note) >= 0){
			int pos = (int32_t)__HAL_TIM_GET_COUNTER(&htim5);
			bool inverser = (sens > 0 && pos >= BALAYAGE_BORNE) ||
							(sens < 0 && pos <= BALAYAGE_BORNE) ||
							((now - t_inversion) > pdMS_TO_TICKS(BALAYAGE_MAX));

			if(inverser){
				sens = -sens;
				HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, (sens > 0) ? GPIO_PIN_RESET : GPIO_PIN_SET);
				t_inversion = now;
			}
			Vent_Marche();
			t_bal = now + pdMS_TO_TICKS(PERIODE_VENT_MS);
		}

	}
}

void task_interface_aff(void *pvParameters){
	Affichages aff;
	ssd1306_Init();

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



