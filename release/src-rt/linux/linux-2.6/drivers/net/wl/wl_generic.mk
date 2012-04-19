#
# Generic portion of the Broadcom wl driver makefile
#
# input: O_TARGET, CONFIG_WL_CONF and wl_suffix
# output: obj-m, obj-y
#
# $Id: wl_generic.mk,v 1.10 2011-01-21 22:12:09 Exp $
#

REBUILD_WL_MODULE=$(shell if [ -d "$(src)/$(SRCBASE)/wl/sys" -a "$(REUSE_PREBUILT_WL)" != "1" ]; then echo 1; else echo 0; fi)
REBUILD_WL_MODULE=0

# If source directory (src/wl/sys) exists and REUSE_PREBUILT_WL is undefined, 
# then build inside $(SRCBASE)/wl/sys, otherwise use pre-builts
ifeq ($(REBUILD_WL_MODULE),1)

    # Get the source files and flags from the specified config file
    # (Remove config's string quotes before trying to use the file)
    ifeq ($(CONFIG_WL_CONF),)
         $(error var_vlist($(VLIST)) var_config_wl_use($(shell env|grep CONFIG_WL_USE)))
         $(error CONFIG_WL_CONF is undefined)
    endif
    
    WLCONFFILE := $(strip $(subst ",,$(CONFIG_WL_CONF))) 
    WLCFGDIR   := $(src)/$(SRCBASE)/wl/config
    
    # define OS flag to pick up wl osl file from wl.mk
    WLLX=1
    ifdef CONFIG_PLC
    PLC=1
    endif
    include $(WLCFGDIR)/$(WLCONFFILE)
    include $(WLCFGDIR)/wl.mk
    
    ifeq ($(WLFILES),)
         $(error WLFILES is undefined in $(WLCFGDIR)/$(WLCONFFILE))
    endif
    
    ifeq ("$(CONFIG_WL_EMULATOR)","y") 
         WLFILES += wl_bcm57emu.c
    endif
    
    WL_SOURCE := $(WLFILES)
    WL_DFLAGS := $(WLFLAGS)
    # If source directory (src/wl/sys) exists, use sources to build objects
    WL_OBJS   := $(foreach file, $(WL_SOURCE), \
		 $(if $(wildcard $(src)/$(SRCBASE)/wl/sys/$(file)), \
		 $(addprefix $(SRCBASE)/wl/sys/, $(patsubst %.c,%.o,$(file)))))
    WL_OBJS   += $(foreach file, $(WL_SOURCE), \
		 $(if $(wildcard $(src)/$(SRCBASE)/bcmcrypto/$(file)), \
		 $(addprefix $(SRCBASE)/bcmcrypto/, $(patsubst %.c,%.o,$(file)))))
    WL_OBJS   += $(foreach file, $(WL_SOURCE), \
		 $(if $(wildcard $(src)/$(SRCBASE)/shared/$(file)), \
		 $(addprefix $(SRCBASE)/shared/, $(patsubst %.c,%.o,$(file)))))
    WL_OBJS   += $(foreach file, $(WL_SOURCE), \
		 $(if $(wildcard $(src)/$(SRCBASE)/bcmsdio/sys/$(file)), \
		 $(addprefix $(SRCBASE)/bcmsdio/sys/, $(patsubst %.c,%.o,$(file)))))
    
    # need -I. to pick up wlconf.h in build directory
   
    ifeq ($(CONFIG_WL_ALL_PASSIVE_ON),y)
    EXTRA_CFLAGS    += -DWL_ALL_PASSIVE_ON -DWL_ALL_PASSIVE
    else
    ifeq ($(CONFIG_WL_ALL_PASSIVE_RUNTIME),y)
    EXTRA_CFLAGS    += -DWL_ALL_PASSIVE
    endif
    endif
    EXTRA_CFLAGS += -DDMA $(WL_DFLAGS) -I$(src) -I$(src)/.. -I$(src)/$(SRCBASE)/wl/linux \
		    -I$(src)/$(SRCBASE)/wl/sys -Werror

    ifneq ("$(CONFIG_CC_OPTIMIZE_FOR_SIZE)","y")
         EXTRA_CFLAGS += -finline-limit=2048
    endif
    
    # If the PHY_HAL flag is defined we look in directory wl/phy for the
    # phy source files.
    ifneq ($(findstring PHY_HAL,$(WL_DFLAGS)),)
        WL_OBJS   += $(foreach file, $(WL_SOURCE), \
		     $(if $(wildcard $(src)/$(SRCBASE)/wl/phy/$(file)), \
		     $(addprefix $(SRCBASE)/wl/phy/, $(patsubst %.c,%.o,$(file)))))
        EXTRA_CFLAGS += -I$(src)/$(SRCBASE)/wl/phy
    endif

    # wl-objs is for linking to wl.o
    $(TARGET)-objs := $(WLCONF_O) $(WL_OBJS)
    obj-$(CONFIG_WL) := $(TARGET).o

else # SRCBASE/wl/sys doesn't exist

    # Otherwise, assume prebuilt object module(s) in src/wl/linux directory
    prebuilt := wl_$(wl_suffix).o
    $(TARGET)-objs := $(SRCBASE)/wl/linux/$(prebuilt)
    obj-$(CONFIG_WL) := $(TARGET).o

    ifeq ("$(CONFIG_WL_USBAP)","y")
        wl_high-objs := $(SRCBASE)/wl/linux/wl_high.o
        obj-m += wl_high.o
    endif
endif


#WL_CONF_H: wlconf.h

UPDATESH   := $(WLCFGDIR)/diffupdate.sh

WLTUNEFILE ?= wltunable_lx_router.h

$(obj)/$(WLCONF_O): $(obj)/$(WLCONF_H) FORCE

$(obj)/$(WLCONF_H): $(WLCFGDIR)/$(WLTUNEFILE) FORCE
	[ ! -f $@ ] || chmod +w $@
	@echo "check and update config file"
	@echo $(if $(VLIST),"VLIST          = $(VLIST)")
	@echo "CONFIG_WL_CONF = $(CONFIG_WL_CONF)"
	@echo "WLTUNEFILE     = $(WLTUNEFILE)"
	cp $< wltemp
	$(UPDATESH) wltemp $@

FORCE:


clean-files += $(SRCBASE)/wl/sys/*.o $(SRCBASE)/wl/phy/*.o $(SRCBASE)/wl/sys/.*.*.cmd $(SRCBASE)/wl/phy/.*.*.cmd $(WLCONF_H) $(WLCONF_O)
