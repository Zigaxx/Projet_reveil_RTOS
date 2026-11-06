/**
 * @file    app_types.h
 * @brief   Types de données partagés entre les modules (structures, enums).
 * @note    Projet Réveil simulateur d'aube - Mines Saint-Étienne (M. Diallo / A. Stachnik)
 *          Code issu du main.c d'origine : les fonctions sont reprises à l'identique,
 *          seuls des commentaires ont été ajoutés.
 */

#ifndef INC_APP_TYPES_H_
#define INC_APP_TYPES_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
	uint16_t h;
	uint16_t min;
	uint16_t sec;
} Heure;

typedef struct
{
	uint16_t vitesse_ventilo;
	uint16_t sel_affichage;
	uint16_t sel_melodie;
	bool stop_alarme;
	bool snooze_alarme;
	bool activer_alarme;
	bool activer_crepuscule;
	bool activer_retroeclairage;
	int seuil_luminosite;
	int seuil_temperature;
	int seuil_qa;
} Consignes;

typedef enum {
	CMD_SET_AFFICHAGE, CMD_SET_MELODIE, CMD_BUETOOTH, CMD_SET_CREPUSCULE, CMD_SNOOZE, CMD_STOP_REVEIL
} CommandeType;

typedef enum {
	CMD_SET_HEURE, CMD_SET_ALARM, CMD_SEUIL_LUMI, CMD_SEUIL_QA, CMD_SEUIL_TEMP
} CommandeBluetoothType;

typedef struct {
	CommandeType type;
	char str[20];
} Commande;


typedef struct
{
	//LED Adressables
	bool led_valide; // Ordre pour les LEDS adressables
	uint8_t led_niveau; //
	// ECRAN
	bool ecran_valide;
	uint16_t ecran_sel; // 0 = heure, 1 = temp, 2 = QA, 3 = QS
	Heure ecran_heure; // heure courante
	uint16_t ecran_valeur; // temp, QA ou QS selon ecran_sel
	bool ecran_retro;
} Affichages;

typedef struct
{
	bool act_rafraichissement;
	bool act_balayage;
	bool act_alarme;        // true = sonner, false = couper
	bool alarme_valide; 	// Provisoire Signal d'ordre
	bool ventilo_valide;	// Provisoire
}Actions;

// Buzzer
typedef struct
{
	uint16_t feq;
	uint16_t duree;
}Note;


typedef struct
{
	const Note* notes;
	uint8_t nb;
}Melodie;

typedef struct
{
	uint16_t temperature;
	uint16_t luminosite;
	uint16_t qualite_air;
	uint16_t qualite_sommeil;
}Infos;

#endif /* INC_APP_TYPES_H_ */
