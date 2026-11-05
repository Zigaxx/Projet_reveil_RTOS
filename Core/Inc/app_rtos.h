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
#include "ssd1306_fonts.h"

#include "app_types.h"

// ----- DEFINE -----
#define PERIODE_ACQ_MS      (1000)
#define PERIODE_VENT_MS     (1000)      // 5000 pour tester
#define PERIODE_HORLOGE     1000
#define PERIODE_MAJ_LED     100
#define DUREE_AUBE          3000
#define DUREE_CREP          3000
#define SNOOZE_MIN          10
#define NB_TOF              4
#define LDR_ADC             0                // rang dans le scan ADC
#define POT_ADC             1
#define SGP40_ADDR          (0x59 << 1)

#define NB_LEDS             4
#define LED_BITS            (NB_LEDS*24)
#define LED_RESET_SLOTS     48
#define LED_T0              32
#define LED_T1              64

#define BALAYAGE_BORNE      500
#define BALAYAGE_PAS        50
#define BALAYAGE_MAX        5000

#define NB_MELODIES  (sizeof(melodies)/sizeof(melodies[0]))

#define TC72_REG_CONTROL      0x00
#define TC72_REG_TEMP_MSB     0x01
#define TC72_REG_TEMP_LSB     0x02
#define TC72_REG_CONTROL_ID   0x03

// --- Peripheriques ----
extern ADC_HandleTypeDef hadc1;
extern I2C_HandleTypeDef hi2c1;
extern SPI_HandleTypeDef hspi2;
extern TIM_HandleTypeDef htim2, htim3, htim5, htim8;

// ---- Donnes partagees -----
extern Consignes consignes;
extern Heure     heure;
extern Infos     infos_acq;
// ---- Objets freertos -----
extern xTaskHandle xTaskAcqLumi;
extern xTaskHandle xTaskAcqTempQA;
extern xTaskHandle xTaskAcqTof;
extern xTaskHandle xTaskInterfaceActions;
extern xTaskHandle xTaskInterfaceAff;
extern xTaskHandle xTaskGestionHorloge;
extern xTaskHandle xTaskGestionAffCrepAube;
extern xTaskHandle xTaskGestionAlarme;
extern xTaskHandle xTaskGestionVentilateur;

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

void TC72_Init(SPI_HandleTypeDef *hspi, GPIO_TypeDef *CS_Port, uint16_t CS_Pin);
void TC72_WriteRegister(uint8_t reg, uint8_t data);
uint8_t TC72_ReadRegister(uint8_t reg);
float TC72_ReadTemperature(void);
float Lire_Temperature();

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
bool Crep_active();
void  aff_led(uint8_t niveau);
bool Alarme_active();
void  Act_alarme(bool on);
Heure Set_alarme(Heure base, int delta);
void  Act_ventilateur(bool rafraichir, bool balayer);




#endif /* INC_APP_RTOS_H_ */
