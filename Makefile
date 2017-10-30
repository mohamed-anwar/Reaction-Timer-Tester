ARD_BLD = arduino-builder
AVRDUDE = avrdude
RM = rm

# Supported Boards: UNO, LEONARDO
BOARD = LEONARDO

ifeq ($(BOARD), UNO)
ARD_BLD_FQBN = "arduino:avr:uno:cpu=atmega328p"
PART_NO = m328p
PROGRAMMER = arduino
endif

ifeq ($(BOARD), LEONARDO)
ARD_BLD_FQBN = "arduino:avr:leonardo:cpu=atmega32u4"
PART_NO = m32u4
PROGRAMMER = avr109
endif

BAUDRATE = 57600
PORT = /dev/ttyACM0
CONFIG = /etc/avrdude/avrdude.conf
LIBS = /usr/share/arduino/libraries/
ARD_HW = /usr/share/arduino/hardware/

ARD_BLD_FLAGS = -hardware $(ARD_HW) -fqbn $(ARD_BLD_FQBN) -compile -verbose -libraries $(LIBS) -build-path $(PWD)/build

all: Reaction

.PHONY: Reaction upload clean
Reaction: Reaction.ino
	@$(ARD_BLD) $(ARD_BLD_FLAGS) $<

upload:
	@$(AVRDUDE) -C $(CONFIG) -p $(PART_NO) -c $(PROGRAMMER) -b $(BAUDRATE) -D -U flash:w:build/Reaction.ino.hex:i -P $(PORT) -v

clean:
	@$(RM) -rf build/*
