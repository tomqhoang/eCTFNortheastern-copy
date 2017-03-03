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
PROGRAMMER = dragon_jtag

# Compiler configurations.
CDEFS = -g3 -ggdb3 -mmcu=${MCU} -DF_CPU=${F_CPU} -DBAUD=${BAUD} -DRB_PASSWORD=\"${PASSWORD}\"
CLINKER = -nostartfiles -Wl,--section-start=.text=0x1E000 -Wl,-Map,bootloader.map
CWARN =  -Wall
COPT = -std=gnu99 -O1 -fno-tree-scev-cprop -mcall-prologues \
       -fno-inline-small-functions -fsigned-char

CFLAGS  = $(CDEFS) $(CLINKER) $(CWARN) $(COPT)

# Include file paths.
INCLUDES = -I./include

# Run clean even when all files have been removed.
.PHONY: clean

all:    flash.hex eeprom.hex
	@echo  Simple bootloader has been compiled and packaged as intel hex.

encrypt.o: constants.o
	$(CC) $(CFLAGS) $(INCLUDES) -c src/encrypt.c -o encrypt.o constants.o

encryption_key_schedule.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/encryption_key_schedule.c -o encryption_key_schedule.o

decrypt.o: constants.o
	$(CC) $(CFLAGS) $(INCLUDES) -c src/decrypt.c -o decrypt.o constants.o

constants.o: 
	$(CC) $(CFLAGS) $(INCLUDES) -c src/constants.c -o constants.o

sha2_small_common.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/sha2_small_common.c -o sha2_small_common.o

sha256.o: sha2_small_common.o
	$(CC) $(CFLAGS) $(INCLUDES) -c src/sha256.c -o sha256.o sha2_small_common.o

uart.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/uart.c

sys_startup.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/sys_startup.c

bootloader.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c src/bootloader.c

bootloader_dbg.elf: uart.o sys_startup.o bootloader.o sha256.o encrypt.o decrypt.o encryption_key_schedule.o
	$(CC) $(CFLAGS) $(INCLUDES) -o bootloader_dbg.elf uart.o sys_startup.o bootloader.o sha256.o sha2_small_common.o encrypt.o decrypt.o encryption_key_schedule.o constants.o

strip: bootloader_dbg.elf
	$(STRIP) bootloader_dbg.elf -o bootloader.elf

flash.hex: strip
	$(OBJCOPY) -R .eeprom -O ihex bootloader.elf flash.hex
	avr-size flash.hex

eeprom.hex: strip
	$(OBJCOPY) -j .eeprom --set-section-flags=.eeprom="alloc,load" --change-section-lma .eeprom=0 -O ihex bootloader.elf eeprom.hex

flash: flash.hex eeprom.hex
	 avrdude -F -P usb -p m1284p -c $(PROGRAMMER)  -u -U flash:w:flash.hex:i \
						    -U eeprom:w:eeprom.hex:i \
						    -U lfuse:w:lfuse.hex:i \
						    -U hfuse:w:hfuse.hex:i \
						    -U efuse:w:efuse.hex:i

debug: flash.hex eeprom.hex
	# Launch avarice: a tool that creates a debug server for the AVR and Dragon
	avarice -R -g :4242 &
	# Launch the avr debugger avr-gdb. The configuation for this tool is included 
	# in .gdbinit
	avr-gdb


clean:
	$(RM) -v *.hex *.o *.elf $(MAIN)

