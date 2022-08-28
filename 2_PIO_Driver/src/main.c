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

/*  Default pin configuration (no attribute). */
#define _PIO_DEFAULT             (0u << 0)
/*  The internal pin pull-up is active. */
#define _PIO_PULLUP              (1u << 0)
/*  The internal glitch filter is active. */
#define _PIO_DEGLITCH            (1u << 1)
/*  The internal debouncing filter is active. */
#define _PIO_DEBOUNCE            (1u << 3)

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

/**
 * \brief Set a high output level on all the PIOs defined in ul_mask.
 * This has no immediate effects on PIOs that are not output, but the PIO
 * controller will save the value if they are changed to outputs.
 *
 * \param p_pio Pointer to a PIO instance.
 * \param ul_mask Bitmask of one or more pin(s) to configure.
 */
void _pio_set(Pio *p_pio, const uint32_t ul_mask)
/* aciona um pino digital quando o mesmo é configurado como output (fazer ele virar 3.3V).*/
/* *p_pio: é um endereço recebido do tipo Pio, ele indica o endereço de memória na qual o PIO (periférico) em questão está mapeado (vamos ver isso em detalhes).*/
/* ul_mask: é a máscara na qual iremos aplicar ao registrador que controla os pinos para colocarmos 1 na saída.*/
{
    p_pio->PIO_SODR = ul_mask;
}

/**
 * \brief Set a low output level on all the PIOs defined in ul_mask.
 * This has no immediate effects on PIOs that are not output, but the PIO
 * controller will save the value if they are changed to outputs.
 *
 * \param p_pio Pointer to a PIO instance.
 * \param ul_mask Bitmask of one or more pin(s) to configure.
 */
void _pio_clear(Pio *p_pio, const uint32_t ul_mask)
{
	p_pio->PIO_CODR = ul_mask;
}

/**
 * \brief Configure PIO internal pull-up.
 *
 * \param p_pio Pointer to a PIO instance.
 * \param ul_mask Bitmask of one or more pin(s) to configure.
 * \param ul_pull_up_enable Indicates if the pin(s) internal pull-up shall be
 * configured.
 */
void _pio_pull_up(Pio *p_pio, const uint32_t ul_mask,
        const uint32_t ul_pull_up_enable)
{
	/* Enable the pull-up(s) if necessary */
	if (ul_pull_up_enable) {
		p_pio->PIO_PUER = ul_mask;
	} else {
		p_pio->PIO_PUDR = ul_mask;
	}
 }

/**
 * \brief Configure one or more pin(s) or a PIO controller as inputs.
 * Optionally, the corresponding internal pull-up(s) and glitch filter(s) can
 * be enabled.
 *
 * \param p_pio Pointer to a PIO instance.
 * \param ul_mask Bitmask indicating which pin(s) to configure as input(s).
 * \param ul_attribute PIO attribute(s).
 */
void _pio_set_input(Pio *p_pio, const uint32_t ul_mask,
        const uint32_t ul_attribute)
{
	_pio_pull_up(p_pio, ul_mask, ul_attribute & _PIO_PULLUP);

	/* Enable Input Filter if necessary */
	if (ul_attribute & (_PIO_DEGLITCH | _PIO_DEBOUNCE)) {
		p_pio->PIO_IFER = ul_mask;
	} else {
		p_pio->PIO_IFDR = ul_mask;
	}
	/* Enable de-glitch or de-bounce if necessary */
	if (ul_attribute & _PIO_DEGLITCH) {
		p_pio->PIO_IFSCDR = ul_mask;
	} else {
		if (ul_attribute & _PIO_DEBOUNCE) {
			p_pio->PIO_IFSCER = ul_mask;
		}
	}

	/* Configure pin as input */
	p_pio->PIO_ODR = ul_mask;
	p_pio->PIO_PER = ul_mask;
}

/**
 * \brief Configure one or more pin(s) of a PIO controller as outputs, with
 * the given default value. Optionally, the multi-drive feature can be enabled
 * on the pin(s).
 *
 * \param p_pio Pointer to a PIO instance.
 * \param ul_mask Bitmask indicating which pin(s) to configure.
 * \param ul_default_level Default level on the pin(s).
 * \param ul_multidrive_enable Indicates if the pin(s) shall be configured as
 * open-drain.
 * \param ul_pull_up_enable Indicates if the pin shall have its pull-up
 * activated.
 */
void _pio_set_output(Pio *p_pio, const uint32_t ul_mask,
        const uint32_t ul_default_level,
        const uint32_t ul_multidrive_enable,
        const uint32_t ul_pull_up_enable)
{
	pio_pull_up(p_pio, ul_mask, ul_pull_up_enable);

	/* Enable multi-drive if necessary */
	if (ul_multidrive_enable) {
		p_pio->PIO_MDER = ul_mask;
		} else {
		p_pio->PIO_MDDR = ul_mask;
	}

	/* Set default value */
	if (ul_default_level) {
		p_pio->PIO_SODR = ul_mask;
		} else {
		p_pio->PIO_CODR = ul_mask;
	}

	/* Configure pin(s) as output(s) */
	p_pio->PIO_OER = ul_mask;
	p_pio->PIO_PER = ul_mask;
}

// Função de inicialização do uC
void init(void){
	// Initialize the board clock
	sysclk_init();

	// Desativa WatchDog Timer
	WDT->WDT_MR = WDT_MR_WDDIS;
	
	// Ativa o PIO na qual o LED foi conectado
	// para que possamos controlar o LED.
	//pmc_enable_periph_clk(LED_PIO_ID);
	pmc_enable_periph_clk(LED_PIO_ID1);
	pmc_enable_periph_clk(LED_PIO_ID2);
	pmc_enable_periph_clk(LED_PIO_ID3);
	
	// Inicializa PIO do botao
	//pmc_enable_periph_clk(BUT_PIO_ID);
	pmc_enable_periph_clk(BUT_PIO_ID1);
	pmc_enable_periph_clk(BUT_PIO_ID2);
	pmc_enable_periph_clk(BUT_PIO_ID3);
	
	//Inicializa PC8 como saída
	//pio_set_output(LED_PIO, LED_IDX_MASK, 0, 0, 0);
	_pio_set_output(LED_PIO1, LED_IDX_MASK1, 0, 0, 0);
	_pio_set_output(LED_PIO2, LED_IDX_MASK2, 0, 0, 0);
	_pio_set_output(LED_PIO3, LED_IDX_MASK3, 0, 0, 0);
	
	// Inicializa o botão como entrada
	//pio_set_input(BUT_PIO, BUT_IDX_MASK, 0);
	//pio_set_input(BUT_PIO1, BUT_IDX_MASK1, 0);
	//pio_set_input(BUT_PIO2, BUT_IDX_MASK2, 0);
	//pio_set_input(BUT_PIO3, BUT_IDX_MASK3, 0);
	
	// Realiza o pull-up no botão
	//pio_pull_up(BUT_PIO, BUT_IDX_MASK, 1);
	//_pio_pull_up(BUT_PIO1, BUT_IDX_MASK1, 1);
	//_pio_pull_up(BUT_PIO2, BUT_IDX_MASK2, 1);
	//_pio_pull_up(BUT_PIO3, BUT_IDX_MASK3, 1);
	
	_pio_set_input(BUT_PIO1, BUT_IDX_MASK1, _PIO_PULLUP | _PIO_DEBOUNCE);
	_pio_set_input(BUT_PIO2, BUT_IDX_MASK2, _PIO_PULLUP | _PIO_DEBOUNCE);
	_pio_set_input(BUT_PIO3, BUT_IDX_MASK3, _PIO_PULLUP | _PIO_DEBOUNCE);

}

int main(void)
{
	// inicializa sistema e IOs
	init();
	//pio_set(LED_PIO, LED_IDX_MASK);
	pio_set(LED_PIO1, LED_IDX_MASK1);
	pio_set(LED_PIO2, LED_IDX_MASK2);
	pio_set(LED_PIO3, LED_IDX_MASK3);

	// SUPER LOOP
	// aplicacoes embarcadas no devem sair do while(1).
	while(1) {
		// Verifica valor do pino que o botão está conectado
		if(!pio_get(BUT_PIO1, PIO_INPUT, BUT_IDX_MASK1)) {
			// Pisca LED
			for (int i=0; i<10; i++) {
				_pio_clear(LED_PIO1, LED_IDX_MASK1);  // Limpa o pino LED_PIO_PIN
				delay_ms(100);                         // delay
				_pio_set(LED_PIO1, LED_IDX_MASK1);    // Ativa o pino LED_PIO_PIN
				delay_ms(100);                         // delay
			}
		}
			
		// Verifica valor do pino que o botão está conectado
		else if(!pio_get(BUT_PIO2, PIO_INPUT, BUT_IDX_MASK2)) {
			// Pisca LED
			for (int i=0; i<10; i++) {
				_pio_clear(LED_PIO2, LED_IDX_MASK2);  // Limpa o pino LED_PIO_PIN
				delay_ms(100);                         // delay
				_pio_set(LED_PIO2, LED_IDX_MASK2);    // Ativa o pino LED_PIO_PIN
				delay_ms(100);                         // delay
			}
		}
		
		// Verifica valor do pino que o botão está conectado
		else if(!pio_get(BUT_PIO3, PIO_INPUT, BUT_IDX_MASK3)) {
			// Pisca LED
			for (int i=0; i<10; i++) {
				_pio_clear(LED_PIO3, LED_IDX_MASK3);  // Limpa o pino LED_PIO_PIN
				delay_ms(100);                         // delay
				_pio_set(LED_PIO3, LED_IDX_MASK3);    // Ativa o pino LED_PIO_PIN
				delay_ms(100);                         // delay
			}
		}
			
		else  {
		// Ativa o pino LED_IDX (par apagar)
		_pio_set(LED_PIO1, LED_IDX_MASK1);
		_pio_set(LED_PIO2, LED_IDX_MASK2);
		_pio_set(LED_PIO3, LED_IDX_MASK3);
		}
	}
	// Nunca devemos chegar aqui !
	return 0;
}