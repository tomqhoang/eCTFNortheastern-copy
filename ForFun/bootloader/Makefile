# Hardware configuration settings.
MCU = atmega1284p
F_CPU = 20000000
BAUD = 115200

# Secret password default value.
PASSWORD ?= password

# Tool aliases.
CC = avr-gcc
STRIP  = avr-strip
OBJCOPY = avr-objcopy

# Compiler configurations.
CDEFS = -mmcu=${MCU} -DF_CPU=${F_CPU} -DBAUD=${BAUD} -DRB_PASSWORD=\"${PASSWORD}\"
CLINKER = -nostartfiles -Wl,--section-start=.text=0x1E000 
CWARN =  -Wall
COPT = -std=gnu99 -Os -fno-tree-scev-cprop -mcall-prologues \
       -fno-inline-small-functions -fsigned-char

CFLAGS  = $(CDEFS) $(CLINKER) $(CWARN) $(COPT)

# Include file paths.
INCLUDES = -I./include

# Run clean even when all files have been removed.
.PHONY: clean

all:    flash.hex eeprom.hex
	@echo  Simple bootloader has been compiled and packaged as intel hex.

sha256.o: sha2_small_common.o
	$(CC) $(CFLAGS) $(INCLUDES) -c src/sha256.c -o sha256.o sha2_small_common.o

sha2_small_common.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/sha2_small_common.c -o sha2_small_common.o

gf256mul.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/gf256mul.S -o gf256mul.o

aes_dec.o: gf256mul.o aes256_enc.o aes256_dec.o
	$(CC) $(CFLAGS) $(INCLUDES) -c src/aes_dec.c -o aes_dec.o gf256mul.o aes256_enc.o aes256_dec.o

aes_enc.o: gf256mul.o
	$(CC) $(CFLAGS) $(INCLUDES) -c src/aes_enc.c -o aes_enc.o gf256mul.o

aes_sbox.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/aes_sbox.c -o aes_sbox.o

aes_invsbox.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/aes_invsbox.c -o aes_invsbox.o

aes_keyschedule.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/aes_keyschedule.c -o aes_keyschedule.o

aes256_enc.o: gf256mul.o
	$(CC) $(CFLAGS) $(INCLUDES) -c src/aes256_enc.c -o aes256_enc.o gf256mul.o

aes256_dec.o: gf256mul.o
	$(CC) $(CFLAGS) $(INCLUDES) -c src/aes256_dec.c -o aes256_dec.o gf256mul.o

uart.o: 
	$(CC) $(CFLAGS) $(INCLUDES) -c src/uart.c

sys_startup.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/sys_startup.c

bootloader.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/bootloader.c

bootloader.elf: uart.o sys_startup.o bootloader.o sha256.o aes_dec.o aes_enc.o aes_sbox.o aes_invsbox.o aes_keyschedule.o
	$(CC) $(CFLAGS) $(INCLUDES) -o bootloader.elf uart.o sys_startup.o bootloader.o sha256.o sha2_small_common.o aes_dec.o aes_enc.o aes_sbox.o aes_invsbox.o aes_keyschedule.o gf256mul.o

strip: bootloader.elf
	$(STRIP) bootloader.elf -o bootloader.elf

flash.hex: strip $(OBJ)
	$(OBJCOPY) -R .eeprom -O ihex bootloader.elf flash.hex

eeprom.hex: strip
	$(OBJCOPY) -j .eeprom --set-section-flags=.eeprom="alloc,load" --change-section-lma .eeprom=0 -O ihex bootloader.elf eeprom.hex

clean:
	$(RM) -v *.hex *.o *.elf $(MAIN)


