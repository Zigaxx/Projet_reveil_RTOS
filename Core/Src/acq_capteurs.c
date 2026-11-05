#include "app_rtos.h"


uint16_t sgp40_baseline = 0;

uint16_t ADC_Lire(uint32_t canal){
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


float Lire_Temperature(){
    uint8_t cmd_std[2] = {0x80, 0x00}; // 0x80 = Adresse registre de contrôle (écriture), 0x00 = Mode Conversion Continue (recommandé pour éviter les blocages one-shot)
    uint8_t reg_temp_msb = 0x02;       // Adresse de début de lecture (MSB de la température)
    uint8_t rx[2] = {0};

    // 1. Configuration du capteur (Mode continu pour actualiser la température en permanence)
    HAL_GPIO_WritePin(CE_GPIO_Port, CE_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(CSN_GPIO_Port, CSN_Pin, GPIO_PIN_RESET);

    if(HAL_SPI_Transmit(&hspi2, cmd_std, 2, 10) != HAL_OK) {
        HAL_GPIO_WritePin(CSN_GPIO_Port, CSN_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(CE_GPIO_Port, CE_Pin, GPIO_PIN_RESET);
        return -1000.00f;
    }
    HAL_GPIO_WritePin(CSN_GPIO_Port, CSN_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(CE_GPIO_Port, CE_Pin, GPIO_PIN_RESET);

    // Petite pause pour laisser le composant respirer
    vTaskDelay(pdMS_TO_TICKS(200));

    // 2. Lecture séquentielle de la température (MSB à 0x02 puis LSB automatiquement à 0x03)
    HAL_GPIO_WritePin(CE_GPIO_Port, CE_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(CSN_GPIO_Port, CSN_Pin, GPIO_PIN_RESET);

    // Envoi de l'adresse de départ du registre MSB (bit MSB à 0 pour une lecture -> 0x02)
    if(HAL_SPI_Transmit(&hspi2, &reg_temp_msb, 1, 10) != HAL_OK) {
        HAL_GPIO_WritePin(CSN_GPIO_Port, CSN_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(CE_GPIO_Port, CSN_Pin, GPIO_PIN_RESET);
        return -1000.00f;
    }

    // Réception de 2 octets d'affilée : rx[0] = MSB, rx[1] = LSB
    if(HAL_SPI_Receive(&hspi2, rx, 2, 10) != HAL_OK) {
        HAL_GPIO_WritePin(CSN_GPIO_Port, CSN_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(CE_GPIO_Port, CSN_Pin, GPIO_PIN_RESET);
        return -1000.00f;
    }

    HAL_GPIO_WritePin(CSN_GPIO_Port, CSN_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(CE_GPIO_Port, CE_Pin, GPIO_PIN_RESET);

    // 3. Extraction et calcul de la température
    int8_t msb = (int8_t)rx[0];  // Partie entière signée (gère les températures négatives)
    uint8_t lsb = rx[1];         // Partie décimale

    float decimale = 0.0f;
    if (lsb & 0x80) decimale += 0.5f;   // Bit 7 du LSB = 0.5°C
    if (lsb & 0x40) decimale += 0.25f;  // Bit 6 du LSB = 0.25°C

    float temperature = (float)msb + decimale;
    return temperature;
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




