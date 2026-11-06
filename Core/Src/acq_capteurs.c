#include "app_rtos.h"




uint16_t sgp40_baseline = 0;

uint16_t ADC_Lire(uint8_t canal){
	uint16_t valeur[2];

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



HAL_StatusTypeDef TC72_Transfer(uint8_t *tx, uint8_t *rx, uint16_t n){
	HAL_GPIO_WritePin(CSN_GPIO_Port, CSN_Pin, GPIO_PIN_SET);   // CSN toujours haut
	HAL_GPIO_WritePin(CE_GPIO_Port, CE_Pin, GPIO_PIN_SET);     // CE actif à l'état haut
	vTaskDelay(pdMS_TO_TICKS(1));
	HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(&hspi2, tx, rx, n, 10);
	HAL_GPIO_WritePin(CE_GPIO_Port, CE_Pin, GPIO_PIN_RESET);
	return st;
}

void TC72_Init(){
	uint8_t rx[4] = {0};

	uint8_t w[2] = {0x80, 0x04};            // écriture du contrôle : conversion continue
	TC72_Transfer(w, rx, 2);
	vTaskDelay(pdMS_TO_TICKS(200));         // première conversion ≈ 150 ms

	uint8_t c[2] = {0x00, 0x00};            // relecture du contrôle : doit afficher 04
	TC72_Transfer(c, rx, 2);
	printf("TC72 controle = 0x%02X (attendu 0x04)\r\n", rx[1]);

	uint8_t i[2] = {0x03, 0x00};            // ID fabricant
	TC72_Transfer(i, rx, 2);
	printf("TC72 ID = 0x%02X (attendu 0x54)\r\n", rx[1]);
}

float Lire_Temperature(){
	static float derniere = 0.0f;
	uint8_t tx[4] = {0x02, 0, 0, 0};        // lecture séquentielle : MSB, LSB, contrôle
	uint8_t rx[4] = {0};

	if(TC72_Transfer(tx, rx, 4) != HAL_OK){
		return derniere;
	}

	int8_t  msb = (int8_t)rx[1];
	uint8_t lsb = rx[2];

	float t = (float)msb;
	if(lsb & 0x80) t += 0.5f;
	if(lsb & 0x40) t += 0.25f;

	derniere = t;
	return t;
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

		if(HAL_I2C_Master_Receive(&hi2c1, SGP40_ADDR, rx, 3, 50) == HAL_OK){
			v = (uint16_t)((rx[0] << 8) | rx[1] );
		}
	}
	xSemaphoreGive(mtx_i2cHandle);
	return v;
}

uint16_t Lire_QA(){
	uint16_t raw = 0;

	// On fait 10 mesures pour satbliliser la sortie
	raw = QA_Init();
	vTaskDelay(pdMS_TO_TICKS(10));
	/*for (int i = 0; i < 10; ++i) {
		raw = QA_Init();
		vTaskDelay(pdMS_TO_TICKS(1000));
	}*/
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
	TC72_Init();

	while(1){
		float temp = Lire_Temperature();
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




