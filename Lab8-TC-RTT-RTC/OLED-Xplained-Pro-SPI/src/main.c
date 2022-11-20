#include <asf.h>

#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"


typedef struct  {
	uint32_t year;
	uint32_t month;
	uint32_t day;
	uint32_t week;
	uint32_t hour;
	uint32_t minute;
	uint32_t second;
} calendar;


volatile char but1_flag;
volatile char rtc_alarm_flag = 0;
volatile char rtc_count_flag = 0;
volatile char rtt_alarm_flag = 0;
volatile char new_tc_flag = 0;
volatile int led3_counter = 0;


/* LED da placa SAME70 */
#define LED_PIO	          PIOC
#define LED_PIO_ID		  ID_PIOC
#define LED_PIO_IDX		  8
#define LED_PIO_IDX_MASK  (1 << LED_PIO_IDX)

/* LED 1 da placa OLED */
#define LED1_PIO          PIOA
#define LED1_PIO_ID       ID_PIOA
#define LED1_PIO_IDX      0
#define LED1_PIO_IDX_MASK  (1 << LED1_PIO_IDX)

/* LED 2 da placa OLED */
#define LED2_PIO          PIOC
#define LED2_PIO_ID       ID_PIOC
#define LED2_PIO_IDX      30
#define LED2_PIO_IDX_MASK  (1 << LED2_PIO_IDX)

/* LED 3 da placa OLED */
#define LED3_PIO          PIOB
#define LED3_PIO_ID       ID_PIOB
#define LED3_PIO_IDX      2
#define LED3_PIO_IDX_MASK  (1 << LED3_PIO_IDX)

/* Botao 1 da placa OLED */
#define BUT1_PIO          PIOD
#define BUT1_PIO_ID       ID_PIOD
#define BUT1_PIO_IDX      28
#define BUT1_PIO_IDX_MASK (1 << BUT1_PIO_IDX)


void but1_callback(void);
void draw_time(uint32_t hora_atual, uint32_t minuto_atual, uint32_t segundo_atual);
void pin_toggle(Pio *pio, uint32_t mask);
static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource);
void RTC_init(Rtc *rtc, uint32_t id_rtc, calendar t, uint32_t irq_type);
void TC_init(Tc *TC, int ID_TC, int TC_CHANNEL, int freq);
void leds_init(void);
void io_init(void);


void but1_callback(void) {
	but1_flag = 1;
}


void pin_toggle (Pio *pio, uint32_t mask) {
	if (pio_get_output_data_status(pio, mask)) {
		pio_clear(pio, mask);	
	}
	else {
		pio_set(pio,mask);	
	}
}


void draw_time(uint32_t hora_atual, uint32_t minuto_atual, uint32_t segundo_atual) {
	gfx_mono_draw_line(0, 0, 128, 32, GFX_PIXEL_CLR);
	char time_display[10];
	sprintf(time_display, "%02d:%02d:%02d", hora_atual, minuto_atual,segundo_atual);
	gfx_mono_draw_string(time_display, 25, 16, &sysfont);
	gfx_mono_draw_string("JAMES CLOCK", 10, 0, &sysfont);
}


/************************************************************************/
/* HANDLERS                                                             */
/************************************************************************/

void TC0_Handler(void) {
	/* A leitura do periferico informa que a interrupcao foi satisfeita */
	volatile uint32_t status = tc_get_status(TC0, 0);
	pin_toggle(LED_PIO, LED_PIO_IDX_MASK);
}


void TC1_Handler(void) {
	volatile uint32_t status = tc_get_status(TC0, 1);
	pin_toggle(LED1_PIO, LED1_PIO_IDX_MASK);
}


void TC2_Handler(void) {
	volatile uint32_t status = tc_get_status(TC0, 2);
	pin_toggle(LED3_PIO, LED3_PIO_IDX_MASK);
	new_tc_flag = 1;
}


void RTT_Handler(void) {
	uint32_t ul_status;

	/* RTT status - ACK */
	ul_status = rtt_get_status(RTT);

	/* Interrupcao por alarme => muda para interrupcao por contagem */
	if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
		RTT_init(1, 4, RTT_MR_RTTINCIEN);
		pin_toggle(LED2_PIO, LED2_PIO_IDX_MASK);
	}

	/* Interrupcao por contagem => */
	if ((ul_status & RTT_SR_RTTINC) == RTT_SR_RTTINC) {
		 // por enquanto nao faz nada
	}
}


void RTC_Handler(void) {
	uint32_t ul_status = rtc_get_status(RTC);
	
	/* Contagem de segundos */
	if ((ul_status & RTC_SR_SEC) == RTC_SR_SEC) {
		rtc_count_flag = 1;
	}
	
	/* Interrupcao por alarme por data ou tempo */
	if ((ul_status & RTC_SR_ALARM) == RTC_SR_ALARM) {
		rtc_alarm_flag = 1;
	}

	rtc_clear_status(RTC, RTC_SCCR_SECCLR);
	rtc_clear_status(RTC, RTC_SCCR_ALRCLR);
	rtc_clear_status(RTC, RTC_SCCR_ACKCLR);
	rtc_clear_status(RTC, RTC_SCCR_TIMCLR);
	rtc_clear_status(RTC, RTC_SCCR_CALCLR);
	rtc_clear_status(RTC, RTC_SCCR_TDERRCLR);
}



/************************************************************************/
/* INITS                                                                */
/************************************************************************/

void TC_init(Tc *TC, int ID_TC, int TC_CHANNEL, int freq) {
	uint32_t ul_div;
	uint32_t ul_tcclks;
	uint32_t ul_sysclk = sysclk_get_cpu_hz();

	/* Configura o PMC */
	pmc_enable_periph_clk(ID_TC);

	/* Configura o TC para operar em freq hz e interrupçcão no RC compare */
	tc_find_mck_divisor(freq, ul_sysclk, &ul_div, &ul_tcclks, ul_sysclk);
	tc_init(TC, TC_CHANNEL, ul_tcclks | TC_CMR_CPCTRG);
	tc_write_rc(TC, TC_CHANNEL, (ul_sysclk / ul_div) / freq);

	/* Configura NVIC*/
	NVIC_SetPriority(ID_TC, 4);
	NVIC_EnableIRQ((IRQn_Type) ID_TC);
	tc_enable_interrupt(TC, TC_CHANNEL, TC_IER_CPCS);
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

	/* Configura NVIC */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 4);
	NVIC_EnableIRQ(RTT_IRQn);

	/* Ativa interrupcao do RTT */
	if (rttIRQSource & (RTT_MR_RTTINCIEN | RTT_MR_ALMIEN)) {
		rtt_enable_interrupt(RTT, rttIRQSource);	
	}
	else {
		rtt_disable_interrupt(RTT, RTT_MR_RTTINCIEN | RTT_MR_ALMIEN);	
	}
}


void RTC_init(Rtc *rtc, uint32_t id_rtc, calendar t, uint32_t irq_type) {
	/* Configura o PMC */
	pmc_enable_periph_clk(ID_RTC);

	/* Default RTC configuration, 24-hour mode */
	rtc_set_hour_mode(rtc, 0);

	/* Configura data e hora manualmente */
	rtc_set_date(rtc, t.year, t.month, t.day, t.week);
	rtc_set_time(rtc, t.hour, t.minute, t.second);

	/* Configure RTC interrupts */
	NVIC_DisableIRQ(id_rtc);
	NVIC_ClearPendingIRQ(id_rtc);
	NVIC_SetPriority(id_rtc, 4);
	NVIC_EnableIRQ(id_rtc);

	/* Ativa interrupcao via alarme */
	rtc_enable_interrupt(rtc, irq_type);
}


void leds_init(void) {
	pmc_enable_periph_clk(LED_PIO_ID);
	pmc_enable_periph_clk(LED1_PIO_ID);
	pmc_enable_periph_clk(LED2_PIO_IDX);
	pmc_enable_periph_clk(LED3_PIO_IDX);
	pio_configure(LED_PIO, PIO_OUTPUT_1, LED_PIO_IDX_MASK, PIO_DEFAULT);
	pio_configure(LED1_PIO, PIO_OUTPUT_0, LED1_PIO_IDX_MASK, PIO_DEFAULT);
	pio_configure(LED2_PIO, PIO_OUTPUT_0, LED2_PIO_IDX_MASK, PIO_DEFAULT);
	//pio_configure(LED3_PIO, PIO_OUTPUT_1, LED3_PIO_IDX_MASK, PIO_DEFAULT);
	pio_set_output(LED3_PIO, LED3_PIO_IDX_MASK, 1, 0, 0);
}


void buts_init(void) {
	pmc_enable_periph_clk(BUT1_PIO_ID);
	pio_configure(BUT1_PIO, PIO_INPUT, BUT1_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(BUT1_PIO, BUT1_PIO_IDX_MASK, 60);
	
	pio_handler_set(
		BUT1_PIO,
		BUT1_PIO_ID,
		BUT1_PIO_IDX_MASK,
		PIO_IT_FALL_EDGE,
		but1_callback
	);
	
	pio_enable_interrupt(BUT1_PIO, BUT1_PIO_IDX_MASK);
	pio_get_interrupt_status(BUT1_PIO);
	NVIC_EnableIRQ(BUT1_PIO_ID);
	NVIC_SetPriority(BUT1_PIO_ID, 4);
}


void io_init(void) {
	buts_init();
	leds_init();
	
	/* Inicializa o TC0, canal 0 para contar piscagem do LED */
	TC_init(TC0, ID_TC0, 0, 5);
	tc_start(TC0, 0);
	
	/* Inicializa o TC0, canal 1 para contar piscagem do LED1 */
	TC_init(TC0, ID_TC1, 1, 4);
	tc_start(TC0, 1);
	
	/* Ativa RTT para inverter sinal do LED2 a cada 4s 
	* Usa interrupcao do tipo contagem (sem alarme)
	*/
	RTT_init(1, 4, RTT_MR_ALMIEN);
	
	/** Configura RTC */
	calendar rtc_calendar = {2018, 3, 19, 12, 15, 45 ,1};
	RTC_init(RTC, ID_RTC, rtc_calendar, RTC_IER_ALREN | RTC_IER_SECEN);
}



/************************************************************************/
/* MAIN                                                                 */
/************************************************************************/
int main (void) {
	board_init();
	sysclk_init();
	
	
	delay_init();
	io_init();
	
	WDT->WDT_MR = WDT_MR_WDDIS;

	gfx_mono_ssd1306_init();
    //gfx_mono_draw_string("APS 4", 50, 16, &sysfont);
	
	/* Leitura do valor atual do RTC */
	uint32_t hora_atual, minuto_atual, segundo_atual;

	while(1) {
		if (rtc_count_flag) {
			rtc_get_time(RTC, &hora_atual, &minuto_atual, &segundo_atual);
			draw_time(hora_atual, minuto_atual, segundo_atual);
			rtc_count_flag = 0;
		}
		if (but1_flag) {
			tc_disable_interrupt(TC0, 2, TC_IDR_CPCS);
			if (!pio_get_output_data_status(LED3_PIO, LED3_PIO_IDX_MASK)) {
				pio_set(LED3_PIO, LED3_PIO_IDX_MASK);
			}
			rtc_get_time(RTC, &hora_atual, &minuto_atual, &segundo_atual);
			/* configura alarme do RTC para daqui 20 segundos */
			if (segundo_atual >= 40) {
				rtc_set_time_alarm(RTC, 1, hora_atual, 1, minuto_atual+1, 1, segundo_atual + 20 - 60);
			}
			else {
				rtc_set_time_alarm(RTC, 1, hora_atual, 1, minuto_atual, 1, segundo_atual + 20);
			}
			but1_flag = 0;
		}
		if (rtc_alarm_flag) {
			// Inicia novo TC para o LED3
			TC_init(TC0, ID_TC2, 2, 6);
			tc_start(TC0, 2);
			rtc_alarm_flag = 0;
		}
		if (new_tc_flag) {
			// Faz com que o led pisque por um tempo
			led3_counter++;
			if (led3_counter >= 120) {
				led3_counter = 0;
				tc_disable_interrupt(TC0, 2, TC_IDR_CPCS);
				if (!pio_get_output_data_status(LED3_PIO, LED3_PIO_IDX_MASK)) {
					pio_set(LED3_PIO, LED3_PIO_IDX_MASK);
				}
			}
			new_tc_flag = 0;
		}
		
		rtt_alarm_flag = 0;
		pmc_sleep(SAM_PM_SMODE_SLEEP_WFI);
	}
}
