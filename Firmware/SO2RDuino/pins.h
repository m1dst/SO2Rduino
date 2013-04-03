// SO2Rduino - An SO2R Box built on an Arduino clone
//
// Copyright 2010, Paul Young
//
// This file contains all of the pin definitions and
// macros to access them.

#ifndef PINS_H

#define PB0 0
#define PB1 1
#define PB2 2
#define PB3 3
#define PB4 4
#define PB5 5

#define PC0 0
#define PC1 1
#define PC2 2
#define PC3 3
#define PC4 4
#define PC5 5

#define PD0 0
#define PD1 1
#define PD2 2
#define PD3 3
#define PD4 4
#define PD5 5
#define PD6 6
#define PD7 7

// Item		Pin	Microprocessor Pin
// ------------------------------------------
// 
// RX 1/2		D13	PB5

#define SET_RX2              PORTB |= _BV(PB5)
#define CLEAR_RX2            PORTB &= ~_BV(PB5)
// RX Stereo	D11	PB3

#define SET_STEREO           PORTB |= _BV(PB3)
#define CLEAR_STEREO         PORTB &= ~_BV(PB3)

// TX 1/2		D9	PB1

#define SET_TX2               PORTB |= _BV(PB1)
#define CLEAR_TX2             PORTB &= ~_BV(PB1)
 
// TX1 LED		D8	PB0

#define SET_TX1               PORTB |= _BV(PB0)
#define CLEAR_TX1             PORTB &= ~_BV(PB0)

// RX1 LED		A4	PC4

#define SET_RX1_LED           PORTC |= _BV(PC4)
#define CLEAR_RX1_LED         PORTC &= ~_BV(PC4)

// RX2 LED		A5	PC5

#define SET_RX2_LED           PORTC |= _BV(PC5)
#define CLEAR_RX2_LED         PORTC &= ~_BV(PC5)

// PTT switch	D12	PB4

#define GET_PTT_SWITCH        (PINB & _BV(PB4))

// PTT DTR		D2	PD2

#define GET_PTT_DTR           (PIND & _BV(PD2))

// CW Key		D3	PD3

#define GET_CW_KEY            (PIND & _BV(PD3))

// PTT TX 1	D4	PD4

#define SET_PTT1              PORTD |= _BV(PD4)

// PTT TX 2	D5	PD5

#define SET_PTT2              PORTD |= _BV(PD5)

#define CLEAR_PTT             PORTD &= ~(_BV(PD5) | _BV(PD4))

// CW TX1		D6	PD6

#define SET_CW1               PORTD |= _BV(PD6)

// CW TX2		D7	PD7

#define SET_CW2               PORTD |= _BV(PD7)

#define CLEAR_CW              PORTD &= ~(_BV(PD6) | _BV(PD7))

// AUX CLK		D13	PB5
 
#define SET_AUX_CLK           PORTB |= _BV(PB5)

#define CLEAR_AUX_CLK         PORTB &= ~(_BV(PB5))

// AUX DATA	D11	PB3

#define SET_AUX_DATA          PORTB |= _BV(PB3)

#define CLEAR_AUX_DATA        PORTB &= ~(_BV(PB3))

// AUX STROBE	D10	PB2

#define SET_AUX_STROBE        PORTB |= _BV(PB2)

#define CLEAR_AUX_STROBE      PORTB &= ~(_BV(PB2))

// TX 1 Switch	A0	PC0

#define GET_TX1_SWITCH        (PINC & _BV(PC0))

// TX 2 Switch	A1	PC1

#define GET_TX2_SWITCH        (PINC & _BV(PC1))
 
// RX 1 Switch	A2	PC2

#define GET_RX1_SWITCH        (PINC & _BV(PC2))
 
// RX 2 Switch	A3	PC3

#define GET_RX2_SWITCH        (PINC & _BV(PC3))
 
// The per-port direction and pull-ups

#define PORT_B_DDR  _BV(PB5) | _BV(PB3) | _BV(PB2) | _BV(PB1) | _BV(PB0) 
#define PORT_B_PULL 0

#define PORT_C_DDR  _BV(PC4) | _BV(PC5)
#define PORT_C_PULL _BV(PC3) | _BV(PC2) | _BV(PC1) | _BV(PC0)

#define PORT_D_DDR  _BV(PD1) | _BV(PD4) | _BV(PD5) | _BV(PD6) | _BV(PD7)
#define PORT_D_PULL _BV(PD4) | _BV(PD5) | _BV(PD6) | _BV(PD7)

#endif // PINS_H

