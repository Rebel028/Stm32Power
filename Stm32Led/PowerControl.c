#include <stm32f10x.h>
#include <stm32f10x_gpio.h>
#include <stm32f10x_rcc.h>
#include "stm32f10x_tim.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* ���������� ���������� � ������� GPIO */
#include "hd44780.h"
#include "hd44780_stm32f10x.h"
#include "stm32f10x_exti.h"
#include <misc.h>
#include <PowerControl.h>

/*��������� ��������*/
//void encoder_init(void)
//{	
//	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
//	
//	/* ����������� TIM3 */
//	TIM_TimeBaseInitTypeDef TIMER_InitStructure;
//	TIM_TimeBaseStructInit(&TIMER_InitStructure);
//	// ������� TIM_Period - �� ������� �������� ������� ��� ������� ��������
//	TIMER_InitStructure.TIM_Period = TIM_PERIOD;
//	// ���������� �������� � ������ ����
//	TIMER_InitStructure.TIM_CounterMode = TIM_CounterMode_Up | TIM_CounterMode_Down;
//	TIM_TimeBaseInit(TIM3, &TIMER_InitStructure);
// 
//	/* ����������� Encoder Interface */
//	TIM_EncoderInterfaceConfig(TIM3, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);
// 
//	TIM_Cmd(TIM3, ENABLE);
//}

void timer_init(TIM_TypeDef* TIMx, int period, int prescaler)
{
	TIM_TimeBaseInitTypeDef timer;
	TIM_OCInitTypeDef timerPWM;
	
	TIM_TimeBaseStructInit(&timer);
	timer.TIM_Prescaler = prescaler;
	timer.TIM_Period = period;
	timer.TIM_ClockDivision = 0;
	timer.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIMx, &timer);
 
	TIM_OCStructInit(&timerPWM);
	timerPWM.TIM_OCMode = TIM_OCMode_PWM1;
	timerPWM.TIM_OutputState = TIM_OutputState_Enable;
	timerPWM.TIM_Pulse = 10;
	timerPWM.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OC1Init(TIMx, &timerPWM); 
	
	TIM_Cmd(TIMx, ENABLE);
}

/*��������� ���������� ��������*/
void enc_exti_init(void)
{
	// ���������� - ��� �������������� ������� �����, ��������
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	
	EXTI_InitTypeDef EXTI_InitStruct;
	NVIC_InitTypeDef NVIC_InitStruct;
	
	// ��������� ������ ����������
	NVIC_InitStruct.NVIC_IRQChannel = EXTI0_IRQn;
	// ������������� ���������
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0x08;      // 0x00 - 0x0F
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0x08;
	// ��������� ����������
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStruct);
 
	// PB0 ��������� � EXTI_Line0
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource0);
	EXTI_InitStruct.EXTI_Line = EXTI_Line0;
	// ��������� ����������
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	// �� �����
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
	EXTI_Init(&EXTI_InitStruct);
}

/*��������� ���������� ������*/
void button_exti_init(void)
{
	// ���������� - ��� �������������� ������� �����, ��������
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	
	EXTI_InitTypeDef EXTI_InitStruct;
	NVIC_InitTypeDef NVIC_InitStruct;
	
	// ��������� ������ ����������
	NVIC_InitStruct.NVIC_IRQChannel = EXTI1_IRQn;
	// ������������� ���������
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0x08;       // 0x00 - 0x0F
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0x08;
	// ��������� ����������
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStruct);
 
	// PB3 ��������� � EXTI_Line0
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource3);
	EXTI_InitStruct.EXTI_Line = EXTI_Line1;
	// ��������� ����������
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	// �� �����
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
	EXTI_Init(&EXTI_InitStruct);
}

void init_lcd(void)
{
	/* ���������� ������� */
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

	/* ����������� �������: ��������� ��������� �������� (�����������),
	   ��������� ���� ���������� � ���������� ������ GPIO (������������). */
	lcd_pindriver.interface = HD44780_STM32F10X_PINDRIVER_INTERFACE;
	/* ���� ����� �������� ���� ������� ����������� GPIO ��� �������
	   (����� �� �����), �������� ����� ��� (���������� ���� ���):
	 lcd_pindriver.interface.configure = NULL; */
	lcd_pindriver.pinout = lcd_pinout;
	lcd_pindriver.assert_failure_handler = hd44780_assert_failure_handler;

	/* �, �������, ������ ������������ �������: ��������� ��� �������,
	   ������� ��������, ���������� ������ ������� (������������) � �����.
	   �� ������ ������ �������� ��� ����� - ������������ ��� ���
	   ����� RW ������� (� ��������� ������ ��� ����� ������� � GND),
	   � �� �� ��� ���������� ����������. */
	const HD44780_Config lcd_config =
	{
		(HD44780_GPIO_Interface*)&lcd_pindriver,
		delay_microseconds,
		hd44780_assert_failure_handler,
		HD44780_OPT_USE_RW
	};

	/* ��, � ������ �� ����������: ����� ������������ �� GPIO,
	   �������������� �������: 16x2, 4-������ ���������, ������� 5x8 �����. */
	//RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	hd44780_init(&lcd, HD44780_MODE_4BIT, &lcd_config, 16, 2, HD44780_CHARSIZE_5x8);
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

/*���������*/
void PrintHeader(Mode mode)
{
	char str = *stringFromMode(mode);
	hd44780_write_string(&lcd, &str);
}

static inline char *stringFromMode(Mode f)
{
	static const char *strings[] = { 	
		"Duty cycle",
		"Period",
		"Polarity",
		"Prescaler",
		"Pulse",
	};

	return strings[f];
}