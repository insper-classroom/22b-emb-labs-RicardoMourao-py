#include <asf.h>
#include "asf.h"
#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"

// LED
#define LED_PIO      PIOC
#define LED_PIO_ID   ID_PIOC
#define LED_IDX      8
#define LED_IDX_MASK (1 << LED_IDX)

// Botão para piscar o led
#define BUT_PIO      PIOA
#define BUT_PIO_ID   ID_PIOA
#define BUT_IDX  11
#define BUT_IDX_MASK (1 << BUT_IDX)

// Botão para alterar a frequência do led (botão 1 da placa oled)
#define BUT1_PIO           PIOD
#define BUT1_PIO_ID        ID_PIOD
#define BUT1_PIO_IDX		28
#define BUT1_PIO_IDX_MASK  (1u << BUT1_PIO_IDX)

/************************/
/* constants                                                            */
/************************/

/************************/
/* flags                                                                */
/************************/
volatile char but_flag;
volatile char but1_flag;

/************************/
/* variaveis globais                                                    */
/************************/
/************************/
/* prototype                                                            */
/************************/

/************************/
/* handler / callbacks                                                  */
/************************/

void but_callback (void) {
	if (pio_get(BUT_PIO, PIO_INPUT, BUT_IDX_MASK)) {
		but_flag = 1;
		} else {
		return;
	}
}

void but1_callback (void) {
	if (!pio_get(BUT1_PIO, PIO_INPUT, BUT1_PIO_IDX_MASK)) {
		but1_flag = 1;
		} else {
		return;
	}
}

/************************/
/* funções                                                              */
/************************/
int aumenta_freq (int t) {
	t -= 100;
	return t;
}

int diminui_freq (int t) {
	t += 100;
	return t;
}

// pisca led N vez no periodo T
void pisca_led(int n, int t){
	for (int i=0;i<n;i++){
		pio_clear(LED_PIO, LED_IDX_MASK);
		delay_ms(t);
		pio_set(LED_PIO, LED_IDX_MASK);
		delay_ms(t);
	}
}

// Inicializa botao SW0 do kit com interrupcao
void io_init(void) {
	
	// Configura led
	pmc_enable_periph_clk(LED_PIO_ID);
	pio_configure(LED_PIO, PIO_OUTPUT_0, LED_IDX_MASK, PIO_DEFAULT);

	// Inicializa clock do periférico PIO responsavel pelo botao
	pmc_enable_periph_clk(BUT_PIO_ID);
	pmc_enable_periph_clk(BUT1_PIO_ID);

	// Configura PIO para lidar com o pino do botão como entrada
	// com pull-up
	pio_configure(BUT_PIO, PIO_INPUT, BUT_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(BUT_PIO, BUT_IDX_MASK, 60);
	
	pio_configure(BUT1_PIO, PIO_INPUT, BUT1_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(BUT1_PIO, BUT1_PIO_IDX_MASK, 60);

	// Configura interrupção no pino referente ao botao e associa
	// função de callback caso uma interrupção for gerada
	// a função de callback é a: but_callback()
	pio_handler_set(BUT_PIO,
	BUT_PIO_ID,
	BUT_IDX_MASK,
	PIO_IT_EDGE,
	but_callback);
	
	// Função de interrupção para lidar com o botão 1
	pio_handler_set(BUT1_PIO,
	BUT1_PIO_ID,
	BUT1_PIO_IDX_MASK,
	PIO_IT_EDGE,
	but1_callback);

	// Ativa interrupção e limpa primeira IRQ gerada na ativacao
	pio_enable_interrupt(BUT_PIO, BUT_IDX_MASK);
	pio_get_interrupt_status(BUT_PIO);
	
	pio_enable_interrupt(BUT1_PIO, BUT1_PIO_IDX_MASK);
	pio_get_interrupt_status(BUT1_PIO);
	
	// Configura NVIC para receber interrupcoes do PIO do botao
	// com prioridade 4 (quanto mais próximo de 0 maior)
	NVIC_EnableIRQ(BUT_PIO_ID);
	NVIC_SetPriority(BUT_PIO_ID, 4); // Prioridade 4
	
	NVIC_EnableIRQ(BUT1_PIO_ID);
	NVIC_SetPriority(BUT1_PIO_ID, 4);
}


/************************/
/* Main                                                                 */
/************************/

// Funcao principal chamada na inicalizacao do uC.
void main(void) {
	
	// Inicializa clock
	
	
	board_init();
	sysclk_init();
	delay_init();

	// Desativa watchdog
	WDT->WDT_MR = WDT_MR_WDDIS;

	// configura botao com interrupcao
	io_init();
	
	// Init OLED
	gfx_mono_ssd1306_init();
	int t = 500;
	char str[10];
	sprintf(str, "%d", t);
	gfx_mono_draw_string("    ", 29, 20, &sysfont);
	gfx_mono_draw_string(str, 29, 20, &sysfont);
	
	// super loop
	// aplicacoes embarcadas no devem sair do while(1).
	while(1) {
		
		// Checa se o botão 1 foi pressionado
		if (but1_flag) {
			delay_ms(1000);
			if (!pio_get(BUT1_PIO, PIO_INPUT, BUT1_PIO_IDX_MASK)) {
				t = diminui_freq(t);
				} else {
				t = aumenta_freq(t);
			}
			but1_flag = 0;
		}
		
		// Checa se o botão de acender o led foi precionado
		if (but_flag) {
			pisca_led(30, t);
			but_flag = 0;
		}
		char str[10];
		sprintf(str, "%d", t);
		gfx_mono_draw_string("    ", 29, 20, &sysfont);
		gfx_mono_draw_string(str, 29, 20, &sysfont);
		// Entra em sleep mode
		pmc_sleep(SAM_PM_SMODE_SLEEP_WFI);
	}
}