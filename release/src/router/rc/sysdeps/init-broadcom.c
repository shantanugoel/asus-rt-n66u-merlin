/*

	Copyright 2005, Broadcom Corporation
	All Rights Reserved.

	THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
	KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
	SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
	FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.

*/

#include "rc.h"
#include "shared.h"
#include "interface.h"

#include <termios.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <time.h>
#include <errno.h>
#include <paths.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <sys/klog.h>
#ifdef LINUX26
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#endif
#include <wlutils.h>
#include <bcmdevs.h>

void init_devs(void)
{
#if 0
#ifdef CONFIG_BCMWL5
	// ctf must be loaded prior to any other modules
	if (nvram_get_int("ctf_disable") == 0)
		modprobe("ctf");
#endif
#endif
}


void generate_switch_para(void)
{
	int model, cfg;
	char lan[SWCFG_BUFSIZE], wan[SWCFG_BUFSIZE];

	// generate nvram nvram according to system setting
	model = get_model();

	if (is_routing_enabled()) {
		cfg = nvram_get_int("switch_stb_x");
		if (cfg < SWCFG_DEFAULT || cfg > SWCFG_STB34)
			cfg = SWCFG_DEFAULT;
	} else	cfg = SWCFG_BRIDGE;

	switch(model) {
		case MODEL_RTN12:
		case MODEL_RTN12B1:
		case MODEL_RTN12C1:
		{	                              /* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 4, 3, 2, 1, 0, 5 };
			/* TODO: switch_wantag? */

			//if (!is_routing_enabled())
			//	nvram_set("lan_ifnames", "eth0 eth1"); //override
			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, cfg, 1, "u");
			nvram_set("vlan0ports", lan);
			nvram_set("vlan1ports", wan);
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, cfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
			break;
		}

		case MODEL_RTN53:
		{	                              /* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 4, 3, 2, 1, 0, 5 };
			/* TODO: switch_wantag? */
			/* TODO: why lan on vlan2? */

			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, cfg, 1, "u");
			nvram_set("vlan2ports", lan);
			nvram_set("vlan1ports", wan);
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, cfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
 			break;
		}

		case MODEL_RTN15U:
		{	                              /* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 4, 3, 2, 1, 0, 8 };
			/* TODO: switch_wantag? */

			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, cfg, 1, "u");
			nvram_set("vlan1ports", lan);
			nvram_set("vlan2ports", wan);
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, cfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
			break;
		}

		case MODEL_RTN16:
		{	                              /* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 0, 4, 3, 2, 1, 8 };
			int wancfg = nvram_invmatch("switch_wantag", "none") ? SWCFG_DEFAULT : cfg;

			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, wancfg, 1, "u");
			nvram_set("vlan1ports", lan);
			nvram_set("vlan2ports", wan);
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, wancfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
			break;
		}

		case MODEL_RTN66U:
		{	                              /* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 0, 1, 2, 3, 4, 8 };
			int wancfg = nvram_invmatch("switch_wantag", "none") ? SWCFG_DEFAULT : cfg;

			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, wancfg, 1, "u");
			nvram_set("vlan1ports", lan);
			nvram_set("vlan2ports", wan);
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, wancfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
			break;
		}

		case MODEL_RTN10U:
		{	                              /* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 0, 1, 2, 3, 4, 5 };
			/* TODO: switch_wantag? */

			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, cfg, 1, "u");
			nvram_set("vlan0ports", lan);
			nvram_set("vlan1ports", wan);
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, cfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
 			break;
		}
	}
}

void enable_jumbo_frame()
{
	int model;

	model = get_model();

	if(model!=MODEL_RTN66U && model!=MODEL_RTN16 && model!=MODEL_RTN15U) return ;
	if(nvram_get_int("jumbo_frame_enable"))
		eval("et", "robowr", "0x40", "0x01", "0x1f");
	else eval("et", "robowr", "0x40", "0x01", "0x00");
}

void init_switch()
{
	generate_switch_para();

#ifdef CONFIG_BCMWL5
	// ctf should be disabled when some functions are enabled
	if(nvram_get_int("qos_enable") || nvram_get_int("vts_enable_x") || nvram_get_int("url_enable_x") || nvram_get_int("ctf_disable_force")
#ifdef RTCONFIG_WIRELESSREPEATER
	|| nvram_get_int("sw_mode") == SW_MODE_REPEATER
#endif
	)
		nvram_set("ctf_disable", "1");
	else nvram_set("ctf_disable", "0");

        // ctf must be loaded prior to any other modules 
	if (nvram_get_int("ctf_disable") == 0)
		modprobe("ctf");
#endif

#ifdef RTCONFIG_SHP
	if(nvram_get_int("qos_enable") || nvram_get_int("macfilter_enable_x") || nvram_get_int("lfp_disable_force")) {
		nvram_set("lfp_diable", "1");
	}
	else {
		nvram_set("lfp_diable", "0");
	}

	if(nvram_get_int("lfp_disable")==0) {
		restart_lfp();
	}
#endif
	modprobe("et");
	modprobe("bcm57xx");
	enable_jumbo_frame();
}

int
switch_exist(void) {
	//check switch boot up or not
        system("et robord 0 0 &> /tmp/switch_check");
        char buf[32];
        memset(buf, 0, 32);
        FILE *fp;
        if( (fp = fopen("/tmp/switch_check", "r")) != NULL ) {
                while(fgets(buf, sizeof(buf), fp)) {
                        if(strstr(buf, "et interface not found")) {
                                _dprintf("No switch interface!!!\n");
                                return 0;
                        }
                }
                fclose(fp);
                system("rm -rf /tmp/switch_check");
        }
	return 1;
}

void config_switch()
{
	generate_switch_para();
	
	// setup switch
	// implement config switch as vlan here
	if(nvram_get_int("jumbo_frame_enable"))
		eval("et", "robowr", "0x40", "0x01", "0x1f");
	else eval("et", "robowr", "0x40", "0x01", "0x00");
}

void init_wl(void)
{
#ifdef RTCONFIG_EMF
	modprobe("emf");
	modprobe("igs");
#endif
	modprobe("wl");

#ifdef RTCONFIG_BRCM_USBAP
		/* We have to load USB modules after loading PCI wl driver so
		 * USB driver can decide its instance number based on PCI wl
		 * instance numbers (in hotplug_usb())
		 */
		modprobe("usbcore");
		modprobe("usb-storage");

		{
			char	insmod_arg[128];
			int	i = 0, maxwl_eth = 0, maxunit = -1;
			char	ifname[16] = {0};
			int	unit = -1;
			char arg1[20] = {0};
			char arg2[20] = {0};
			char arg3[20] = {0};
			char arg4[20] = {0};
			char arg5[20] = {0};
			char arg6[20] = {0};
			char arg7[20] = {0};
			const int wl_wait = 3;	/* max wait time for wl_high to up */

			/* Save QTD cache params in nvram */
			sprintf(arg1, "log2_irq_thresh=%d", atoi(nvram_safe_get("ehciirqt")));
			sprintf(arg2, "qtdc_pid=%d", atoi(nvram_safe_get("qtdc_pid")));
			sprintf(arg3, "qtdc_vid=%d", atoi(nvram_safe_get("qtdc_vid")));
			sprintf(arg4, "qtdc0_ep=%d", atoi(nvram_safe_get("qtdc0_ep")));
			sprintf(arg5, "qtdc0_sz=%d", atoi(nvram_safe_get("qtdc0_sz")));
			sprintf(arg6, "qtdc1_ep=%d", atoi(nvram_safe_get("qtdc1_ep")));
			sprintf(arg7, "qtdc1_sz=%d", atoi(nvram_safe_get("qtdc1_sz")));

			eval("insmod", "ehci-hcd", arg1, arg2, arg3, arg4, arg5,
				arg6, arg7);

			/* Search for existing PCI wl devices and the max unit number used.
			 * Note that PCI driver has to be loaded before USB hotplug event.
			 * This is enforced in rc.c
			 */
			#define DEV_NUMIFS 8
			for (i = 1; i <= DEV_NUMIFS; i++) {
				sprintf(ifname, "eth%d", i);
				if (!wl_probe(ifname)) {
					if (!wl_ioctl(ifname, WLC_GET_INSTANCE, &unit,
						sizeof(unit))) {
						maxwl_eth = i;
						maxunit = (unit > maxunit) ? unit : maxunit;
					}
				}
			}

			/* Set instance base (starting unit number) for USB device */
			sprintf(insmod_arg, "instance_base=%d", maxunit + 1);
			eval("insmod", "wl_high", insmod_arg);

			/* Hold until the USB/HSIC interface is up (up to wl_wait sec) */
			sprintf(ifname, "eth%d", maxwl_eth + 1);
			i = wl_wait;
			while (wl_probe(ifname) && i--) {
				sleep(1);
			}
			if (!wl_ioctl(ifname, WLC_GET_INSTANCE, &unit, sizeof(unit)))
				cprintf("wl%d is up in %d sec\n", unit, wl_wait - i);
			else
				cprintf("wl%d not up in %d sec\n", unit, wl_wait);
		}
#ifdef LINUX26
		mount("usbdeffs", "/proc/bus/usb", "usbfs", MS_MGC_VAL, NULL);
#else
		mount("none", "/proc/bus/usb", "usbdevfs", MS_MGC_VAL, NULL);
#endif /* LINUX26 */
#endif /* __CONFIG_USBAP__ */

}


void init_syspara(void)
{
	char *ptr;
	int model;

	nvram_set("firmver", rt_version);
	nvram_set("productid", rt_buildname);	
	nvram_set("buildno", rt_serialno);
	nvram_set("buildinfo", rt_buildinfo);
	ptr = NULL;//nvram_safe_get("sb/1/macaddr");

	model = get_model();
	switch(model) {
		case MODEL_RTN53:
		case MODEL_RTN15U:
		case MODEL_RTN10U:
                        if (!nvram_get("sb/1/macaddr")) //eth2(5G)
                                nvram_set("sb/1/macaddr", "00:22:15:A5:03:04");
                        if (!nvram_get("0:macaddr")) //eth2(5G)
                                nvram_set("0:macaddr", "00:22:15:A5:03:04");
			if (!nvram_get("et0macaddr")) //eth0, eth1
				nvram_set("et0macaddr", "00:22:15:A5:03:00");
                        if (!nvram_get("pci/2/1/macaddr")) //eth2(5G)
                                nvram_set("pci/2/1/macaddr", "00:22:15:A5:03:04");
		        if(nvram_get("regulation_domain_5g")) {
                		nvram_set("wl1_country_code", nvram_get("regulation_domain_5g"));
				nvram_set("pci/2/1/ccode", nvram_get("regulation_domain_5g"));
			}
		        else {
                		nvram_set("wl1_country_code", "US");
				nvram_set("pci/2/1/ccode", "US");
			}
			break;	
		case MODEL_RTN66U:
			if (!nvram_get("et0macaddr")) //eth0, eth1
				nvram_set("et0macaddr", "00:22:15:A5:03:00");
                        if (!nvram_get("pci/2/1/macaddr")) //eth2(5G)
                                nvram_set("pci/2/1/macaddr", "00:22:15:A5:03:04");
		        if(nvram_get("regulation_domain_5G")) {
                		nvram_set("wl1_country_code", nvram_get("regulation_domain_5G"));
				nvram_set("pci/2/1/ccode", nvram_get("regulation_domain_5G"));
			}
		        else {
                		nvram_set("wl1_country_code", "US");
				nvram_set("pci/2/1/ccode", "US");
			}
			break;
		default:
			if (!nvram_get("et0macaddr")) 
				nvram_set("et0macaddr", "00:22:15:A5:03:00");
			break;
	}

	nvram_set("pci/1/1/macaddr", nvram_safe_get("et0macaddr"));

        if(nvram_get("regulation_domain")) {
                nvram_set("wl0_country_code", nvram_get("regulation_domain"));
		nvram_set("pci/1/1/ccode", nvram_get("regulation_domain"));
	}
	else {
		nvram_set("wl0_country_code", "US");
		nvram_set("pci/1/1/ccode", "US");
	}

	if (nvram_get("secret_code"))
		nvram_set("wps_device_pin", nvram_get("secret_code"));
	else
		nvram_set("wps_device_pin", "12345670");
}

void chanspec_fix_5g(int unit)
{
	char tmp[100], prefix[]="wlXXXXXXX_";
	int channel;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	channel = atoi(nvram_safe_get(strcat_r(prefix, "channel", tmp)));

	if ((channel == 36) || (channel == 44) || (channel == 52) || (channel == 60) || (channel == 100) || (channel == 108) || (channel == 116) || (channel == 124) || (channel == 132) || (channel == 149) || (channel == 157))
	{
		dbG("fix nctrlsb of channel %d as %s\n", channel, "lower");
		nvram_set(strcat_r(prefix, "nctrlsb", tmp), "lower");
	}
	else if ((channel == 40) || (channel == 48) || (channel == 56) || (channel == 64) || (channel == 104) || (channel == 112) || (channel == 120) || (channel == 128) || (channel == 136) || (channel == 153) || (channel == 161))
	{
		dbG("fix nctrlsb of channel %d as %s\n", channel, "upper");
		nvram_set(strcat_r(prefix, "nctrlsb", tmp), "upper");
	}
}

static int set_wltxpwoer_once = 0; 

int set_wltxpower()
{
	char ifnames[256];
	char name[64], ifname[64], *next = NULL;
	int unit = -1, subunit = -1;
	int i;
	char tmp[100], prefix[]="wlXXXXXXX_";
	char tmp2[100], prefix2[]="pci/x/1/";
	int txpower = 0, maxpwr = 0;
	double offset = 0.000;
	int commit_needed = 0;
	char blver1, blver2, blver3, blver4;

	sscanf(nvram_safe_get("bl_version"), "%c.%c.%c.%c", &blver1, &blver2, &blver3, &blver4);
	snprintf(ifnames, sizeof(ifnames), "%s %s",
		 nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
	remove_dups(ifnames, sizeof(ifnames));

	i = 0;
	foreach(name, ifnames, next) {
		if (nvifname_to_osifname(name, ifname, sizeof(ifname)) != 0)
			continue;

		if (wl_probe(ifname) || wl_ioctl(ifname, WLC_GET_INSTANCE, &unit, sizeof(unit)))
			continue;

		/* Convert eth name to wl name */
		if (osifname_to_nvifname(name, ifname, sizeof(ifname)) != 0)
			continue;

		/* Slave intefaces have a '.' in the name */
		if (strchr(ifname, '.'))
			continue;

		if (get_ifname_unit(ifname, &unit, &subunit) < 0)
			continue;

		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
		snprintf(prefix2, sizeof(prefix2), "pci/%d/1/", unit + 1);
		txpower = nvram_get_int(wl_nvname("TxPower", unit, 0));

		dbG("unit: %d, txpower: %d\n", unit, txpower);

		if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))
		{
			if (txpower == 80)
			{
				if (set_wltxpwoer_once)
				{
#if 0
					if (nvram_get(strcat_r(prefix2, "maxp2ga0", tmp2)))
					{
						nvram_unset(strcat_r(prefix2, "maxp2ga0", tmp2));
						commit_needed++;
					}
					if (nvram_get(strcat_r(prefix2, "maxp2ga1", tmp2)))
					{
						nvram_unset(strcat_r(prefix2, "maxp2ga1", tmp2));
						commit_needed++;
					}
					if (nvram_get(strcat_r(prefix2, "maxp2ga2", tmp2)))
					{
						nvram_unset(strcat_r(prefix2, "maxp2ga2", tmp2));
						commit_needed++;
					}
#else
					if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "0x64"))
					{
						nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2), "0x64");
						nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2), "0x64");
						nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2), "0x64");
						commit_needed++;
					}
#endif
				}
#ifndef MEDIA_REVIEW
				if (nvram_match(strcat_r(prefix, "country_code", tmp), "US")
					&& nvram_match(strcat_r(prefix2, "regrev", tmp2), "2"))
				{
					nvram_set(strcat_r(prefix2, "regrev", tmp2), "39");
					commit_needed++;
				}
				else if (nvram_match(strcat_r(prefix, "country_code", tmp), "EU")
					&& nvram_match(strcat_r(prefix2, "regrev", tmp2), "5"))
				{
					nvram_set(strcat_r(prefix2, "regrev", tmp2), "3");
					commit_needed++;
				}
#endif
			}
			else
			{
				if (txpower < 30)
					offset = -6.0;
				else if (txpower < 50)
					offset = -4.0;
				else if (txpower < 81)
					offset = -2.0;
				else if (txpower < 151)
					offset = 0.5;
				else if (txpower < 221)
					offset = 1.0;
				else if (txpower < 291)
					offset = 1.5;
				else if (txpower < 361)
					offset = 2.0;
				else if (txpower < 431)
					offset = 2.5;
				else
					offset = 3.0;

				maxpwr = 4 * (25 + offset);
				sprintf(tmp, "0x%x", maxpwr);

				if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), tmp))
				{
					nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2), tmp);
					commit_needed++;
				}
				if (!nvram_match(strcat_r(prefix2, "maxp2ga1", tmp2), tmp))
				{
					nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2), tmp);
					commit_needed++;
				}
				if (!nvram_match(strcat_r(prefix2, "maxp2ga2", tmp2), tmp))
				{
					nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2), tmp);
					commit_needed++;
				}
#ifndef MEDIA_REVIEW
				if (nvram_match(strcat_r(prefix, "country_code", tmp), "US")
					&& nvram_match(strcat_r(prefix2, "regrev", tmp2), "39"))
				{
					nvram_set(strcat_r(prefix2, "regrev", tmp2), "2");
					commit_needed++;
				}
				else if (nvram_match(strcat_r(prefix, "country_code", tmp), "EU")
					&& nvram_match(strcat_r(prefix2, "regrev", tmp2), "3"))
				{
					nvram_set(strcat_r(prefix2, "regrev", tmp2), "5");
					commit_needed++;
				}
#endif
			}
#if 0
			dbG("maxp2ga0: %s\n", nvram_get(strcat_r(prefix2, "maxp2ga0", tmp2)) ? nvram_get(strcat_r(prefix2, "maxp2ga0", tmp2)) : "NULL");
			dbG("maxp2ga1: %s\n", nvram_get(strcat_r(prefix2, "maxp2ga1", tmp2)) ? nvram_get(strcat_r(prefix2, "maxp2ga1", tmp2)) : "NULL");
			dbG("maxp2ga2: %s\n", nvram_get(strcat_r(prefix2, "maxp2ga2", tmp2)) ? nvram_get(strcat_r(prefix2, "maxp2ga2", tmp2)) : "NULL");
			dbG("unit: %d, country code: %s, regrev: %s\n", unit, nvram_safe_get(strcat_r(prefix, "country_code", tmp)), nvram_get(strcat_r(prefix2, "regrev", tmp2)));
#endif
		}
		else if (nvram_match(strcat_r(prefix, "nband", tmp), "1"))
		{
			if (txpower == 80)
			{
				if (set_wltxpwoer_once)
				{
#if 0
					if (nvram_get(strcat_r(prefix2, "maxp5ga0", tmp2)))
					{
						nvram_unset(strcat_r(prefix2, "maxp5ga0", tmp2));
						commit_needed++;
					}
					if (nvram_get(strcat_r(prefix2, "maxp5ga1", tmp2)))
					{
						nvram_unset(strcat_r(prefix2, "maxp5ga1", tmp2));
						commit_needed++;
					}
					if (nvram_get(strcat_r(prefix2, "maxp5ga2", tmp2)))
					{
						nvram_unset(strcat_r(prefix2, "maxp5ga2", tmp2));
						commit_needed++;
					}
					if (nvram_get(strcat_r(prefix2, "maxp5gha0", tmp2)))
					{
						nvram_unset(strcat_r(prefix2, "maxp5gha0", tmp2));
						commit_needed++;
					}
					if (nvram_get(strcat_r(prefix2, "maxp5gha1", tmp2)))
					{
						nvram_unset(strcat_r(prefix2, "maxp5gha1", tmp2));
						commit_needed++;
					}
					if (nvram_get(strcat_r(prefix2, "maxp5gha2", tmp2)))
					{
						nvram_unset(strcat_r(prefix2, "maxp5gha2", tmp2));
						commit_needed++;
					}
#else
					if ((blver1 >= '1') && (blver2 >= '0') && (blver3 >= '1') && (blver4 >= '0'))
					{
						if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "0x5e"))
						{
							nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2), "0x5e");
							nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2), "0x5e");
							nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2), "0x5e");
							nvram_set(strcat_r(prefix2, "maxp5gha0", tmp2), "0x5e");
							nvram_set(strcat_r(prefix2, "maxp5gha1", tmp2), "0x5e");
							nvram_set(strcat_r(prefix2, "maxp5gha2", tmp2), "0x5e");
							commit_needed++;
						}
					}
					else
					{
						if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "0x50"))
						{
							nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2), "0x50");
							nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2), "0x50");
							nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2), "0x50");
							nvram_set(strcat_r(prefix2, "maxp5gha0", tmp2), "0x50");
							nvram_set(strcat_r(prefix2, "maxp5gha1", tmp2), "0x50");
							nvram_set(strcat_r(prefix2, "maxp5gha2", tmp2), "0x50");
							commit_needed++;
						}
					}
#endif
				}
#ifndef MEDIA_REVIEW
				if (nvram_match(strcat_r(prefix, "country_code", tmp), "Q2")
					&& nvram_match(strcat_r(prefix2, "regrev", tmp2), "0"))
				{
					nvram_set(strcat_r(prefix2, "regrev", tmp2), "2");
					commit_needed++;
				}
				else if (nvram_match(strcat_r(prefix, "country_code", tmp), "EU")
					&& nvram_match(strcat_r(prefix2, "regrev", tmp2), "3"))
				{
					nvram_set(strcat_r(prefix2, "regrev", tmp2), "0");
					commit_needed++;
				}
#endif
			}
			else
			{
				if ((blver1 >= '1') && (blver2 >= '0') && (blver3 >= '1') && (blver4 >= '0'))
				{
					if (txpower < 30)
						maxpwr = 70;
					else if (txpower < 50)
						maxpwr = 78;
					else if (txpower < 81)
						maxpwr = 86;
					else
						maxpwr = 94;
				}
				else
				{
					if (txpower < 30)
						offset = -6.5;
					else if (txpower < 50)
						offset = -3.5;
					else if (txpower < 81)
						offset = -2.0;
					else if (txpower < 111)
						offset = 0.5;
					else if (txpower < 141)
						offset = 1.0;
					else if (txpower < 171)
						offset = 1.5;
					else if (txpower < 201)
						offset = 2.0;
					else if (txpower < 231)
						offset = 2.5;
					else
						offset = 3.0;

					maxpwr = 4 * (20 + offset);
				}

				sprintf(tmp, "0x%x", maxpwr);

//				if ((blver1 >= '1') && (blver2 >= '0') && (blver3 >= '1') && (blver4 >= '0'))
				{
					if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), tmp))
					{
						nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2), tmp);
						commit_needed++;
					}
					if (!nvram_match(strcat_r(prefix2, "maxp5ga1", tmp2), tmp))
					{
						nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2), tmp);
						commit_needed++;
					}
					if (!nvram_match(strcat_r(prefix2, "maxp5ga2", tmp2), tmp))
					{
						nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2), tmp);
						commit_needed++;
					}
					if (!nvram_match(strcat_r(prefix2, "maxp5gha0", tmp2), tmp))
					{
						nvram_set(strcat_r(prefix2, "maxp5gha0", tmp2), tmp);
						commit_needed++;
					}
					if (!nvram_match(strcat_r(prefix2, "maxp5gha1", tmp2), tmp))
					{
						nvram_set(strcat_r(prefix2, "maxp5gha1", tmp2), tmp);
						commit_needed++;
					}
					if (!nvram_match(strcat_r(prefix2, "maxp5gha2", tmp2), tmp))
					{
						nvram_set(strcat_r(prefix2, "maxp5gha2", tmp2), tmp);
						commit_needed++;
					}
				}

#ifndef MEDIA_REVIEW
				if (nvram_match(strcat_r(prefix, "country_code", tmp), "Q2")
					&& nvram_match(strcat_r(prefix2, "regrev", tmp2), "2"))
				{
					nvram_set(strcat_r(prefix2, "regrev", tmp2), "0");
					commit_needed++;
				}
				else if (nvram_match(strcat_r(prefix, "country_code", tmp), "EU")
					&& nvram_match(strcat_r(prefix2, "regrev", tmp2), "0"))
				{
					nvram_set(strcat_r(prefix2, "regrev", tmp2), "3");
					commit_needed++;
				}
#endif
			}
#if 0
			dbG("maxp5ga0: %s\n", nvram_get(strcat_r(prefix2, "maxp5ga0", tmp2)) ? nvram_get(strcat_r(prefix2, "maxp5ga0", tmp2)) : "NULL");
			dbG("maxp5ga1: %s\n", nvram_get(strcat_r(prefix2, "maxp5ga1", tmp2)) ? nvram_get(strcat_r(prefix2, "maxp5ga1", tmp2)) : "NULL");
			dbG("maxp5ga2: %s\n", nvram_get(strcat_r(prefix2, "maxp5ga2", tmp2)) ? nvram_get(strcat_r(prefix2, "maxp5ga1", tmp2)) : "NULL");
			dbG("maxp5gha0: %s\n", nvram_get(strcat_r(prefix2, "maxp5gha0", tmp2)) ? nvram_get(strcat_r(prefix2, "maxp5gha0", tmp2)) : "NULL");
			dbG("maxp5gha1: %s\n", nvram_get(strcat_r(prefix2, "maxp5gha1", tmp2)) ? nvram_get(strcat_r(prefix2, "maxp5gha1", tmp2)) : "NULL");
			dbG("maxp5gha2: %s\n", nvram_get(strcat_r(prefix2, "maxp5gha2", tmp2)) ? nvram_get(strcat_r(prefix2, "maxp5gha2", tmp2)) : "NULL");
			dbG("unit: %d, country code: %s, regrev: %s\n", unit, nvram_safe_get(strcat_r(prefix, "country_code", tmp)), nvram_get(strcat_r(prefix2, "regrev", tmp2)));
#endif
		}

		i++;
	}

	if (!set_wltxpwoer_once)
		set_wltxpwoer_once = 1;

	if (commit_needed)
		nvram_commit();

	return 0;
}

#ifdef RTCONFIG_WIRELESSREPEATER
// this function is used to jutisfy which band(unit) to be forced connected.
int is_ure(int unit)
{
	// forced to connect to which band
	// is it suitable 
	if(nvram_get_int("sw_mode")==SW_MODE_REPEATER) {
		if(nvram_get_int("wlc_band")==unit) return 1;
	}
	return 0;
}
#endif

void generate_wl_para(int unit, int subunit)
{
	dbG("unit %d subunit %d\n", unit, subunit);

	char tmp[100], prefix[]="wlXXXXXXX_";
	char tmp2[100], prefix2[]="wlXXXXXXX_";
	char list[640];
	char *nv, *nvp, *b;
	char word[32], *next;
	int match;
	int wps_band;

	if (subunit == -1)
	{
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
#if 0
		if (unit == nvram_get_int("wps_band") && nvram_match("wps_enable", "1"))
#else
                if (nvram_match("wps_enable", "1"))
#endif
                        nvram_set(strcat_r(prefix, "wps_mode", tmp), "enabled");
		else
			nvram_set(strcat_r(prefix, "wps_mode", tmp), "disabled");
	}
	else
	{
		snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
		snprintf(prefix2, sizeof(prefix2), "wl%d_", unit);
	}

#ifdef RTCONFIG_WIRELESSREPEATER
	// convert wlc_xxx to wlX_ according to wlc_band == unit
	if (is_ure(unit)) {
		if(subunit==-1) {
			nvram_set("ure_disable", "0");

			nvram_set(strcat_r(prefix, "ssid", tmp), nvram_safe_get("wlc_ssid"));
			nvram_set(strcat_r(prefix, "auth_mode_x", tmp), nvram_safe_get("wlc_auth_mode"));
	
			nvram_set(strcat_r(prefix, "wep_x", tmp), nvram_safe_get("wlc_wep"));

			nvram_set(strcat_r(prefix, "key", tmp), nvram_safe_get("wlc_key"));

			nvram_set(strcat_r(prefix, "key1", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "key2", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "key3", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "key4", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "crypto", tmp), nvram_safe_get("wlc_crypto"));
			nvram_set(strcat_r(prefix, "wpa_psk", tmp), nvram_safe_get("wlc_wpa_psk"));
			nvram_set(strcat_r(prefix, "bw", tmp), nvram_safe_get("wlc_nbw_cap"));
		}
		else if(subunit==1) {	
			nvram_set(strcat_r(prefix, "bss_enabled", tmp), "1");	
			//nvram_set(strcat_r(prefix, "hwaddr", tmp), nvram_safe_get("et0macaddr"));
/*
			nvram_set(strcat_r(prefix, "ssid", tmp), nvram_safe_get("wlc_ure_ssid"));
			nvram_set(strcat_r(prefix, "auth_mode_x", tmp), nvram_safe_get("wlc_auth_mode"));	
			nvram_set(strcat_r(prefix, "wep_x", tmp), nvram_safe_get("wlc_wep"));
			nvram_set(strcat_r(prefix, "key", tmp), nvram_safe_get("wlc_key"));
			nvram_set(strcat_r(prefix, "key1", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "key2", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "key3", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "key4", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "crypto", tmp), nvram_safe_get("wlc_crypto"));
			nvram_set(strcat_r(prefix, "wpa_psk", tmp), nvram_safe_get("wlc_wpa_psk"));
			nvram_set(strcat_r(prefix, "bw", tmp), nvram_safe_get("wlc_nbw_cap"));
*/
		}
	}	
	//TODO: recover nvram from repeater
#endif
	if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "shared"))
	{
		nvram_set("wps_enable", "0");
		nvram_set(strcat_r(prefix, "auth", tmp), "1");
	}
	else nvram_set(strcat_r(prefix, "auth", tmp), "0");

	if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "psk"))
		nvram_set(strcat_r(prefix, "akm", tmp), "psk");
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "psk2"))
		nvram_set(strcat_r(prefix, "akm", tmp), "psk2");
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "pskpsk2"))
		nvram_set(strcat_r(prefix, "akm", tmp), "psk psk2");
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "wpa"))
	{
		nvram_set(strcat_r(prefix, "akm", tmp), "wpa");
		nvram_set("wps_enable", "0");
	}
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "wpa2"))
	{
		nvram_set(strcat_r(prefix, "akm", tmp), "wpa2");
		nvram_set("wps_enable", "0");
	}
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "wpawpa2"))
	{
		nvram_set(strcat_r(prefix, "akm", tmp), "wpa wpa2");
		nvram_set("wps_enable", "0");
	}
	else nvram_set(strcat_r(prefix, "akm", tmp), "");

	if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "radius")) 
	{
		nvram_set(strcat_r(prefix, "auth_mode", tmp), "radius");
		nvram_set(strcat_r(prefix, "key", tmp), "2");
		nvram_set("wps_enable", "0");
	}
	else nvram_set(strcat_r(prefix, "auth_mode", tmp), "none");

	if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "radius"))
		nvram_set(strcat_r(prefix, "wep", tmp), "enabled");
	else if (!nvram_match(strcat_r(prefix, "akm", tmp), ""))
		nvram_set(strcat_r(prefix, "wep", tmp), "disabled");
	else if (!nvram_match(strcat_r(prefix, "wep_x", tmp), "0"))
		nvram_set(strcat_r(prefix, "wep", tmp), "enabled");
	else nvram_set(strcat_r(prefix, "wep", tmp), "disabled");

	if (nvram_match(strcat_r(prefix, "mode", tmp), "ap") &&
		strstr(strcat_r(prefix, "akm", tmp), "wpa"))
		nvram_set(strcat_r(prefix, "preauth", tmp), "1");
	else
		nvram_set(strcat_r(prefix, "preauth", tmp), "");

	if (!nvram_match(strcat_r(prefix, "macmode", tmp), "disabled")) {
		list[0] = 0;
		nv = nvp = strdup(nvram_safe_get(strcat_r(prefix, "maclist_x", tmp)));
		if (nv) {
			while ((b = strsep(&nvp, "<")) != NULL) {
				if (strlen(b)==0) continue;
				if (list[0]==0)
					sprintf(list, "%s", b);
				else
					sprintf(list, "%s %s", list, b);
			}
			free(nv);
		}
		nvram_set(strcat_r(prefix, "maclist", tmp), list);
	}
	else nvram_set(strcat_r(prefix, "maclist", tmp), "");

	if (subunit==-1)
	{
		// wds mode control
#ifdef RTCONFIG_WIRELESSREPEATER
		if (is_ure(unit) && subunit==-1) nvram_set(strcat_r(prefix, "mode", tmp), "wet");
		else
#endif
		if (nvram_match(strcat_r(prefix, "mode_x", tmp), "1")) // wds only
			nvram_set(strcat_r(prefix, "mode", tmp), "wds");

		else if (nvram_match(strcat_r(prefix, "mode_x", tmp), "3")) // special defined for client
			nvram_set(strcat_r(prefix, "mode", tmp), "wet");
		else nvram_set(strcat_r(prefix, "mode", tmp), "ap");

		// TODO use lazwds directly
		//if(!nvram_match(strcat_r(prefix, "wdsmode_ex", tmp), "2"))
		//	nvram_set(strcat_r(prefix, "lazywds", tmp), "1");
		//else nvram_set(strcat_r(prefix, "lazywds", tmp), "0");

		// TODO need sw_mode ?
		// handle wireless wds list
		if (!nvram_match(strcat_r(prefix, "mode_x", tmp), "0")) {
			if (nvram_match(strcat_r(prefix, "wdsapply_x", tmp), "1")) {
				list[0] = 0;
				nv = nvp = strdup(nvram_safe_get(strcat_r(prefix, "wdslist", tmp)));
				if (nv) {
					while ((b = strsep(&nvp, "<")) != NULL) {
						if (strlen(b)==0) continue;
						if (list[0]==0) 
							sprintf(list, "%s", b);
						else
							sprintf(list, "%s %s", list, b);
					}
					free(nv);
				}
				nvram_set(strcat_r(prefix, "wds", tmp), list);
				nvram_set(strcat_r(prefix, "lazywds", tmp), "0");
			}
			else {
				nvram_set(strcat_r(prefix, "wds", tmp), "");
				nvram_set(strcat_r(prefix, "lazywds", tmp), "1");
			}
		} else {
			nvram_set(strcat_r(prefix, "wds", tmp), "");
			nvram_set(strcat_r(prefix, "lazywds", tmp), "0");
		}

		if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "0"))		// disable, but bcm says "auto"
			nvram_set(strcat_r(prefix, "mrate", tmp), "0");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "1"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "1000000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "2"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "2000000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "3"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "5500000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "4"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "6000000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "5"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "9000000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "6"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "11000000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "7"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "12000000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "8"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "18000000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "9"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "24000000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "10"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "36000000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "11"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "48000000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "12"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "54000000");
		else
			nvram_set(strcat_r(prefix, "mrate", tmp), "0");

		if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "0"))		// auto => b/g/n mixed or a/n mixed
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-1");	// auto rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "-1");
			nvram_set(strcat_r(prefix, "nreqd", tmp), "0");
			nvram_set(strcat_r(prefix, "gmode", tmp), "1");
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
		}
		else if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "1"))	// n only
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-1");	// auto rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "-1");
			nvram_set(strcat_r(prefix, "nreqd", tmp), "1");
			nvram_set(strcat_r(prefix, "gmode", tmp), "1");
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
		}
#if 0
		else if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "2"))	// b/g mixed
#else
		else								// b/g mixed
#endif
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-2");	// legacy rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "0");
			nvram_set(strcat_r(prefix, "nreqd", tmp), "0");
			nvram_set(strcat_r(prefix, "gmode", tmp), "1");
		}

		if (nvram_match(strcat_r(prefix, "bw", tmp), "1"))
		{
			nvram_set(strcat_r(prefix, "nbw_cap", tmp), "1");
			nvram_set(strcat_r(prefix, "obss_coex", tmp), "1");
		}
		else if (nvram_match(strcat_r(prefix, "bw", tmp), "2"))
		{
			nvram_set(strcat_r(prefix, "nbw_cap", tmp), "1");
			nvram_set(strcat_r(prefix, "obss_coex", tmp), "0");
		}
		else
		{
			nvram_set(strcat_r(prefix, "nbw_cap", tmp), "0");
			nvram_set(strcat_r(prefix, "obss_coex", tmp), "1");
		}

		if (unit) chanspec_fix_5g(unit);

		match = 0;
		foreach (word, "lower upper", next)
		{
			if (nvram_match(strcat_r(prefix, "nctrlsb", tmp), word))
			{
				match = 1;
				break;
			}
		}

/*
		if ((nvram_match(strcat_r(prefix, "channel", tmp), "0")))
			nvram_unset(strcat_r(prefix, "nctrlsb", tmp));
		else
*/
		if (!match)
			nvram_set(strcat_r(prefix, "nctrlsb", tmp), "lower");

		dbG("bw: %s\n", nvram_safe_get(strcat_r(prefix, "bw", tmp)));
		dbG("channel: %s\n", nvram_safe_get(strcat_r(prefix, "channel", tmp)));
		dbG("nbw_cap: %s\n", nvram_safe_get(strcat_r(prefix, "nbw_cap", tmp)));
		dbG("nctrlsb: %s\n", nvram_safe_get(strcat_r(prefix, "nctrlsb", tmp)));
		dbG("obss_coex: %s\n", nvram_safe_get(strcat_r(prefix, "obss_coex", tmp)));
	}
	else
	{
		if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1"))
		{
			nvram_set(strcat_r(prefix, "bss_maxassoc", tmp), nvram_safe_get(strcat_r(prefix2, "bss_maxassoc", tmp2)));
			nvram_set(strcat_r(prefix, "ap_isolate", tmp), nvram_safe_get(strcat_r(prefix2, "ap_isolate", tmp2)));
			nvram_set(strcat_r(prefix, "net_reauth", tmp), nvram_safe_get(strcat_r(prefix2, "net_reauth", tmp2)));
			nvram_set(strcat_r(prefix, "radius_ipaddr", tmp), nvram_safe_get(strcat_r(prefix2, "radius_ipaddr", tmp2)));
			nvram_set(strcat_r(prefix, "radius_key", tmp), nvram_safe_get(strcat_r(prefix2, "radius_key", tmp2)));
			nvram_set(strcat_r(prefix, "radius_port", tmp), nvram_safe_get(strcat_r(prefix2, "radius_port", tmp2)));
			nvram_set(strcat_r(prefix, "wme", tmp), nvram_safe_get(strcat_r(prefix2, "wme", tmp2)));
			nvram_set(strcat_r(prefix, "wme_bss_disable", tmp), nvram_safe_get(strcat_r(prefix2, "wme_bss_disable", tmp2)));
			nvram_set(strcat_r(prefix, "wpa_gtk_rekey", tmp), nvram_safe_get(strcat_r(prefix2, "wpa_gtk_rekey", tmp2)));
			nvram_set(strcat_r(prefix, "wmf_bss_enable", tmp), nvram_safe_get(strcat_r(prefix2, "wmf_bss_enable", tmp2)));

			if (!nvram_get(strcat_r(prefix, "lanaccess", tmp)))
				nvram_set(strcat_r(prefix, "lanaccess", tmp), "off");

			if (!nvram_get(strcat_r(prefix, "expire", tmp)))
				nvram_set(strcat_r(prefix, "expire", tmp), "0");
			else
			{
				snprintf(prefix2, sizeof(prefix2), "wl%d.%d_", unit, subunit);
				nvram_set(strcat_r(prefix, "expire_tmp", tmp), nvram_safe_get(strcat_r(prefix2, "expire", tmp2)));
			}
		}
		else
		{
			nvram_set(strcat_r(prefix, "expire", tmp), "0");
			nvram_set(strcat_r(prefix, "expire_tmp", tmp), "0");
		}
	}

	/* Disable nmode for WEP and TKIP for TGN spec */
	if (nvram_match(strcat_r(prefix, "wep", tmp), "enabled") ||
		nvram_match(strcat_r(prefix, "crypto", tmp), "tkip"))
	{
		nvram_set(strcat_r(prefix, "nmode", tmp), "0");
#if 0
		if (subunit != -1)
		{
			snprintf(prefix, sizeof(prefix), "wl%d_", unit);
			nvram_set(strcat_r(prefix, "nmode", tmp), "0");
		}
#endif
	}
	else
	{
		nvram_set(strcat_r(prefix, "nmode", tmp), "-1");
#if 0
		if (subunit != -1)
		{
			snprintf(prefix, sizeof(prefix), "wl%d_", unit);
			nvram_set(strcat_r(prefix, "nmode", tmp), "-1");
		}
#endif
	}
}

void
set_wan_tag(char *interface) {
	int model, wan_vid, iptv_vid, voip_vid, wan_prio, iptv_prio, voip_prio;
	char wan_dev[10], port_id[7], tag_register[7], vlan_entry[7];
	
        model = get_model();
        wan_vid = nvram_get_int("switch_wan0tagid");
        iptv_vid = nvram_get_int("switch_wan1tagid");
        voip_vid = nvram_get_int("switch_wan2tagid");
        wan_prio = nvram_get_int("switch_wan0prio");
        iptv_prio = nvram_get_int("switch_wan1prio");
        voip_prio = nvram_get_int("switch_wan2prio");

	sprintf(wan_dev, "vlan%d", wan_vid);
	
        switch(model) {
        case MODEL_RTN16:
                eval("vconfig", "rem", "vlan2");
        	//config wan port
        	sprintf(port_id, "%d", wan_vid);
        	eval("vconfig", "add", interface, port_id);
        	//wan_prio = wan_prio << 13;
        	//sprintf(tag_register, "0x%x", (wan_prio | wan_vid));
        	//eval("et", "robowr", "0x34", "0x10", tag_register);
        	//Set vlan table entry (port id = vlan entry id)
        	sprintf(vlan_entry, "0x%x", wan_vid);
        	eval("et", "robowr", "0x05", "0x83", "0x0101");
        	eval("et", "robowr", "0x05", "0x81", vlan_entry);
        	eval("et", "robowr", "0x05", "0x80", "0x0000");
        	eval("et", "robowr", "0x05", "0x80", "0x0080");
        	
		//Set Wan port PRIO
		if(nvram_invmatch("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if(nvram_match("switch_stb_x", "3")) {
			//config LAN 3 = VoIP
			if(nvram_match("switch_wantag", "m1_fiber")) {
                                //Just forward packets between port 0 & 3, without untag
                                sprintf(vlan_entry, "0x%x", voip_vid);
                                _dprintf("vlan entry: %s\n", vlan_entry);
                                eval("et", "robowr", "0x05", "0x83", "0x0005");
                                eval("et", "robowr", "0x05", "0x81", vlan_entry);
                                eval("et", "robowr", "0x05", "0x80", "0x0000");
                                eval("et", "robowr", "0x05", "0x80", "0x0080");
                        }
                        else { //Nomo case, untag it.
                                voip_prio = voip_prio << 13;
                                sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
                                eval("et", "robowr", "0x34", "0x14", tag_register);
                                _dprintf("lan 3 tag register: %s\n", tag_register);
                                //Set vlan table entry register
                                sprintf(vlan_entry, "0x%x", voip_vid);
                                _dprintf("vlan entry: %s\n", vlan_entry);
                                eval("et", "robowr", "0x05", "0x83", "0x0805");
                                eval("et", "robowr", "0x05", "0x81", vlan_entry);
                                eval("et", "robowr", "0x05", "0x80", "0x0000");
                                eval("et", "robowr", "0x05", "0x80", "0x0080");
                        }
		}
                else if(nvram_match("switch_stb_x", "4")) {
                        //config LAN 4 = IPTV
                        iptv_prio = iptv_prio << 13;
                        sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
                        eval("et", "robowr", "0x34", "0x12", tag_register);
                        _dprintf("lan 4 tag register: %s\n", tag_register);
        		//Set vlan table entry register
                        sprintf(vlan_entry, "0x%x", iptv_vid);
                        _dprintf("vlan entry: %s\n", vlan_entry);
                        eval("et", "robowr", "0x05", "0x83", "0x0403");
                        eval("et", "robowr", "0x05", "0x81", vlan_entry);
                        eval("et", "robowr", "0x05", "0x80", "0x0000");
                        eval("et", "robowr", "0x05", "0x80", "0x0080");
                }                               
                else if(nvram_match("switch_stb_x", "6")) {
                        //config LAN 3 = VoIP
			if(nvram_match("switch_wantag", "singtel_mio")) {
				//Just forward packets between port 0 & 3, without untag
	                        sprintf(vlan_entry, "0x%x", voip_vid);
        	                _dprintf("vlan entry: %s\n", vlan_entry);
                	        eval("et", "robowr", "0x05", "0x83", "0x0005");
                        	eval("et", "robowr", "0x05", "0x81", vlan_entry);
	                        eval("et", "robowr", "0x05", "0x80", "0x0000");
        	                eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else { //Nomo case, untag it.
        			voip_prio = voip_prio << 13;
        			sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
	        		eval("et", "robowr", "0x34", "0x14", tag_register);
        			_dprintf("lan 3 tag register: %s\n", tag_register);
        			//Set vlan table entry register
        			sprintf(vlan_entry, "0x%x", voip_vid);
	        		_dprintf("vlan entry: %s\n", vlan_entry);
        			eval("et", "robowr", "0x05", "0x83", "0x0805");
        			eval("et", "robowr", "0x05", "0x81", vlan_entry);
                        	eval("et", "robowr", "0x05", "0x80", "0x0000");
	                        eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
                        //config LAN 4 = IPTV
        		iptv_prio = iptv_prio << 13;
        		sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
        		eval("et", "robowr", "0x34", "0x12", tag_register);
        		_dprintf("lan 4 tag register: %s\n", tag_register);
        		//Set vlan table entry register
        		sprintf(vlan_entry, "0x%x", iptv_vid);
        		_dprintf("vlan entry: %s\n", vlan_entry);
        		eval("et", "robowr", "0x05", "0x83", "0x0403");
        		eval("et", "robowr", "0x05", "0x81", vlan_entry);
                        eval("et", "robowr", "0x05", "0x80", "0x0000");
                        eval("et", "robowr", "0x05", "0x80", "0x0080");
                }
                break;
                
        case MODEL_RTN66U:
                eval("vconfig", "rem", "vlan2");
        	//config wan port
        	sprintf(port_id, "%d", wan_vid);
        	eval("vconfig", "add", interface, port_id);
        	//wan_prio = wan_prio << 13;
        	//sprintf(tag_register, "0x%x", (wan_prio | wan_vid));
        	//eval("et", "robowr", "0x34", "0x10", tag_register);
        	//Set vlan table entry (port id = vlan entry id)
        	sprintf(vlan_entry, "0x%x", wan_vid);
        	eval("et", "robowr", "0x05", "0x83", "0x0101");
        	eval("et", "robowr", "0x05", "0x81", vlan_entry);
        	eval("et", "robowr", "0x05", "0x80", "0x0000");
        	eval("et", "robowr", "0x05", "0x80", "0x0080");
		//Set Wan port PRIO
		if(nvram_invmatch("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if(nvram_match("switch_stb_x", "3")) {
			if(nvram_match("switch_wantag", "m1_fiber")) {
                                //Just forward packets between port 0 & 3, without untag
                                sprintf(vlan_entry, "0x%x", voip_vid);
                                _dprintf("vlan entry: %s\n", vlan_entry);
                                eval("et", "robowr", "0x05", "0x83", "0x0009");
                                eval("et", "robowr", "0x05", "0x81", vlan_entry);
                                eval("et", "robowr", "0x05", "0x80", "0x0000");
                                eval("et", "robowr", "0x05", "0x80", "0x0080");
                        }
                        else { //Nomo case, untag it.
                                voip_prio = voip_prio << 13;
                                sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
                                eval("et", "robowr", "0x34", "0x16", tag_register);
                                _dprintf("lan 3 tag register: %s\n", tag_register);
                                //Set vlan table entry register
                                sprintf(vlan_entry, "0x%x", voip_vid);
                                _dprintf("vlan entry: %s\n", vlan_entry);
                                eval("et", "robowr", "0x05", "0x83", "0x1009");
                                eval("et", "robowr", "0x05", "0x81", vlan_entry);
                                eval("et", "robowr", "0x05", "0x80", "0x0000");
                                eval("et", "robowr", "0x05", "0x80", "0x0080");
                        }
		}
                else if(nvram_match("switch_stb_x", "4")) {
                        //config LAN 4 = IPTV
                        iptv_prio = iptv_prio << 13;
                        sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
                        eval("et", "robowr", "0x34", "0x18", tag_register);
                        _dprintf("lan 4 tag register: %s\n", tag_register);
        		//Set vlan table entry register
                        sprintf(vlan_entry, "0x%x", iptv_vid);
                        _dprintf("vlan entry: %s\n", vlan_entry);
                        eval("et", "robowr", "0x05", "0x83", "0x2011");
                        eval("et", "robowr", "0x05", "0x81", vlan_entry);
                        eval("et", "robowr", "0x05", "0x80", "0x0000");
                        eval("et", "robowr", "0x05", "0x80", "0x0080");
                }
                else if(nvram_match("switch_stb_x", "6")) {
                        //config LAN 3 = VoIP
			if(nvram_match("switch_wantag", "singtel_mio")) {
				//Just forward packets between port 0 & 3, without untag
	                        sprintf(vlan_entry, "0x%x", voip_vid);
        	                _dprintf("vlan entry: %s\n", vlan_entry);
                	        eval("et", "robowr", "0x05", "0x83", "0x0009");
                        	eval("et", "robowr", "0x05", "0x81", vlan_entry);
	                        eval("et", "robowr", "0x05", "0x80", "0x0000");
        	                eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else { //Nomo case, untag it.
        			voip_prio = voip_prio << 13;
        			sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
	        		eval("et", "robowr", "0x34", "0x16", tag_register);
        			_dprintf("lan 3 tag register: %s\n", tag_register);
        			//Set vlan table entry register
        			sprintf(vlan_entry, "0x%x", voip_vid);
	        		_dprintf("vlan entry: %s\n", vlan_entry);
        			eval("et", "robowr", "0x05", "0x83", "0x1009");
        			eval("et", "robowr", "0x05", "0x81", vlan_entry);
                        	eval("et", "robowr", "0x05", "0x80", "0x0000");
	                        eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
                        //config LAN 4 = IPTV
        		iptv_prio = iptv_prio << 13;
        		sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
        		eval("et", "robowr", "0x34", "0x18", tag_register);
        		_dprintf("lan 4 tag register: %s\n", tag_register);
        		//Set vlan table entry register
        		sprintf(vlan_entry, "0x%x", iptv_vid);
        		_dprintf("vlan entry: %s\n", vlan_entry);
        		eval("et", "robowr", "0x05", "0x83", "0x2011");
        		eval("et", "robowr", "0x05", "0x81", vlan_entry);
                        eval("et", "robowr", "0x05", "0x80", "0x0000");
                        eval("et", "robowr", "0x05", "0x80", "0x0080");
                }
                break;
	}
	return;
}

char *get_wlifname(int unit, int subunit, int subunit_x, char *buf)
{
	sprintf(buf, "wl%d.%d", unit, subunit);

	return buf;
}

int
wl_exist(char *ifname, int band)
{
        char buf[128];
        int ret = 0;
        memset(buf, 0, 128);
        sprintf(buf, "wl -i %s status &> /tmp/wl_check", ifname);
        system(buf);
        FILE *fp;
        if( (fp = fopen("/tmp/wl_check", "r")) != NULL ) {
                while(fgets(buf, sizeof(buf), fp)) {
                        if( band==1 ) { //it should be 2G
                                if( strstr(buf, "Chanspec: 2.4GHz") ) {
                                        ret = 1;
                                        break;
                                }
                        }
                        else if( band==2 ) { //it should be 5G
                                if( strstr(buf, "Chanspec: 5GHz") ) {
                                        ret = 1;
                                        break;
                                }
                        }

                }
                fclose(fp);
                system("rm -rf /tmp/wl_check");
        }
        return ret;
}

