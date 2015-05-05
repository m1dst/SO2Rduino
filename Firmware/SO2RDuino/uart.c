// SO2Rduino - An SO2R Box built on an Arduino clone
//
// Copyright 2010, Paul Young
//
// This file contains the UART routines

#include "Arduino.h"
#include "uart.h"
#include "avr/pgmspace.h"

char                    in_buf[UBLEN];  // UART input buffer
static byte             in_len;         // Number of chars in buffer
static char             out_buf[UBLEN]; // UART output circular buffer
static byte             out_add;        // Place to add a character
static byte             out_remove;     // Place to remove a character

void
uart_send_char(const char c)
//----------------------------------------------------------------------
// Add a character to the UART output buffer
//----------------------------------------------------------------------
{

    // Check for space in the buffer
    if ((out_remove == (out_add + 1)) ||
        ((out_remove == 0) && out_add == (UBLEN-1))) {
            return;
    }
    
    out_buf[out_add++] = c;
    if (out_add == UBLEN) {
        out_add = 0;
    }
}

void
uart_send_string(const char* bp)
//----------------------------------------------------------------------
// Add a character string to the UART output buffer
//----------------------------------------------------------------------
{
    while (*bp) {
        uart_send_char(*bp++);
    }
}

void
uart_send_prog_string(const char PROGMEM * bp)
//----------------------------------------------------------------------
// Add a character string from flash to the UART output buffer
//----------------------------------------------------------------------
{
    byte c;
    while ((c=pgm_read_byte(bp++))) {
        uart_send_char(c);
    }
}

void
uart_init()
//----------------------------------------------------------------------
// Set up the UART
//----------------------------------------------------------------------
{

    in_len = 0;
    out_add = 0;
    out_remove = 0;
   
    // Set the UART to 9600 baud
#define MYUBRR 103

    UBRR0H = (MYUBRR>>8);
    UBRR0L = (unsigned char)MYUBRR;

    // Enable receiver and transmitter
    UCSR0B = _BV(RXEN0)| _BV(TXEN0);

    // Set 8 bit, one stop bit
    UCSR0C = _BV(UCSZ00) | _BV(UCSZ01);
}

void
uart_clear_buffer()
//----------------------------------------------------------------------
// Clear the UART input buffer
//----------------------------------------------------------------------
{
    in_len = 0;
}

boolean
do_uart()
//----------------------------------------------------------------------
// Send and receive characters
//----------------------------------------------------------------------
{
    // Check for characters to send and the UART being ready
    if ((out_add != out_remove) && (UCSR0A & _BV(UDRE0))) {
        UDR0 = out_buf[out_remove++];
        if (out_remove == UBLEN) {
            out_remove = 0;
        }
    }
    
    // Check for UART having a character to input
    if (UCSR0A & _BV(RXC0)) {
        char c = UDR0;
        // Special handling for return, means end of command
        if (c == '\r') {
            in_buf[in_len] = '\0';
            return true;
        }
        if (in_len < (UBLEN-2)) {
            in_buf[in_len++] = c;
        }
    }

    return false;
}
