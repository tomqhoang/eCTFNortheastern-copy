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
#include <string.h>
#include <util/delay.h>
#include "uart.h"
#include <avr/boot.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <cipher.h>
#include <sha256.h>

#define OK ((unsigned char) 0x00)
#define ERROR ((unsigned char) 0x01)

void test_encryption(void);
void program_flash(uint32_t page_address, unsigned char *data);
void load_firmware(void);
void boot_firmware(void);
void readback(void);
int store_password(void);
int cmp(uint8_t *, uint8_t *, int);
void test_receive_frame(void);

uint16_t fw_size EEMEM = 0;
uint16_t fw_version EEMEM = 0;

int main(void) {
    uint8_t password_stored = 0;

    UART1_init();  // Init UART1 (virtual com port)
    UART0_init();  // Init UART0
    wdt_reset();

    UART1_putchar('A');

    DDRB &= ~((1 << PB2) | (1 << PB3) | (1 << PB4));  // Configure Port B Pins 2 and 3 as inputs
    PORTB |= (1 << PB2) | (1 << PB3) | (1 << PB4);  // Enable pullups - give port time to settle

    // If jumper is present on pin 2, load new firmware
    if (!(PINB & (1 << PB2))) {
        UART1_putchar('U');
        load_firmware();
    }
    else if (!(PINB & (1 << PB3))) {
        UART1_putchar('R');
        readback();
    }
    else if (!(PINB & (1 << PB4)) && password_stored == 0)
    {
	UART1_putchar('K');
	password_stored = store_password();
    }
    else {
        UART1_putchar('X');
	test_receive_frame();
        boot_firmware();
    }
}

void test_receive_frame(void)
{
    uint8_t receive[8] = {0};

    wdt_enable(WDTO_2S);  // Start the Watchdog Timer

    while(!UART1_data_available()) {  // Wait for data
        __asm__ __volatile__("");
    }

    for (int i = 0; i < 8; i++)
    {
	wdt_reset();
	receive[i] = UART1_getchar();
    }

    for (int i = 0; i < 8; i++)
    {
	wdt_reset();
	UART0_putchar(receive[i]);
    }

    while (1) __asm__ __volatile__("");
}

void test_encryption(void)
{
	// SIMON
    uint8_t data[8] = {0};
    for(int ii = 0; ii < 8; ii++) {
		data[ii] = 3*ii;
    }
    uint8_t round_keys[176] = {0};
    uint8_t key[16] = {0};
    key[3] = 3;
    key[5] = 5;
    key[11] = 11;
    key[13] = 7;


    RunEncryptionKeySchedule(key, round_keys);
    Encrypt(data, round_keys);
    Decrypt(data, round_keys);
    //SHA256
    uint8_t dest[32] = {0};
    uint8_t sha_data[64];  
    uint32_t length = 0;  
    uint8_t input_data[64] = {0};
    sha256(dest, input_data, length);
}

/*
 * Interface with host readback tool.
 */
void readback(void) {
    wdt_enable(WDTO_2S);  // Start the Watchdog Timer

    uint8_t recv_size = 72;
    uint8_t input_buffer[recv_size];
    int i;

    // receive frame in form (password | start_addr | size | hash)
    for (i = 0; i < recv_size; i++)
    {
	input_buffer[i] = UART1_getchar();
    }

    /*
    // Read in start address (4 bytes)
    uint32_t start_addr = ((uint32_t)UART1_getchar()) << 24;
    start_addr |= ((uint32_t)UART1_getchar()) << 16;
    start_addr |= ((uint32_t)UART1_getchar()) << 8;
    start_addr |= ((uint32_t)UART1_getchar());
    wdt_reset();

    // Read in size (4 bytes)
    uint32_t size = ((uint32_t)UART1_getchar()) << 24;
    size |= ((uint32_t)UART1_getchar()) << 16;
    size |= ((uint32_t)UART1_getchar()) << 8;
    size |= ((uint32_t)UART1_getchar());
    wdt_reset();

    // Read the memory out to UART1
    for (uint32_t addr = start_addr; addr < start_addr + size; ++addr) {
        unsigned char byte = pgm_read_byte_far(addr);  // Read a byte from flash
        wdt_reset();

        UART1_putchar(byte);  // Write the byte to UART1 
        wdt_reset();
    }

    while(1) __asm__ __volatile__("");  // Wait for watchdog timer to reset.
    */
}

/*
 * Load the firmware into flash.
 */
void load_firmware(void) {
    int frame_length = 0;
    unsigned char rcv = 0;
    unsigned char data[SPM_PAGESIZE];  // SPM_PAGESIZE is the size of a page
    unsigned int data_index = 0;
    unsigned int page = SPM_PAGESIZE; // start at second page
    uint16_t version = 0;
    uint16_t size = 0;

    wdt_enable(WDTO_2S);  // Start the Watchdog Timer

    while(!UART1_data_available()) {  // Wait for data
        __asm__ __volatile__("");
    }

    // Get the version
    rcv = UART1_getchar();
    version = (uint16_t)rcv << 8;
    rcv = UART1_getchar();
    version |= (uint16_t)rcv;

    // Get the size
    rcv = UART1_getchar();
    size = (uint16_t)rcv << 8;
    rcv = UART1_getchar();
    size |= (uint16_t)rcv;

    // Compare to old version and abort if older (note special case for version 0)
    if (version != 0 && version < eeprom_read_word(&fw_version)) {
        UART1_putchar(ERROR);  // Reject the metadata
        
        while(1) {  // Wait for watchdog timer to reset
            __asm__ __volatile__("");
        }
    }
    else if (version != 0) {  // Update version number in EEPROM
        wdt_reset();
        eeprom_update_word(&fw_version, version);
    }

    // Write new firmware size to EEPROM
    wdt_reset();
    eeprom_update_word(&fw_size, size);
    wdt_reset();

    UART1_putchar(OK);  // Acknowledge the metadata

    while (1) {  // Loop here until you can get all your characters
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
        }

        // If we filed our page buffer, program it
        if(data_index == SPM_PAGESIZE || frame_length == 0) {
            wdt_reset();
            program_flash(page, data);
            page += SPM_PAGESIZE;
            data_index = 0;
#if 1
            // Write debugging messages to UART0.
            UART0_putchar('P');
            UART0_putchar(page>>8);
            UART0_putchar(page);
#endif
            wdt_reset();

        }

        UART1_putchar(OK);  // Acknowledge the frame
    }
}

/*
 * Ensure the firmware is loaded correctly and boot it up.
 */
void boot_firmware(void) {
    wdt_enable(WDTO_2S);  // Start the Watchdog Timer

    // Write out the release message
    uint8_t cur_byte;
    uint32_t addr = (uint32_t)eeprom_read_word(&fw_size);

    // Reset if firmware size is 0 (indicates no firmware is loaded)
    if(addr == 0) {
        while(1) __asm__ __volatile__("");  // Wait for watchdog timer to reset
    }
    wdt_reset();

    // Write out release message to UART0
    do {
        cur_byte = pgm_read_byte_far(addr);
        UART0_putchar(cur_byte);
        ++addr;
    } while (cur_byte != 0);

    // Stop the Watchdog Timer
    wdt_reset();
    wdt_disable();

    asm("jmp 0000");  // Perform the jmp to the firmware
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
void program_flash(uint32_t page_address, unsigned char* data) {
    int i = 0;
    boot_page_erase_safe(page_address);

    for(i = 0; i < SPM_PAGESIZE; i += 2) {
        uint16_t w = data[i];  // Make a word out of two bytes
        w += data[i+1] << 8;
        boot_page_fill_safe(page_address+i, w);
    }

    boot_page_write_safe(page_address);
    boot_rww_enable_safe();  // We can just enable it after every program too
}

/* Store hashed password and key in first page of flash */
int store_password(void)
{
    unsigned char i;
    unsigned char rcv = 80;
    unsigned char secret_buffer[80] = {0};  
    unsigned char sha_buffer[SPM_PAGESIZE] = {0};

    wdt_enable(WDTO_2S);  // Start the Watchdog Timer

    while(!UART1_data_available()) {  // Wait for data
        __asm__ __volatile__("");
    }

    // get data in form (password | key | hash)
    for (i = 0; i < rcv; i++)
    {
	wdt_reset();
	secret_buffer[i] = UART1_getchar();
    }

    // hash password and key
    sha256(sha_buffer, secret_buffer, (uint32_t) 48);

    // check hash with received hash
    if (cmp(sha_buffer, secret_buffer + 48, (int) 32) != 0)
    {
	wdt_reset();
	UART0_putchar('E');
	while (1) __asm__ __volatile__("");
    }

    // hash password and store to buffer
    sha256(sha_buffer, secret_buffer, (uint32_t) 256);

    // store key after hashed password
    for (i = 0; i < 16; i++)
    {
	    sha_buffer[i + 32] = secret_buffer[i + 32];
    }

    // program hashed password and key to flash
    program_flash((uint32_t) 0, sha_buffer);

    return 1;
}

/* return 0 if two buffers are identical, 1 otherwise */
int cmp(uint8_t *c1, uint8_t *c2, int length)
{
    for (int i = 0; i < length; i++)
    {
	if (c1[i] != c2[i])
		return 1;
    }

    return 0;
}
