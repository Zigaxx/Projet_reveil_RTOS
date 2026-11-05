/*
 * app_rtos.h
 *
 *  Created on: Nov 5, 2026
 *      Author: azert
 */

#ifndef INC_APP_RTOS_H_
#define INC_APP_RTOS_H_

#include "main.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "ssd1306.h"

#include "app_types.h"

// ---- Donnes partagees -----
extern Consignes consignes;
extern Heure     heure;
extern Infos     infos_acq;

// ---- Objets freertos -----
extern xSemaphoreHandle mtx_consignesHandle, mtx_infosHandle, mtx_adcHandle;
extern xSemaphoreHandle mtx_i2cHandle, mtx_heureHandle;
extern xSemaphoreHandle sem_alarmeHandle, sem_refreshHandle, sem_ledHandle;
extern xQueueHandle InfosHandle, AffHandle, ActionsHandle;

void App_init();

// ---- Prototypes des fonctions ----

// ---------- Prototypes des tâches ----------
void task_acq_luminosite(void *pvParameters);
void task_acq_temp_qa(void *pvParameters);
void task_acq_tof(void *pvParameters);
void task_interface_actions(void *pvParameters);
void task_interface_aff(void *pvParameters);
void task_gestion_horloge(void *pvParameters);
void task_gestion_aff_crep_aube(void *pvParameters);
void task_gestion_alarme(void *pvParameters);
void task_gestion_ventilateur(void *pvParameters);

// ---------- Acquisition ----------
uint16_t ADC_Lire(uint32_t canal);
uint16_t Lire_Luminosite();

void     Temp_Init();
uint16_t Lire_Temperature();

uint16_t QA_Init();
uint16_t Lire_QA();

void     ToF_Init();
bool     ToF_Lire(uint16_t distances[NB_TOF]);
uint16_t Calcul_QS(const uint16_t distances[NB_TOF]);

// ---------- Interfaces de sortie ----------
void       Buzzer_Note(uint16_t freq);
TickType_t Delai(TickType_t echeance, TickType_t now);
void       Leds_Envoyer(uint8_t niveau);
void       Ecran_Afficher(const Affichages *a);
void       Vent_Marche();
void       Vent_Arret();

// ---------- Horloge, affichage, alarme, ventilateur ----------
void  Heure_Incrementer(Heure *h);
bool  Heure_Egales(Heure a, Heure b);
void  Aff_ecran(Heure h, uint16_t sel, uint16_t valeur, bool retro);
void  aff_led(uint8_t niveau);
void  Act_alarme(bool on);
Heure Set_alarme(Heure base, int delta);
void  Act_ventilateur(bool rafraichir, bool balayer);

#endif /* INC_APP_RTOS_H_ */
