#include "app_rtos.h"

// ---- Donnees partagees ----
Consignes consignes;
Heure heure = {0,0,0};
Infos infos_acq = {0};

// ---- Objets RTOS ----
xTaskHandle xTaskAcqLumi;
xTaskHandle xTaskAcqTempQA;
xTaskHandle xTaskAcqTof;
xTaskHandle xTaskInterfaceActions;
xTaskHandle xTaskInterfaceAff;
xTaskHandle xTaskGestionHorloge;
xTaskHandle xTaskGestionAffCrepAube;
xTaskHandle xTaskGestionAlarme;
xTaskHandle xTaskGestionVentilateur;

xSemaphoreHandle mtx_consignesHandle, mtx_infosHandle, mtx_adcHandle;
xSemaphoreHandle mtx_i2cHandle, mtx_heureHandle;
xSemaphoreHandle sem_alarmeHandle, sem_refreshHandle, sem_ledHandle;

xQueueHandle InfosHandle, AffHandle, ActionsHandle;


void App_init(){
	// ---- Semaphore ----
	sem_ledHandle     = xSemaphoreCreateBinary();
	sem_alarmeHandle  = xSemaphoreCreateBinary();
	sem_refreshHandle = xSemaphoreCreateBinary();

	mtx_consignesHandle = xSemaphoreCreateMutex();
	mtx_infosHandle     = xSemaphoreCreateMutex();
	mtx_adcHandle       = xSemaphoreCreateMutex();
	mtx_i2cHandle       = xSemaphoreCreateMutex();
	mtx_heureHandle     = xSemaphoreCreateMutex();
	// ---- Mailboxes ----
	InfosHandle   = xQueueCreate(1, sizeof(Infos));
	AffHandle     = xQueueCreate(4, sizeof(Affichages));
	ActionsHandle = xQueueCreate(4, sizeof(Actions));

	// ---- Taches ----
	xTaskCreate(task_acq_luminosite,       /* Function that implements the task. */
	   			"TASK_ACQ_LUMI",          /* Text name for the task. */
	   			256,      /* Stack size in words, not bytes. */
	   			NULL,    /* Parameter passed into the task. */
	   			tskIDLE_PRIORITY+1,/* Priority at which the task is created. */
	   			&xTaskAcqLumi  );      /* Used to pass out the created task's handle. */

	xTaskCreate(task_acq_temp_qa,       /* Function that implements the task. */
				"TASK_ACQ_TEMPQA",          /* Text name for the task. */
				512,      /* Stack size in words, not bytes. */
				NULL,    /* Parameter passed into the task. */
				tskIDLE_PRIORITY+2,/* Priority at which the task is created. */
				&xTaskAcqTempQA  );      /* Used to pass out the created task's handle. */

	xTaskCreate(task_acq_tof,       /* Function that implements the task. */
				"TASK_ACQ_TOF",          /* Text name for the task. */
				256,      /* Stack size in words, not bytes. */
				NULL,    /* Parameter passed into the task. */
				tskIDLE_PRIORITY+3,/* Priority at which the task is created. */
				&xTaskAcqTof  );      /* Used to pass out the created task's handle. */
	xTaskCreate(task_gestion_ventilateur,       /* Function that implements the task. */
				"TASK_GESTION_VENTILATEUR",          /* Text name for the task. */
				256,      /* Stack size in words, not bytes. */
				NULL,    /* Parameter passed into the task. */
				tskIDLE_PRIORITY+5,/* Priority at which the task is created. */
				&xTaskGestionVentilateur  );      /* Used to pass out the created task's handle. */
	xTaskCreate(task_interface_actions,       /* Function that implements the task. */
	   			"TASK_INTERFACE_ACTIONS",          /* Text name for the task. */
	   			256,      /* Stack size in words, not bytes. */
	   			NULL,    /* Parameter passed into the task. */
	   			tskIDLE_PRIORITY+6,/* Priority at which the task is created. */
	   			&xTaskInterfaceActions  );      /* Used to pass out the created task's handle. */

	xTaskCreate(task_interface_aff,       /* Function that implements the task. */
				"TASK_INTERFACE_AFF",          /* Text name for the task. */
				256,      /* Stack size in words, not bytes. */
				NULL,    /* Parameter passed into the task. */
				tskIDLE_PRIORITY+7,/* Priority at which the task is created. */
				&xTaskInterfaceAff  );      /* Used to pass out the created task's handle. */

	xTaskCreate(task_gestion_aff_crep_aube,       /* Function that implements the task. */
				"TASK_GESTION_AFF_CREP_AUBE",          /* Text name for the task. */
				256,      /* Stack size in words, not bytes. */
				NULL,    /* Parameter passed into the task. */
				tskIDLE_PRIORITY+8,/* Priority at which the task is created. */
				&xTaskGestionAffCrepAube );      /* Used to pass out the created task's handle. */
	xTaskCreate(task_gestion_alarme,       /* Function that implements the task. */
				"TASK_GESTION_ALARME",          /* Text name for the task. */
				256,      /* Stack size in words, not bytes. */
				NULL,    /* Parameter passed into the task. */
				tskIDLE_PRIORITY+10,/* Priority at which the task is created. */
				&xTaskGestionAlarme  );      /* Used to pass out the created task's handle. */
	xTaskCreate(task_gestion_horloge,       /* Function that implements the task. */
				"TASK_GESTION_HORLOGE",          /* Text name for the task. */
				256,      /* Stack size in words, not bytes. */
				NULL,    /* Parameter passed into the task. */
				tskIDLE_PRIORITY+11,/* Priority at which the task is created. */
				&xTaskGestionHorloge );      /* Used to pass out the created task's handle. */

}


