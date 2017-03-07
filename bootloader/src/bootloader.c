/* bootloader.c
 *
 * If Port B Pin 2 (PB2 on the protostack board) is pulled to ground the 
 * bootloader will wait for data to appear on UART1 (which will be interpretted
 * as an updated firmware package).
 * 
 * If the PB2 pin is NOT pulled to ground, but 
 * Port B Pin 3 (PB3 on the protostack board) is pulled to ground, then the 
 * bootloader will enter flash memory readback mode.
 * 
 * If PB4 is pulled to ground, then the bootloader will enter store password mode.  
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

#include <avr/boot.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <sha256.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <util/delay.h>

#include "decrypt.h"
#include "encrypt.h"
#include "encryption_key_schedule.h"
#include "uart.h"

#define OK ((unsigned char) 0x00)
#define ERROR ((unsigned char) 0x01)

//void test_encryption(void);
void program_flash(uint32_t page_address, unsigned char *data);
void load_firmware(void);
void boot_firmware(void);
void readback(void);
int cmp(uint8_t *, uint8_t *, int);
int store_password(void);

uint16_t fw_size EEMEM = 0;
uint16_t fw_version EEMEM = 0;
uint8_t key_stored EEMEM = 0;

int main(void) {
    UART1_init();  // Init UART1 (virtual com p`ort)
    UART0_init();  // Init UART0
    wdt_reset();

    DDRB &= ~((1 << PB2) | (1 << PB3));  // Configure Port B Pins 2 and 3 as inputs
    PORTB |= (1 << PB2) | (1 << PB3) | (1 << PB4);  // Enable pullups - give port time to settle

    // If jumper is present on pin 2, load new firmware
    if(eeprom_read_byte(&key_stored) == 0){
        eeprom_update_byte(&key_stored, store_password());
    }
    else if (!(PINB & (1 << PB2))) {
        load_firmware();
    }
    else if (!(PINB & (1 << PB3))) {
        UART1_putchar('R');
        readback();
    }
    else {
        UART1_putchar('B');
        //test_encryption();
        //boot_firmware();
    }
}


/*
 * Interface with host readback tool. Takes in a message of 80 bytes, that follows this
 format: encrypted pw (32 bytes), start address (8 bytes), encrypted readback size (8 bytes),
 and hash of previous contents (32 bytes)
 */
void readback(void) {
    wdt_enable(WDTO_2S);  // Start the Watchdog Timer
    uint8_t index = 0; //index for loops
    uint8_t rcv = 80; //length of message
    uint16_t hash_message_length = 384; //length of message minus hash in bits, used as param for sha256
    uint16_t hash_length = 256;
    uint8_t readback_input_buffer[80] = {0}; //stores incoming message
    uint8_t hash_results[32] = {0};

    while(!UART1_data_available()) {  // Wait for data
        __asm__ __volatile__("");
    }

    index = 0; //reset index
    // get data in form (hashed password | key)
    while(index < rcv) {
       wdt_reset();
       readback_input_buffer[index] = UART1_getchar();
       index++;
    }

    //integrity check 
    sha256(hash_results, readback_input_buffer, hash_message_length);
    wdt_reset();
    if(cmp(hash_results, readback_input_buffer + 48, (int) 32) != 0){
        UART0_putchar('F');
         while(1){
            __asm__ __volatile__("");
        } 
    }
    wdt_reset();
    // Compiled out at O1
    if(cmp(hash_results, readback_input_buffer + 48, (int) 32) != 0){
        UART0_putchar('F');
        while(1){
            __asm__ __volatile__("");
        }
    }

    uint8_t key[16] = {0};
    uint8_t round_keys[176] = {0};
    uint8_t hashed_password[32] = {0};
    wdt_reset();

    memcpy_PF(key, 0xEF20, 16); //copy key from memory
    wdt_reset();

    RunEncryptionKeySchedule(key, round_keys); //copy key from memory
    wdt_reset();

    //Start password check process by decrypting password from buffer, in place
    for(uint8_t i = 0; i < 4; i++){
        wdt_reset();
        Decrypt(readback_input_buffer + i*8, round_keys);
    }

    wdt_reset();

    memcpy_PF(hashed_password, 0xEF00, 32); //copy key from memory
    wdt_reset();


    sha256(hash_results, readback_input_buffer, hash_length); //Hash plaintext user password
    wdt_reset();

    //Compare decrypted, hashed user input password with stored hashed password
    if(cmp(hashed_password, hash_results, (int) 32) != 0){
        UART0_putchar('F');
        while(1){
            __asm__ __volatile__("");
        }
    }

    if(cmp(hashed_password, hash_results, (int) 32) != 0){
        UART0_putchar('F');
        while(1){
            __asm__ __volatile__("");
        }
    }

    Decrypt(readback_input_buffer + 32, round_keys); //decrypt start address
    Decrypt(readback_input_buffer + 40, round_keys); //decrypt size

    uint32_t size = 0;
    uint32_t start_addr = 0;

    for(uint8_t i = 0; i < 8; i++){
        wdt_reset();
        size = size + (readback_input_buffer[40 - i] << i*8);
    }

    for(uint8_t i = 0; i < 8; i++){
        wdt_reset();
        start_addr = start_addr + (readback_input_buffer[48 - i] << i*8);
    }

    if(start_addr > 0xFFFF){
        UART0_putchar('F');
         while(1) __asm__ __volatile__("");
    }

    if(start_addr + size > 0xFFFF){
        UART0_putchar('F');
         while(1) __asm__ __volatile__("");
    }

    // Read the memory out to UART1
    for (uint32_t addr = start_addr; addr < start_addr + size; ++addr) {
        unsigned char byte = pgm_read_byte_far(addr);  // Read a byte from flash
        wdt_reset();

        UART1_putchar(byte);  // Write the byte to UART1 
        wdt_reset();
    }

    while(1) __asm__ __volatile__("");  // Wait for watchdog timer to reset.
}

/* Store hashed password and key in first page of flash */

int __attribute__((optimize("O0"))) store_password(void)
{
    wdt_enable(WDTO_2S);  // Start the Watchdog Timer

    //Double check flag
    if(eeprom_read_byte(&key_stored) != 0){
	   while(1) __asm__ __volatile__("");
    }

    //Triple check flag
    if(eeprom_read_byte(&key_stored) != 0){
       while(1) __asm__ __volatile__("");
    }

    uint8_t i;
    uint8_t index;
    uint8_t rcv = 48; // 32 + 16 + 32 bits for password, key, and hash
    uint8_t secret_buffer[48] = {0};  
    uint8_t sha_buffer[SPM_PAGESIZE] = {0};

    UART1_putchar('K');

    while(!UART1_data_available()) {  // Wait for data

        __asm__ __volatile__("");
    }

    index = 0;
    // get data in form (hashed password | key)
    while(index < rcv) {
	   wdt_reset();
	   secret_buffer[index] = UART1_getchar();
	   index++;
    }
 

    index = 0;
    // Program in page sizes, so switch to array of page size length
    for (i = 0; i < 48; i++)
    {
	wdt_reset();
        sha_buffer[index] = secret_buffer[index];
	    index++;
    }

    // store as (hashed password | key) in page that starts at 0xEF00
    program_flash((uint32_t) 0xEF00, sha_buffer);

    return 1;
}
/*
//used this to test signature and version hash were working properly
void test_encryption(void){
    uint8_t key[16] = {0};
    memcpy_PF(key, 0xEF20, 16);

    uint8_t round_keys[176] = {0};

    uint8_t data[8] = {0};
    uint8_t data1[8] = {0};
    uint8_t data2[8] = {0};
    data1[7] = 0xFF;
    data2[0] = 0xFF;

    uint8_t hash0[8] = {0};
    hash0[0] = 0xd9;
    hash0[1] = 0x57;
    hash0[2] = 0x5d;
    hash0[3] = 0xf0;
    hash0[4] = 0x0f;
    hash0[5] = 0xe9;
    hash0[6] = 0x56;
    hash0[7] = 0x27;
   
    RunEncryptionKeySchedule(key, round_keys);
    Encrypt(hash0, round_keys);
    Encrypt(data1, round_keys);
    Encrypt(data2, round_keys);
}
*/

/*
 * Load the firmware into flash.
 */
void load_firmware(void) {
    int frame_length = 0;
    unsigned char rcv = 0;
    unsigned char data[SPM_PAGESIZE];  // SPM_PAGESIZE is the size of a page
    unsigned int data_index = 0;
    unsigned int page = 0;
    uint16_t version = 0;
    uint16_t old_version = 65535; // set to max to ensure accuracy
    uint16_t size = 0;
    uint8_t key[16] = {0};
    uint8_t round_keys[176] = {0};
    uint8_t sig[32] = {0};
    uint8_t page_hash[32] = {0};
    unsigned int sig_index = 0;
    uint32_t hash_length = 0;
    uint8_t max_segments = 0;
    uint16_t segment_index = 0;
    
    // copy key
    memcpy_PF(key, 0xEF20, 16);
    RunEncryptionKeySchedule(key, round_keys);

    wdt_enable(WDTO_2S);  // Start the Watchdog Timer

    UART1_putchar('U');
    wdt_reset();

    while(!UART1_data_available()) {  // Wait for data
        __asm__ __volatile__("");
    }
    wdt_reset();

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

    UART1_putchar(OK);

    wdt_reset();

    // read version hash
    for(int i = 0; i < 32; i++){
	wdt_reset();
	sig[sig_index] = UART1_getchar();
	sig_index++;
    }

    
    data[0] = version << 8;
    data[1] = version;

    sig_index = 2;
    //insert offset to retrieve and store signature
    for (int i = 0; i < 16; i++)
    {
	wdt_reset();
	data[sig_index] = key[sig_index - 2];
	sig_index++;
    }


    // compare encrypted hash with received
    sha256(page_hash, (uint8_t *) data, (uint32_t) 144);
    if(cmp(page_hash, sig, (int) 32) != 0){
	   UART0_putchar('F');
	while(1){
	    __asm__ __volatile__("");
	}
	
    }
    wdt_reset();
   
    if(cmp(page_hash, sig, (int) 32) != 0){
        UART0_putchar('F');
        while(1){
            __asm__ __volatile__("");
        }

    }

    // temporarily store the old version here
    old_version = eeprom_read_word(&fw_version);

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

    // If the new version number is less than the old version number, something is wrong.
    if(old_version > version){
        UART1_putchar(ERROR);

	while(1){
	    __asm__ __volatile__("");
	   }
    }

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

    // If the new version number is less than the old version number, something is wrong.
    if(old_version > version){
        UART1_putchar(ERROR);

        while(1){
            __asm__ __volatile__("");
        }
    }


    // Write new firmware size to EEPROM
    wdt_reset();
    eeprom_update_word(&fw_size, size);
    wdt_reset();

    UART1_putchar(OK);  // Acknowledge the metadata

    data_index = 0;
    uint8_t frame_counter = 0;
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

    	frame_counter++;
	
        // If we filed our page buffer, program it
        if(data_index == SPM_PAGESIZE || frame_length == 0) {


	    wdt_reset();

	    if (frame_length == 0)
		UART1_putchar('D');

            UART1_putchar(OK);
	    sig_index = 0;
	    for(int i = 0; i < 32; i++){
	    	wdt_reset();
		sig[sig_index] = UART1_getchar();
		sig_index++;
	    }

	    // last frame received
	    if(frame_length == 0)
	       {
		hash_length = (frame_counter << 4) << 3;
	    }

	    else {
		  hash_length = 2048;
	    }

            ///Need to compare retreived hash, so we compute the encrypted hash
	    sha256(page_hash, data, hash_length);    
        wdt_reset();

	    Encrypt(page_hash,round_keys);
        Encrypt(page_hash+8, round_keys);
        Encrypt(page_hash+16, round_keys);
	    Encrypt(page_hash+24, round_keys);

	    wdt_reset();

	    if(cmp(page_hash, sig, (int) 32) != 0){
	    	UART0_putchar('F');
		while(1){
		    __asm__ __volatile__("");
		}		
	    }
	    wdt_reset();
	    if(cmp(page_hash, sig, (int)32) != 0){
		UART0_putchar('F');
		while(1){
	            __asm__ __volatile__("");
		}
	    }

	    wdt_reset();
	    
	    
	    max_segments = (frame_counter) * 2; 
	    //decrypted in SIMON block sizes		
	    for(uint8_t i = 0; i < max_segments; i++){
		wdt_reset();
		Decrypt(data + i*8, round_keys);
	    }


	    segment_index = max_segments << 3;
	    while (segment_index < 256)
	    {
		wdt_reset();
		data[segment_index] = 0;
		segment_index++;
	    }

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
	    frame_counter = 0;

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
// TODO: make sure this for loop works the way it is
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

/*
 * This function takes two arrays and their length and compares them for equality.
 */
int cmp(uint8_t *c1, uint8_t *c2, int length)
{
    for (int i = 0; i < length; i++)
    {
	if (c1[i] != c2[i])
	    return 1;
    }

    return 0;
}
