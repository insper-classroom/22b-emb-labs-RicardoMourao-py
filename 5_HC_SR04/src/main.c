#include <asf.h>
#include "conf_board.h"

#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"

/* Botao da placa */
#define BUT_PIO     PIOD
#define BUT_PIO_ID  ID_PIOD
#define BUT_PIO_PIN 28
#define BUT_PIO_PIN_MASK (1 << BUT_PIO_PIN)

#define X_PIO     PIOD
#define X_PIO_ID  ID_PIOD
#define X_PIO_PIN 11
#define X_PIO_PIN_MASK (1 << X_PIO_PIN)

#define TRIGGER_PIO     PIOC
#define TRIGGER_PIO_ID  ID_PIOC
#define TRIGGER_PIO_PIN 13
#define TRIGGER_PIO_PIN_MASK (1 << TRIGGER_PIO_PIN)

/** RTOS  */
#define TASK_OLED_STACK_SIZE                (1024*6/sizeof(portSTACK_TYPE))
#define TASK_OLED_STACK_PRIORITY            (tskIDLE_PRIORITY)

QueueHandle_t xQueueModo;
QueueHandle_t xQueueTempo;

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,  signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);

/** prototypes */

void but_callback(void);
void X_callback(void);
static void BUT_init(void);
void io_init(void);
static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource);

/************************/
/* RTOS application funcs                                               */
/************************/

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask, signed char *pcTaskName) {
	printf("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
	for (;;) {	}
}

extern void vApplicationIdleHook(void) { }

extern void vApplicationTickHook(void) { }

extern void vApplicationMallocFailedHook(void) {
	configASSERT( ( volatile void * ) NULL );
}

/************************/
/* handlers / callbacks                                                 */
/************************/
void RTT_Handler(void) {
	uint32_t ul_status;
	ul_status = rtt_get_status(RTT);

	/* IRQ due to Alarm */
	if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
		printf("RTT passou do limite de tempo");
	}
}

void but_callback(void) {
	int x = 1;
	BaseType_t xHigherPriorityTaskWoken = pdTRUE;
	xQueueSendFromISR(xQueueModo, &x, &xHigherPriorityTaskWoken);
}

void X_callback(void) {
	if (pio_get(X_PIO, PIO_INPUT, X_PIO_PIN_MASK)) {
		RTT_init(1/(58*1e-6*2), 0, 0);
	} 
	else {
		uint32_t cont = rtt_read_timer_value(RTT);
		BaseType_t xHigherPriorityTaskWoken = pdTRUE;
		xQueueSendFromISR(xQueueTempo, &cont, &xHigherPriorityTaskWoken);
	}
}
/************************/
/* TASKS                                                                */
/************************/

static void task_oled(void *pvParameters) {
	gfx_mono_ssd1306_init();
	
	io_init();
	printf("task oled\n");
	
	int x;
	int cont;
	for (;;)  {
		if (xQueueReceive(xQueueModo, &x, 0)) {
			pio_set(TRIGGER_PIO, TRIGGER_PIO_PIN_MASK);
			delay_us(10);
			pio_clear(TRIGGER_PIO, TRIGGER_PIO_PIN_MASK);
		}
		
		if (xQueueReceive(xQueueTempo, &cont, 0)) {
			float tempo = cont*(58*1e-6*2);
			float dist = (tempo * 340) / 2;
			char s_dist[50];
			sprintf(s_dist, "%.2f m", dist);
			gfx_mono_draw_string("                   ", 0, 0, &sysfont);
			gfx_mono_draw_string(s_dist, 0, 0, &sysfont);
		}

	}
}

/************************/
/* funcoes                                                              */
/************************/

void io_init(void) {
	pmc_enable_periph_clk(TRIGGER_PIO);
	pmc_enable_periph_clk(X_PIO);
	pmc_enable_periph_clk(BUT_PIO);

	pio_configure(TRIGGER_PIO, PIO_OUTPUT_0, TRIGGER_PIO_PIN_MASK, PIO_PULLUP| PIO_DEBOUNCE);
	pio_configure(X_PIO, PIO_INPUT, X_PIO_PIN_MASK, PIO_DEFAULT);
	pio_configure(BUT_PIO, PIO_INPUT, BUT_PIO_PIN_MASK, PIO_PULLUP| PIO_DEBOUNCE);

	pio_handler_set(BUT_PIO, BUT_PIO_ID, BUT_PIO_PIN_MASK, PIO_IT_FALL_EDGE,
	but_callback);
	pio_handler_set(X_PIO, X_PIO_ID, X_PIO_PIN_MASK, PIO_IT_EDGE,
	X_callback);

	pio_enable_interrupt(TRIGGER_PIO, TRIGGER_PIO_PIN_MASK);
	pio_enable_interrupt(X_PIO, X_PIO_PIN_MASK);
	pio_enable_interrupt(BUT_PIO, BUT_PIO_PIN_MASK);

	pio_get_interrupt_status(TRIGGER_PIO);
	pio_get_interrupt_status(X_PIO);
	pio_get_interrupt_status(BUT_PIO);

	NVIC_EnableIRQ(TRIGGER_PIO_ID);
	NVIC_SetPriority(TRIGGER_PIO_ID, 4);

	NVIC_EnableIRQ(X_PIO_ID);
	NVIC_SetPriority(X_PIO_ID, 4);

	NVIC_EnableIRQ(BUT_PIO_ID);
	NVIC_SetPriority(BUT_PIO_ID, 4);
}

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

static void BUT_init(void) {
	/* configura prioridae */
	NVIC_EnableIRQ(BUT_PIO_ID);
	NVIC_SetPriority(BUT_PIO_ID, 4);

	/* conf botão como entrada */
	pio_configure(BUT_PIO, PIO_INPUT, BUT_PIO_PIN_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(BUT_PIO, BUT_PIO_PIN_MASK, 60);
	pio_enable_interrupt(BUT_PIO, BUT_PIO_PIN_MASK);
	pio_handler_set(BUT_PIO, BUT_PIO_ID, BUT_PIO_PIN_MASK, PIO_IT_FALL_EDGE , but_callback);
}

static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource) {

	uint16_t pllPreScale = (int) (((float) 32768) / freqPrescale);
	
	rtt_sel_source(RTT, false);
	rtt_init(RTT, pllPreScale);
	
	if (rttIRQSource & RTT_MR_ALMIEN) {
		uint32_t ul_previous_time;
		ul_previous_time = rtt_read_timer_value(RTT);
		while (ul_previous_time == rtt_read_timer_value(RTT));
		rtt_write_alarm_time(RTT, IrqNPulses+ul_previous_time);
	}

	/* config NVIC */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 4);
	NVIC_EnableIRQ(RTT_IRQn);

	/* Enable RTT interrupt */
	if (rttIRQSource & (RTT_MR_RTTINCIEN | RTT_MR_ALMIEN))
	rtt_enable_interrupt(RTT, rttIRQSource);
	else
	rtt_disable_interrupt(RTT, RTT_MR_RTTINCIEN | RTT_MR_ALMIEN);
	
}

/************************/
/* main                                                                 */
/************************/


int main(void) {
	/* Initialize the SAM system */
	sysclk_init();
	board_init();

	/* Initialize the console uart */
	configure_console();

	/* Create task to control oled */
	if (xTaskCreate(task_oled, "oled", TASK_OLED_STACK_SIZE, NULL, TASK_OLED_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create oled task\r\n");
	}
	
	xQueueModo = xQueueCreate(32, sizeof(uint32_t));
	if (xQueueModo == NULL)
	printf("Não criou a queue Modo\n");
	
	xQueueTempo = xQueueCreate(32, sizeof(uint32_t));
	if (xQueueTempo == NULL)
	printf("Não criou a queue Tempo\n");

	/* Start the scheduler. */
	vTaskStartScheduler();

	/* RTOS não deve chegar aqui !! */
	while(1){}

	/* Will only get here if there was insufficient memory to create the idle task. */
	return 0;
}
