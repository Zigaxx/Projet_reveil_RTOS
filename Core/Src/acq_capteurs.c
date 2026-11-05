#include "app_rtos.h"

volatile int16_t temp_comp_c = 25;            // température utilisée pour compenser le SGP40
uint16_t sgp40_baseline = 0;

uint16_t ADC_Lire(uint32_t canal){
	uint8_t valeur[2];

	xSemaphoreTake(mtx_adcHandle,portMAX_DELAY);
	HAL_ADC_Start(&hadc1);
	for (int i = 0; i < 2; i++) {
		if(HAL_ADC_PollForConversion(&hadc1,10) == HAL_OK){
			valeur[i] = (uint16_t)HAL_ADC_GetValue(&hadc1);
		}
	}
	HAL_ADC_Stop(&hadc1);

	xSemaphoreGive(mtx_adcHandle);
	return valeur[canal];
}

uint16_t Lire_Luminosite(){
	return ADC_Lire(LDR_ADC);
}

void Temp_Init(){
	uint8_t tx[2] = {0x80,0x00}; //
	HAL_GPIO_WritePin(CE_GPIO_Port, CE_Pin, GPIO_PIN_SET);
	HAL_SPI_Transmit(&hspi2, tx, 2, 10);
	HAL_GPIO_WritePin(CE_GPIO_Port, CE_Pin, GPIO_PIN_RESET);
	vTaskDelay(pdMS_TO_TICKS(200)); // attente de 150 ms + marge
}

uint16_t Lire_Temperature(){
	uint16_t  derniere = 0;
	uint8_t tx[4] = {0x02,0,0,0}; // Addr lecture, MSB, LSB, Control bytes
	uint8_t rx[4] = {0};

	HAL_GPIO_WritePin(CE_GPIO_Port, CE_Pin, GPIO_PIN_SET);
	HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(&hspi2, tx, rx, 4, 10);
	HAL_GPIO_WritePin(CE_GPIO_Port, CE_Pin, GPIO_PIN_RESET);

	if(st == HAL_OK){
		int8_t msb = (int8_t)rx[1];
		temp_comp_c = msb;
		derniere = (msb < 0) ? 0 : (uint16_t)msb;
	}

	return derniere;
}

uint16_t QA_Init(){
	uint8_t cmd[8] = {0x26, 0x0F, 0x80, 0x00, 0xA2, 0x66, 0x66, 0x93};
	uint8_t rx[3] = {0};
	uint16_t v = 0;

	xSemaphoreTake(mtx_i2cHandle,portMAX_DELAY);
	if(HAL_I2C_Master_Transmit(&hi2c1, SGP40_ADDR, cmd, 8, 50) == HAL_OK){
		xSemaphoreGive(mtx_i2cHandle);
		vTaskDelay(pdMS_TO_TICKS(35));
		xSemaphoreTake(mtx_i2cHandle,portMAX_DELAY);

		if(HAL_I2C_Master_Receive(&hi2c1, SGP40_ADDR, cmd, 8, 50) == HAL_OK){
			v = (uint16_t)((rx[0] << 8) | rx[1] );
		}
	}
	xSemaphoreGive(mtx_i2cHandle);
	return v;
}

uint16_t Lire_QA(){
	uint16_t raw = 0;

	// On fait 10 mesures pour satbliliser la sortie
	for (int i = 0; i < 10; ++i) {
		raw = QA_Init();
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
	if(raw == 0){ // erreur lecture
		return 0;
	}

	if(raw > sgp40_baseline){
		sgp40_baseline = raw;
	}
	return sgp40_baseline - raw; // donc 0 = pur et val elevé = pollué
}

// TOF
void ToF_Init(){}

bool ToF_Lire(uint16_t distances[NB_TOF]){
	return false;
}

uint16_t Calcul_QS(const uint16_t distances[NB_TOF]){
	return 0;
}



void task_acq_luminosite(void *pvParameters){
	TickType_t  dernier = xTaskGetTickCount();

	while(1){
		uint16_t lumi = Lire_Luminosite();

		xSemaphoreTake(mtx_infosHandle,portMAX_DELAY);
		infos_acq.luminosite = lumi;
		xQueueOverwrite(InfosHandle,&infos_acq);
		xSemaphoreGive(mtx_infosHandle);

		vTaskDelayUntil(&dernier, pdMS_TO_TICKS(PERIODE_ACQ_MS));
	}
}

void task_acq_temp_qa(void *pvParameters){
	TickType_t  dernier = xTaskGetTickCount();

	while(1){
		uint16_t temp = Lire_Temperature();
		uint16_t qa = Lire_QA();

		xSemaphoreTake(mtx_infosHandle,portMAX_DELAY);
		infos_acq.temperature = temp;
		infos_acq.qualite_air = qa;
		xQueueOverwrite(InfosHandle,&infos_acq);
		xSemaphoreGive(mtx_infosHandle);

		vTaskDelayUntil(&dernier, pdMS_TO_TICKS(PERIODE_ACQ_MS));
	}
}

void task_acq_tof(void *pvParameters){
	TickType_t  dernier = xTaskGetTickCount();
	uint16_t distances[NB_TOF] = {0};
	ToF_Init();

	while(1){
		if(ToF_Lire(distances)){
			uint16_t qs = Calcul_QS(distances);

			xSemaphoreTake(mtx_infosHandle,portMAX_DELAY);
			infos_acq.qualite_sommeil = qs;
			xQueueOverwrite(InfosHandle,&infos_acq);
			xSemaphoreGive(mtx_infosHandle);
		}
		vTaskDelayUntil(&dernier, pdMS_TO_TICKS(PERIODE_ACQ_MS));
	}
}




