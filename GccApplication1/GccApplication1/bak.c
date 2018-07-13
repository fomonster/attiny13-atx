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

#define RESET_OUT_PORT PORTB
#define RESET_OUT_PIN PB1
#define RESET_OUT_DDR DDRB

#define DEBOUNCE_TIME 50        /* time to wait while de-bouncing button */
#define LOCK_INPUT_TIME 100     /* time to wait after a button press */
#define HOLD_THRESHOLD 500	    /* button hold time */

/* Стейт машина */

enum LED_STATE {
	OFF,
	ON,
	FLASHING
} static led1State;

//static uint16_t btn1DownTime;
volatile int flashingCounter = 0;

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

/* Инициализация */

void init_hardware(void) {

	STATUS_LED_DDR |= (1<<STATUS_LED_PIN); 
	POWER_ON_DDR |= (1<<POWER_ON_PIN); 
	RESET_OUT_DDR |= (1<<RESET_OUT_PIN);
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
	led1State = ON;
	
	// TODO: btn1DownTime = TCNT1;
}

void on_btn_up(void) {
	// should it be TCNT1 / 1000 ?
	/*if (TCNT0 > btn1DownTime + HOLD_THRESHOLD) {
		led1State = FLASHING;
	}
	else {
		led1State = OFF;
	}
	
	btn1DownTime = 0;*/

	led1State = OFF;
}

/* Прерывания */

// Прерывание кнопочное
ISR(PCINT0_vect) {
	// Отключаем прерывания на время обработки
	cli();
	
	// Подавление дребезга
	delay_ms(DEBOUNCE_TIME);
	
	// Проверяем нажата кнопка или отжата (при нажатой кнопке низкий уровень сигнала)
	if((BUTTON_IN_PORT & (1<<BUTTON_PIN)) == 0) { 
		on_btn_down();
	} else {
		on_btn_up();		
	}
	
	// Включаем прерывания в конце обработки	
	sei();
}

// Прерывание таймера
ISR (TIM0_OVF_vect) {
	if (led1State == OFF) { led_off(); }
	else if (led1State == ON) { led_on(); }
	else if (led1State == FLASHING) {
		if (++flashingCounter > 36 ) { // 9600000Hz / 1024 = 9375, 9375 / 256 (8 bit overflow) = 36,62109375
			led_toggle();
			flashingCounter = 0;
		}
	}
}

/* Точка входа */
int main(void)
{
	led1State = FLASHING;
	
	init_hardware();
	
	init_buttons();
	
	init_timers();
	
	sei();
	
	while(1) {
		;
	}
	return 0;
}

