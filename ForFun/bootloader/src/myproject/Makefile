############################################
#
#  Makefile to compile the AES256 libraries
#
############################################

TARGET=main

# Here all the dirs to include to find .h files
DIR= .
DIR+= aes/
DIR+= gf256mul/

# Here C source files
SRC= $(TARGET).c
SRC+= aes/aes_dec.c
SRC+= aes/aes_enc.c
SRC+= aes/aes_sbox.c
SRC+= aes/aes_invsbox.c
SRC+= aes/aes_keyschedule.c
SRC+= aes/aes256_enc.c
SRC+= aes/aes256_dec.c

# Here ASM source files, Make them always end in a capital 'S', ex: asmfile.S
ASRC= gf256mul/gf256mul.S

# frequency in hz
F_CPU=20000000

# Microcontroller to be used
MCU=atmega1284

# Optimization 1, 2, 3 and s for size
OPT = s

# You should not have to change anything below here

# Here we add the prefix -I for the compiler ex: '-I ../dir/subdir/'
IDIR = ${addprefix -I , $(DIR)}

# Compiler C flags
CFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU)UL -O$(OPT) -ffunction-sections
LDFLAGS = -lm --gc-sections -Wl

# Compiler
CC      = avr-gcc
AR      = avr-ar
OBJCOPY = avr-objcopy
SIZE    = avr-size

# C files and ASM files to object files
OBJ = $(SRC:%.c=%.o) $(ASRC:%.S=%.o) 

all: $(TARGET).elf $(TARGET).hex size

clean:
	rm -rf *.o *.hex *.elf $(OBJ)

# Compile: create object files from C and ASM source files.
%.o: %.c
	$(CC) $(CFLAGS) $(IDIR) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) $(IDIR) -c $< -o $@

$(TARGET).elf: $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.hex: %.elf
	$(OBJCOPY) -O ihex -R .eeprom -R .fuse -R .lock -R .signature $< $@ 	 program: $(TARGET).hex 	avrdude -p m328p -P /dev/ttyUSB0     -c stk500v1   -b 57600 -U flash:w:$(TARGET).hex 	 # Display the size of generated elf file ELFSIZE = $(SIZE) --mcu=$(MCU) --format=avr $(TARGET).elf size: 	@if test -f $(TARGET).elf; then echo; $(ELFSIZE); 2>/dev/null; echo; fi
