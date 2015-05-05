// SO2Rduino - An SO2R Box built on an Arduino clone
//
// Copyright 2010, Paul Young
//
// This file contains the UART prototypes
//
#ifndef UART_H

// The uart file is C, not C++ so this is needed to cause the
// compiler to generate the correct routine names.
#ifdef __cplusplus
extern "C"{
#endif

// The input buffer is used in command parsing.
#define UBLEN 64                        // UART Buffer length
extern char in_buf[UBLEN];  // UART input buffer

extern void uart_send_char(const char c);
extern void uart_send_string(const char* bp);
extern void uart_send_prog_string(const char PROGMEM * bp);
extern void uart_init(void);
extern void uart_clear_buffer(void);
extern boolean do_uart(void);

#ifdef __cplusplus
}
#endif

#endif
