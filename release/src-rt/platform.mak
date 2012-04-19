export LINUXDIR := $(SRCBASE)/linux/linux-2.6

EXTRA_CFLAGS := -DLINUX26 -DCONFIG_BCMWL5 -DDEBUG_NOISY -DDEBUG_RCTEST -pipe

export CONFIG_LINUX26=y
export CONFIG_BCMWL5=y


define platformRouterOptions
endef

define platformBusyboxOptions
endef

define platformKernelConfig

# prepare config_base

# prepare prebuilt sysdep kernel module
	@( \
	if [ -d $(SRCBASE)/wl/sysdeps/$(BUILD_NAME) ]; then \
		cp -rf $(SRCBASE)/wl/sysdeps/$(BUILD_NAME)/linux $(SRCBASE)/wl/. ; \
	else \
		cp -rf $(SRCBASE)/wl/sysdeps/default/linux $(SRCBASE)/wl/. ; \
	fi; \
	)
endef


