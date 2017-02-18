/*
 * bootloader.c
 *
 * If Port B Pin 2 (PB2 on the protostack board) is pulled to ground the 
 * bootloader will wait for data to appear on UART1 (which will be interpretted
 * as an updated firmware package).
 * 
 * If the PB2 pin is NOT pulled to ground, but 
 * Port B Pin 3 (PB3 on the protostack board) is pulled to ground, then the 
 * bootloader will enter flash memory readback mode. 
 * 
 * If NEITHER of these pins are pulled to ground, then the bootloader will 
 * execute the application from flash.
 *
 * If data is sent on UART for an update, the bootloader will expect that data 
 * to be sent in frames. A frame consists of two sections:
 * 1. Two bytes for the length of the data section
 * 2. A data section of length defined in the length section
 *
 * [ 0x02 ]  [ variable ]
 * ----------------------
 * |  Length |  Data... |
 *
 * Frames are stored in an intermediate buffer until a complete page has been
 * sent, at which point the page is written to flash. See program_flash() for
 * information on the process of programming the flash memory. Note that if no
 * frame is received after 2 seconds, the bootloader will time out and reset.
 *
 */

#include <avr/io.h>
#include <stdint.h>
#include <stdio.h>
#include <util/delay.h>
#include "uart.h"
#include <avr/boot.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h> 
#include "aes.h"

#define OK    ((unsigned char)0x00)
#define ERROR ((unsigned char)0x01)
#define F_CPU 1000000UL

void program_flash(uint32_t page_address, unsigned char *data);
void load_firmware(void);
void boot_firmware(void);
void readback(void);
static void test(void);
static void UART_send_char(char c);
static void UART_send_str(char *str);
static char hexa_to_ascii(uint8_t input);
static void print_hex(uint8_t *ptr, uint8_t size);

uint16_t fw_size EEMEM = 0;
uint16_t fw_version EEMEM = 0;
#define LED PB5 

int main(void)
{
    // Init UART1 (virtual com port)
    UART1_init();

    UART0_init();
    wdt_reset();

    // Configure Port B Pins 2 and 3 as inputs.
    DDRB &= ~((1 << PB2) | (1 << PB3) | (0 << PB5));

    // Enable pullups - give port time to settle.
    PORTB |= (1 << PB2) | (1 << PB3) | (0 << PB5);

    // If jumper is present on pin 2, load new firmware.
    if(!(PINB & (1 << PB2)))
    {
        UART1_putchar('U');
        load_firmware();
    }
    else if(!(PINB & (1 << PB3)))
    {
        UART1_putchar('R');
        readback();
    }
    else
    {
        UART1_putchar('C');
        //boot_firmware();
        while(1)
        {
            test();
        }
    }
    //UART1_putchar('U');
    //load_firmware();
    //_delay_ms(1000);
    
} // main

/*
 * Interface with host readback tool.
 */
void readback(void)
{
    UART0_putchar('S');
    // Start the Watchdog Timer
    wdt_enable(WDTO_2S);

    // Read in start address (4 bytes).
    uint32_t start_addr = 0;
    //uint32_t start_addr = ((uint32_t)UART1_getchar()) << 24;
    //start_addr |= ((uint32_t)UART1_getchar()) << 16;
    //start_addr |= ((uint32_t)UART1_getchar()) << 8;
    //start_addr |= ((uint32_t)UART1_getchar());

    wdt_reset();

    // Read in size (4 bytes).
    uint32_t size = 2000;
    //uint32_t size = ((uint32_t)UART1_getchar()) << 24;
    //size |= ((uint32_t)UART1_getchar()) << 16;
    //size |= ((uint32_t)UART1_getchar()) << 8;
    //size |= ((uint32_t)UART1_getchar());

    wdt_reset();
    UART0_putchar('P');

    // Read the memory out to UART1.
    for(uint32_t addr = start_addr; addr < start_addr + size; ++addr)
    {
        UART0_putchar('D');
        // Read a byte from flash.
        unsigned char byte = pgm_read_byte_far(addr);
        wdt_reset();

        // Write the byte to UART1.
        UART1_putchar(byte);
        wdt_reset();
    }
    UART0_putchar('B');

    while(1) __asm__ __volatile__(""); // Wait for watchdog timer to reset.
}


/*
 * Load the firmware into flash.
 */
void load_firmware(void)
{
    int frame_length = 0;
    unsigned char rcv = 0;
    unsigned char data[SPM_PAGESIZE]; // SPM_PAGESIZE is the size of a page.
    unsigned int data_index = 0;
    unsigned int page = 0;
    uint16_t version = 0;
    uint16_t size = 0;

    // Start the Watchdog Timer
    wdt_enable(WDTO_2S);

    /* Wait for data */
    while(!UART1_data_available())
    {
        __asm__ __volatile__("");
    }

    // Get version.
    rcv = UART1_getchar();
    version = (uint16_t)rcv << 8;
    rcv = UART1_getchar();
    version |= (uint16_t)rcv;
    if (version == 5)
    {
        UART0_putchar('o');
    }
    else if (version < 5)
    {
        UART0_putchar('w');
    }
    else if (version > 5)
    {
        UART0_putchar('e');
    }
    // Get size.
    rcv = UART1_getchar();
    size = (uint16_t)rcv << 8;
    rcv = UART1_getchar();
    size |= (uint16_t)rcv;

    // Compare to old version and abort if older (note special case for version
    // 0).
    if (version != 0 && version < eeprom_read_word(&fw_version))
    {
        UART1_putchar(ERROR); // Reject the metadata.
        // Wait for watchdog timer to reset.
        while(1)
        {
            __asm__ __volatile__("");
        }
    }
    else if(version != 0)
    {
        // Update version number in EEPROM.
        wdt_reset();
        eeprom_update_word(&fw_version, version);
    }

    // Write new firmware size to EEPROM.
    wdt_reset();
    eeprom_update_word(&fw_size, size);
    wdt_reset();

    UART1_putchar(OK); // Acknowledge the metadata.

    /* Loop here until you can get all your characters and stuff */
    while (1)
    {
        wdt_reset();

        // Get two bytes for the length.
        rcv = UART1_getchar();
        frame_length = (int)rcv << 8;
        rcv = UART1_getchar();
        frame_length += (int)rcv;

        UART0_putchar((unsigned char)rcv);
        wdt_reset();

        // Get the number of bytes specified
        for(int i = 0; i < frame_length; ++i){
            wdt_reset();
            data[data_index] = UART1_getchar();
            data_index += 1;
        } //for

        // If we filed our page buffer, program it
        if(data_index == SPM_PAGESIZE || frame_length == 0)
        {
            wdt_reset();
            program_flash(page, data);
            page += SPM_PAGESIZE;
            data_index = 0;
#if 1
            // Write debugging messages to UART0.
            //UART0_putchar('P');
            //UART0_putchar(page>>8);
            //UART0_putchar(page);
#endif
            wdt_reset();

        } // if

        UART1_putchar(OK); // Acknowledge the frame.
    } // while(1)
}


/*
 * Ensure the firmware is loaded correctly and boot it up.
 */
void boot_firmware(void)
{
    // Start the Watchdog Timer.
    wdt_enable(WDTO_2S);

    // Write out the release message.
    uint8_t cur_byte;
    uint32_t addr = (uint32_t)eeprom_read_word(&fw_size);

    // Reset if firmware size is 0 (indicates no firmware is loaded).
    if(addr == 0)
    {
        // Wait for watchdog timer to reset.
        while(1) __asm__ __volatile__("");
    }

    wdt_reset();

    // Write out release message to UART0.
    do
    {
        cur_byte = pgm_read_byte_far(addr);
        UART0_putchar(cur_byte);
        ++addr;
    } while (cur_byte != 0);

    // Stop the Watchdog Timer.
    wdt_reset();
    wdt_disable();

    /* Make the leap of faith. */
    asm ("jmp 0000");
}


/*
 * To program flash, you need to access and program it in pages
 * On the atmega1284p, each page is 128 words, or 256 bytes
 *
 * Programing involves four things,
 * 1. Erasing the page
 * 2. Filling a page buffer
 * 3. Writing a page
 * 4. When you are done programming all of your pages, enable the flash
 *
 * You must fill the buffer one word at a time
 */
void program_flash(uint32_t page_address, unsigned char *data)
{
    int i = 0;

    boot_page_erase_safe(page_address);

    for(i = 0; i < SPM_PAGESIZE; i += 2)
    {
        uint16_t w = data[i];    // Make a word out of two bytes
        w += data[i+1] << 8;
        boot_page_fill_safe(page_address+i, w);
    }

    boot_page_write_safe(page_address);
    boot_rww_enable_safe(); // We can just enable it after every program too
}

static void test(void)
{
    // aes256 is 32 byte key
    uint8_t key[32] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21, 22,23,24,25,26,27,28,29,30,31};

    // aes256 is 16 byte data size
    uint8_t data[16] = "text to encrypt";

    // the context where the round keys are stored
    aes256_ctx_t ctx;

    UART_send_str("\n** AES256-TEST **\n");

    // Initialize the AES256 with the desired key
    aes256_init(key, &ctx);
    UART_send_str("Key: '");
    print_hex(key, 32);
    UART_send_str("'\n");

    // Encrypt text and show the result in hexa
    aes256_enc(data, &ctx);
    UART_send_str("Encrypted: '");
    print_hex(data, 16);
    UART_send_str("'\n");

    // Decrypt data and show the result in ascii
    aes256_dec(data, &ctx);
    UART_send_str("Decrypted: '");
    UART_send_str(data);
    UART_send_str("'\n");

}

/*!	\fn 	static void UART_send_char(char c)
*	\brief	Send a char through UART0
*/
static void UART_send_char(char c)
{
    // wait until the last char has been sent
    //while(!(UCSR0A & (1<<UDRE0)));

    // load the new char to transmit
    //UDR0 = c;
    UART1_putchar(c);
}

/*!	\fn 	static void UART_send_str(char *str);
*	\brief	Send a string through UART0
*/
static void UART_send_str(char *data)
{
    while(*data != 0)
    {
        UART_send_char(*data++);
    }
}

/*!	\fn 	static char hexa_to_ascii(uint8_t input)
*	\brief	Convert the lower nibble of hexa number to his ascii 
*           representation. Usefull to print hexadecimal
*           values trought UART.\n
*           For example: hexa_to_ascii(0x0A) would return ascii 'A'.
* 
*   \param  uint8_t input - The value to be converted
*   \return char - The ascii representation of the lower nibble of the input
*/
static char hexa_to_ascii(uint8_t input)
{

    input &= 0x0F;

    if (input < 10)
    {
        input = input + '0';
    }
    else
    {
        input = input + 'A' - 10;
    }

    return (char)input;
}

/*!	\fn 	static void print_hex(uint8_t *ptr, uint8_t size)
*	\brief	Print hexadecimal representation of an array of uint8_t.
* 
*   \param  uint8_t *ptr - The pointer to the array of uint8_t
*   \param  uint8_t size - The size of the array
*/
static void print_hex(uint8_t *ptr, uint8_t size)
{
    uint8_t i;

    for(i=0; i<size; i++)     {         UART_send_char(hexa_to_ascii(ptr[i]>>4));
        UART_send_char(hexa_to_ascii(ptr[i]&0x0F));
    }
}

