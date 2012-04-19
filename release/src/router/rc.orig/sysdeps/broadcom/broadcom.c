/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <stdio.h>
#include <bcmnvram.h>
#include <net/if_arp.h>
#include <shutils.h>
#include <sys/signal.h>
#include <semaphore_mfp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <etioctl.h>
#include <rc.h>
typedef u_int64_t __u64;
typedef u_int32_t __u32;
typedef u_int16_t __u16;
typedef u_int8_t __u8;
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <ctype.h>

//This define only used for switch 53125
#define SWITCH_PORT_0_UP	0x0001
#define SWITCH_PORT_1_UP	0x0002
#define SWITCH_PORT_2_UP        0x0004
#define SWITCH_PORT_3_UP        0x0008
#define SWITCH_PORT_4_UP        0x0010

#define SWITCH_PORT_0_GIGA	0x0002
#define SWITCH_PORT_1_GIGA      0x0008
#define SWITCH_PORT_2_GIGA      0x0020
#define SWITCH_PORT_3_GIGA      0x0080
#define SWITCH_PORT_4_GIGA      0x0200
//End

char cmd[32];

int 
setCommit(void)
{
        eval("nvram", "set", "asuscfecommit=");
        eval("nvram", "commit");
	puts("1");
	return 1;
}

int
setMAC_2G(const char *mac)
{
	if( mac==NULL || !isValidMacAddr(mac) )
		return 0;

        eval("killall", "wsc");
        memset(cmd, 0, 32);
        sprintf(cmd, "asuscfeet0macaddr=%s", mac);
        eval("nvram", "set", cmd );
        sprintf(cmd, "asuscfepci/1/1/macaddr=%s", mac);
	eval("nvram", "set", cmd );
        puts(nvram_safe_get("et0macaddr"));
        return 1;
}

int
setMAC_5G(const char *mac)
{
        if( mac==NULL || !isValidMacAddr(mac) )
                return 0;

        eval("killall", "wsc");
        memset(cmd, 0, 32);
        sprintf(cmd, "asuscfepci/2/1/macaddr=%s", mac);
        eval("nvram", "set", cmd );
        puts(nvram_safe_get("pci/2/1/macaddr"));
        return 1;
}

int
setCountryCode_2G(const char *cc)
{
	if( cc==NULL || !isValidCountryCode(cc) )
		return 0;

        eval("killall", "wsc");
        memset(cmd, 0, 32);
        sprintf(cmd, "asuscferegulation_domain=%s", cc);
        eval("nvram", "set", cmd );
        puts(nvram_safe_get("regulation_domain"));
        return 1;
}

int
setCountryCode_5G(const char *cc)
{
	if( cc==NULL || !isValidCountryCode(cc) )
                return 0;

        eval("killall", "wsc");
        memset(cmd, 0, 32);
        sprintf(cmd, "asuscferegulation_domain_5G=%s", cc);
        eval("nvram", "set", cmd );
        puts(nvram_safe_get("regulation_domain_5G"));
        return 1;
}

int
setRegrev_2G(const char *regrev)
{
	if( regrev==NULL || !isValidRegrev(regrev) )
		return 0;

        eval("killall", "wsc");
        memset(cmd, 0, 32);
        sprintf(cmd, "asuscfepci/1/1/regrev=%s", regrev);
        eval("nvram", "set", cmd );
	puts(nvram_safe_get("pci/1/1/regrev"));
	return 1;
}

int
setRegrev_5G(const char *regrev)
{
        if( regrev==NULL || !isValidRegrev(regrev) )
                return 0;

        eval("killall", "wsc");
        memset(cmd, 0, 32);
        sprintf(cmd, "asuscfepci/2/1/regrev=%s", regrev);
        eval("nvram", "set", cmd );
        puts(nvram_safe_get("pci/2/1/regrev"));
	return 1;
}

int
setPIN(const char *pin)
{
        if( pin==NULL ) 
		return 0;

	if( pincheck(pin) )
        {
                eval("killall", "wsc");
                memset(cmd, 0, 32);
                sprintf(cmd, "asuscfesecret_code=%s", pin);
                eval("nvram", "set", cmd );
                puts(nvram_safe_get("secret_code"));
                return 1;
	}
	else
		return 0; 
}

int
set40M_Channel_2G(char *channel)
{
        if( channel==NULL || !isValidChannel(1, channel) )
                return 0;

	nvram_set( "wl0_channel", channel);
	nvram_set( "wl0_nbw_cap", "1");
	nvram_set( "wl0_nctrlsb", "lower");
	nvram_set( "wl0_obss_coex", "0");
	eval("wlconf", "eth1", "down" );
	eval("wlconf", "eth1", "up" );
	eval("wlconf", "eth1", "start" );
	puts("1");
	return 1;
}

int
set40M_Channel_5G(char *channel)
{
        if( channel==NULL || !isValidChannel(0, channel) )
                return 0;

        nvram_set( "wl1_channel", channel);
        nvram_set( "wl1_nbw_cap", "1");
        nvram_set( "wl1_nctrlsb", "lower");
        nvram_set( "wl1_obss_coex", "0");
        eval( "wlconf", "eth2", "down" );
        eval( "wlconf", "eth2", "up" );
        eval( "wlconf", "eth2", "start" );
	puts("1");
        return 1;
}

int
ResetDefault(void) {
	eval("mtd-erase","-d","nvram");
        puts("1");
	return 0;
}

static void
syserr(char *s)
{
        perror(s);
        exit(1);
}

static int
et_check(int s, struct ifreq *ifr)
{
        struct ethtool_drvinfo info;

        memset(&info, 0, sizeof(info));
        info.cmd = ETHTOOL_GDRVINFO;
        ifr->ifr_data = (caddr_t)&info;
        if (ioctl(s, SIOCETHTOOL, (caddr_t)ifr) < 0) {
                /* print a good diagnostic if not superuser */
                if (errno == EPERM)
                        syserr("siocethtool");
                return (-1);
        }

        if (!strncmp(info.driver, "et", 2))
                return (0);
        else if (!strncmp(info.driver, "bcm57", 5))
                return (0);

        return (-1);
}

static void
et_find(int s, struct ifreq *ifr)
{
        char proc_net_dev[] = "/proc/net/dev";
        FILE *fp;
        char buf[512], *c, *name;

        ifr->ifr_name[0] = '\0';

        /* eat first two lines */
        if (!(fp = fopen(proc_net_dev, "r")) ||
            !fgets(buf, sizeof(buf), fp) ||
            !fgets(buf, sizeof(buf), fp))
                return;

        while (fgets(buf, sizeof(buf), fp)) {
                c = buf;
                while (isspace(*c))
                        c++;
                if (!(name = strsep(&c, ":")))
                        continue;
                strncpy(ifr->ifr_name, name, IFNAMSIZ);
                if (et_check(s, ifr) == 0)
                        break;
                ifr->ifr_name[0] = '\0';
        }

        fclose(fp);
}

int
GetPhyStatus(void)
{
        char switch_link_status[]="GGGGG";
        struct ifreq ifr;
        int vecarg[5];
        int s;
	char output[25];

        if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
                syserr("socket");

        et_find(s, &ifr);

        if (!*ifr.ifr_name) {
                 //printf("et interface not found\n");
                 return 0;
        }

	//Get Link Speed
        vecarg[0] = strtoul("0x1", NULL, 0) << 16;;
        vecarg[0] |= strtoul("0x4", NULL, 0) & 0xffff;

        ifr.ifr_data = (caddr_t) vecarg;
        if (ioctl(s, SIOCGETCROBORD, (caddr_t)&ifr) < 0)
                syserr("etcrobord");
        else {
                if( !(vecarg[1] & SWITCH_PORT_0_GIGA) )
                        memcpy(&switch_link_status[0], "M", 1);
                if( !(vecarg[1] & SWITCH_PORT_1_GIGA) )
                        memcpy(&switch_link_status[1], "M", 1);
                if( !(vecarg[1] & SWITCH_PORT_2_GIGA) )
                        memcpy(&switch_link_status[2], "M", 1);
                if( !(vecarg[1] & SWITCH_PORT_3_GIGA) )
                        memcpy(&switch_link_status[3], "M", 1);
                if( !(vecarg[1] & SWITCH_PORT_4_GIGA) )
                        memcpy(&switch_link_status[4], "M", 1);
        }

        //Get Link Status
        vecarg[0] = strtoul("0x1", NULL, 0) << 16;;
        vecarg[0] |= strtoul("0x0", NULL, 0) & 0xffff;

        ifr.ifr_data = (caddr_t) vecarg;
        if (ioctl(s, SIOCGETCROBORD, (caddr_t)&ifr) < 0)
                syserr("etcrobord");
        else {
                if( !(vecarg[1] & SWITCH_PORT_0_UP) )
                        memcpy(&switch_link_status[0], "X", 1);
                if( !(vecarg[1] & SWITCH_PORT_1_UP) )
                        memcpy(&switch_link_status[1], "X", 1);
                if( !(vecarg[1] & SWITCH_PORT_2_UP) )
                        memcpy(&switch_link_status[2], "X", 1);
                if( !(vecarg[1] & SWITCH_PORT_3_UP) )
                        memcpy(&switch_link_status[3], "X", 1);
                if( !(vecarg[1] & SWITCH_PORT_4_UP) )
                        memcpy(&switch_link_status[4], "X", 1);
        }

	sprintf(output, "W0=%c;L1=%c;L2=%c;L3=%c;L4=%c", switch_link_status[0],
		switch_link_status[1], switch_link_status[2],
		switch_link_status[3], switch_link_status[4]);
	puts(output);

        return 1;
}

int 
setAllLedOn(void)
{
	//LAN, WAN Led On
	eval("et", "robowr", "0", "0x18", "0x01ff");
	eval("et", "robowr", "0", "0x1a", "0x01e0");

	led_control(LED_WPS, LED_ON);
	led_control(LED_POWER, LED_ON);
	led_control(LED_WPS, LED_ON);
	led_control(LED_USB, LED_ON);
	eval("radio", "on");
	puts("1");
	return 0;
}

int 
setAllLedOff(void)
{
	//LAN, WAN Led Off
        eval("et", "robowr", "0", "0x18", "0x01e0");
        eval("et", "robowr", "0", "0x1a", "0x01e0");

	led_control(LED_WPS, LED_ON);
	led_control(LED_POWER, LED_OFF);
	led_control(LED_WPS, LED_OFF);
	led_control(LED_USB, LED_OFF);
	eval("radio","off");
	puts("1");
	return 0;
}

#ifdef RTCONFIG_FANCTRL
int
setFanOn(void)
{
	led_control(FAN, FAN_ON);
	if( button_pressed(BTN_FAN) )
		puts("1");
	else
		puts("ATE_ERROR");
}

int
setFanOff(void)
{
        led_control(FAN, FAN_OFF);
        if( !button_pressed(BTN_FAN) )
                puts("1");
        else
                puts("ATE_ERROR");
}
#endif

int
getMAC_2G() {
	puts(nvram_safe_get("et0macaddr"));
	return 0;
}

int
getMAC_5G() {
        puts(nvram_safe_get("pci/2/1/macaddr"));
        return 0;
}

int 
getBootVer(void) {
	puts(nvram_safe_get("bl_version"));
	return 0;
}

int 
getPIN(void) {
	puts(nvram_safe_get("secret_code"));
	return 0;
}

int
getCountryCode_2G(void) {
	puts(nvram_safe_get("regulation_domain"));
	return 0;
}

int
getCountryCode_5G(void) {
        puts(nvram_safe_get("regulation_domain_5G"));
	return 0;
}

int 
getRegrev_2G(void) {
	puts(nvram_safe_get("pci/1/1/regrev"));
	return 0;
}

int
getRegrev_5G(void) {
        puts(nvram_safe_get("pci/2/1/regrev"));
	return 0;
}

int
Get_USB_Port_Info(int port_x)
{
	char output_buf[16];
	char usb_pid[14];
	char usb_vid[14];
	sprintf(usb_pid, "usb_path%d_pid", port_x);
	sprintf(usb_vid, "usb_path%d_vid", port_x);

	if( strcmp(nvram_get(usb_pid),"") && strcmp(nvram_get(usb_vid),"") ) {
		sprintf(output_buf, "%s/%s",nvram_get(usb_pid),nvram_get(usb_vid));
		puts(output_buf);
	}
	else
		puts("N/A");

	return 1;
}

int
Get_USB_Port_Folder(int port_x)
{
        char usb_folder[19];
        sprintf(usb_folder, "usb_path%d_fs_path0", port_x);
	if( strcmp(nvram_safe_get(usb_folder),"") ) 
		puts(nvram_safe_get(usb_folder));
        else
                puts("N/A");

        return 1;
}

int
Get_SD_Card_Info(int port_x)
{
        char check_cmd[48];
	char sd_info_buf[128];
	int get_sd_card = 1;
	FILE *fp;

        sprintf(check_cmd, "test_disk2 %s &> /var/sd_info.txt", nvram_get("usb_path3_fs_path0"));
	system(check_cmd);

	if(fp = fopen("/var/sd_info.txt", "r")) {
		while(fgets(sd_info_buf, 128, fp)!=NULL) {
			if(strstr(sd_info_buf, "No partition")||strstr(sd_info_buf, "No disk"))
				get_sd_card=0;
		}
		if(get_sd_card)
			puts("1");
		else
			puts("0");
		fclose(fp);
		eval("rm", "-rf", "/var/sd_info.txt");
	}
	else
		puts("ATE_ERROR");

        return 1;
}

int
Get_SD_Card_Folder(void)
{
        if( strcmp(nvram_safe_get("usb_path3_fs_path0"),"") ) 
                puts(nvram_safe_get("usb_path3_fs_path0"));
        else
                puts("N/A");

        return 1;
}

