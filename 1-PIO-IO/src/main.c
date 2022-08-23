/************************************************************************
* 5 semestre - Eng. da Computao - Insper
* Rafael Corsi - rafael.corsi@insper.edu.br
*
* Material:
*  - Kit: ATMEL SAME70-XPLD - ARM CORTEX M7
*
* Objetivo:
*  - Demonstrar configuraçao do PIO
*
* Periféricos:
*  - PIO
*  - PMC
*
* Log:
*  - 10/2018: Criação
************************************************************************/

/************************************************************************/
/* includes                                                             */
/************************************************************************/

#include "asf.h"

/************************************************************************/
/* defines                                                              */
/************************************************************************/

// LED1
#define LED_PIO1      PIOA
#define LED_PIO_ID1   ID_PIOA
#define LED_IDX1      0
#define LED_IDX_MASK1 (1 << LED_IDX1)

// Botão 1
#define BUT_PIO1      PIOD
#define BUT_PIO_ID1   ID_PIOD
#define BUT_IDX1      28
#define BUT_IDX_MASK1 (1 << BUT_IDX1)

// LED2
#define LED_PIO2      PIOC
#define LED_PIO_ID2   ID_PIOC
#define LED_IDX2      30
#define LED_IDX_MASK2 (1 << LED_IDX2)

// Botão 2
#define BUT_PIO2      PIOC
#define BUT_PIO_ID2   ID_PIOC
#define BUT_IDX2      31
#define BUT_IDX_MASK2 (1 << BUT_IDX2)

// LED3
#define LED_PIO3      PIOB
#define LED_PIO_ID3   ID_PIOB
#define LED_IDX3      2
#define LED_IDX_MASK3 (1 << LED_IDX3)

// Botão 3
#define BUT_PIO3      PIOA
#define BUT_PIO_ID3   ID_PIOA
#define BUT_IDX3      19
#define BUT_IDX_MASK3 (1 << BUT_IDX3)

/************************************************************************/
/* prototypes                                                           */
/************************************************************************/


/************************************************************************/
/* constants                                                            */
/************************************************************************/

/************************************************************************/
/* variaveis globais                                                    */
/************************************************************************/

/************************************************************************/
/* handlers / callbacks                                                 */
/************************************************************************/

/************************************************************************/
/* funções                                                              */
/************************************************************************/

/************************************************************************/
/* Main                                                                 */
/************************************************************************/
/* Funcao principal chamada na inicalizacao do uC.                      */
int main(void)
{
	// Inicializa clock
	sysclk_init();

	// Desativa watchdog
	WDT->WDT_MR = WDT_MR_WDDIS;

	// Ativa PIOs
	pmc_enable_periph_clk(LED_PIO_ID1);
	pmc_enable_periph_clk(BUT_PIO_ID1);
	pmc_enable_periph_clk(LED_PIO_ID2);
	pmc_enable_periph_clk(BUT_PIO_ID2);
	pmc_enable_periph_clk(LED_PIO_ID3);
	pmc_enable_periph_clk(BUT_PIO_ID3);

	// Configura Pinos
	pio_configure(LED_PIO1, PIO_OUTPUT_0, LED_IDX_MASK1, PIO_DEFAULT);
	pio_configure(BUT_PIO1, PIO_INPUT, BUT_IDX_MASK1, PIO_PULLUP);
	pio_configure(LED_PIO2, PIO_OUTPUT_0, LED_IDX_MASK2, PIO_DEFAULT);
	pio_configure(BUT_PIO2, PIO_INPUT, BUT_IDX_MASK2, PIO_PULLUP);
	pio_configure(LED_PIO3, PIO_OUTPUT_0, LED_IDX_MASK3, PIO_DEFAULT);
	pio_configure(BUT_PIO3, PIO_INPUT, BUT_IDX_MASK3, PIO_PULLUP);

	// SUPER LOOP
	// aplicacoes embarcadas no devem sair do while(1).
	while(1) {
		// Verifica valor do pino que o botão está conectado
		if(!pio_get(BUT_PIO1, PIO_INPUT, BUT_IDX_MASK1)) {
			// Pisca LED
			for (int i=0; i<10; i++) {
				pio_clear(LED_PIO1, LED_IDX_MASK1);  // Limpa o pino LED_PIO_PIN
				delay_ms(100);                         // delay
				pio_set(LED_PIO1, LED_IDX_MASK1);    // Ativa o pino LED_PIO_PIN
				delay_ms(100);                         // delay
			}
		}
			
		// Verifica valor do pino que o botão está conectado
		else if(!pio_get(BUT_PIO2, PIO_INPUT, BUT_IDX_MASK2)) {
			// Pisca LED
			for (int i=0; i<10; i++) {
				pio_clear(LED_PIO2, LED_IDX_MASK2);  // Limpa o pino LED_PIO_PIN
				delay_ms(100);                         // delay
				pio_set(LED_PIO2, LED_IDX_MASK2);    // Ativa o pino LED_PIO_PIN
				delay_ms(100);                         // delay
			}
		}
		
		// Verifica valor do pino que o botão está conectado
		else if(!pio_get(BUT_PIO3, PIO_INPUT, BUT_IDX_MASK3)) {
			// Pisca LED
			for (int i=0; i<10; i++) {
				pio_clear(LED_PIO3, LED_IDX_MASK3);  // Limpa o pino LED_PIO_PIN
				delay_ms(100);                         // delay
				pio_set(LED_PIO3, LED_IDX_MASK3);    // Ativa o pino LED_PIO_PIN
				delay_ms(100);                         // delay
			}
		}
			
		else  {
		// Ativa o pino LED_IDX (par apagar)
		pio_set(LED_PIO1, LED_IDX_MASK1);
		pio_set(LED_PIO2, LED_IDX_MASK2);
		pio_set(LED_PIO3, LED_IDX_MASK3);
		}
	}
	// Nunca devemos chegar aqui !
	return 0;
}