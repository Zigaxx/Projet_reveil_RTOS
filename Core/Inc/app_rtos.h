/**
 * @file    app_rtos.h
 * @brief   Header commun : includes, objets FreeRTOS et variables globales (extern), prototypes.
 * @note    Projet Réveil simulateur d'aube - Mines Saint-Étienne (M. Diallo / A. Stachnik)
 *          Code issu du main.c d'origine : les fonctions sont reprises à l'identique,
 *          seuls des commentaires ont été ajoutés.
 */

#ifndef INC_APP_RTOS_H_
#define INC_APP_RTOS_H_

#include "main.h"
#include "cmsis_os.h"      /* inclut FreeRTOS.h, task.h, queue.h, semphr.h */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "ssd1306.h"
#include "ssd1306_fonts.h"

#include "app_types.h"
#include "app_config.h"

/* ---- Handles de périphériques HAL (définis dans main.c par CubeMX) ---- */
extern ADC_HandleTypeDef  hadc1;
extern I2C_HandleTypeDef  hi2c1;
extern SPI_HandleTypeDef  hspi2;
extern UART_HandleTypeDef hlpuart1;
extern TIM_HandleTypeDef  htim2;
extern TIM_HandleTypeDef  htim3;
extern TIM_HandleTypeDef  htim5;
extern TIM_HandleTypeDef  htim8;

/* ---- Mutex / sémaphores ---- */
extern SemaphoreHandle_t mtx_consignesHandle;
extern SemaphoreHandle_t HeureMutexSemaphHandle;
extern SemaphoreHandle_t AlarmeMutexSemaphHandle;
extern SemaphoreHandle_t sem_ledHandle;      // fin de trame DMA
extern SemaphoreHandle_t mtx_adcHandle;
extern SemaphoreHandle_t mtx_i2cHandle;
extern SemaphoreHandle_t sem_alarmeHandle;
extern SemaphoreHandle_t mtx_infosHandle;

/* ---- Files de messages (mailboxes) ---- */
extern QueueHandle_t xQueueCommandes;
extern QueueHandle_t AffHandle;
extern QueueHandle_t InfosHandle;
extern QueueHandle_t ActionsHandle;

/* ---- Handles de tâches ---- */
extern xTaskHandle xTaskGestionAffCrepAube;
extern xTaskHandle xTaskInterfaceActions;
extern xTaskHandle xTaskInterfaceAff;
extern xTaskHandle xTaskGestionHorloge;
extern xTaskHandle xTaskGestionAlarme;

/* ---- Variables globales partagées ---- */
extern Consignes consignes;
extern Heure heure;
extern Heure alarme;
extern Infos infos_acq;

extern uint8_t bluetoothChar;
extern char bluetoothMsg[TAILLE_MAX_MSG_BLUETOOTH];
extern uint8_t indexBluetoothMsg;

extern uint16_t time_tof1;
extern uint16_t time_tof2;

extern const Note mel_beep[];
extern const Note mel_1[];
extern const Note mel_2[];
extern const Note mel_3[];
extern const Melodie melodies[];

extern uint16_t led_buffer[LED_BITS + LED_RESET_SLOTS];
extern uint16_t sgp40_baseline;

/* ---- Initialisation de l'application (objets FreeRTOS + création des tâches) ---- */
void App_init(void);

/* ---- Horloge (horloge.c) ---- */
void Heure_Incrementer(Heure *h);
bool Heure_Egales(Heure a, Heure b);
void Aff_ecran(Heure h, uint16_t sel, uint16_t valeur, bool retro);
void task_gestion_horloge(void *pvParameters);

/* ---- Commandes Bluetooth / boutons (commandes.c) ---- */
bool CommandBlutoothTypeExtraction(char *cmd, uint8_t *index, char *commandType);
bool ChooseBluetoothCommandType(char * cmd, CommandeBluetoothType *type);
bool TimeExtraction(char *cmd, uint8_t *index, Heure *value);
bool SplitCommandBluetooth(char * cmd, int *seuil, Heure *horaire, CommandeBluetoothType *type);
void GestionCommandTask (void *pvParameters);

/* ---- ToF (tof.c) ---- */
void TofTask(void *argument);

/* ---- Aube / crépuscule (aube_crep.c) ---- */
void aff_led(uint8_t niveau);
void task_gestion_aff_crep_aube(void *pvParameters);

/* ---- Alarme (alarme.c) ---- */
void Act_alarme(bool on);                  // Activer la melodie de l'alarme
Heure Set_alarme(Heure base,int delta);    // Reglage de l'alarme (le snooze y compris)
void task_gestion_alarme(void *pvParameters);

/* ---- Ventilateur (ventilateur.c) ---- */
void Vent_Marche();
void Vent_Arret();
void Act_ventilateur(bool rafraichir, bool balayer);
void task_gestion_ventilateur(void * pvParameters);

/* ---- Interfaces de sortie : buzzer, LEDs, écran (if_sorties.c) ---- */
void Buzzer_Note(uint16_t freq);
TickType_t Delai(TickType_t echeance, TickType_t now);
void Leds_Envoyer(uint8_t niveau);
void Ecran_Afficher(const Affichages *a);
void task_interface_actions(void *pvParameters);
void task_interface_aff(void *pvParameters);

/* ---- Acquisitions capteurs (acq_capteurs.c) ---- */
uint16_t ADC_Lire(uint8_t canal);
uint16_t Lire_Luminosite();
HAL_StatusTypeDef TC72_Transfer(uint8_t *tx, uint8_t *rx, uint16_t n);
void TC72_Init();
float Lire_Temperature();
uint16_t QA_Init();
uint16_t Lire_QA();
void task_acq_luminosite(void * pvParameters);
void task_acq_temp_qa(void * pvParameters);

#endif /* INC_APP_RTOS_H_ */
