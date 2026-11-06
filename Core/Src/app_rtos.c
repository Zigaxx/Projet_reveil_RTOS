/**
 * @file    app_rtos.c
 * @brief   Définition des variables globales et des objets FreeRTOS, création des tâches (App_init).
 * @note    Projet Réveil simulateur d'aube - Mines Saint-Étienne (M. Diallo / A. Stachnik)
 *          Code issu du main.c d'origine : les fonctions sont reprises à l'identique,
 *          seuls des commentaires ont été ajoutés.
 */

#include "app_rtos.h"

/* ---- Mutex / sémaphores ---- */
SemaphoreHandle_t mtx_consignesHandle;
SemaphoreHandle_t HeureMutexSemaphHandle;
SemaphoreHandle_t AlarmeMutexSemaphHandle;
SemaphoreHandle_t sem_ledHandle; // fin de trame DMA
SemaphoreHandle_t mtx_adcHandle;
SemaphoreHandle_t mtx_i2cHandle;
SemaphoreHandle_t sem_alarmeHandle;
SemaphoreHandle_t mtx_infosHandle;

/* ---- Files de messages (mailboxes) ---- */
QueueHandle_t xQueueCommandes;
QueueHandle_t AffHandle;
QueueHandle_t InfosHandle;
QueueHandle_t ActionsHandle;

/* ---- Handles de tâches ---- */
xTaskHandle xTaskGestionAffCrepAube;
xTaskHandle xTaskInterfaceActions;
xTaskHandle xTaskInterfaceAff;
xTaskHandle xTaskGestionHorloge;
xTaskHandle xTaskGestionAlarme;

/* ---- Variables globales partagées ---- */
Consignes consignes;
Heure heure = {0,0,0};
Heure alarme = {0,0,0};
Infos infos_acq = {0};

uint8_t bluetoothChar;
char bluetoothMsg[TAILLE_MAX_MSG_BLUETOOTH];
uint8_t indexBluetoothMsg;

uint16_t time_tof1;
uint16_t time_tof2;

/**
 * @brief  Crée les mutex, sémaphores, files et tâches de l'application.
 * @note   À appeler dans main() avant osKernelStart() (zone USER CODE RTOS_THREADS).
 *         Priorités RMS : horloge +11, TofTask +10, Commandes +9, crép/aube +8,
 *         if_aff +7, if_actions +6, alarme +4, ventilateur/acquisitions +2.
 */
void App_init(void)
{
	/* ---- Mutex et sémaphores ---- */
	mtx_consignesHandle = xSemaphoreCreateMutex();
	sem_ledHandle = xSemaphoreCreateBinary();
	mtx_adcHandle = xSemaphoreCreateMutex();
	mtx_i2cHandle = xSemaphoreCreateMutex();
	sem_alarmeHandle = xSemaphoreCreateBinary();
	mtx_consignesHandle = xSemaphoreCreateMutex();
	HeureMutexSemaphHandle = xSemaphoreCreateMutex();
	AlarmeMutexSemaphHandle = xSemaphoreCreateMutex();
	mtx_infosHandle = xSemaphoreCreateMutex();

	/* ---- Files de messages ---- */
	InfosHandle = xQueueCreate(1, sizeof(Infos) );
	xQueueCommandes = xQueueCreate(10, sizeof(Commande));
	AffHandle = xQueueCreate(4, sizeof(Affichages) );
	ActionsHandle = xQueueCreate(4, sizeof( Actions) );

	/* ---- Tâches ---- */
	xTaskCreate(task_gestion_horloge,       /* Function that implements the task. */
			"TASK_GESTION_HORLOGE",          /* Text name for the task. */
			256,      /* Stack size in words, not bytes. */
			NULL,    /* Parameter passed into the task. */
			tskIDLE_PRIORITY+11,/* Priority at which the task is created. */
			&xTaskGestionHorloge );      /* Used to pass out the created task's handle. */
	xTaskCreate(task_interface_actions,       /* Function that implements the task. */
			"TASK_INTERFACE_ACTIONS",          /* Text name for the task. */
			256,      /* Stack size in words, not bytes. */
			NULL,    /* Parameter passed into the task. */
			tskIDLE_PRIORITY+6,/* Priority at which the task is created. */
			&xTaskInterfaceActions  );      /* Used to pass out the created task's handle. */

	xTaskCreate(task_interface_aff,       /* Function that implements the task. */
			"TASK_INTERFACE_AFF",          /* Text name for the task. */
			512,      /* Stack size in words, not bytes. */
			NULL,    /* Parameter passed into the task. */
			tskIDLE_PRIORITY+7,/* Priority at which the task is created. */
			&xTaskInterfaceAff  );      /* Used to pass out the created task's handle. */

	/* USER CODE BEGIN RTOS_THREADS */
	xTaskCreate(task_gestion_aff_crep_aube,       /* Function that implements the task. */
			"TASK_GESTION_AFF_CREP_AUBE",          /* Text name for the task. */
			256,      /* Stack size in words, not bytes. */
			NULL,    /* Parameter passed into the task. */
			tskIDLE_PRIORITY+8,/* Priority at which the task is created. */
			NULL );      /* Used to pass out the created task's handle. */

	/* add threads, ... */
	xTaskCreate(TofTask,       /* Function that implements the task. */
			"TofTask",          /* Text name for the task. */
			512,      /* Stack size in words, not bytes. */
			NULL,    /* Parameter passed into the task. */
			tskIDLE_PRIORITY+10,/* Priority at which the task is created. */
			NULL );      /* Used to pass out the created task's handle. */

	xTaskCreate(GestionCommandTask,       /* Function that implements the task. */
			"Commandes",          /* Text name for the task. */
			256,      /* Stack size in words, not bytes. */
			NULL,    /* Parameter passed into the task. */
			tskIDLE_PRIORITY+9,/* Priority at which the task is created. */
			NULL );      /* Used to pass out the created task's handle. */
	xTaskCreate(task_gestion_alarme,       /* Function that implements the task. */
			"alarmes",          /* Text name for the task. */
			256,      /* Stack size in words, not bytes. */
			NULL,    /* Parameter passed into the task. */
			tskIDLE_PRIORITY+4,/* Priority at which the task is created. */
			NULL );      /* Used to pass out the created task's handle. */
	xTaskCreate(task_gestion_ventilateur,       /* Function that implements the task. */
			"ventilateur",          /* Text name for the task. */
			256,      /* Stack size in words, not bytes. */
			NULL,    /* Parameter passed into the task. */
			tskIDLE_PRIORITY+2,/* Priority at which the task is created. */
			NULL );      /* Used to pass out the created task's handle. */
	xTaskCreate(task_acq_luminosite,       /* Function that implements the task. */
			"luminosite",          /* Text name for the task. */
			256,      /* Stack size in words, not bytes. */
			NULL,    /* Parameter passed into the task. */
			tskIDLE_PRIORITY+2,/* Priority at which the task is created. */
			NULL );      /* Used to pass out the created task's handle. */
	xTaskCreate(task_acq_temp_qa,       /* Function that implements the task. */
			"temperature/QA",          /* Text name for the task. */
			512,      /* Stack size in words, not bytes. */
			NULL,    /* Parameter passed into the task. */
			tskIDLE_PRIORITY+2,/* Priority at which the task is created. */
			NULL );      /* Used to pass out the created task's handle. */
}
