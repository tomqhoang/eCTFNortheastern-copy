/* UART configuration headers */
#ifndef UART_H_
#define UART_H_

#include <stdbool.h>

/*
 * Initializes UART1
 * BAUD must be set and setbaud imported before calling this
 */
void UART1_init(void);

void UART1_putchar(unsigned char data);

bool UART1_data_available(void);
unsigned char UART1_getchar(void);

void UART1_flush(void);

void UART1_putstring(char* str);

/*
 * Initializes UART0
 * BAUD must be set and setbaud imported before calling this
 */
void UART0_init(void);

void UART0_putchar(unsigned char data);

bool UART0_data_available(void);
unsigned char UART0_getchar(void);

void UART0_flush(void);

void UART0_putstring(char* str);

#endif