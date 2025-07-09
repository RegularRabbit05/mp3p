BUILD_PRX = 1
TARGET = Application
OBJS = main.o
INCDIR =
CFLAGS = -Wall -O3 -DDEBUG=false
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
LIBS= -lraylib -lpng -lz -lglut -lGLU -lGL -lpspfpu -lpspvfpu -lpspusb -lpspusbstor -lpsppower -lpspaudio -lpspaudiolib -lmad -lpspmp3 -lpspjpeg

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = (M)P3P
PSP_EBOOT_ICON = .assets/ICON0.png
PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

realclean:
	/bin/rm -f $(OBJS) PARAM.SFO $(TARGET).elf $(TARGET).prx

all: realclean