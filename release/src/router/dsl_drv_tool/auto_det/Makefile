CFLAGS += -Wall -O

CFLAGS += -DASUS -DASUS_NVRAM
CFLAGS += -I$(TOP)/shared -I$(SRCBASE)/include
LDFLAGS = -L$(TOP)/shared -lshared -L$(TOP)/nvram -lnvram
INSTALL = install

all: auto_det

auto_det: auto_det auto_det.c discover.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

install: auto_det
	$(STRIP) auto_det
	$(INSTALL) -d $(INSTALLDIR)/usr/sbin 
	$(INSTALL) auto_det $(INSTALLDIR)/usr/sbin

clean:
	rm -f auto_det *.o




