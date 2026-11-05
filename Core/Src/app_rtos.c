#include "app_rtos.h"

// ---- Donnees partagees ----
Consignes consignes;
Heure heure = {0,0,0};
Infos infos_acq = {0};

// ---- Objets RTOS ----
xSemaphoreHandle mtx_consignesHandle, mtx_infosHandle, mtx_adcHandle;
xSemaphoreHandle mtx_i2cHandle, mtx_heureHandle;
xSemaphoreHandle sem_alarmeHandle, sem_refreshHandle, sem_ledHandle;
xQueueHandle InfosHandle, AffHandle, ActionsHandle;


void App_Init(){
	// ---- Semaphore ----

	// ---- Mailboxes ----

	// ---- Taches ----
}


