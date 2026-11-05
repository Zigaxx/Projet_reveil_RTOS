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
#include "ssd1306_fonts.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PERIODE_ACQ_MS (5*60*1000)
#define NB_TOF 4
#define LDR_ADC ADC_CHANNEL_13

#define SGP40_ADDR     (0x59 << 1)

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
#define PERIODE_HORLOGE  1000#define DUREE_AUBE 30000
#define DUREE_CREP 30000
#define PERIODE_MAJ_LED 100#define SNOOZE_MIN  10 // Temps du snooze du reveil
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



#define NB_MELODIES  (sizeof(melodies)/sizeof(melodies[0]))

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
	Heure alarme;
	bool stop_alarme;
	bool snooze_alarme;
	bool activer_alarme;
	bool activer_crepuscule;
	bool activer_retroeclairage;
	uint16_t seuil_luminosite;
	uint16_t seuil_temperature;
	uint16_t seuil_qa;
} Consignes;

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

typedef struct
{
	uint16_t temperature;
	uint16_t luminosite;
	uint16_t qualite_air;
	uint16_t qualite_sommeil;
}Infos;

volatile int16_t temp_comp_c = 25;            // température utilisée pour compenser le SGP40
uint16_t sgp40_baseline = 0;

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
xTaskHandle xTaskAcqLumi;
xTaskHandle xTaskAcqTempQA;
xTaskHandle xTaskAcqTof;
xTaskHandle xTaskInterfaceActions;
xTaskHandle xTaskInterfaceAff;

void task_acq_luminosite(void *pvParameters);
void task_acq_temp_qa(void *pvParameters);
void task_acq_tof(void *pvParameters);
void task_interface_actions(void *pvParameters);
void task_interface_aff(void *pvParameters);
// DECLARATION SEMAPHORES
xSemaphoreHandle sem_ledHandle; // fin de trame DMA
xSemaphoreHandle mtx_infosHandle;
xSemaphoreHandle mtx_consignesHandle;
xSemaphoreHandle mtx_adcHandle;
xSemaphoreHandle mtx_i2cHandle;
// MAILBOXES
xQueueHandle InfosHandle;
xQueueHandle AffHandle;

// FONCTIONS
uint16_t ADC_Lire(uint32_t canal);

uint16_t Lire_Luminosite();

void Temp_Init();
uint16_t Lire_Temperature();

uint16_t QA_Init();
uint16_t Lire_QA();

void ToF_Init();
bool ToF_Lire(uint16_t distances[NB_TOF]);
uint16_t Calcul_QS(const uint16_t distances[NB_TOF]);
// Buzzer
void Buzzer_Note(uint16_t freq);
TickType_t Delai(TickType_t echeance, TickType_t now);
// Affichaes
void Leds_Envoyer(uint8_t niveau);
void Ecran_Afficher(const Affichages *a);
// Ventilateur
void Vent_Marche();
void Vent_Arret();

typedef struct
{
	uint16_t temperature;
	uint16_t luminosite;
	uint16_t qualite_air;
	uint16_t qualite_sommeil;
}Infos;

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


// DECLARATION TACHES
xTaskHandle xTaskGestionHorloge;

void task_gestion_horloge(void *pvParameters);
// DECLARATION SEMAPHORES
xSemaphoreHandle mtx_consignesHandle;
xSemaphoreHandle sem_alarmeHandle;
xSemaphoreHandle mtx_heureHandle;
// MAILBOXES
xQueueHandle InfosHandle;
xQueueHandle AffHandle;

// FONCTIONS
void Heure_Incrementer(Heure *h);
bool Heure_Egales(Heure a, Heure b);
void Aff_ecran(Heure h, uint16_t sel, uint16_t valeur, bool retro);




// DECLARATION TACHES
xTaskHandle xTaskGestionAffCrepAube;

void task_gestion_aff_crep_aube(void *pvParameters);
// DECLARATION SEMAPHORES
xSemaphoreHandle mtx_consignesHandle;
// MAILBOXES
QueueHandle_t AffHandle;

// FONCTIONS
void aff_led(uint8_t niveau);

typedef struct
{
	bool set_seuil_lumi;
	bool set_seuil_temp;
	bool set_seuil_qa;
	bool set_heure;
	bool set_alarme;
	bool set_crep;
	bool set_stop_reveil;
	bool set_snooze;
	bool set_melodie;
	bool set_affichage;
} Commandes;

typedef struct
{
	bool act_rafraichissement;
	bool act_balayage;
	bool act_alarme;        // true = sonner, false = couper
	bool alarme_valide; 	// Provisoire Signal d'ordre
}Actions;

// DECLARATION TACHES
xTaskHandle xTaskGestionAlarme;

void task_gestion_alarme(void *pvParameters);
// DECLARATION SEMAPHORES
xSemaphoreHandle sem_alarmeHandle;
xSemaphoreHandle mtx_consignesHandle;

// MAILBOXES
QueueHandle_t ActionsHandle;

// FONCTIONS
void Act_alarme(bool on); // Activer la melodie de l'alarme
Heure Set_alarme(Heure base,int delta); // Reglage de l'alarme (le snooze y compris)

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

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

Consignes consignes;
Heure heure = {0,0,0};
Infos infos_acq = {0};


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
  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  mtx_infosHandle = xSemaphoreCreateMutex();
  sem_ledHandle = xSemaphoreCreateBinary();
  mtx_adcHandle = xSemaphoreCreateMutex();
  mtx_consignesHandle = xSemaphoreCreateMutex();
  mtx_i2cHandle = xSemaphoreCreateMutex();
  sem_alarmeHandle = xSemaphoreCreateBinary();
  mtx_heureHandle = xSemaphoreCreateMutex();
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  InfosHandle = xQueueCreate(1, sizeof(Infos) );
  AffHandle = xQueueCreate(4, sizeof(Affichages) );
  ActionsHandle = xQueueCreate(4, sizeof( int) );
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  xTaskCreate(task_acq_luminosite,       /* Function that implements the task. */
  			"TASK_ACQ_LUMI",          /* Text name for the task. */
  			1024,      /* Stack size in words, not bytes. */
  			NULL,    /* Parameter passed into the task. */
  			tskIDLE_PRIORITY+1,/* Priority at which the task is created. */
  			&xTaskAcqLumi  );      /* Used to pass out the created task's handle. */

  xTaskCreate(task_acq_temp_qa,       /* Function that implements the task. */
  			"TASK_ACQ_TEMPQA",          /* Text name for the task. */
  			1024,      /* Stack size in words, not bytes. */
  			NULL,    /* Parameter passed into the task. */
  			tskIDLE_PRIORITY+2,/* Priority at which the task is created. */
  			&xTaskAcqTempQA  );      /* Used to pass out the created task's handle. */

  xTaskCreate(task_acq_tof,       /* Function that implements the task. */
  			"TASK_ACQ_TOF",          /* Text name for the task. */
  			1024,      /* Stack size in words, not bytes. */
  			NULL,    /* Parameter passed into the task. */
  			tskIDLE_PRIORITY+3,/* Priority at which the task is created. */
  			&xTaskAcqTof  );      /* Used to pass out the created task's handle. */

  xTaskCreate(task_interface_actions,       /* Function that implements the task. */
 			"TASK_INTERFACE_ACTIONS",          /* Text name for the task. */
 			1024,      /* Stack size in words, not bytes. */
 			NULL,    /* Parameter passed into the task. */
 			tskIDLE_PRIORITY+6,/* Priority at which the task is created. */
 			&xTaskInterfaceActions  );      /* Used to pass out the created task's handle. */

  xTaskCreate(task_interface_aff,       /* Function that implements the task. */
  			"TASK_INTERFACE_AFF",          /* Text name for the task. */
  			1024,      /* Stack size in words, not bytes. */
  			NULL,    /* Parameter passed into the task. */
  			tskIDLE_PRIORITY+7,/* Priority at which the task is created. */
  			&xTaskInterfaceAff  );      /* Used to pass out the created task's handle. */
  xTaskCreate(task_gestion_horloge,       /* Function that implements the task. */
  			"TASK_GESTION_HORLOGE",          /* Text name for the task. */
  			1024,      /* Stack size in words, not bytes. */
  			NULL,    /* Parameter passed into the task. */
  			tskIDLE_PRIORITY+11,/* Priority at which the task is created. */
  			&xTaskGestionHorloge );      /* Used to pass out the created task's handle. */

  xTaskCreate(task_gestion_aff_crep_aube,       /* Function that implements the task. */
			"TASK_GESTION_AFF_CREP_AUBE",          /* Text name for the task. */
			1024,      /* Stack size in words, not bytes. */
			NULL,    /* Parameter passed into the task. */
			tskIDLE_PRIORITY+8,/* Priority at which the task is created. */
			&xTaskGestionAffCrepAube );      /* Used to pass out the created task's handle. */

  xTaskCreate(task_gestion_alarme,       /* Function that implements the task. */
                          "TASK_GESTION_ALARME",          /* Text name for the task. */
                          1024,      /* Stack size in words, not bytes. */
                          NULL,    /* Parameter passed into the task. */
                          tskIDLE_PRIORITY+10,/* Priority at which the task is created. */
  						&xTaskGestionAlarme  );      /* Used to pass out the created task's handle. */
  /* add threads, ... */
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
  hcan1.Init.TimeSeg1 = CAN_BS1_6TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_4TQ;
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
  hlpuart1.Init.BaudRate = 209700;
  hlpuart1.Init.WordLength = UART_WORDLENGTH_7B;
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
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_HARD_OUTPUT;
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
  htim8.Init.Prescaler = 0;
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
  HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, DIS_Pin|CE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : SW2_Pin SW1_Pin */
  GPIO_InitStruct.Pin = SW2_Pin|SW1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : DIR_Pin */
  GPIO_InitStruct.Pin = DIR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(DIR_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : DIS_Pin CE_Pin */
  GPIO_InitStruct.Pin = DIS_Pin|CE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

// FONCTIONS //
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

// TACHES //

void task_gestion_alarme(void *pvParameters){
	Heure alarme_base;
	bool snooze_actif = false;
	while(1){
		xSemaphoreTake(sem_alarmeHandle, portMAX_DELAY); // On bloque jusqu'au signla activer_alarme

		xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY); // Sauvegarde alarme de base
		if(snooze_actif == false){
			alarme_base = consignes.alarme;
		}

		consignes.stop_alarme = false;
		consignes.snooze_alarme = false;
		xSemaphoreGive(mtx_consignesHandle);

		Act_alarme(true);

		// Attente stop ou snooze
		bool fini = false;
		while(fini != true){
			bool stop,snooze;
			xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
			stop = consignes.stop_alarme;
			snooze = consignes.snooze_alarme;
			xSemaphoreGive(mtx_consignesHandle);

			if(stop == true){
				xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
				if(snooze_actif == true){// Reconduite du reveil au lendemain
					consignes.alarme = alarme_base;
				}
				consignes.activer_alarme = false;
				consignes.stop_alarme = false;
				xSemaphoreGive(mtx_consignesHandle);
				snooze_actif = false;
				fini = true;
			}
			else if(snooze == true){
				xSemaphoreTake(mtx_consignesHandle, portMAX_DELAY);
				consignes.alarme = Set_alarme(alarme_base,SNOOZE_MIN);
				consignes.activer_alarme = false;
				consignes.stop_alarme = false;
				xSemaphoreGive(mtx_consignesHandle);
				snooze_actif = true;
				fini = true;
			}
			else{
				vTaskDelay(pdMS_TO_TICKS(50)); // Contuniation de la mélodie
			}
		}
		Act_alarme(false);
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
