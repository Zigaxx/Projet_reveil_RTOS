/*
 * app_types.h
 *
 *  Created on: Nov 5, 2026
 *      Author: azert
 */

#ifndef INC_APP_TYPES_H_
#define INC_APP_TYPES_H_

// ----- TYPEDEF -----
typedef struct { uint16_t h, min, sec; } Heure;

typedef struct {
	uint16_t vitesse_ventilo, sel_affichage, sel_melodie;
	Heure    alarme;
	bool     stop_alarme, snooze_alarme, activer_alarme;
	bool     activer_crepuscule, activer_retroeclairage;
	uint16_t seuil_luminosite, seuil_qa;
	float  seuil_temperature;
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

typedef struct {
	uint16_t luminosite, qualite_air, qualite_sommeil;
	float   temperature;
} Infos;

typedef struct { uint16_t feq; uint16_t duree; } Note;
typedef struct { const Note *notes; uint8_t nb; } Melodie;


#endif /* INC_APP_TYPES_H_ */
