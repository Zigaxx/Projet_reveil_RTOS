/**
 * @file    commandes.c
 * @brief   Commandes utilisateur : décodage Bluetooth, tâche de gestion des commandes, callbacks boutons/UART.
 * @note    Projet Réveil simulateur d'aube - Mines Saint-Étienne (M. Diallo / A. Stachnik)
 *          Code issu du main.c d'origine : les fonctions sont reprises à l'identique,
 *          seuls des commentaires ont été ajoutés.
 */

#include "app_rtos.h"

/* ----------------------------------------------------------------------
 * Décodage des commandes bluetooth (ex. "heure = 12:30:00", "temperature 25")
 * ---------------------------------------------------------------------- */

/**
 * Extrait le premier mot (type de commande) jusqu'au premier espace.
 * @param cmd Message reçu
 * @param index Index courant dans cmd (mis à jour)
 * @param commandType Buffer recevant le mot extrait
 * @return false si la commande est invalide (fin de ligne avant un espace)
 */
bool CommandBlutoothTypeExtraction(char *cmd, uint8_t *index, char *commandType)
{
	bool haveCommand = true; //inversé par rapport au nom
	while(haveCommand)
	{
		if(cmd[*index] == '\r' || cmd[*index] == '\n' || cmd[*index] == '\0' || *index >= TAILLE_MAX_MSG_BLUETOOTH)
		{
			return false; // la commande n'est pas valide
		}
		if(cmd[*index] != ' ')
		{
			commandType[*index] = cmd[*index];
			*index += 1;
		}
		else
		{
			haveCommand = false;
		}
	}
	//ajout caractère fin de mot
	commandType[*index] = '\0';

	return true;
}

/**
 * Convertit le mot extrait en type de commande bluetooth.
 * @return false si la commande est inconnue
 */
bool ChooseBluetoothCommandType(char * cmd, CommandeBluetoothType *type)
{
	if (strcmp(cmd, "luminosite") == 0) {
		*type = CMD_SEUIL_LUMI;
	}
	else if (strcmp(cmd, "temperature") == 0) {
		*type = CMD_SEUIL_TEMP;
	}
	else if (strcmp(cmd, "QA") == 0) {
		*type = CMD_SEUIL_QA;
	}
	else if (strcmp(cmd, "heure") == 0) {
		*type = CMD_SET_HEURE;
	}
	else if (strcmp(cmd, "alarme") == 0) {
		*type = CMD_SET_ALARM;
	}
	else
	{
		return false; // Commande inconnue
	}
	return true;
}

/**
 * Extrait une heure au format hh:mm:ss à partir de l'index courant.
 * @return false si le format est invalide
 */
bool TimeExtraction(char *cmd, uint8_t *index, Heure *value)
{
	//récupérer heure, avancer, récupérer min avancer, récupérer sec
	value->h = atoi(&cmd[*index]);

	while (cmd[*index] != ':')
	{
		if(cmd[*index] == '\r' || cmd[*index] == '\n' || cmd[*index] == '\0' || *index >= TAILLE_MAX_MSG_BLUETOOTH) //vérification du pour ne pas tomber dans une erreur ou boucle infinie
		{
			return false;
		}
		*index += 1;
	}
	*index += 1; // pour sauter le ':'

	value->min = atoi(&cmd[*index]);
	while (cmd[*index] != ':')
	{
		if(cmd[*index] == '\r' || cmd[*index] == '\n' || cmd[*index] == '\0' || *index >= TAILLE_MAX_MSG_BLUETOOTH) //vérification du pour ne pas tomber dans une erreur ou boucle infinie
		{
			return false;
		}
		*index += 1;
	}
	*index += 1; // pour sauter le ':'

	value->sec = atoi(&cmd[*index]);

	return true;
}

/**
 * Découpe un message bluetooth complet : type, puis valeur (seuil entier ou heure).
 * @param seuil Reçoit la valeur pour les commandes de seuil
 * @param horaire Reçoit l'heure pour les commandes heure/alarme
 * @param type Reçoit le type de commande
 * @return true si le message est valide
 */
bool SplitCommandBluetooth(char * cmd, int *seuil, Heure *horaire, CommandeBluetoothType *type)
{
	//1ère partie: extraction du type de la commande
	uint8_t indexmsg = 0; // index qui évolue au fil de l'évolution des fonctions, c'est voulu !!!
	char msgCommandType[TAILLE_MAX_MSG_BLUETOOTH];

	if( !CommandBlutoothTypeExtraction(cmd, &indexmsg, msgCommandType))
	{
		return false;
	}

	//2e partie conversion du type de commande
	if(!ChooseBluetoothCommandType(msgCommandType,type) )
		return false;

	//3e partie décodage de la valeur
	//a. on avance jusqu'à la valeur
	while (cmd[indexmsg] == ' ' || cmd[indexmsg] == '=')
	{
		indexmsg += 1;
	}

	//b on sépare selon le type
	if(*type == CMD_SEUIL_LUMI || *type == CMD_SEUIL_TEMP || *type == CMD_SEUIL_QA) // on a reçu un seuil
	{
		*seuil = atoi(&cmd[indexmsg]);
	}
	else if(!TimeExtraction(cmd, &indexmsg, horaire) )
	{
		return false;
	}

	return true;
}

/* ----------------------------------------------------------------------
 * Tâche gestion des commandes
 * ---------------------------------------------------------------------- */

/**
 * Tâche de gestion des commandes : attend sur xQueueCommandes (boutons, bluetooth, ToF)
 * et met à jour les consignes / l'heure / l'alarme sous mutex.
 */
void GestionCommandTask (void *pvParameters)
{
	Commande cmd;

	while(1)
	{
		xQueueReceive(xQueueCommandes, &cmd, portMAX_DELAY);

		switch (cmd.type)
		{
		case CMD_BUETOOTH:

			int seuil;
			Heure horaire;
			CommandeBluetoothType cmdType;
			SplitCommandBluetooth(cmd.str,&seuil, &horaire, &cmdType);

			switch (cmdType)
			{
			case CMD_SET_HEURE:
				xSemaphoreTake(HeureMutexSemaphHandle, portMAX_DELAY);
				heure = horaire;
				xSemaphoreGive(HeureMutexSemaphHandle);
				break;
			case CMD_SET_ALARM:
				xSemaphoreTake(AlarmeMutexSemaphHandle, portMAX_DELAY);
				alarme = horaire;
				xSemaphoreGive(AlarmeMutexSemaphHandle);
				break;
			case CMD_SEUIL_LUMI:
				xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
				consignes.seuil_luminosite = seuil;
				xSemaphoreGive(mtx_consignesHandle);
				break;
			case CMD_SEUIL_QA:
				xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
				consignes.seuil_qa = seuil;
				xSemaphoreGive(mtx_consignesHandle);
				break;
			case CMD_SEUIL_TEMP:
				xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
				consignes.seuil_temperature = seuil;
				xSemaphoreGive(mtx_consignesHandle);
				break;
			default:
				break;
			}
			break;

			case CMD_SET_AFFICHAGE:
				if(consignes.sel_affichage >= MAX_SEL_AFFICHAGE)
				{
					xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
					consignes.sel_affichage = 0;
					xSemaphoreGive(mtx_consignesHandle);
				}
				else
				{
					xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
					consignes.sel_affichage += 1;
					xSemaphoreGive(mtx_consignesHandle);
				}
				break;

			case CMD_SET_MELODIE:
				if(consignes.sel_melodie >= NB_MELODIES)
				{
					xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
					consignes.sel_melodie = 0;
					xSemaphoreGive(mtx_consignesHandle);
				}
				else
				{
					xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
					consignes.sel_melodie += 1;
					xSemaphoreGive(mtx_consignesHandle);
				}
				break;

			case CMD_SET_CREPUSCULE:
				xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
				consignes.activer_crepuscule = true;
				xSemaphoreGive(mtx_consignesHandle);
				break;

			case CMD_SNOOZE:
				xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
				consignes.snooze_alarme = true;
				xSemaphoreGive(mtx_consignesHandle);
				break;

			case CMD_STOP_REVEIL:
				xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
				consignes.stop_alarme = true;
				xSemaphoreGive(mtx_consignesHandle);
				break;

			default:
				break;
		}
	}
}

/* ----------------------------------------------------------------------
 * Callbacks (interruptions)
 * ---------------------------------------------------------------------- */

/**
 * Callback EXTI : boutons SW1 (affichage), SW2 (mélodie), B1 (crépuscule) avec anti-rebond 50 ms.
 * Envoie la commande correspondante à xQueueCommandes.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	Commande cmd = {0};
	BaseType_t woken = pdFALSE; //sert à reveiller la tache gestion de commande

	static TickType_t dernierSW1 = 0;
	static TickType_t dernierSW2 = 0;
	static TickType_t dernierB1 = 0;
	TickType_t now = xTaskGetTickCountFromISR();


	if(GPIO_Pin == SW1_Pin)
	{
		if ((now - dernierSW1) < pdMS_TO_TICKS(50))
			return;
		dernierSW1 = now;
		cmd.type = CMD_SET_AFFICHAGE;
	}
	else if(GPIO_Pin == SW2_Pin)
	{
		if ((now - dernierSW2) < pdMS_TO_TICKS(50))
			return;
		dernierSW2 = now;
		cmd.type = CMD_SET_MELODIE;
	}
	else if(GPIO_Pin == B1_Pin)
	{
		if ((now - dernierB1) < pdMS_TO_TICKS(50))
			return;
		dernierB1 = now;
		cmd.type = CMD_SET_CREPUSCULE;
	}

	xQueueSendFromISR(xQueueCommandes,&cmd,&woken);
	portYIELD_FROM_ISR(woken);
}

/**
 * Callback réception UART (LPUART1, Bluetooth) : assemble les caractères jusqu'à \r ou \n
 * puis envoie une commande CMD_BUETOOTH à xQueueCommandes.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == LPUART1)
	{
		BaseType_t xWoken = pdFALSE;
		uint8_t c = bluetoothChar;

		if (c == '\n' || c == '\r')
		{
			if (indexBluetoothMsg > 0)
			{
				Commande cmd;
				cmd.type = CMD_BUETOOTH;
				bluetoothMsg[indexBluetoothMsg] = '\0'; // ajout du caractère de fin
				strcpy(cmd.str, bluetoothMsg); // copie du message dans la commadne à envoyer
				xQueueSendFromISR(xQueueCommandes, &cmd, &xWoken);
				indexBluetoothMsg = 0;
			}
		}
		else if (indexBluetoothMsg < sizeof(bluetoothMsg) - 1)
		{
			bluetoothMsg[indexBluetoothMsg] = (char)c;
			indexBluetoothMsg += 1;
		}
		else
		{
			indexBluetoothMsg = 0; // ligne trop longue : on jette
		}

		HAL_UART_Receive_IT(&hlpuart1, (uint8_t *) &bluetoothChar, 1);
		portYIELD_FROM_ISR(xWoken);
	}
}

