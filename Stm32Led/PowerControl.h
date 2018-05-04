#pragma once
#include <stm32f10x.h>

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
void timer_init(TIM_TypeDef* TIMx, int period, int prescaler);
void RefreshPercentageValue(int currentValue, int base);
void RefreshNumericValue(int currentValue);
void RefreshStringValue(char string[32]);
static inline char *stringFromMode(Mode f);
void PrintHeader(Mode mode);