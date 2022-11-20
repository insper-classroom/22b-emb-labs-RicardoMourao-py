/************************************************************************/
/* includes                                                             */
/************************************************************************/

#include <asf.h>
#include <string.h>
#include "ili9341.h"
#include "lvgl.h"
#include "touch/touch.h"
#include "page3.h"

/************************************************************************/
/* DECLARES                                                             */
/************************************************************************/
LV_FONT_DECLARE(dseg25);
LV_FONT_DECLARE(dseg40);
LV_FONT_DECLARE(dseg70);
LV_FONT_DECLARE(clock);
LV_FONT_DECLARE(heat);

/************************************************************************/
/* EXTERNAL SYMBOLS                                                     */
/************************************************************************/
#define LV_SYMBOL_CLOCK "\xEF\x80\x97"
#define MY_SYMBOL_WIND "\xEF\x9C\xAE"

/************************************************************************/
/* LCD / LVGL                                                           */
/************************************************************************/

#define LV_HOR_RES_MAX (320)
#define LV_VER_RES_MAX (240)

/*A static or global variable to store the buffers*/
static lv_disp_draw_buf_t disp_buf;

/*Static or global buffer(s). The second buffer is optional*/
static lv_color_t buf_1[LV_HOR_RES_MAX * LV_VER_RES_MAX];
static lv_disp_drv_t disp_drv; /*A variable to hold the drivers. Must be static or global.*/
static lv_indev_drv_t indev_drv;

/************************************************************************/
/* AFEC                                                                 */
/************************************************************************/
#define AFEC_POT AFEC1
#define AFEC_POT_ID ID_AFEC1
#define AFEC_POT_CHANNEL 6 // Canal do pino PC31

/************************************************************************/
/* GLOBALS                                                              */
/************************************************************************/
static lv_obj_t *labelBtn1;
static lv_obj_t *labelBtnMenu;
static lv_obj_t *labelBtnClock;
static lv_obj_t *labelBtnHome;
static lv_obj_t *labelBtnSettings;
static lv_obj_t *labelUpButton;
static lv_obj_t *labelDownButton;
static lv_obj_t *labelFloor;
static lv_obj_t *labelFloorDigit;
static lv_obj_t *labelClock;
static lv_obj_t *labelWeekDay;
static lv_obj_t *labelSetValue;
static lv_obj_t *labelBtnClock2;
static lv_obj_t *labelTempImage;
static lv_obj_t *labelSetText;

volatile uint muda_tempo = 0;

/************************************************************************/
/* TASKS                                                                */
/************************************************************************/
TaskHandle_t xRtcHandle;
TaskHandle_t xAdcHandle;

/************************************************************************/
/* QUEUES & SEMAPHORES                                                  */
/************************************************************************/
SemaphoreHandle_t xSemaphoreRTC;

QueueHandle_t xQueueADC;

/************************************************************************/
/* TYPES & STRUCTS                                                      */
/************************************************************************/
typedef struct
{
  uint32_t year;
  uint32_t month;
  uint32_t day;
  uint32_t week;
  uint32_t hour;
  uint32_t minute;
  uint32_t second;
} calendar;

typedef struct
{
  uint value;
  uint decimal;
} adcData;

typedef struct
{
  uint value;
} adcReadData;

/************************************************************************/
/* RTOS                                                                 */
/************************************************************************/

#define TASK_LCD_STACK_SIZE (1024 * 6 / sizeof(portSTACK_TYPE))
#define TASK_LCD_STACK_PRIORITY (tskIDLE_PRIORITY)

#define TASK_RTC_STACK_SIZE (1024 * 4 / sizeof(portSTACK_TYPE))
#define TASK_RTC_STACK_PRIORITY (tskIDLE_PRIORITY)

#define TASK_ADC_STACK_SIZE (1024 * 10 / sizeof(portSTACK_TYPE))
#define TASK_ADC_STACK_PRIORITY (tskIDLE_PRIORITY)

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask, signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask, signed char *pcTaskName)
{
  printf("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
  for (;;)
  {
  }
}

extern void vApplicationIdleHook(void) {}

extern void vApplicationTickHook(void) {}

extern void vApplicationMallocFailedHook(void)
{
  configASSERT((volatile void *)NULL);
}

/************************************************************************/
/* PROTOTYPES                                                           */
/************************************************************************/
void task_rtc(void *pvParameters);
void task_adc(void *pvParameters);
static void config_AFEC_pot(Afec *afec, uint32_t afec_id, uint32_t afec_channel, afec_callback_t callback);

/************************************************************************/
/* REAL TIMERS INITS                                                    */
/************************************************************************/
void RTC_init(Rtc *rtc, uint32_t id_rtc, calendar t, uint32_t irq_type)
{
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

void TC_init(Tc *TC, int ID_TC, int TC_CHANNEL, int freq)
{
  uint32_t ul_div;
  uint32_t ul_tcclks;
  uint32_t ul_sysclk = sysclk_get_cpu_hz();

  pmc_enable_periph_clk(ID_TC);

  tc_find_mck_divisor(freq, ul_sysclk, &ul_div, &ul_tcclks, ul_sysclk);
  tc_init(TC, TC_CHANNEL, ul_tcclks | TC_CMR_CPCTRG);
  tc_write_rc(TC, TC_CHANNEL, (ul_sysclk / ul_div) / freq);

  NVIC_SetPriority((IRQn_Type)ID_TC, 4);
  NVIC_EnableIRQ((IRQn_Type)ID_TC);
  tc_enable_interrupt(TC, TC_CHANNEL, TC_IER_CPCS);
}

/************************************************************************/
/* REAL TIME HANDLERS                                                   */
/************************************************************************/
void RTC_Handler(void)
{
  uint32_t ul_status = rtc_get_status(RTC);

  /* seccond tick */
  if ((ul_status & RTC_SR_SEC) == RTC_SR_SEC && muda_tempo == 0)
  {
    // o c�digo para irq de segundo vem aqui
    BaseType_t xHigherPriorityTaskWoken = pdTRUE;
    xSemaphoreGiveFromISR(xSemaphoreRTC, &xHigherPriorityTaskWoken);
  }

  /* Time or date alarm */
  if ((ul_status & RTC_SR_ALARM) == RTC_SR_ALARM)
  {
    // o c�digo para irq de alame vem aqui
  }

  rtc_clear_status(RTC, RTC_SCCR_SECCLR);
  rtc_clear_status(RTC, RTC_SCCR_ALRCLR);
  rtc_clear_status(RTC, RTC_SCCR_ACKCLR);
  rtc_clear_status(RTC, RTC_SCCR_TIMCLR);
  rtc_clear_status(RTC, RTC_SCCR_CALCLR);
  rtc_clear_status(RTC, RTC_SCCR_TDERRCLR);
}

void TC1_Handler(void)
{
  volatile uint32_t ul_dummy;

  ul_dummy = tc_get_status(TC0, 1);

  /* Avoid compiler warning */
  UNUSED(ul_dummy);

  /* Selecina canal e inicializa convers�o */
  afec_channel_enable(AFEC_POT, AFEC_POT_CHANNEL);
  afec_start_software_conversion(AFEC_POT);
}

/************************************************************************/
/* BUTTONS HANDLERS                                                     */
/************************************************************************/

static void event_handler(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
}

static void menu_handler(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
}

static void clk_handler(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
}

static void home_handler(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
}

/**
 * \brief Muda a vari�vel muda_tempo definida para:
 * 0: bot�es up e down editam a temperatura
 * 1: bot�es up e down editam os minutos
 * 2: bot�es up e down editam a hora
 *
 * \param e evento de bot�o
 *
 * \return void
 */
static void settings_handler(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_CLICKED)
  {
    muda_tempo++;
    if (muda_tempo > 2)
    {
      muda_tempo = 0;
    }

    /* Desativa interrup��o do RTC e suspende a task */
    if (muda_tempo == 1)
    {
      rtc_disable_interrupt(RTC, RTC_IER_SECEN);
      vTaskSuspend(xRtcHandle);
    }

    else if (muda_tempo == 0)
    {
      /* Reativa interrup��o do RTC e a task */
      rtc_enable_interrupt(RTC, RTC_IER_SECEN);
      vTaskResume(xRtcHandle);
    }
  }
}

static void clk2_handler(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
}

static void up_handler(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  char *p;
  int x;
  if (code == LV_EVENT_CLICKED)
  {
    if (muda_tempo == 1)
    {
      uint32_t minuto_atual, segundo_atual, hora_atual;
      rtc_get_time(RTC, &hora_atual, &minuto_atual, &segundo_atual);
      minuto_atual++;
      rtc_set_time(RTC, hora_atual, minuto_atual, segundo_atual);
      lv_label_set_text_fmt(labelClock, "%02d:%02d", hora_atual, minuto_atual);
    }
    else if (muda_tempo == 2)
    {
      uint32_t minuto_atual, segundo_atual, hora_atual;
      rtc_get_time(RTC, &hora_atual, &minuto_atual, &segundo_atual);
      hora_atual++;
      rtc_set_time(RTC, hora_atual, minuto_atual, segundo_atual);
      lv_label_set_text_fmt(labelClock, "%02d:%02d", hora_atual, minuto_atual);
    }
    else if (muda_tempo == 0)
    {
      p = lv_label_get_text(labelSetValue);
      x = atoi(p);
      lv_label_set_text_fmt(labelSetValue, "%02d", x + 1);
    }
  }
}

static void down_handler(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  char *p;
  int x;
  /* Quando o bot�o � pressionado e muda_tempo=1, altera minuto */
  if ((code == LV_EVENT_CLICKED) && muda_tempo == 1)
  {
    uint32_t minuto_atual, segundo_atual, hora_atual;
    rtc_get_time(RTC, &hora_atual, &minuto_atual, &segundo_atual);
    minuto_atual--;
    rtc_set_time(RTC, hora_atual, minuto_atual, segundo_atual);
    lv_label_set_text_fmt(labelClock, "%02d:%02d", hora_atual, minuto_atual);
  }
  /* Quando o bot�o � pressionado e muda_tempo=2, altera hora */
  else if ((code == LV_EVENT_CLICKED) && muda_tempo == 2)
  {
    uint32_t minuto_atual, segundo_atual, hora_atual;
    rtc_get_time(RTC, &hora_atual, &minuto_atual, &segundo_atual);
    hora_atual--;
    rtc_set_time(RTC, hora_atual, minuto_atual, segundo_atual);
    lv_label_set_text_fmt(labelClock, "%02d:%02d", hora_atual, minuto_atual);
  }

  /* Quando o bot�o � pressionado e muda_tempo=0, altera temperatura */
  else if (code == LV_EVENT_CLICKED && muda_tempo == 0)
  {
    p = lv_label_get_text(labelSetValue);
    x = atoi(p);
    lv_label_set_text_fmt(labelSetValue, "%02d", x - 1);
  }
}

/************************************************************************/
/* CALLBACKS                                                            */
/************************************************************************/
static void AFEC_pot_Callback(void)
{
  adcReadData adc;
  adc.value = afec_channel_get_value(AFEC_POT, AFEC_POT_CHANNEL);
  BaseType_t xHigherPriorityTaskWoken = pdTRUE;
  xQueueSendFromISR(xQueueADC, &adc, &xHigherPriorityTaskWoken);
}

/************************************************************************/
/* LVGL DESIGNS                                                         */
/************************************************************************/

void lv_termostato(void)
{
  static lv_style_t style;
  static lv_obj_t *labelCelcius;
  static lv_obj_t *labelFloorTemp;
  static lv_obj_t *labelEndOfMenu;

  lv_style_init(&style);
  lv_style_set_bg_color(&style, lv_color_black());
  lv_style_set_pad_all(&style, 0);
  lv_style_set_border_width(&style, 0);

  /* Configure power button */
  lv_obj_t *btn1 = lv_btn_create(lv_scr_act());
  lv_obj_add_event_cb(btn1, event_handler, LV_EVENT_ALL, NULL);
  lv_obj_align(btn1, LV_ALIGN_BOTTOM_LEFT, 5, -5);
  lv_obj_add_style(btn1, &style, 0);
  labelBtn1 = lv_label_create(btn1);
  lv_label_set_text(labelBtn1, "[ " LV_SYMBOL_POWER);
  lv_obj_center(labelBtn1);

  /* Configure menu button */
  lv_obj_t *btnMenu = lv_btn_create(lv_scr_act());
  lv_obj_add_event_cb(btnMenu, menu_handler, LV_EVENT_ALL, NULL);
  lv_obj_align_to(btnMenu, btn1, LV_ALIGN_OUT_RIGHT_TOP, 5, 0);
  lv_obj_add_style(btnMenu, &style, 0);
  labelBtnMenu = lv_label_create(btnMenu);
  lv_label_set_text(labelBtnMenu, "| M |");
  lv_obj_center(labelBtnMenu);

  /* Configure clock button */
  lv_obj_t *btnClock = lv_btn_create(lv_scr_act());
  lv_obj_add_event_cb(btnClock, clk_handler, LV_EVENT_ALL, NULL);
  lv_obj_align_to(btnClock, btnMenu, LV_ALIGN_OUT_RIGHT_TOP, 5, 3);
  lv_obj_add_style(btnClock, &style, 0);
  lv_obj_set_style_text_font(btnClock, &clock, LV_STATE_DEFAULT);
  labelBtnClock = lv_label_create(btnClock);
  lv_label_set_text(labelBtnClock, LV_SYMBOL_CLOCK " ]");
  lv_obj_center(labelBtnClock);

  /* Configure home button */
  lv_obj_t *btnHome = lv_btn_create(lv_scr_act());
  lv_obj_add_event_cb(btnHome, home_handler, LV_EVENT_ALL, NULL);
  lv_obj_align_to(btnHome, btnClock, LV_ALIGN_OUT_TOP_RIGHT, 30, -15);
  lv_obj_add_style(btnHome, &style, 0);
  labelBtnHome = lv_label_create(btnHome);
  lv_label_set_text(labelBtnHome, LV_SYMBOL_HOME);
  lv_obj_center(labelBtnHome);

  /* Configure down button */
  lv_obj_t *btnDown = lv_btn_create(lv_scr_act());
  lv_obj_add_event_cb(btnDown, down_handler, LV_EVENT_ALL, NULL);
  lv_obj_align(btnDown, LV_ALIGN_BOTTOM_RIGHT, 0, -5);
  lv_obj_add_style(btnDown, &style, 0);
  labelDownButton = lv_label_create(btnDown);
  lv_label_set_text(labelDownButton, " | " LV_SYMBOL_DOWN " ]");
  lv_obj_center(labelDownButton);

  /* Configure up button */
  lv_obj_t *btnUp = lv_btn_create(lv_scr_act());
  lv_obj_add_event_cb(btnUp, up_handler, LV_EVENT_ALL, NULL);
  lv_obj_align_to(btnUp, btnDown, LV_ALIGN_OUT_LEFT_TOP, -20, 0);
  lv_obj_add_style(btnUp, &style, 0);
  labelUpButton = lv_label_create(btnUp);
  lv_label_set_text(labelUpButton, " [ " LV_SYMBOL_UP " ");
  lv_obj_center(labelUpButton);

  /* Configure end of left menu */
  labelEndOfMenu = lv_label_create(lv_scr_act());
  lv_obj_align_to(labelEndOfMenu, btnClock, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
  lv_obj_set_style_text_color(labelEndOfMenu, lv_color_white(), LV_STATE_DEFAULT);
  lv_label_set_text(labelEndOfMenu, " ]");

  /* Configure label temperature floor */
  labelFloor = lv_label_create(lv_scr_act());
  lv_obj_align(labelFloor, LV_ALIGN_LEFT_MID, 35, -45);
  lv_obj_set_style_text_font(labelFloor, &dseg70, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(labelFloor, lv_color_white(), LV_STATE_DEFAULT);
  lv_label_set_text_fmt(labelFloor, "%02d", 23);

  /* Configure label temperature digit floor */
  labelFloorDigit = lv_label_create(lv_scr_act());
  lv_obj_align_to(labelFloorDigit, labelFloor, LV_ALIGN_OUT_RIGHT_MID, 0, 5);
  lv_obj_set_style_text_font(labelFloorDigit, &dseg40, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(labelFloorDigit, lv_color_white(), LV_STATE_DEFAULT);
  lv_label_set_text_fmt(labelFloorDigit, ".%d", 4);

  /* Configure week day label */
  labelWeekDay = lv_label_create(lv_scr_act());
  lv_obj_align_to(labelWeekDay, labelFloor, LV_ALIGN_OUT_TOP_LEFT, 0, 0);
  lv_obj_set_style_text_font(labelWeekDay, &lv_font_montserrat_12, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(labelWeekDay, lv_color_white(), LV_STATE_DEFAULT);
  lv_label_set_text(labelWeekDay, "MON");

  /* Configure FLOOR TEMP label */
  labelFloorTemp = lv_label_create(lv_scr_act());
  lv_obj_align_to(labelFloorTemp, labelFloor, LV_ALIGN_OUT_LEFT_MID, 20, 0);
  lv_obj_set_style_text_font(labelFloorTemp, &lv_font_montserrat_8, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(labelFloorTemp, lv_color_white(), LV_STATE_DEFAULT);
  lv_label_set_text(labelFloorTemp, "FLOOR\nTEMP");

  /* Configure clock floor */
  labelClock = lv_label_create(lv_scr_act());
  lv_obj_align(labelClock, LV_ALIGN_TOP_RIGHT, -5, 5);
  lv_obj_set_style_text_font(labelClock, &dseg25, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(labelClock, lv_color_white(), LV_STATE_DEFAULT);
  lv_label_set_text_fmt(labelClock, "%02d:%02d", 12, 40);

  /* Configure set value label */
  labelSetValue = lv_label_create(lv_scr_act());
  lv_obj_align_to(labelSetValue, labelClock, LV_ALIGN_OUT_BOTTOM_LEFT, -5, 25);
  lv_obj_set_style_text_font(labelSetValue, &dseg40, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(labelSetValue, lv_color_white(), LV_STATE_DEFAULT);
  lv_label_set_text_fmt(labelSetValue, "%02d", 22);

  /* Configure settings button */
  lv_obj_t *btnSettings = lv_btn_create(lv_scr_act());
  lv_obj_add_event_cb(btnSettings, settings_handler, LV_EVENT_ALL, NULL);
  lv_obj_align_to(btnSettings, labelSetValue, LV_ALIGN_OUT_LEFT_TOP, 0, 0);
  lv_obj_add_style(btnSettings, &style, 0);
  labelBtnSettings = lv_label_create(btnSettings);
  lv_label_set_text(labelBtnSettings, LV_SYMBOL_SETTINGS);
  lv_obj_center(labelBtnSettings);

  /* Configure set text label */
  labelSetText = lv_label_create(lv_scr_act());
  lv_obj_align_to(labelSetText, labelBtnSettings, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_text_font(labelSetText, &lv_font_montserrat_12, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(labelSetText, lv_color_white(), LV_STATE_DEFAULT);
  lv_label_set_text(labelSetText, "SET");

  /* Configure clock2 button */
  lv_obj_t *btnClock2 = lv_btn_create(lv_scr_act());
  lv_obj_add_event_cb(btnClock2, clk2_handler, LV_EVENT_ALL, NULL);
  lv_obj_align_to(btnClock2, labelSetValue, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);
  lv_obj_add_style(btnClock2, &style, 0);
  lv_obj_set_style_text_font(btnClock2, &clock, LV_STATE_DEFAULT);
  labelBtnClock2 = lv_label_create(btnClock2);
  lv_label_set_text(labelBtnClock2, LV_SYMBOL_CLOCK);
  lv_obj_center(labelBtnClock2);

  /* Configure week day label */
  labelTempImage = lv_label_create(lv_scr_act());
  lv_obj_align_to(labelTempImage, labelSetValue, LV_ALIGN_OUT_BOTTOM_RIGHT, 25, 8);
  lv_obj_set_style_text_font(labelTempImage, &heat, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(labelTempImage, lv_color_white(), LV_STATE_DEFAULT);
  lv_label_set_text(labelTempImage, MY_SYMBOL_WIND);

  /* Configure �C label */
  labelCelcius = lv_label_create(lv_scr_act());
  lv_obj_align_to(labelCelcius, labelSetValue, LV_ALIGN_OUT_RIGHT_TOP, 5, 0);
  lv_obj_set_style_text_font(labelCelcius, &lv_font_montserrat_12, LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(labelCelcius, lv_color_white(), LV_STATE_DEFAULT);
  lv_label_set_text(labelCelcius, " �C");
}

/************************************************************************/
/* TASKS                                                                */
/************************************************************************/

static void task_lcd(void *pvParameters)
{
  int px, py;

  lv_termostato();

  for (;;)
  {
    lv_tick_inc(50);
    lv_task_handler();
    vTaskDelay(50);
  }
}

void task_rtc(void *pvParameters)
{
  uint32_t hora_atual, minuto_atual, segundo_atual;
  // uint32_t current_year, current_month, current_day, current_week;

  calendar rtc_calendar = {2022, 5, 19, 12, 15, 45, 1};
  RTC_init(RTC, ID_RTC, rtc_calendar, RTC_IER_SECEN);

  for (;;)
  {
    if (xSemaphoreTake(xSemaphoreRTC, portMAX_DELAY) == pdTRUE)
    {
      // rtc_get_date(RTC, &current_year, &current_month, &current_day, &current_week);
      rtc_get_time(RTC, &hora_atual, &minuto_atual, &segundo_atual);
      if (segundo_atual % 2 == 0)
      {
        lv_label_set_text_fmt(labelClock, "%02d:%02d", hora_atual, minuto_atual);
      }
      else
      {
        lv_label_set_text_fmt(labelClock, "%02d %02d", hora_atual, minuto_atual);
      }
    }
  }
}

void task_adc(void *pvParameters)
{
  // configura PROC e TC para controlar a leitura
  config_AFEC_pot(AFEC_POT, AFEC_POT_ID, AFEC_POT_CHANNEL, AFEC_pot_Callback);
  TC_init(TC0, ID_TC1, 1, 2);
  tc_start(TC0, 1);

  adcData adc;
  adcReadData adcRead;
  float data;

  printf("Task ADC iniciada\n");

  for (;;)
  {
    if (xQueueReceive(xQueueADC, &(adcRead), 500))
    {
      printf("adc: %d\n", adcRead);
      data = (float)(adcRead.value * 100) / 4095;
      printf("data: %f\n", data);
      adc.value = (int)data;
      uint decimal = data * 10;
      adc.decimal = (decimal % 10);
      lv_label_set_text_fmt(labelFloor, "%02d", adc.value);
      lv_label_set_text_fmt(labelFloorDigit, ".%01d", adc.decimal);
    }
    vTaskDelay(50);
  }
}

/************************************************************************/
/* CONFIGS                                                              */
/************************************************************************/

static void configure_lcd(void)
{
  /**LCD pin configure on SPI*/
  pio_configure_pin(LCD_SPI_MISO_PIO, LCD_SPI_MISO_FLAGS); //
  pio_configure_pin(LCD_SPI_MOSI_PIO, LCD_SPI_MOSI_FLAGS);
  pio_configure_pin(LCD_SPI_SPCK_PIO, LCD_SPI_SPCK_FLAGS);
  pio_configure_pin(LCD_SPI_NPCS_PIO, LCD_SPI_NPCS_FLAGS);
  pio_configure_pin(LCD_SPI_RESET_PIO, LCD_SPI_RESET_FLAGS);
  pio_configure_pin(LCD_SPI_CDS_PIO, LCD_SPI_CDS_FLAGS);

  ili9341_init();
  ili9341_backlight_on();
}

static void configure_console(void)
{
  const usart_serial_options_t uart_serial_options = {
      .baudrate = USART_SERIAL_EXAMPLE_BAUDRATE,
      .charlength = USART_SERIAL_CHAR_LENGTH,
      .paritytype = USART_SERIAL_PARITY,
      .stopbits = USART_SERIAL_STOP_BIT,
  };

  /* Configure console UART. */
  stdio_serial_init(CONSOLE_UART, &uart_serial_options);

  /* Specify that stdout should not be buffered. */
  setbuf(stdout, NULL);
}

static void config_AFEC_pot(Afec *afec, uint32_t afec_id, uint32_t afec_channel,
                            afec_callback_t callback)
{
  /*************************************
   * Ativa e configura AFEC
   *************************************/
  /* Ativa AFEC - 0 */
  afec_enable(afec);

  /* struct de configuracao do AFEC */
  struct afec_config afec_cfg;

  /* Carrega parametros padrao */
  afec_get_config_defaults(&afec_cfg);

  /* Configura AFEC */
  afec_init(afec, &afec_cfg);

  /* Configura trigger por software */
  afec_set_trigger(afec, AFEC_TRIG_SW);

  /*** Configuracao espec�fica do canal AFEC ***/
  struct afec_ch_config afec_ch_cfg;
  afec_ch_get_config_defaults(&afec_ch_cfg);
  afec_ch_cfg.gain = AFEC_GAINVALUE_0;
  afec_ch_set_config(afec, afec_channel, &afec_ch_cfg);

  /*
  * Calibracao:
  * Because the internal ADC offset is 0x200, it should cancel it and shift
  down to 0.
  */
  afec_channel_set_analog_offset(afec, afec_channel, 0x200);

  /***  Configura sensor de temperatura ***/
  struct afec_temp_sensor_config afec_temp_sensor_cfg;

  afec_temp_sensor_get_config_defaults(&afec_temp_sensor_cfg);
  afec_temp_sensor_set_config(afec, &afec_temp_sensor_cfg);

  /* configura IRQ */
  afec_set_callback(afec, afec_channel, callback, 1);
  NVIC_SetPriority(afec_id, 4);
  NVIC_EnableIRQ(afec_id);
}

/************************************************************************/
/* port lvgl                                                            */
/************************************************************************/

void my_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
  ili9341_set_top_left_limit(area->x1, area->y1);
  ili9341_set_bottom_right_limit(area->x2, area->y2);
  ili9341_copy_pixels_to_screen(color_p, (area->x2 + 1 - area->x1) * (area->y2 + 1 - area->y1));

  /* IMPORTANT!!!
   * Inform the graphics library that you are ready with the flushing*/
  lv_disp_flush_ready(disp_drv);
}

void my_input_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
  int px, py, pressed;

  if (readPoint(&px, &py))
    data->state = LV_INDEV_STATE_PRESSED;
  else
    data->state = LV_INDEV_STATE_RELEASED;

  data->point.x = px;
  data->point.y = py;
}

void configure_lvgl(void)
{
  lv_init();
  lv_disp_draw_buf_init(&disp_buf, buf_1, NULL, LV_HOR_RES_MAX * LV_VER_RES_MAX);

  lv_disp_drv_init(&disp_drv);       /*Basic initialization*/
  disp_drv.draw_buf = &disp_buf;     /*Set an initialized buffer*/
  disp_drv.flush_cb = my_flush_cb;   /*Set a flush callback to draw to the display*/
  disp_drv.hor_res = LV_HOR_RES_MAX; /*Set the horizontal resolution in pixels*/
  disp_drv.ver_res = LV_VER_RES_MAX; /*Set the vertical resolution in pixels*/

  lv_disp_t *disp;
  disp = lv_disp_drv_register(&disp_drv); /*Register the driver and save the created display objects*/

  /* Init input on LVGL */
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_input_read;
  lv_indev_t *my_indev = lv_indev_drv_register(&indev_drv);
}

/************************************************************************/
/* main                                                                 */
/************************************************************************/
int main(void)
{
  /* board and sys init */
  board_init();
  sysclk_init();
  configure_console();

  /* LCd, touch and lvgl init*/
  configure_lcd();
  configure_touch();
  configure_lvgl();

  xSemaphoreRTC = xSemaphoreCreateBinary();
  if (xSemaphoreRTC == NULL)
  {
    printf("Failed to create RTC semaphore");
  }

  xQueueADC = xQueueCreate(64, sizeof(adcReadData));
  if (xQueueADC == NULL)
  {
    printf("Falha ao criar queueADC \n");
  }

  /* Create task to control oled */
  if (xTaskCreate(task_lcd, "LCD", TASK_LCD_STACK_SIZE, NULL, TASK_LCD_STACK_PRIORITY, NULL) != pdPASS)
  {
    printf("Failed to create lcd task\r\n");
  }

  /* Create task to control rtc */
  if (xTaskCreate(task_rtc, "RTC", TASK_RTC_STACK_SIZE, NULL, TASK_RTC_STACK_PRIORITY, &xRtcHandle) != pdPASS)
  {
    printf("Failed to create rtc task\r\n");
  }

  /* Create task to control potentiometer data */
  if (xTaskCreate(task_adc, "ADC", TASK_ADC_STACK_SIZE, NULL, TASK_ADC_STACK_PRIORITY, &xAdcHandle) != pdPASS)
  {
    printf("Failed to create adc task\r\n");
  }

  /* Start the scheduler. */
  vTaskStartScheduler();

  while (1)
  {
  }
}
