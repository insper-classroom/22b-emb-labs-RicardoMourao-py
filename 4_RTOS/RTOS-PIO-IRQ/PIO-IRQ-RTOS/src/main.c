#include "conf_board.h"
#include <asf.h>

/************************/
/* BOARD CONFIG                                                         */
/************************/

#define BUT_PIO PIOA
#define BUT_PIO_ID ID_PIOA
#define BUT_PIO_PIN 11
#define BUT_PIO_PIN_MASK (1 << BUT_PIO_PIN)

#define LED_PIO PIOC
#define LED_PIO_ID ID_PIOC
#define LED_PIO_IDX 8
#define LED_IDX_MASK (1 << LED_PIO_IDX)

// Botão 1 - OLED
#define BUT1_PIO           PIOD
#define BUT1_PIO_ID        ID_PIOD
#define BUT1_PIO_IDX       28
#define BUT1_PIO_IDX_MASK (1u << BUT1_PIO_IDX)

#define USART_COM_ID ID_USART1
#define USART_COM USART1

/************************/
/* RTOS                                                                */
/************************/

#define TASK_LED_STACK_SIZE (4096 / sizeof(portSTACK_TYPE))
#define TASK_LED_STACK_PRIORITY (tskIDLE_PRIORITY)
#define TASK_BUT_STACK_SIZE (4096 / sizeof(portSTACK_TYPE))
#define TASK_BUT_STACK_PRIORITY (tskIDLE_PRIORITY)

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,
signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);

/************************/
/* recursos RTOS                                                        */
/************************/

/** Queue for msg log send data */
QueueHandle_t xQueueLed;

/** Queue para armazenar sequência de botões apaertados */
QueueHandle_t xQueueBut;


/************************/
/* prototypes local                                                     */
/************************/

void but_callback(void);
static void BUT_init(void);
static void BUT1_init(void);
void pin_toggle(Pio *pio, uint32_t mask);
static void USART1_init(void);
void LED_init(int estado);

/************************/
/* RTOS application funcs                                               */
/************************/

/**
* \brief Called if stack overflow during execution
*/
extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,
signed char *pcTaskName) {
	printf("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
	/* If the parameters have been corrupted then inspect pxCurrentTCB to
	* identify which task has overflowed its stack.
	*/
	for (;;) {
	}
}

/**
* \brief This function is called by FreeRTOS idle task
*/
extern void vApplicationIdleHook(void) { pmc_sleep(SAM_PM_SMODE_SLEEP_WFI); }

/**
* \brief This function is called by FreeRTOS each tick
*/
extern void vApplicationTickHook(void) {}

extern void vApplicationMallocFailedHook(void) {
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

	/* Force an assert. */
	configASSERT((volatile void *)NULL);
}

/************************/
/* handlers / callbacks                                                 */
/************************/

void but_callback(void) {
	//BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	//xSemaphoreGiveFromISR(xSemaphoreBut, &xHigherPriorityTaskWoken);
	
	int freq = -100;
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xQueueSendFromISR(xQueueBut, (void *)&freq, &xHigherPriorityTaskWoken);
}

void but1_callback(void) {
	//BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	//xSemaphoreGiveFromISR(xSemaphoreBut1, &xHigherPriorityTaskWoken);
	
	int freq = 100;
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xQueueSendFromISR(xQueueBut, (void *)&freq, &xHigherPriorityTaskWoken);
}

/************************/
/* TASKS                                                                */
/************************/

static void task_led(void *pvParameters) {

	LED_init(1);

	uint32_t msg = 0;
	uint32_t delay = 2000;

	/* tarefas de um RTOS não devem retornar */
	for (;;) {
		/* verifica se chegou algum dado na queue, e espera por 0 ticks */
		if (xQueueReceive(xQueueLed, &delay, (TickType_t) delay)) {
			/* chegou novo valor, atualiza delay ! */
			/* aqui eu poderia verificar se msg faz sentido (se esta no range certo)
			*/
			/* converte ms -> ticks */
			//printf("Msg received: %d\n", msg);
			//delay = msg / portTICK_PERIOD_MS;
			printf("task_led: %d \n", delay);
		}

		/* pisca LED */
		pin_toggle(LED_PIO, LED_IDX_MASK);

		/* suspende por delay */
		vTaskDelay(delay);
	}
}

static void task_but(void *pvParameters) {

	/* iniciliza botao */
	BUT_init();
	BUT1_init();

	uint32_t delayTicks = 2000;
	
	uint32_t delay = 2000;

	for (;;) {
		
		uint32_t msg = 0;
		if (xQueueReceive(xQueueBut, &msg, (TickType_t)0)) {
			//printf("But apertado, msg = %d\n", msg);
			delay += msg;
			delay = delay / portTICK_PERIOD_MS;
			//printf("%d", delay);
			xQueueSend(xQueueLed, (void *)&delay, 10);
			
		}
	}
}

/************************/
/* funcoes                                                              */
/************************/

/**
* \brief Configure the console UART.
*/
static void configure_console(void) {
	const usart_serial_options_t uart_serial_options = {
		.baudrate = CONF_UART_BAUDRATE,
		.charlength = CONF_UART_CHAR_LENGTH,
		.paritytype = CONF_UART_PARITY,
		.stopbits = CONF_UART_STOP_BITS,
	};

	/* Configure console UART. */
	stdio_serial_init(CONF_UART, &uart_serial_options);

	/* Specify that stdout should not be buffered. */
	setbuf(stdout, NULL);
}

void pin_toggle(Pio *pio, uint32_t mask) {
	if (pio_get_output_data_status(pio, mask))
	pio_clear(pio, mask);
	else
	pio_set(pio, mask);
}

void LED_init(int estado){
	pmc_enable_periph_clk(LED_PIO_ID);
	pio_set_output(LED_PIO, LED_IDX_MASK, estado, 0, 0);
};


static void BUT_init(void) {
	// Configura PIO para lidar com o pino do botão como entrada
	// com pull-up
	pio_configure(BUT_PIO, PIO_INPUT, BUT_PIO_PIN_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	//pio_set_debounce_filter(BUT_PIO, BUT_PIO_PIN_MASK, 60);

	// Configura interrupção no pino referente ao botao e associa
	// função de callback caso uma interrupção for gerada
	// a função de callback é a: but_callback()
	pio_handler_set(BUT_PIO,
	BUT_PIO_ID,
	BUT_PIO_PIN_MASK,
	PIO_IT_FALL_EDGE,
	but_callback);

	// Ativa interrupção e limpa primeira IRQ gerada na ativacao
	pio_enable_interrupt(BUT_PIO, BUT_PIO_PIN_MASK);
	pio_get_interrupt_status(BUT_PIO);
	
	// Configura NVIC para receber interrupcoes do PIO do botao
	// com prioridade 4 (quanto mais próximo de 0 maior)
	NVIC_EnableIRQ(BUT_PIO_ID);
	NVIC_SetPriority(BUT_PIO_ID, 4); // Prioridade 4
}

static void BUT1_init(void) {
	// Configura PIO para lidar com o pino do botão como entrada
	// com pull-up
	pio_configure(BUT1_PIO, PIO_INPUT, BUT1_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	//pio_set_debounce_filter(BUT1_PIO, BUT1_PIO_IDX_MASK, 60);

	// Configura interrupção no pino referente ao botao e associa
	// função de callback caso uma interrupção for gerada
	// a função de callback é a: but_callback()
	pio_handler_set(BUT1_PIO,
	BUT1_PIO_ID,
	BUT1_PIO_IDX_MASK,
	PIO_IT_FALL_EDGE,
	but1_callback);

	// Ativa interrupção e limpa primeira IRQ gerada na ativacao
	pio_enable_interrupt(BUT1_PIO, BUT1_PIO_IDX_MASK);
	pio_get_interrupt_status(BUT1_PIO);
	
	// Configura NVIC para receber interrupcoes do PIO do botao
	// com prioridade 4 (quanto mais próximo de 0 maior)
	NVIC_EnableIRQ(BUT1_PIO_ID);
	NVIC_SetPriority(BUT1_PIO_ID, 4); // Prioridade 4
}

/************************/
/* main                                                                 */
/************************/

/**
*  \brief FreeRTOS Real Time Kernel example entry point.
*
*  \return Unused (ANSI-C compatibility).
*/
int main(void) {
	/* Initialize the SAM system */
	sysclk_init();
	board_init();
	configure_console();
	
	printf("Sys init ok \n");

	/* Attempt to create a semaphore. */
	
	//xSemaphoreBut = xSemaphoreCreateBinary();
	//if (xSemaphoreBut == NULL)
	//printf("falha em criar o semaforo \n");

	/* Attempt to create a semaphore. */
	//xSemaphoreBut1 = xSemaphoreCreateBinary();
	//if (xSemaphoreBut1 == NULL)
	//printf("falha em criar o semaforo \n");

	/* cria queue com 32 "espacos" */
	/* cada espaço possui o tamanho de um inteiro*/
	xQueueLed = xQueueCreate(32, sizeof(uint32_t));
	if (xQueueLed == NULL)
	printf("Falhou na criação da queue \n");
	
	xQueueBut = xQueueCreate(32, sizeof(uint32_t));
	if (xQueueBut == NULL)
	printf("Falhou na criação da queue \n");

	/* Create task to make led blink */
	if (xTaskCreate(task_led, "Led", TASK_LED_STACK_SIZE, NULL,
	TASK_LED_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create test led task\r\n");
		} else {
		printf("task led created \r\n");
		
	}
	/* Create task to monitor processor activity */
	if (xTaskCreate(task_but, "BUT", TASK_BUT_STACK_SIZE, NULL,
	TASK_BUT_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create UartTx task\r\n");
		} else {
		printf("task led but \r\n");
	}

	/* Start the scheduler. */
	vTaskStartScheduler();

	/* RTOS não deve chegar aqui !! */
	while (1) {
	}

	/* Will only get here if there was insufficient memory to create the idle
	* task. */
	return 0;
}