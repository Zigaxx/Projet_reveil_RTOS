/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdlib.h>
#include "ssd1306.h"
#include <stdbool.h>

#include <string.h>
#include "vl53l1_api.h"

#include "ssd1306_fonts.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
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


/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define NB_MELODIES 2 // à choisir selon les mélodies sauvegardées. Faire -1 par rapport au nombre réel
#define MAX_SEL_AFFICHAGE 3 // heure, temperature, QA, QS => 4-1=3

#define TAILLE_MAX_MSG_BLUETOOTH 20

#define TOF_PRESENT_MM 300
#define TOF_HOLD_MS 600
#define TOF_SWIPE_WINDOW_MS 1000
#define TOF_PERIOD_MS 20

#define DUREE_AUBE 30000
#define DUREE_CREP 30000
#define PERIODE_MAJ_LED 100

//Ventilateur
#define PERIODE_VENT_MS 5000
// LED Adressables
#define NB_LEDS 4
#define LED_BITS (NB_LEDS*24)
#define LED_RESET_SLOTS 48 //48 x 1,25 us = 60 us (>50 us)
#define LED_T0 32 //0,4 us
#define LED_T1 64 //0,8 us

// Balayage
#define BALAYAGE_BORNE 500        // en pas d'encodeur, à calibrer
#define BALAYAGE_PAS 50
#define BALAYAGE_MAX 5000       // sécurité si l'encodeur ne répond pas


#define PERIODE_HORLOGE  1000


#define SNOOZE_MIN  1 // Temps du snooze du reveil
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

CAN_HandleTypeDef hcan1;

I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef hlpuart1;
UART_HandleTypeDef huart2;

SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim5;
TIM_HandleTypeDef htim8;
DMA_HandleTypeDef hdma_tim3_ch4_up;

osThreadId defaultTaskHandle;
/* USER CODE BEGIN PV */

//semaphores

SemaphoreHandle_t mtx_consignesHandle;
SemaphoreHandle_t HeureMutexSemaphHandle;
SemaphoreHandle_t AlarmeMutexSemaphHandle;

// mailbbox

QueueHandle_t xQueueCommandes;
QueueHandle_t AffHandle;


const Note mel_beep[] = { {2000,200}, {0,200} };
const Note mel_1[]    = { {523,200}, {659,200}, {784,200}, {1047,400}, {0,300} };
const Note mel_2[]    = { {784,150}, {988,150}, {1175,150}, {1568,300}, {0,400} };

const Melodie melodies[] = {
		{ mel_beep, sizeof(mel_beep)/sizeof(Note) },    // 0 = beep
		{ mel_1,    sizeof(mel_1)/sizeof(Note)    },
		{ mel_2,    sizeof(mel_2)/sizeof(Note)    },
};

uint16_t led_buffer[LED_BITS + LED_RESET_SLOTS];

// DECLARATION TACHES

xTaskHandle xTaskGestionAffCrepAube;
xTaskHandle xTaskInterfaceActions;
xTaskHandle xTaskInterfaceAff;
xTaskHandle xTaskGestionHorloge;
xTaskHandle xTaskGestionAlarme;

// DECLARATION SEMAPHORES
SemaphoreHandle_t sem_ledHandle; // fin de trame DMA
SemaphoreHandle_t mtx_adcHandle;
SemaphoreHandle_t mtx_consignesHandle;
SemaphoreHandle_t mtx_i2cHandle;
SemaphoreHandle_t mtx_consignesHandle;
SemaphoreHandle_t sem_alarmeHandle;
SemaphoreHandle_t mtx_heureHandle;
SemaphoreHandle_t mtx_consignesHandle;
SemaphoreHandle_t mtx_consignesHandle;
SemaphoreHandle_t HeureMutexSemaphHandle;
SemaphoreHandle_t AlarmeMutexSemaphHandle;

// MAILBOXES
QueueHandle_t AffHandle;
QueueHandle_t InfosHandle;
QueueHandle_t AffHandle;
QueueHandle_t ActionsHandle;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);
static void MX_CAN1_Init(void);
static void MX_LPUART1_UART_Init(void);
static void MX_SPI2_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM5_Init(void);
static void MX_TIM8_Init(void);
void StartDefaultTask(void const * argument);

/* USER CODE BEGIN PFP */

bool CommandBlutoothTypeExtraction(char *cmd, uint8_t *index, char *commandType);
bool ChooseBluetoothCommandType(char * cmd, CommandeBluetoothType *type);
bool TimeExtraction(char *cmd, uint8_t *index, Heure *value);
bool SplitCommandBluetooth(char * cmd, int *seuil, Heure *horaire, CommandeBluetoothType *type);

void TofTask(void *argument);
void GestionCommandTask (void *pvParameters);

void task_gestion_aff_crep_aube(void *pvParameters);
void aff_led(uint8_t niveau);

void task_interface_actions(void *pvParameters);
void task_interface_aff(void *pvParameters);

uint16_t ADC_Lire(uint32_t canal);
// Buzzer
void Buzzer_Note(uint16_t freq);
TickType_t Delai(TickType_t echeance, TickType_t now);
// Affichaes
void Leds_Envoyer(uint8_t niveau);
void Ecran_Afficher(const Affichages *a);
// Ventilateur
void Vent_Marche();
void Vent_Arret();

void Heure_Incrementer(Heure *h);
bool Heure_Egales(Heure a, Heure b);
void Aff_ecran(Heure h, uint16_t sel, uint16_t valeur, bool retro);
void task_gestion_horloge(void *pvParameters);

void Act_alarme(bool on); // Activer la melodie de l'alarme
Heure Set_alarme(Heure base,int delta); // Reglage de l'alarme (le snooze y compris)
void task_gestion_alarme(void *pvParameters);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

//----------------------------------------------------------------------------------
//-------------Declaration de Variables globales
//----------------------------------------------------------------------------------

Consignes consignes;
Heure heure = {0,0,0};
Heure alarme = {0,0,0};

uint8_t bluetoothChar;
char bluetoothMsg[TAILLE_MAX_MSG_BLUETOOTH];
uint8_t indexBluetoothMsg;

uint16_t time_tof1;
uint16_t time_tof2;

static VL53L1_Dev_t tof[2];
static bool tofPresent[2];


//----------------------------------------------------------------------------------
//-------------Fonction pour commandes bluetooth
//----------------------------------------------------------------------------------

static void SendCmd(CommandeType t)
{
	Commande c = {0};
	c.type = t;
	xQueueSend(xQueueCommandes, &c, 0);
}

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


//----------------------------------------------------------------------------------
//-------------Fonctions pour TOFs (snooze et stop)
//----------------------------------------------------------------------------------

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


//----------------------------------------------------------------------------------
//-------------Fonctions affichage aube/crepuscule
//----------------------------------------------------------------------------------


void aff_led(uint8_t niveau){
	Affichages aff;
	aff.led_valide = true;
	aff.led_niveau = niveau;
	xQueueSend(AffHandle, &aff, pdMS_TO_TICKS(100));
}

// Déclaration des fonctions

static bool Alarme_active(){
	xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
	bool r = consignes.activer_alarme;
	xSemaphoreGive(mtx_consignesHandle);
	return r;
}

static bool Crep_active(){
	xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
	bool r = consignes.activer_crepuscule;
	xSemaphoreGive(mtx_consignesHandle);
	return r;
}

//----------------------------------------------------------------------------------
//-------------Fonctions ADC
//----------------------------------------------------------------------------------

uint16_t ADC_Lire(uint32_t canal)
{
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

//----------------------------------------------------------------------------------
//-------------Fonctions Buzzer
//----------------------------------------------------------------------------------


void Buzzer_Note(uint16_t freq)
{
	if(freq == 0){
		__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_1,0);
	}
	else{
		uint32_t arr = 1000000/freq -1;
		__HAL_TIM_SET_AUTORELOAD(&htim8,arr);
		__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_1,arr/2); // On mets le volume à 50 %
	}
}

TickType_t Delai(TickType_t echeance, TickType_t now)
{
	int d = echeance - now;
	if(d > 0){
		return (TickType_t)d;
	}
	return 0;
}

//----------------------------------------------------------------------------------
//-------------Fonctions Leds
//----------------------------------------------------------------------------------

void Leds_Envoyer(uint8_t niveau){
	uint32_t n = ((uint32_t)niveau * niveau) / 255;
	uint32_t r = n;
	uint32_t g = (n * 150) / 255;
	uint32_t b = (n * 30) / 255;
	uint32_t grb = (g << 16) | (r << 8) | b;

	for (int i = 0; i < NB_LEDS; i++) {
		for (int j = 0; j < 24; ++j) {
			led_buffer[i*24 + j] = (grb & (1UL << (23 - j))) ? LED_T1 : LED_T0;
		}
	}

	for (int i = 0; i < LED_RESET_SLOTS; ++i) {
		led_buffer[LED_BITS + i] = 0;
	}

	HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_4, (uint32_t*)led_buffer, LED_BITS + LED_RESET_SLOTS);
	xSemaphoreTake(sem_ledHandle, pdMS_TO_TICKS(20));
	HAL_TIM_PWM_Stop_DMA(&htim3, TIM_CHANNEL_4);
}


//----------------------------------------------------------------------------------
//-------------Fonctions Affichage
//----------------------------------------------------------------------------------

void Ecran_Afficher(const Affichages *a){
	char l1[16], l2[20];

	snprintf(l1, sizeof(l1), "%02u:%02u:%02u",(unsigned)a->ecran_heure.h, (unsigned)a->ecran_heure.min, (unsigned)a->ecran_heure.sec);

	switch (a->ecran_sel) {
	case 0:
		snprintf(l2, sizeof(l2), "%02u:%02u:%02u",(unsigned)alarme.h, (unsigned)alarme.min, (unsigned)alarme.sec);
		break;
	case 1:
		snprintf(l2, sizeof(l2), "Temp: %u C", (unsigned)a->ecran_valeur);
		break;
	case 2:
		snprintf(l2, sizeof(l2), "QA: %u ppm", (unsigned)a->ecran_valeur);
		break;
	case 3:
		snprintf(l2, sizeof(l2), "Sommeil: %u", (unsigned)a->ecran_valeur);
		break;
	default:
		l2[0] = '\0';
		break;
	}

	if (a->ecran_retro)
	{
		ssd1306_Fill(White);  // Fond blanc
		// Écrire le texte en noir
	}
	else
	{
		ssd1306_Fill(Black);  // Fond noir
		// Écrire le texte en blanc
	}
	ssd1306_SetCursor(0, 0);
	ssd1306_WriteString(l1, Font_11x18, White);
	ssd1306_SetCursor(0, 30);
	ssd1306_WriteString(l2, Font_11x18, White);
	xSemaphoreTake(mtx_i2cHandle, portMAX_DELAY);
	ssd1306_UpdateScreen();


	xSemaphoreGive(mtx_i2cHandle);
}

//----------------------------------------------------------------------------------
//-------------Fonctions Ventilateur
//----------------------------------------------------------------------------------

void Vent_Marche(){
	uint16_t pot = ADC_Lire(ADC_CHANNEL_11);
	__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_1,(uint32_t)pot * 3999 / 4095);
	HAL_GPIO_WritePin(DIS_GPIO_Port, DIS_Pin, GPIO_PIN_RESET); // activation du moteur
}

void Vent_Arret(){
	__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_1,0);
	HAL_GPIO_WritePin(DIS_GPIO_Port, DIS_Pin, GPIO_PIN_SET); // désactivation du moteur
}

//----------------------------------------------------------------------------------
//-------------Fonctions Alarme
//----------------------------------------------------------------------------------

void Act_alarme(bool on){
	Actions action;
	action.alarme_valide = true;
	action.act_alarme = on;
	xQueueSend(ActionsHandle, &action, pdMS_TO_TICKS(100));
}

Heure Set_alarme(Heure base,int delta){
	Heure alarme = base;
	int total = base.h*60 + base.min + delta;
	alarme.h = total / 60;
	alarme.min = total % 60;
	alarme.sec = base.sec;
	return alarme;
}



/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{

	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_DMA_Init();
	MX_USART2_UART_Init();
	MX_I2C1_Init();
	MX_ADC1_Init();
	MX_CAN1_Init();
	MX_LPUART1_UART_Init();
	MX_SPI2_Init();
	MX_TIM2_Init();
	MX_TIM3_Init();
	MX_TIM5_Init();
	MX_TIM8_Init();
	/* USER CODE BEGIN 2 */

	HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);                   // Lancement ventilateur (PA5)
	HAL_TIM_Encoder_Start(&htim5, TIM_CHANNEL_ALL);             // Lancement encodeur
	HAL_GPIO_WritePin(DIS_GPIO_Port, DIS_Pin, GPIO_PIN_SET);    // moteur désactivé au départ
	// moteur désactivé au départ
	HAL_UART_Receive_IT(&hlpuart1, (uint8_t *) &bluetoothChar, 1);
	/* USER CODE END 2 */

	/* USER CODE BEGIN RTOS_MUTEX */
	/* add mutexes, ... */
	/* USER CODE END RTOS_MUTEX */

	/* USER CODE BEGIN RTOS_SEMAPHORES */
	/* add semaphores, ... */
	mtx_consignesHandle = xSemaphoreCreateMutex();
	sem_ledHandle = xSemaphoreCreateBinary();
	mtx_adcHandle = xSemaphoreCreateMutex();
	mtx_i2cHandle = xSemaphoreCreateMutex();
	sem_alarmeHandle = xSemaphoreCreateBinary();
	mtx_consignesHandle = xSemaphoreCreateMutex();
	mtx_heureHandle = xSemaphoreCreateMutex();
	HeureMutexSemaphHandle = xSemaphoreCreateMutex();
	AlarmeMutexSemaphHandle = xSemaphoreCreateMutex();
	/* USER CODE END RTOS_SEMAPHORES */

	/* USER CODE BEGIN RTOS_QUEUES */
	/* add queues, ... */
	InfosHandle = xQueueCreate(1, sizeof(Infos) );
	xQueueCommandes = xQueueCreate(10, sizeof(Commande));
	AffHandle = xQueueCreate(4, sizeof(Affichages) );
	ActionsHandle = xQueueCreate(4, sizeof( int) );
	/* USER CODE END RTOS_QUEUES */

	/* USER CODE BEGIN RTOS_TIMERS */
	/* start timers, add new ones, ... */
	/* USER CODE END RTOS_TIMERS */

	/* USER CODE BEGIN RTOS_THREADS */
	xTaskCreate(task_gestion_alarme,       /* Function that implements the task. */
			"TASK_GESTION_ALARME",          /* Text name for the task. */
			1024,      /* Stack size in words, not bytes. */
			NULL,    /* Parameter passed into the task. */
			tskIDLE_PRIORITY+10,/* Priority at which the task is created. */
			&xTaskGestionAlarme  );      /* Used to pass out the created task's handle. */
	/* add threads, ... */
	/* USER CODE END RTOS_THREADS */

	/* USER CODE BEGIN RTOS_THREADS */
	osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
	defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

	xTaskCreate(task_gestion_horloge,       /* Function that implements the task. */
			"TASK_GESTION_HORLOGE",          /* Text name for the task. */
			1024,      /* Stack size in words, not bytes. */
			NULL,    /* Parameter passed into the task. */
			tskIDLE_PRIORITY+11,/* Priority at which the task is created. */
			&xTaskGestionHorloge );      /* Used to pass out the created task's handle. */

	//	xTaskCreate(task_interface_actions,       /* Function that implements the task. */
	//			"TASK_INTERFACE_ACTIONS",          /* Text name for the task. */
	//			1024,      /* Stack size in words, not bytes. */
	//			NULL,    /* Parameter passed into the task. */
	//			tskIDLE_PRIORITY+6,/* Priority at which the task is created. */
	//			&xTaskInterfaceActions  );      /* Used to pass out the created task's handle. */

	xTaskCreate(task_interface_aff,       /* Function that implements the task. */
			"TASK_INTERFACE_AFF",          /* Text name for the task. */
			1024,      /* Stack size in words, not bytes. */
			NULL,    /* Parameter passed into the task. */
			tskIDLE_PRIORITY+7,/* Priority at which the task is created. */
			&xTaskInterfaceAff  );      /* Used to pass out the created task's handle. */

	/* USER CODE BEGIN RTOS_THREADS */
	xTaskCreate(task_gestion_aff_crep_aube,       /* Function that implements the task. */
			"TASK_GESTION_AFF_CREP_AUBE",          /* Text name for the task. */
			1024,      /* Stack size in words, not bytes. */
			NULL,    /* Parameter passed into the task. */
			tskIDLE_PRIORITY+8,/* Priority at which the task is created. */
			NULL );      /* Used to pass out the created task's handle. */

	/* add threads, ... */
	xTaskCreate(TofTask,       /* Function that implements the task. */
			"TofTask",          /* Text name for the task. */
			1024,      /* Stack size in words, not bytes. */
			NULL,    /* Parameter passed into the task. */
			tskIDLE_PRIORITY+5,/* Priority at which the task is created. */
			NULL );      /* Used to pass out the created task's handle. */

	xTaskCreate(GestionCommandTask,       /* Function that implements the task. */
			"Commandes",          /* Text name for the task. */
			1024,      /* Stack size in words, not bytes. */
			NULL,    /* Parameter passed into the task. */
			tskIDLE_PRIORITY+9,/* Priority at which the task is created. */
			NULL );      /* Used to pass out the created task's handle. */
	/* USER CODE END RTOS_THREADS */

	/* Start scheduler */
	osKernelStart();

	/* We should never get here as control is now taken by the scheduler */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1)
	{
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/** Configure the main internal regulator output voltage
	 */
	if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = 1;
	RCC_OscInitStruct.PLL.PLLN = 10;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
	RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
	RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
			|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
	{
		Error_Handler();
	}
}

/**
 * @brief ADC1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC1_Init(void)
{

	/* USER CODE BEGIN ADC1_Init 0 */

	/* USER CODE END ADC1_Init 0 */

	ADC_MultiModeTypeDef multimode = {0};
	ADC_ChannelConfTypeDef sConfig = {0};

	/* USER CODE BEGIN ADC1_Init 1 */

	/* USER CODE END ADC1_Init 1 */


	/** Common config
	 */
	hadc1.Instance = ADC1;
	hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
	hadc1.Init.Resolution = ADC_RESOLUTION_12B;
	hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
	hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
	hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
	hadc1.Init.LowPowerAutoWait = DISABLE;
	hadc1.Init.ContinuousConvMode = DISABLE;
	hadc1.Init.NbrOfConversion = 2;
	hadc1.Init.DiscontinuousConvMode = DISABLE;
	hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
	hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
	hadc1.Init.DMAContinuousRequests = DISABLE;
	hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
	hadc1.Init.OversamplingMode = DISABLE;
	if (HAL_ADC_Init(&hadc1) != HAL_OK)
	{
		Error_Handler();
	}

	/** Configure the ADC multi-mode
	 */
	multimode.Mode = ADC_MODE_INDEPENDENT;
	if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
	{
		Error_Handler();
	}

	/** Configure Regular Channel
	 */
	sConfig.Channel = ADC_CHANNEL_13;
	sConfig.Rank = ADC_REGULAR_RANK_1;
	sConfig.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
	sConfig.SingleDiff = ADC_SINGLE_ENDED;
	sConfig.OffsetNumber = ADC_OFFSET_NONE;
	sConfig.Offset = 0;
	if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
	{
		Error_Handler();
	}

	/** Configure Regular Channel
	 */
	sConfig.Channel = ADC_CHANNEL_11;
	sConfig.Rank = ADC_REGULAR_RANK_2;
	if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN ADC1_Init 2 */


	/* USER CODE END ADC1_Init 2 */

}

/**
 * @brief CAN1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_CAN1_Init(void)
{

	/* USER CODE BEGIN CAN1_Init 0 */

	/* USER CODE END CAN1_Init 0 */

	/* USER CODE BEGIN CAN1_Init 1 */

	/* USER CODE END CAN1_Init 1 */
	hcan1.Instance = CAN1;
	hcan1.Init.Prescaler = 16;
	hcan1.Init.Mode = CAN_MODE_NORMAL;
	hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
	hcan1.Init.TimeSeg1 = CAN_BS1_7TQ;
	hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
	hcan1.Init.TimeTriggeredMode = DISABLE;
	hcan1.Init.AutoBusOff = DISABLE;
	hcan1.Init.AutoWakeUp = DISABLE;
	hcan1.Init.AutoRetransmission = DISABLE;
	hcan1.Init.ReceiveFifoLocked = DISABLE;
	hcan1.Init.TransmitFifoPriority = DISABLE;
	if (HAL_CAN_Init(&hcan1) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN CAN1_Init 2 */

	/* USER CODE END CAN1_Init 2 */

}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void)
{

	/* USER CODE BEGIN I2C1_Init 0 */

	/* USER CODE END I2C1_Init 0 */

	/* USER CODE BEGIN I2C1_Init 1 */

	/* USER CODE END I2C1_Init 1 */
	hi2c1.Instance = I2C1;
	hi2c1.Init.Timing = 0x00F12981;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK)
	{
		Error_Handler();
	}

	/** Configure Analogue filter
	 */
	if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
	{
		Error_Handler();
	}

	/** Configure Digital filter
	 */
	if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN I2C1_Init 2 */

	/* USER CODE END I2C1_Init 2 */

}

/**
 * @brief LPUART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_LPUART1_UART_Init(void)
{

	/* USER CODE BEGIN LPUART1_Init 0 */

	/* USER CODE END LPUART1_Init 0 */

	/* USER CODE BEGIN LPUART1_Init 1 */

	/* USER CODE END LPUART1_Init 1 */
	hlpuart1.Instance = LPUART1;
	hlpuart1.Init.BaudRate = 9600;
	hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
	hlpuart1.Init.StopBits = UART_STOPBITS_1;
	hlpuart1.Init.Parity = UART_PARITY_NONE;
	hlpuart1.Init.Mode = UART_MODE_TX_RX;
	hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&hlpuart1) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN LPUART1_Init 2 */

	/* USER CODE END LPUART1_Init 2 */

}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void)
{

	/* USER CODE BEGIN USART2_Init 0 */

	/* USER CODE END USART2_Init 0 */

	/* USER CODE BEGIN USART2_Init 1 */

	/* USER CODE END USART2_Init 1 */
	huart2.Instance = USART2;
	huart2.Init.BaudRate = 115200;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart2) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN USART2_Init 2 */

	/* USER CODE END USART2_Init 2 */

}

/**
 * @brief SPI2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI2_Init(void)
{

	/* USER CODE BEGIN SPI2_Init 0 */

	/* USER CODE END SPI2_Init 0 */

	/* USER CODE BEGIN SPI2_Init 1 */

	/* USER CODE END SPI2_Init 1 */
	/* SPI2 parameter configuration*/
	hspi2.Instance = SPI2;
	hspi2.Init.Mode = SPI_MODE_MASTER;
	hspi2.Init.Direction = SPI_DIRECTION_2LINES;
	hspi2.Init.DataSize = SPI_DATASIZE_4BIT;
	hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi2.Init.NSS = SPI_NSS_SOFT;
	hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
	hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi2.Init.CRCPolynomial = 7;
	hspi2.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
	hspi2.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
	if (HAL_SPI_Init(&hspi2) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN SPI2_Init 2 */

	/* USER CODE END SPI2_Init 2 */

}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void)
{

	/* USER CODE BEGIN TIM2_Init 0 */

	/* USER CODE END TIM2_Init 0 */

	TIM_MasterConfigTypeDef sMasterConfig = {0};
	TIM_OC_InitTypeDef sConfigOC = {0};

	/* USER CODE BEGIN TIM2_Init 1 */

	/* USER CODE END TIM2_Init 1 */
	htim2.Instance = TIM2;
	htim2.Init.Prescaler = 0;
	htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim2.Init.Period = 3999;
	htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
	{
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
	{
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN TIM2_Init 2 */


	/* USER CODE END TIM2_Init 2 */
	HAL_TIM_MspPostInit(&htim2);

}

/**
 * @brief TIM3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM3_Init(void)
{

	/* USER CODE BEGIN TIM3_Init 0 */

	/* USER CODE END TIM3_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};
	TIM_OC_InitTypeDef sConfigOC = {0};

	/* USER CODE BEGIN TIM3_Init 1 */

	/* USER CODE END TIM3_Init 1 */
	htim3.Instance = TIM3;
	htim3.Init.Prescaler = 0;
	htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim3.Init.Period = 99-1;
	htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
	{
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
	{
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
	{
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN TIM3_Init 2 */

	/* USER CODE END TIM3_Init 2 */
	HAL_TIM_MspPostInit(&htim3);

}

/**
 * @brief TIM5 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM5_Init(void)
{

	/* USER CODE BEGIN TIM5_Init 0 */

	/* USER CODE END TIM5_Init 0 */

	TIM_Encoder_InitTypeDef sConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};

	/* USER CODE BEGIN TIM5_Init 1 */

	/* USER CODE END TIM5_Init 1 */
	htim5.Instance = TIM5;
	htim5.Init.Prescaler = 0;
	htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim5.Init.Period = 4294967295;
	htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
	sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
	sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
	sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
	sConfig.IC1Filter = 0;
	sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
	sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
	sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
	sConfig.IC2Filter = 0;
	if (HAL_TIM_Encoder_Init(&htim5, &sConfig) != HAL_OK)
	{
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN TIM5_Init 2 */


	/* USER CODE END TIM5_Init 2 */

}

/**
 * @brief TIM8 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM8_Init(void)
{

	/* USER CODE BEGIN TIM8_Init 0 */

	/* USER CODE END TIM8_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};
	TIM_OC_InitTypeDef sConfigOC = {0};
	TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

	/* USER CODE BEGIN TIM8_Init 1 */

	/* USER CODE END TIM8_Init 1 */
	htim8.Instance = TIM8;
	htim8.Init.Prescaler = 79;
	htim8.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim8.Init.Period = 65535;
	htim8.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim8.Init.RepetitionCounter = 0;
	htim8.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim8) != HAL_OK)
	{
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim8, &sClockSourceConfig) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_TIM_PWM_Init(&htim8) != HAL_OK)
	{
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim8, &sMasterConfig) != HAL_OK)
	{
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
	sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
	if (HAL_TIM_PWM_ConfigChannel(&htim8, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
	{
		Error_Handler();
	}
	sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
	sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
	sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
	sBreakDeadTimeConfig.DeadTime = 0;
	sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
	sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
	sBreakDeadTimeConfig.BreakFilter = 0;
	sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
	sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
	sBreakDeadTimeConfig.Break2Filter = 0;
	sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
	if (HAL_TIMEx_ConfigBreakDeadTime(&htim8, &sBreakDeadTimeConfig) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN TIM8_Init 2 */

	/* USER CODE END TIM8_Init 2 */
	HAL_TIM_MspPostInit(&htim8);

}

/**

 * Enable DMA controller clock
 */
static void MX_DMA_Init(void)
{


	/* DMA controller clock enable */
	__HAL_RCC_DMA1_CLK_ENABLE();

	/* DMA interrupt init */
	/* DMA1_Channel3_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);

}

/**
<<<<<<< HEAD
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */

static void MX_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	/* USER CODE BEGIN MX_GPIO_Init_1 */

	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOH, SHUT1_Pin|SHUT2_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(DIS_GPIO_Port, DIS_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin : B1_Pin */
	GPIO_InitStruct.Pin = B1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : SHUT1_Pin SHUT2_Pin */
	GPIO_InitStruct.Pin = SHUT1_Pin|SHUT2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

	/*Configure GPIO pin : SW2_Pin */
	GPIO_InitStruct.Pin = SW2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(SW2_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : DIR_Pin */
	GPIO_InitStruct.Pin = DIR_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(DIR_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : SW1_Pin */
	GPIO_InitStruct.Pin = SW1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(SW1_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : INTOF2_Pin INTOF1_Pin */
	GPIO_InitStruct.Pin = INTOF2_Pin|INTOF1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pin : DIS_Pin */
	GPIO_InitStruct.Pin = DIS_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(DIS_GPIO_Port, &GPIO_InitStruct);

	/* EXTI interrupt init*/
	HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

	/* USER CODE BEGIN MX_GPIO_Init_2 */


	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void Heure_Incrementer(Heure *h){
	h->sec++;
	if(h->sec >= 60){
		h->sec = 0;
		h->min++;
	}
	if(h->min >= 60){
		h->min = 0;
		h->h++;
	}
	if(h->h >= 24){
		h->h++;
	}
}

bool Heure_Egales(Heure a, Heure b){
	if( (a.h == b.h) && (a.min == b.min) && (a.sec == b.sec)){
		return true;
	}
	return false;
}

void Aff_ecran(Heure h, uint16_t sel, uint16_t valeur, bool retro){
	Affichages aff;
	aff.ecran_heure = h;
	aff.ecran_sel = sel;
	aff.ecran_valide = true;
	aff.ecran_retro = retro;
	aff.ecran_valeur = valeur;
	xQueueSend(AffHandle, &aff, pdMS_TO_TICKS(10));   // si pleine, la MAJ de la seconde suivante rattrapera
}

void task_gestion_horloge(void *pvParameters){
	TickType_t dernier = xTaskGetTickCount();
	Infos infos;

	while(1){
		vTaskDelayUntil(&dernier, pdMS_TO_TICKS(PERIODE_HORLOGE));

		// Incrementation heure
		Heure h;
		xSemaphoreTake(mtx_heureHandle, portMAX_DELAY);
		Heure_Incrementer(&heure);
		h = heure; // Copie locale pour la comparaison avec l'alarme
		xSemaphoreGive(mtx_heureHandle);

		bool mesure_ok = (xQueuePeek(InfosHandle, &infos, 0) == pdPASS); // dernières mesures (Peek, comme pour le ventilateur)

		// Alarme
		xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
		bool declencher = Heure_Egales(h, alarme) && !consignes.activer_alarme;
		if(declencher){
			consignes.activer_alarme = true; // Pour la gestion de l'aube
		}

		// Retroeclairage
		if(mesure_ok){
			consignes.activer_retroeclairage = (infos.luminosite < consignes.seuil_luminosite);
		}
		bool retro = consignes.activer_retroeclairage;
		uint8_t sel = consignes.sel_affichage;
		xSemaphoreGive(mtx_consignesHandle);

		if(declencher){
			xSemaphoreGive(sem_alarmeHandle); // reveil de la task de gestion d'alarme
		}
		// Selection affichage
		uint8_t valeur =0;
		switch(sel){
		case 1: valeur = infos.temperature;
		case 2: valeur = infos.qualite_air;
		case 3: valeur = infos.qualite_sommeil;
		}
		// Actualisation ecran
		Aff_ecran(h, sel, valeur, retro);
	}
}


//----------------------------------------------------------------------------------
//-------------Tache Affichage_aube/crep
//----------------------------------------------------------------------------------

void task_gestion_aff_crep_aube(void *pvParameters){

	while(1)
	{
		if(Alarme_active())
		{
			uint32_t t = 0;

			while(Alarme_active())
			{
				if(t > DUREE_AUBE)
				{
					t = DUREE_AUBE;
				}
				aff_led((uint8_t)((t * 255) / DUREE_AUBE));
				vTaskDelay(pdMS_TO_TICKS(PERIODE_MAJ_LED));
				t += PERIODE_MAJ_LED;
			}
			aff_led(0);
		}
		else if(Crep_active())
		{
			uint32_t t = 0;

			while(t <= DUREE_CREP && Crep_active() && !Alarme_active())
			{
				aff_led((uint8_t)(255 - (t * 255) / DUREE_CREP));
				vTaskDelay(pdMS_TO_TICKS(PERIODE_MAJ_LED));
				t += PERIODE_MAJ_LED;
			}
			aff_led(0);
			xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
			consignes.activer_crepuscule = false;
			xSemaphoreGive(mtx_consignesHandle);
		}
		else
		{
			vTaskDelay(pdMS_TO_TICKS(PERIODE_MAJ_LED)); // Contuniation de la mélodie
		}
	}
}


//----------------------------------------------------------------------------------
//-------------Tache gestion commandes
//----------------------------------------------------------------------------------

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

//----------------------------------------------------------------------------------
//-------------Tache TOF stop/snooze
//----------------------------------------------------------------------------------

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

//----------------------------------------------------------------------------------
//-------------Tache pour la gestion du buzzer et du ventilateur
//----------------------------------------------------------------------------------
/*
void task_interface_actions(void *pvParameters)
{
	Actions action;
	bool sonne = false, balaye = false;
	uint8_t mel, idx;
	uint8_t sens = 1;
	TickType_t t_note, t_bal, t_inversion;

	while(1){
		// Attente prochaine action
		TickType_t now = xTaskGetTickCount();
		TickType_t wait = portMAX_DELAY;

		if(sonne){
			wait = Delai(t_note,now);
		}

		if(balaye){
			TickType_t d = Delai(t_bal,now);
			if(d < wait){
				wait = d;
			}
		}
		// Reception ordre

		if(action.ventilo_valide){
			if (action.act_rafraichissement || action.act_balayage){
				Vent_Marche();
				balaye = action.act_balayage;
				if(balaye){
					__HAL_TIM_SET_COUNTER(&htim5,0);
					sens =1;
					HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, GPIO_PIN_RESET);
					t_bal = t_inversion = xTaskGetTickCount();
				}
			}
			else{
				balaye = false;
				Vent_Arret();
			}
		}

		if(action.alarme_valide){
			if(action.act_alarme && !sonne){
				xSemaphoreTake(mtx_consignesHandle,portMAX_DELAY);
				if(consignes.sel_melodie < NB_MELODIES){
					mel = consignes.sel_melodie;
				}
				else{
					mel = 0;
				}
				xSemaphoreGive(mtx_consignesHandle);
				idx = 0;
				sonne = true;
				HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_4);
				t_note = xTaskGetTickCount();
			}
			else if(!action.act_alarme){
				sonne = false;
				HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_4);
			}
		}
		// Action
		now = xTaskGetTickCount();

		if(sonne && (now - t_note) >= 0){
			const Note* n= &melodies[mel].notes[idx];
			Buzzer_Note(n->feq);
			t_note = now + pdMS_TO_TICKS(n->duree);
			idx = (idx +1) % melodies[mel].nb;
		}

		if(balaye && (now - t_note) >= 0){
			int pos = __HAL_TIM_GET_COUNTER(&htim5);
			bool inverser = (sens > 0 && pos >= BALAYAGE_BORNE) ||
					(sens < 0 && pos <= BALAYAGE_BORNE) ||
					((now - t_inversion) > pdMS_TO_TICKS(BALAYAGE_MAX));

			if(inverser){
				sens = -sens;
				HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, (sens > 0) ? GPIO_PIN_RESET : GPIO_PIN_SET);
				t_inversion = now;
			}
			Vent_Marche();
			t_bal = now + pdMS_TO_TICKS(PERIODE_VENT_MS);
		}

		vTaskDelay(pdMS_TO_TICKS(20));
	}
}
 */

//----------------------------------------------------------------------------------
//-------------tache interface de l'écran Oled
//----------------------------------------------------------------------------------

void task_interface_aff(void *pvParameters){
	Affichages aff;
	xSemaphoreTake(mtx_i2cHandle, portMAX_DELAY);
	ssd1306_Init();
	xSemaphoreGive(mtx_i2cHandle);

	while(1){
		if(xQueueReceive(AffHandle, &aff, portMAX_DELAY) == pdPASS){
			if(aff.led_valide){
				Leds_Envoyer(aff.led_niveau);
			}

			if(aff.ecran_valide){
				Ecran_Afficher(&aff);
			}
		}
	}
}

void task_gestion_alarme(void *pvParameters){
	Heure alarme_base;
	bool snooze_actif = false;
	while(1)
	{
		xSemaphoreTake(sem_alarmeHandle, portMAX_DELAY); // On bloque jusqu'au signla activer_alarme

		xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY); // Sauvegarde alarme de base
		if(snooze_actif == false)
		{
			alarme_base = alarme;
		}

		consignes.stop_alarme = false;
		consignes.snooze_alarme = false;
		xSemaphoreGive(mtx_consignesHandle);

		Act_alarme(true);

		// Attente stop ou snooze
		bool fini = false;
		while(fini != true)
		{
			bool stop,snooze;
			xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
			stop = consignes.stop_alarme;
			snooze = consignes.snooze_alarme;
			xSemaphoreGive(mtx_consignesHandle);

			if(stop == true)
			{
				if(snooze_actif == true){// Reconduite du reveil au lendemain
					xSemaphoreTake(AlarmeMutexSemaphHandle,portMAX_DELAY);
					alarme = alarme_base;
					xSemaphoreGive(AlarmeMutexSemaphHandle);
				}
				xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
				consignes.activer_alarme = false;
				consignes.stop_alarme = false;
				xSemaphoreGive(mtx_consignesHandle);
				snooze_actif = false;
				fini = true;
			}
			else if(snooze == true)
			{
				xSemaphoreTake(AlarmeMutexSemaphHandle,portMAX_DELAY);
				alarme = Set_alarme(alarme,SNOOZE_MIN);
				xSemaphoreGive(AlarmeMutexSemaphHandle);
				xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
				consignes.activer_alarme = false;
				consignes.stop_alarme = false;
				xSemaphoreGive(mtx_consignesHandle);
				snooze_actif = true;
				fini = true;
			}
			else
			{
				vTaskDelay(pdMS_TO_TICKS(50)); // Contuniation de la mélodie
			}
		}
		Act_alarme(false);
	}
}

//----------------------------------------------------------------------------------
//-------------Callback
//----------------------------------------------------------------------------------

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	Commande cmd;
	BaseType_t woken = pdFALSE; //sert à reveiller la tache gestion de commande

	if(GPIO_Pin == SW1_Pin)
	{
		cmd.type = CMD_SET_AFFICHAGE;
	}
	else if(GPIO_Pin == SW2_Pin)
	{
		cmd.type = CMD_SET_MELODIE;
	}
	else if(GPIO_Pin == B1_Pin)
	{
		cmd.type = CMD_SET_CREPUSCULE;
	}

	xQueueSendFromISR(xQueueCommandes,&cmd,&woken);
	portYIELD_FROM_ISR(woken);
}

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



// Buzzer
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
	if(htim->Instance == TIM3){
		BaseType_t woken = pdFALSE;
		xSemaphoreGiveFromISR(sem_ledHandle,&woken);
		portYIELD_FROM_ISR(woken);
	}
}

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
	/* USER CODE BEGIN 5 */
	/* Infinite loop */
	for(;;)
	{
		osDelay(1);
	}
	/* USER CODE END 5 */
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM6 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	/* USER CODE BEGIN Callback 0 */

	/* USER CODE END Callback 0 */
	if (htim->Instance == TIM6)
	{
		HAL_IncTick();
	}
	/* USER CODE BEGIN Callback 1 */

	/* USER CODE END Callback 1 */
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
	}
	/* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
	/* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
