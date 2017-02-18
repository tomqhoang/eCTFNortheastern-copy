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
CLINKER = -nostartfiles -Wl,--section-start=.text=0x1F000
CWARN =  -Wall
COPT = -std=gnu99 -Os -fno-tree-scev-cprop -mcall-prologues \
       -fno-inline-small-functions -fsigned-char

CFLAGS  = $(CDEFS) $(CLINKER) $(CWARN) $(COPT)

# Include file paths.
INCLUDES = -I./include

# Run clean even when all files have been removed.
.PHONY: clean

ASRC= gf256mul/gf256mul.S
SRC+= include/aes_dec.c
SRC+= include/aes_enc.c
SRC+= include/aes_sbox.c
SRC+= include/aes_invsbox.c
SRC+= include/aes_keyschedule.c
SRC+= include/aes256_enc.c
SRC+= include/aes256_dec.c

OBJ = $(SRC:%.c=%.o) $(ASRC:%.S=%.o)

all:    flash.hex eeprom.hex
	@echo  Simple bootloader has been compiled and packaged as intel hex.

gf256mul.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c include/gf256mul.S

aes_dec.o: gf256mul.o
	$(CC) $(CFLAGS) $(INCLUDES) -c include/aes_dec.c -o gf256mul.o 

aes_enc.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c include/aes_enc.c

aes_sbox.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c include/aes_sbox.c

aes_invsbox.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c include/aes_invsbox.c

aes_keyschedule.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c include/aes_keyschedule.c

aes256_enc.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c include/aes256_enc.c

aes256_dec.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c include/aes256_dec.c 

uart.o: 
	$(CC) $(CFLAGS) $(INCLUDES) -c src/uart.c

sys_startup.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/sys_startup.c

bootloader.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/bootloader.c

bootloader.elf: uart.o sys_startup.o bootloader.o aes_dec.o aes256_dec.o aes256_enc.o aes_enc.o aes_sbox.o aes_invsbox.o aes_keyschedule.o gf256mul.o
	$(CC) $(CFLAGS) $(INCLUDES) -o bootloader.elf uart.o sys_startup.o bootloader.o aes256_dec.o aes256_dec.o aes_dec.o aes_enc.o aes_sbox.o aes_invsbox.o aes_keyschedule.o

strip: bootloader.elf
	$(STRIP) bootloader.elf -o bootloader.elf

flash.hex: strip $(OBJ)
	$(OBJCOPY) -R .eeprom -O ihex bootloader.elf flash.hex

eeprom.hex: strip
	$(OBJCOPY) -j .eeprom --set-section-flags=.eeprom="alloc,load" --change-section-lma .eeprom=0 -O ihex bootloader.elf eeprom.hex

clean:
	$(RM) -v *.hex *.o *.elf $(MAIN)


