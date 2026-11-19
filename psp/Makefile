TARGET = psplink
OBJS = main.o
CFLAGS = -O2 -Wall -Wextra -std=gnu11
LDFLAGS = -Wl,-u,sceNetInetGetErrno
LIBS = -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -lz -lpthreadglue -lpthread -lcglue -lpsprtc -lm -lpspgu -lpspdebug -lc -lpsputility -lpspnet_inet -lpspnet_resolver
BUILD_PRX = 1
ENCRYPT = 1
PSP_FW_VERSION = 620
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PSP Link $(shell sed -n 's/^\#define CURRENT_VERSION "\([^"]*\)"/\1/p' update_format.h)
PSPSDK := $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

main.o: ui/menu.h ui/font.h ui/ca.h update_format.h updater.h

PARAM.SFO: update_format.h
