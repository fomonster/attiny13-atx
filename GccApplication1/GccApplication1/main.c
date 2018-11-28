/*
 * Author : fomonster
 * 
 * Реализация управления блоком питания ATX с помощью кнопки.
 * При выключенном питании любое нажатие включает блок питания.
 * При включенном питании короткое нажатие генерирует сброс, длинное 3с выключает блок питания.
 * При включении опрашивается power ok и если его нет в течении некоторого времени контроллер вык
 * 
 * Флаги программирования (TL866 на вкладке конфиг снять все галки, кроме):
 *   SUT0 = 0
 *   CKSEL0 = 0
 *   RSTDISBL
 *
 *
 */ 

#include <avr/io.h>

// ATtiny13 поставляется с завода с включенным на частоте 9600000 Гц внутренним RC-генератором. При программировании снимаем галку CKDIV8=0 в TL866
#define F_CPU 9600000UL 

#include <stdlib.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <time.h>

#define BUTTON_DDR DDRB
#define BUTTON_OUT_PORT PORTB
#define BUTTON_IN_PORT PINB
#define BUTTON_PIN PB2

#define POWER_OK_DDR DDRB
#define POWER_OK_OUT_PORT PORTB
#define POWER_OK_IN_PORT PINB
#define POWER_OK_PIN PB5

#define STATUS_LED_PORT PORTB
#define STATUS_LED_PIN PB4
#define STATUS_LED_DDR DDRB

#define POWER_ON_PORT PORTB
#define POWER_ON_PIN PB3
#define POWER_ON_DDR DDRB

#define RESET_PORT PORTB
#define RESET_PIN PB1
#define RESET_DDR DDRB

#define RESET_TIME 50			/* время импульса reset */
#define DEBOUNCE_TIME 60        /* время устранения дребезга */
#define POWEROK_THRESHOLD 36	 /* время отложенной проверки power ok - через 1 секунду проверяем примерно и в это время светодиод мерцает */
#define HOLD_THRESHOLD 36 * 3    /* время(счетчик) долгого нажатия кнопки */
#define FLASH_THRESHOLD 4	 /* время(счечик) задержка при мигании светодиодом */
#define FLASH_RESET_THRESHOLD 2	 /* время(счечик) задержка при мигании светодиодом по reset */
#define RESET_THRESHOLD 20	 /* время мерцания reset */

/* Стейт машина */

enum LED_STATE {
	OFF,
	ON,
	FLASHING
} static ledState;

enum BUTTON_STATE {
	UP,
	DOWN
} static buttonState, prevButtonState;

volatile int powerCounter = 0; // счетчик до POWEROK_THRESHOLD, когда питание включено
volatile int buttonDownCounter = 0; // счетчик до, HOLD_THRESHOLD
volatile int flashingCounter = 0; //
volatile int resetFlashingCounter = 0; // счетчик для мигания по reset


volatile int waitUnpress = 0;

/* Утилиты */

void delay_ms(uint16_t ms) {
	while (ms) {
		_delay_ms(1);
		ms--;
	}
}

/* Управление светодиодом */

void led_on(void) {
	STATUS_LED_PORT |= (1<<STATUS_LED_PIN);
}

void led_off(void) {
	STATUS_LED_PORT &= ~(1<<STATUS_LED_PIN);
}

void led_toggle(void) {
	STATUS_LED_PORT ^= (1<<STATUS_LED_PIN);
}

/* Управление питанием */

void power_on(void) {
	POWER_ON_PORT |= (1<<POWER_ON_PIN);
}

void power_off(void) {
	POWER_ON_PORT &= ~(1<<POWER_ON_PIN);
}

void power_toggle(void) {
	POWER_ON_PORT ^= (1<<POWER_ON_PIN);
}

/* Управление выходом сброса */

void reset_on(void) {
	RESET_PORT &= ~(1<<RESET_PIN);	
}

void reset_off(void) {
	RESET_PORT |= (1<<RESET_PIN);
}

void reset_impulse(void) {
	reset_on();
	_delay_ms(RESET_TIME);
	reset_off();
}

/* Инициализация */

void init_hardware(void) {

	STATUS_LED_DDR |= (1<<STATUS_LED_PIN);  // выход включен	
	POWER_ON_DDR |= (1<<POWER_ON_PIN); // выход включен
	RESET_DDR |= (1<<RESET_PIN); // выход включен

	POWER_OK_DDR &= ~(1<<POWER_OK_PIN); // выход отключен
	POWER_OK_OUT_PORT |= (1<<POWER_OK_PIN); // вход включен
}

void init_buttons(void) {

	BUTTON_DDR &= ~(1<<BUTTON_PIN); // выход на кнопке отключен
	BUTTON_OUT_PORT |= (1<<BUTTON_PIN); // вход на кнопке включен

	GIMSK |= (1<<PCIE); // Разрешаем внешние прерывания PCINT0
	PCMSK |= (1<<BUTTON_PIN); // Разрешаем по маске прерывания на ноге кнопки (PCINT2)
}

void init_timers(void) { 
	// prescale timer to 1/1024th the clock rate
	TCCR0B |= (1<<CS02) | (1<<CS00); 

	// enable timer overflow interrupt
	TIMSK0 |=1<<TOIE0;
}

/* события кнопки */
void on_btn_down(void) {
	buttonState = DOWN;
	/*
	buttonDownCounter = 0;

if ( ledState == OFF ) {
	ledState = FLASHING;
	power_on();
	powerCounter = 0;
	flashingCounter = 0;
	resetFlashingCounter = 0;
}*/
}

void on_btn_up(void) {
	if ( buttonState != DOWN ) return;
	
	/*if ( ledState == ON ) { 

		// Генерирование импульса reset
		if ( powerCounter >= POWEROK_THRESHOLD && resetFlashingCounter <= 0) {
			
			reset_on();
			resetFlashingCounter = RESET_THRESHOLD;
			flashingCounter = 0;
		}
		
	}*/
	buttonState = UP;
}

/* Прерывания */
int is_button_down(void)
{
	return (BUTTON_IN_PORT & (1<<BUTTON_PIN)) == 0;
}

// Прерывание кнопочное
ISR(PCINT0_vect) {
	// Отключаем прерывания на время обработки
	cli();	
	// Подавление дребезга
	delay_ms(DEBOUNCE_TIME);
	cli();

	// Проверяем нажата кнопка или отжата (при нажатой кнопке низкий уровень сигнала)
	if(is_button_down()) { 
		on_btn_down();
	} else {
		on_btn_up();
	}
	
	// Включаем прерывания в конце обработки	
	sei();
}

void update(void)
{
	int flashingCounterThreshold = FLASH_THRESHOLD;

	// Управление питанием и ловля power_ok
	if ( ledState != OFF ) {
		powerCounter++;
		if ( powerCounter > POWEROK_THRESHOLD ) {
			powerCounter = POWEROK_THRESHOLD;
		}
	}

	// Проверка power ok
	if ( powerCounter >= POWEROK_THRESHOLD ) {
		if((POWER_OK_IN_PORT & (1<<POWER_OK_PIN)) == 0) {
			ledState = OFF;
			power_off();
		} else {
			ledState = ON;
		}
	}

	// мигание при reset импульсе
	if ( resetFlashingCounter > 0 && ledState != OFF) {
		flashingCounterThreshold = FLASH_RESET_THRESHOLD;
		resetFlashingCounter--;
		if ( resetFlashingCounter < RESET_THRESHOLD - 4 ) {
			reset_off();
		}
		ledState = FLASHING;
		if ( resetFlashingCounter <= 0 ) {
			resetFlashingCounter = 0;
			ledState = ON;
		}
		} else {
		resetFlashingCounter = 0;
		reset_off();
	}

	// Управление светодиодом
	if (ledState == OFF) { led_off(); }
	else if (ledState == ON) { led_on(); }
	else if (ledState == FLASHING) {
		flashingCounter++;
		if (flashingCounter > flashingCounterThreshold ) { // 9600000Hz / 1024 = 9375, 9375 / 256 (8 bit overflow) = 36,62109375 = 1 секунда
			led_toggle();
			flashingCounter = 0;
		}
	}

	//
	if ( buttonState == DOWN ) { // кнопка нажата
		buttonDownCounter++;

		
		if ( buttonDownCounter > HOLD_THRESHOLD && ledState != OFF ) { // ловим долгое нажатие и выключаем питание
			ledState = OFF;
			power_off();
		} else if ( ledState == OFF && prevButtonState != buttonState ) { // ловим простое нажатие и включаем питание
			ledState = FLASHING;
			power_on();
			powerCounter = 0;
			flashingCounter = 0;
			resetFlashingCounter = 0;
		}


	} else {
		buttonDownCounter = 0; 
		if ( prevButtonState != buttonState && ledState != OFF ) { // кнопка отжата только-что
			
			if ( powerCounter >= POWEROK_THRESHOLD && resetFlashingCounter <= 0) {
				
				reset_on();
				resetFlashingCounter = RESET_THRESHOLD;
				flashingCounter = 0;
			}

		}
	}
	prevButtonState = buttonState;

}

// Прерывание таймера
ISR (TIM0_OVF_vect) {
	cli();
	//update();
	sei();
}

/* Точка входа */
int main(void)
{
	prevButtonState = UP;

	powerCounter = 0; // счетчик до POWEROK_THRESHOLD, когда питание включено
	buttonDownCounter = 0; // счетчик до, HOLD_THRESHOLD
	flashingCounter = 0; //
	resetFlashingCounter = 0; // счетчик для мигания по reset

	init_hardware();

	// Если блок питания уже запитан, то устанавливаем соответствующий стейт
	if((POWER_OK_IN_PORT & (1<<POWER_OK_PIN)) == 0) {
		ledState = OFF;
		power_off();
	} else {
		ledState = ON;
		power_on();
	}

	init_buttons();
	buttonState = UP;
	reset_off();
	
	sei();
	while(1) {
		update();
		_delay_ms(27);
	}
	return 0;
}

