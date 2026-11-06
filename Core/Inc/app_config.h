/**
 * @file    app_config.h
 * @brief   Constantes de configuration (durées, périodes, seuils, matériel).
 * @note    Projet Réveil simulateur d'aube - Mines Saint-Étienne (M. Diallo / A. Stachnik)
 *          Code issu du main.c d'origine : les fonctions sont reprises à l'identique,
 *          seuls des commentaires ont été ajoutés.
 */

#ifndef INC_APP_CONFIG_H_
#define INC_APP_CONFIG_H_

/* ---- Interface utilisateur / consignes ---- */
#define NB_MELODIES 3 // à choisir selon les mélodies sauvegardées. Faire -1 par rapport au nombre réel
#define MAX_SEL_AFFICHAGE 3 // heure, temperature, QA, QS => 4-1=3

/* ---- Bluetooth ---- */
#define TAILLE_MAX_MSG_BLUETOOTH 20

/* ---- ToF (gestes stop / snooze) ---- */
#define TOF_PRESENT_MM 300
#define TOF_HOLD_MS 600
#define TOF_SWIPE_WINDOW_MS 1000
#define TOF_PERIOD_MS 20
#define NB_TOF              4

/* ---- Aube / crépuscule ---- */
#define DUREE_AUBE 30000
#define DUREE_CREP 30000
#define PERIODE_MAJ_LED 100

/* ---- Ventilateur ---- */
#define PERIODE_VENT_MS 1000
#define PERIODE_BALAYAGE_MS 500
#define BALAYAGE_BORNE 2700 // en pas d'encodeur, à calibrer
#define BALAYAGE_PAS 200
#define BALAYAGE_MAX 5000000 // sécurité si l'encodeur ne répond pas

/* ---- LED adressables (WS2812 via TIM3 + DMA) ---- */
#define NB_LEDS 4
#define LED_BITS (NB_LEDS*24)
#define LED_RESET_SLOTS 48 //48 x 1,25 us = 60 us (>50 us)
#define LED_T0 32 //0,4 us
#define LED_T1 64 //0,8 us

/* ---- Horloge / alarme ---- */
#define PERIODE_HORLOGE  1000
#define SNOOZE_MIN  1 // Temps du snooze du reveil

/* ---- Acquisitions ---- */
#define PERIODE_ACQ_MS      (1000)
#define LDR_ADC             0                // rang dans le scan ADC
#define POT_ADC             1
#define SGP40_ADDR          (0x59 << 1)

/* ---- Registres TC72 ---- */
#define TC72_REG_CONTROL      0x00
#define TC72_REG_TEMP_MSB     0x01
#define TC72_REG_TEMP_LSB     0x02
#define TC72_REG_CONTROL_ID   0x03

#endif /* INC_APP_CONFIG_H_ */
