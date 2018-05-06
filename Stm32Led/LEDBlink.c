#include <stm32f10x.h>
#include <stm32f10x_gpio.h>
#include <stm32f10x_rcc.h>
#include "stm32f10x_tim.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Подключаем библиотеку и драйвер GPIO */
#include "hd44780.h"
#include "hd44780_stm32f10x.h"
#include "stm32f10x_exti.h"
#include <misc.h>
 
#define BUTTON_PIN GPIO_Pin_4
#define BUTTON_EXTI_LINE EXTI_Line4

#define TIMER TIM4
#define TIM_PERIOD 1000
#define TIM_PRESCALER 1



typedef enum {
	DUTY_CYCLE,
	PERIOD,
	POLARITY,
	PRESCALER,
	PULSE,
}Mode;


void init_lcd(void);
void encoder_init(void);
void enc_exti_init(void);
void button_exti_init(void);
void timer_init(void);
//void timer_init(TIM_TypeDef* TIMx);

void delay_microseconds(uint16_t us);
uint32_t uint32_time_diff(uint32_t now, uint32_t before);
void hd44780_assert_failure_handler(const char *filename, unsigned long line);

void RefreshPercentageValue(int currentValue, int base);
void RefreshNumericValue(int currentValue);
void RefreshStringValue(char string[32]);
static inline char *stringFromMode(Mode f);
void PrintHeader(Mode mode);
void AdjustValue(uint16_t *currentParameter, int positive);

HD44780 lcd;
HD44780_STM32F10x_GPIO_Driver lcd_pindriver;

volatile uint32_t systick_ms = 0;
//volatile uint8_t encoder_status = INIT;
//volatile uint8_t encoder_direction = FORWARD;

/*скважность ШИМ*/ 
int TIM_Numeric = 0;
int lastInt = 1;

char TIM_String[12] = "\0";
char lastChar[12] = "Low";

int TIM_Period = TIM_PERIOD;

int tick = 0;
Mode currentMode = DUTY_CYCLE;


volatile uint16_t *dutyCyclePointer;
volatile uint16_t *periodPointer;
volatile uint16_t *ccerPointer; //userd for |= polarity
volatile uint16_t *prescalerPointer;

volatile uint16_t *currentParam;

int main(void)
{
	SysTick_Config(SystemCoreClock / 1000);
	
	#pragma region GPIO setup code
 
	GPIO_InitTypeDef port; 
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
 
	//encoder
	GPIO_StructInit(&port);
	port.GPIO_Mode = GPIO_Mode_IPU;
	port.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
	port.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOB, &port);
	
	//buttons
	GPIO_StructInit(&port);
	port.GPIO_Mode = GPIO_Mode_IPU;
	port.GPIO_Pin = BUTTON_PIN;
	port.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOB, &port);
	
	//LED	
	GPIO_StructInit(&port);
	port.GPIO_Mode = GPIO_Mode_AF_PP;
	port.GPIO_Pin = GPIO_Pin_6;
	port.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOB, &port);
	#pragma endregion
	
	timer_init();
	init_lcd();
	enc_exti_init();
	//encoder_init();
	PrintHeader(currentMode);
	
	dutyCyclePointer = &TIMER->CCR1;
	periodPointer = &TIMER->ARR;
	ccerPointer = &TIMER->CCER;
	prescalerPointer = &TIMER->PSC;
	
	currentParam = dutyCyclePointer;
	while (1)
	{
		int bit0 = GPIO_ReadInputDataBit(GPIOB, BUTTON_PIN);
		if (bit0 == 0) 
		{
			SelectNextMode();
		}
	
		
		switch (currentMode)
		{
		case DUTY_CYCLE:
			if (lastInt != *dutyCyclePointer)
			{
				lastInt = *dutyCyclePointer;
				RefreshPercentageValue(*dutyCyclePointer, *periodPointer);
			}
			break;
		case PERIOD:
			if (lastInt != *periodPointer)
			{
				lastInt = *periodPointer;
				RefreshNumericValue(*periodPointer);
			}
			break;
		case POLARITY:
			if (TIMER->CCER == 0)
			{
				RefreshStringValue("high");
			}
			else
			{
				RefreshStringValue("low");
			}
			break;
		case PRESCALER:
			if (lastInt != TIMER->PSC)
			{
				lastInt = TIMER->PSC;
				RefreshNumericValue(TIMER->PSC);
			}
			break;
		case PULSE:
			if (lastInt != *dutyCyclePointer)
			{
				lastInt = *dutyCyclePointer;
				RefreshNumericValue(*dutyCyclePointer);
			}
			break;
		default:
			break;
		}
	}
}

void SelectNextMode()
{
	currentMode = (currentMode + 1) % (PULSE + 1);
	hd44780_clear(&lcd);
	PrintHeader(currentMode);
	
	switch (currentMode)
	{
	case DUTY_CYCLE:
		currentParam = dutyCyclePointer;
		//RefreshPercentageValue(TIMER->CCR1, TIM_PERIOD);
		break;
	case PERIOD:
		currentParam = periodPointer;
		//RefreshNumericValue(TIM_Period);
		break;
	case POLARITY:
		currentParam = ccerPointer;
		if (TIMER->CCER == 0)
		{
			RefreshStringValue("high");
		}
		else
		{
			RefreshStringValue("low");
		}
		break;
	case PRESCALER:
		currentParam = prescalerPointer;
		//RefreshNumericValue(TIMER->PSC);
		break;
	case PULSE:
		currentParam = dutyCyclePointer;
		//RefreshNumericValue(TIMER->CCR1);
		break;
	default:
		break;
	}
}

// Обработчик прерывания для энкодера по спаду PB0
void EXTI0_IRQHandler(void)
{
	uint32_t nCount = 3;
 
	// Убеждаемся, что флаг прерывания установлен
	if(EXTI_GetITStatus(EXTI_Line0) != RESET) {
		int bit1 = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1);
		int bit0 = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0);
		
		if (bit1 == 1) 
		{
//			if (TIM_Numeric < TIM_PERIOD)
//			{
//				TIM_Numeric += 8;
//			} 
//			TIM4->CCR1 = TIM_Numeric;
			AdjustValue(currentParam,10);
		}
		else {
			// Ручку крутят против часовой стрелки
//			if(TIM_Numeric > 0)
//			{
//				TIM_Numeric -= 8;
//			}				
//			TIM4->CCR1 = TIM_Numeric;
			AdjustValue(currentParam, -10);
		}
 
		for (nCount *= 100000; nCount; nCount--) ;
 
		// Сбрасываем флаг прерывания
		EXTI_ClearITPendingBit(EXTI_Line0);
	}
}

void AdjustValue(uint16_t *currentParameter, int positive)
{
	if (positive > 0)
	{
		//TIM_Numeric += 10;
		*currentParameter += 10;
	}
	else if (positive < 0)
	{
		//TIM_Numeric -= 10;
		*currentParameter -= 10;
	}
	else
	{
		
	}
	TIM_Numeric = *currentParameter;
}

// Обработчик прерывания для энкодера по спаду PB4
void EXTI4_IRQHandler(void) 
{
	uint32_t nCount = 3;
 
	// Убеждаемся, что флаг прерывания установлен
	if(EXTI_GetITStatus(BUTTON_EXTI_LINE) != RESET) {
		int bit0 = GPIO_ReadInputDataBit(GPIOB, BUTTON_PIN);
		if (bit0 == 0) 
		{
			SelectNextMode();
		}
		for (nCount *= 100000; nCount; nCount--) ;
 
		// Сбрасываем флаг прерывания
		EXTI_ClearITPendingBit(BUTTON_EXTI_LINE);
	}
}


/*Настройка Энкодера*/
void encoder_init(void)
{	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	
	/* Налаштовуємо TIM3 */
	TIM_TimeBaseInitTypeDef TIMER_InitStructure;
	TIM_TimeBaseStructInit(&TIMER_InitStructure);
	// Вказуємо TIM_Period - до скількох рахувати таймеру при обертах енкодера
	TIMER_InitStructure.TIM_Period = TIM_PERIOD;
	// Дозволяємо рахувати у обидва боки
	TIMER_InitStructure.TIM_CounterMode = TIM_CounterMode_Up | TIM_CounterMode_Down;
	TIM_TimeBaseInit(TIM3, &TIMER_InitStructure);
 
	/* Налаштовуємо Encoder Interface */
	TIM_EncoderInterfaceConfig(TIM3, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);
 
	TIM_Cmd(TIM3, ENABLE);
}

void timer_init(void)
{
	TIM_TimeBaseInitTypeDef timer;
	TIM_OCInitTypeDef timerPWM;
	
	TIM_TimeBaseStructInit(&timer);
	timer.TIM_Prescaler = TIM_PRESCALER;
	timer.TIM_Period = TIM_PERIOD;
	timer.TIM_ClockDivision = 0;
	timer.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIMER, &timer);
 
	TIM_OCStructInit(&timerPWM);
	timerPWM.TIM_OCMode = TIM_OCMode_PWM1;
	timerPWM.TIM_OutputState = TIM_OutputState_Enable;
	timerPWM.TIM_Pulse = 10;
	timerPWM.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OC1Init(TIMER, &timerPWM); 
	
	TIM_Cmd(TIMER, ENABLE);
}

/*Настройка прерываний энкодера*/
void enc_exti_init(void)
{
	// Прерывания - это альтернативная функция порта, включаем
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	
	EXTI_InitTypeDef EXTI_InitStruct;
	NVIC_InitTypeDef NVIC_InitStruct;
	
	// Добавляем вектор прерывания
	NVIC_InitStruct.NVIC_IRQChannel = EXTI0_IRQn;
	// Устанавливаем приоритет
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0x08;     // 0x00 - 0x0F
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0x08;
	// Разрешаем прерывание
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStruct);
 
	// PB0 подключен к EXTI_Line0
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource0);
	EXTI_InitStruct.EXTI_Line = EXTI_Line0;
	// Разрешаем прерывание
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	// По спаду
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
	EXTI_Init(&EXTI_InitStruct);
}

/*Настройка прерываний кнопки*/
void button_exti_init(void)
{
	// Прерывания - это альтернативная функция порта, включаем
	//RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	
	EXTI_InitTypeDef EXTI_InitStruct;
	NVIC_InitTypeDef NVIC_InitStruct;
	
	// Добавляем вектор прерывания
	NVIC_InitStruct.NVIC_IRQChannel = EXTI4_IRQn;
	// Устанавливаем приоритет
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0x09;      // 0x00 - 0x0F
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0x09;
	// Разрешаем прерывание
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStruct);
 
	// PB4 подключен к EXTI_Line1
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource4);

	EXTI_InitStruct.EXTI_Line = BUTTON_EXTI_LINE;
	// Разрешаем прерывание
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	// По спаду
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
	EXTI_Init(&EXTI_InitStruct);
	
}

void SysTick_Handler(void)
{
	++systick_ms;
}

void init_lcd(void)
{
	/* Распиновка дисплея */
	const HD44780_STM32F10x_Pinout lcd_pinout =
	{
	{
		/* RS  4     */  { GPIOA, GPIO_Pin_6 },
		/* ENABLE 6  */  { GPIOA, GPIO_Pin_5 },
		/* RW  5     */  { GPIOA, GPIO_Pin_4 },
		/* Backlight */  { NULL, 0 },
		/* DP0       */  { NULL, 0 },
		/* DP1       */  { NULL, 0 },
		/* DP2       */  { NULL, 0 },
		/* DP3       */  { NULL, 0 },
		/* DP4 11    */  { GPIOA, GPIO_Pin_3 },
		/* DP5 12    */  { GPIOA, GPIO_Pin_2 },
		/* DP6 13    */  { GPIOA, GPIO_Pin_1 },
		/* DP7 14    */  { GPIOA, GPIO_Pin_0 },
	}
	};

	/* Настраиваем драйвер: указываем интерфейс драйвера (стандартный),
	   указанную выше распиновку и обработчик ошибок GPIO (необязателен). */
	lcd_pindriver.interface = HD44780_STM32F10X_PINDRIVER_INTERFACE;
	/* Если вдруг захотите сами вручную настраивать GPIO для дисплея
	   (зачем бы вдруг), напишите здесь ещё (библиотека учтёт это):
	 lcd_pindriver.interface.configure = NULL; */
	lcd_pindriver.pinout = lcd_pinout;
	lcd_pindriver.assert_failure_handler = hd44780_assert_failure_handler;

	/* И, наконец, создаём конфигурацию дисплея: указываем наш драйвер,
	   функцию задержки, обработчик ошибок дисплея (необязателен) и опции.
	   На данный момент доступны две опции - использовать или нет
	   вывод RW дисплея (в последнем случае его нужно прижать к GND),
	   и то же для управления подсветкой. */
	const HD44780_Config lcd_config =
	{
		(HD44780_GPIO_Interface*)&lcd_pindriver,
		delay_microseconds,
		hd44780_assert_failure_handler,
		HD44780_OPT_USE_RW
	};

	/* Ну, а теперь всё стандартно: подаём тактирование на GPIO,
	   инициализируем дисплей: 16x2, 4-битный интерфейс, символы 5x8 точек. */
	//RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	hd44780_init(&lcd, HD44780_MODE_4BIT, &lcd_config, 16, 2, HD44780_CHARSIZE_5x8);
}

void delay_microseconds(uint16_t us)
{
	SysTick->VAL = SysTick->LOAD;
	const uint32_t systick_ms_start = systick_ms;

	while (1)
	{
		uint32_t diff = uint32_time_diff(systick_ms, systick_ms_start);

		if (diff >= ((uint32_t)us / 1000) + (us % 1000 ? 1 : 0))
			break;
	}
}

uint32_t uint32_time_diff(uint32_t now, uint32_t before)
{
	return (now >= before) ? (now - before) : (UINT32_MAX - before + now);
}

void hd44780_assert_failure_handler(const char *filename, unsigned long line)
{
	(void)filename; (void)line;
	do {} while (1);
}

void RefreshPercentageValue(int currentValue, int base)
{
	char valueString[32];
	int perc = ((double)currentValue / base) * 100;
	sprintf(valueString, "%d", perc);
	hd44780_move_cursor(&lcd, 0, 1);
	hd44780_write_string(&lcd, valueString);
	hd44780_write_string(&lcd, "%   ");
}

void RefreshNumericValue(int currentValue)
{
//	hd44780_move_cursor(&lcd, 0, 1);
//	hd44780_write_string(&lcd, "                  ");
	char valueString[32];
	sprintf(valueString, "%d", currentValue);
	hd44780_move_cursor(&lcd, 0, 1);
	hd44780_write_string(&lcd, valueString);
}

void RefreshStringValue(char string[32])
{
	hd44780_move_cursor(&lcd, 0, 1);
	hd44780_write_string(&lcd, string);
}

/*Заголовок*/
void PrintHeader(Mode mode)
{
	//char str = *stringFromMode(mode);
	switch (currentMode)
	{
	case DUTY_CYCLE:
		hd44780_write_string(&lcd, "Duty cycle:");
		break;
	case PERIOD:
		hd44780_write_string(&lcd, "Period:");
		break;
	case POLARITY:
		hd44780_write_string(&lcd, "Polarity:");
		break;
	case PRESCALER:
		hd44780_write_string(&lcd, "Prescaler:");
		break;
	case PULSE:
		hd44780_write_string(&lcd, "Pulse:");
		break;
	default:
		break;
	}
}

static inline char *stringFromMode(Mode f)
{
	static const char *strings[] = { 	
	"Duty cycle:",
	"Period:",
	"Polarity:",
	"Prescaler:",
	"Pulse:",
	};
 
	return strings[f];
}

