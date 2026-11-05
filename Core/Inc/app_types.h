/*
 * app_types.h
 *
 *  Created on: Nov 5, 2026
 *      Author: azert
 */

#ifndef INC_APP_TYPES_H_
#define INC_APP_TYPES_H_

// ----- DEFINE -----
#define PERIODE_ACQ_MS      (5*60*1000)
#define PERIODE_VENT_MS     (5*60*1000)      // 5000 pour tester
#define PERIODE_HORLOGE     1000
#define PERIODE_MAJ_LED     100
#define DUREE_AUBE          30000
#define DUREE_CREP          30000
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

// ----- TYPEDEF -----
typedef struct { uint16_t h, min, sec; } Heure;

typedef struct {
	uint16_t vitesse_ventilo, sel_affichage, sel_melodie;
	Heure    alarme;
	bool     stop_alarme, snooze_alarme, activer_alarme;
	bool     activer_crepuscule, activer_retroeclairage;
	uint16_t seuil_luminosite, seuil_temperature, seuil_qa;
} Consignes;

typedef struct {
	bool set_seuil_lumi, set_seuil_temp, set_seuil_qa;
	bool set_heure, set_alarme, set_crep;
	bool set_stop_reveil, set_snooze, set_melodie, set_affichage;
} Commandes;

typedef struct {
	bool act_rafraichissement, act_balayage;
	bool act_alarme;           // true = sonner, false = couper
	bool alarme_valide;        // ce message contient un ordre d'alarme
	bool ventilo_valide;       // ce message contient un ordre de ventilateur
} Actions;

typedef struct {
	bool     led_valide;  uint8_t led_niveau;
	bool     ecran_valide;
	uint16_t ecran_sel;        // 0 heure, 1 temp, 2 QA, 3 QS
	Heure    ecran_heure;
	uint16_t ecran_valeur;
	bool     ecran_retro;
} Affichages;

typedef struct { uint16_t temperature, luminosite, qualite_air, qualite_sommeil; } Infos;

typedef struct { uint16_t feq; uint16_t duree; } Note;
typedef struct { const Note *notes; uint8_t nb; } Melodie;


#endif /* INC_APP_TYPES_H_ */
