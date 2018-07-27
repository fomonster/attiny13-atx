/*
 * Author : fomonster
 * 
 * ���������� ���������� ������ ������� ATX � ������� ������.
 * ��� ����������� ������� ����� ������� �������� ���� �������.
 * ��� ���������� ������� �������� ������� ���������� �����, ������� 3� ��������� ���� �������.
 * ��� ��������� ������������ power ok � ���� ��� ��� � ������� ���������� ������� ���������� ���
 * 
 * ����� ���������������� (TL866 �� ������� ������ ����� ��� �����, �����):
 *   SUT0 = 0
 *   CKSEL0 = 0
 *   RSTDISBL
 *
 *
 */ 

#include <avr/io.h>

// ATtiny13 ������������ � ������ � ���������� �� ������� 9600000 �� ���������� RC-�����������. ��� ���������������� ������� ����� CKDIV8=0 � TL866
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

#define RESET_TIME 50			/* ����� �������� reset */
#define DEBOUNCE_TIME 33        /* ����� ���������� �������� */
#define POWEROK_THRESHOLD 36	 /* ����� ���������� �������� power ok - ����� 1 ������� ��������� �������� � � ��� ����� ��������� ������� */
#define HOLD_THRESHOLD 36 * 3    /* �����(�������) ������� ������� ������ */
#define FLASH_THRESHOLD 4	 /* �����(������) �������� ��� ������� ����������� */
#define FLASH_RESET_THRESHOLD 2	 /* �����(������) �������� ��� ������� ����������� �� reset */
#define RESET_THRESHOLD 20	 /* ����� �������� reset */

/* ����� ������ */

enum LED_STATE {
	OFF,
	ON,
	FLASHING
} static ledState;

enum BUTTON_STATE {
	UP,
	DOWN
} static buttonState;

volatile int powerCounter = 0; // ������� �� POWEROK_THRESHOLD, ����� ������� ��������
volatile int buttonDownCounter = 0; // ������� ��, HOLD_THRESHOLD
volatile int flashingCounter = 0; //
volatile int resetFlashingCounter = 0; // ������� ��� ������� �� reset

/* ������� */

void delay_ms(uint16_t ms) {
	while (ms) {
		_delay_ms(1);
		ms--;
	}
}

/* ���������� ����������� */

void led_on(void) {
	STATUS_LED_PORT |= (1<<STATUS_LED_PIN);
}

void led_off(void) {
	STATUS_LED_PORT &= ~(1<<STATUS_LED_PIN);
}

void led_toggle(void) {
	STATUS_LED_PORT ^= (1<<STATUS_LED_PIN);
}

/* ���������� �������� */

void power_on(void) {
	POWER_ON_PORT |= (1<<POWER_ON_PIN);
}

void power_off(void) {
	POWER_ON_PORT &= ~(1<<POWER_ON_PIN);
}

void power_toggle(void) {
	POWER_ON_PORT ^= (1<<POWER_ON_PIN);
}

/* ���������� ������� ������ */

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

/* ������������� */

void init_hardware(void) {

	STATUS_LED_DDR |= (1<<STATUS_LED_PIN);  // ����� �������	
	POWER_ON_DDR |= (1<<POWER_ON_PIN); // ����� �������
	RESET_DDR |= (1<<RESET_PIN); // ����� �������

	POWER_OK_DDR &= ~(1<<POWER_OK_PIN); // ����� ��������
	POWER_OK_OUT_PORT |= (1<<POWER_OK_PIN); // ���� �������
}

void init_buttons(void) {

	BUTTON_DDR &= ~(1<<BUTTON_PIN); // ����� �� ������ ��������
	BUTTON_OUT_PORT |= (1<<BUTTON_PIN); // ���� �� ������ �������

	GIMSK |= (1<<PCIE); // ��������� ������� ���������� PCINT0
	PCMSK |= (1<<BUTTON_PIN); // ��������� �� ����� ���������� �� ���� ������ (PCINT2)
}

void init_timers(void) { 
	// prescale timer to 1/1024th the clock rate
	TCCR0B |= (1<<CS02) | (1<<CS00); 

	// enable timer overflow interrupt
	TIMSK0 |=1<<TOIE0;
}

/* ������� ������ */

void on_btn_down(void) {
	buttonState = DOWN;
	buttonDownCounter = 0;
}

void on_btn_up(void) {
	if ( buttonState != DOWN ) return;
	
	if ( ledState == OFF ) {
		ledState = FLASHING;
		power_on();
		powerCounter = 0;
		flashingCounter = 0;
		resetFlashingCounter = 0;
	} else {

		// ������������� �������� reset
		if ( powerCounter >= POWEROK_THRESHOLD && resetFlashingCounter <= 0) {
			
			reset_on();
			resetFlashingCounter = RESET_THRESHOLD;
			flashingCounter = 0;
		}
		
	}
	buttonState = UP;
}

/* ���������� */

// ���������� ���������
ISR(PCINT0_vect) {
	// ��������� ���������� �� ����� ���������
	cli();	
	// ���������� ��������
	delay_ms(DEBOUNCE_TIME);
	cli();

	// ��������� ������ ������ ��� ������ (��� ������� ������ ������ ������� �������)
	if((BUTTON_IN_PORT & (1<<BUTTON_PIN)) == 0) { 
		on_btn_down();
	} else {
		on_btn_up();
	}
	
	// �������� ���������� � ����� ���������	
	sei();
}

void update(void)
{
	// ����� ������ �������
	if ( buttonState == DOWN ) {
		buttonDownCounter++;
		if ( buttonDownCounter > HOLD_THRESHOLD ) {
			ledState = OFF;
			buttonState = UP; // �������� ������
			power_off();
		}
		} else {
		buttonDownCounter = 0;
	}
	int flashingCounterThreshold = FLASH_THRESHOLD;
	// ���������� �������� � ����� power_ok
	if ( ledState != OFF ) {
		powerCounter++;
		if ( powerCounter > POWEROK_THRESHOLD ) {
			powerCounter = POWEROK_THRESHOLD;
		}
	}

	// �������� power ok
	if ( powerCounter >= POWEROK_THRESHOLD ) {
		if((POWER_OK_IN_PORT & (1<<POWER_OK_PIN)) == 0) {
			ledState = OFF;
			power_off();
			} else {
			ledState = ON;
		}
	}

	// ������� ��� reset ��������
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

	// ���������� �����������
	if (ledState == OFF) { led_off(); }
	else if (ledState == ON) { led_on(); }
	else if (ledState == FLASHING) {
		flashingCounter++;
		if (flashingCounter > flashingCounterThreshold ) { // 9600000Hz / 1024 = 9375, 9375 / 256 (8 bit overflow) = 36,62109375 = 1 �������
			led_toggle();
			flashingCounter = 0;
		}
	}

}

// ���������� �������
ISR (TIM0_OVF_vect) {
	cli();
	//update();
	sei();
}

/* ����� ����� */
int main(void)
{
	ledState = OFF;
	buttonState = UP;	
	reset_off();	
	power_off();

	init_hardware();
	
	init_buttons();
	
	//init_timers();
	
	sei();
	
	while(1) {
		update();
		_delay_ms(27);
	}
	return 0;
}

