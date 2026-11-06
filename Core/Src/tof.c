/**
 * @file    tof.c
 * @brief   Capteurs ToF VL53L1 : détection de gestes pour snooze (balayage) et stop (2 capteurs maintenus).
 * @note    Projet Réveil simulateur d'aube - Mines Saint-Étienne (M. Diallo / A. Stachnik)
 *          Code issu du main.c d'origine : les fonctions sont reprises à l'identique,
 *          seuls des commentaires ont été ajoutés.
 */

#include "app_rtos.h"
#include "vl53l1_api.h"

/* États internes des 2 capteurs (privés à ce module) */
static VL53L1_Dev_t tof[2];
static bool tofPresent[2];

/**
 * Envoie une commande simple (sans texte) à la tâche de gestion des commandes.
 */
static void SendCmd(CommandeType t)
{
	Commande c = {0};
	c.type = t;
	xQueueSend(xQueueCommandes, &c, 0);
}

/* ----------------------------------------------------------------------
 * Fonctions pour TOFs (snooze et stop)
 * ---------------------------------------------------------------------- */

/**
 * Initialise les 2 capteurs VL53L1 : réveil via SHUT1/SHUT2, changement d'adresse du capteur 1
 * (0x54), mode courte distance, budget 20 ms, démarrage des mesures.
 */
static void TofInit(void)
{
	HAL_GPIO_WritePin(GPIOH, SHUT1_Pin | SHUT2_Pin, GPIO_PIN_RESET);
	vTaskDelay(pdMS_TO_TICKS(10));

	// capteur 1 : on le réveille et on change son adresse
	tof[0].I2cHandle  = &hi2c1;
	tof[0].I2cDevAddr = 0x52;
	HAL_GPIO_WritePin(GPIOH, SHUT1_Pin, GPIO_PIN_SET);
	vTaskDelay(pdMS_TO_TICKS(5));
	VL53L1_WaitDeviceBooted(&tof[0]);
	VL53L1_SetDeviceAddress(&tof[0], 0x54);
	tof[0].I2cDevAddr = 0x54;

	// capteur 2 : garde l'adresse par défaut
	tof[1].I2cHandle  = &hi2c1;
	tof[1].I2cDevAddr = 0x52;
	HAL_GPIO_WritePin(GPIOH, SHUT2_Pin, GPIO_PIN_SET);
	vTaskDelay(pdMS_TO_TICKS(5));
	VL53L1_WaitDeviceBooted(&tof[1]);

	for (int i = 0; i < 2; i++)
	{
		VL53L1_DataInit(&tof[i]);
		VL53L1_StaticInit(&tof[i]);
		VL53L1_SetDistanceMode(&tof[i], VL53L1_DISTANCEMODE_SHORT);
		VL53L1_SetMeasurementTimingBudgetMicroSeconds(&tof[i], 20000);
		VL53L1_StartMeasurement(&tof[i]);
	}
}

/**
 * Met à jour tofPresent[i] si une nouvelle mesure est prête (présence = distance < TOF_PRESENT_MM).
 */
// met à jour tofPresent[i] si une nouvelle mesure est prête
static void TofUpdate(int i)
{
	uint8_t ready = 0;
	VL53L1_GetMeasurementDataReady(&tof[i], &ready);
	if (ready)
	{
		VL53L1_RangingMeasurementData_t m;
		VL53L1_GetRangingMeasurementData(&tof[i], &m);
		VL53L1_ClearInterruptAndStartMeasurement(&tof[i]);
		tofPresent[i] = (m.RangeStatus == 0) && (m.RangeMilliMeter < TOF_PRESENT_MM);
	}
}

/* ----------------------------------------------------------------------
 * Tâche TOF stop/snooze
 * ---------------------------------------------------------------------- */

/**
 * Tâche ToF : snooze = main passant d'un capteur à l'autre dans TOF_SWIPE_WINDOW_MS ;
 * stop = les 2 capteurs couverts pendant TOF_HOLD_MS. Actif uniquement si l'alarme sonne.
 */
void TofTask(void *argument)
{
	bool prev[2] = {false, false};
	TickType_t start[2] = {0, 0};
	TickType_t lastFall = 0;
	int lastFallIdx = -1;
	bool locked = false; // vrai après un stop/snooze, jusqu'à ce que les 2 capteurs soient libres

	xSemaphoreTake(mtx_i2cHandle, portMAX_DELAY);
	TofInit();
	xSemaphoreGive(mtx_i2cHandle);

	for (;;)
	{
		TickType_t now = xTaskGetTickCount();

		for (int i = 0; i < 2; i++)
		{
			xSemaphoreTake(mtx_i2cHandle, portMAX_DELAY);
			TofUpdate(i);
			xSemaphoreGive(mtx_i2cHandle);
			bool cur = tofPresent[i];

			if (cur && !prev[i]) // main arrive sur i
			{
				start[i] = now;
				if (!locked && lastFallIdx == 1 - i && (now - lastFall) < pdMS_TO_TICKS(TOF_SWIPE_WINDOW_MS))
				{
					xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
					bool alarme_active = consignes.activer_alarme;
					xSemaphoreGive(mtx_consignesHandle);

					if (alarme_active)
					{
						SendCmd(CMD_SNOOZE);
						locked = true;
					}
				}
			}
			else if (!cur && prev[i]) // main quitte i
			{
				lastFall = now;
				lastFallIdx = i;
			}

			if (tofPresent[0] && tofPresent[1] && !locked)
			{
				if ((now - start[0]) >= pdMS_TO_TICKS(TOF_HOLD_MS) &&(now - start[1]) >= pdMS_TO_TICKS(TOF_HOLD_MS))
				{
					xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
					bool alarme_active = consignes.activer_alarme;
					xSemaphoreGive(mtx_consignesHandle);
					if (alarme_active)
					{
						SendCmd(CMD_STOP_REVEIL);
						locked = true;
					}
				}
			}

			prev[i] = cur;
		}

		if (!tofPresent[0] && !tofPresent[1])
			locked = false;

		vTaskDelay(pdMS_TO_TICKS(TOF_PERIOD_MS));
	}
}

