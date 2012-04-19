#include <rc.h>
#include <shutils.h>
#ifdef RTCONFIG_RALINK
#include <ralink.h>
#endif

#define MULTICAST_BIT  0x0001
#define UNIQUE_OUI_BIT 0x0002
int isValidMacAddr(const char* mac) {
        int sec_byte;
        int i = 0, s = 0;

	if( strlen(mac)!=17 || !strcmp("00:00:00:00:00:00", mac) )
		return 0;

        while( *mac && i<12 ) {
                if( isxdigit(*mac) ) {
                        if(i==1) {
                                sec_byte= strtol(mac, NULL, 16);
                                if((sec_byte & MULTICAST_BIT)||(sec_byte & UNIQUE_OUI_BIT))
                                        break;
                        }
                        i++;
                }
                else if( *mac==':') {
                        if( i==0 || i/2-1!=s )
                                break;
                        ++s;
                }
                ++mac;
        }
        return( i==12 && s==5 );
}

int
isValidCountryCode(char *Ccode)
{
	char *c = Ccode;
	int i = 0;

        if(strlen(Ccode)==2) {
		while(i<2) { //0~9, A~F
			if( (*c>0x2F && *c<0x3A) || (*c>0x40 && *c<0x5B) ) {
				i++;
				c++;
			}
			else
				break;
		}
	}
	if( i == 2 )
		return 1;
        else
                return 0;
}

int 
isValidRegrev(char *regrev) {
        char *c = regrev;
        int len, i = 0, ret=0;
	
	len = strlen(regrev);

	if( len==1 || len==2 ) {
                while(i<len) { //0~9
                        if( (*c>0x2F && *c<0x3A) ) {
                                i++;
                                c++;
				ret = 1;
                        }
			else {
				ret = 0;
				break;
			}
                }
        }

	return ret;
}

int
isValidChannel(int is_2G, char *channel) {
        char *c = channel;
        int len, i = 0, ret=0;

        len = strlen(channel);

        if( (is_2G && (len==1 || len==2))
	||  (!is_2G && (len==2 || len==3)) ) {
                while(i<len) { //0~9
                        if( (*c>0x2F && *c<0x3A) ) {
                                i++;
                                c++;
                                ret = 1;
                        }
                        else {
                                ret = 0;
                                break;
                        }
                }
        }

        return ret;
}

int
pincheck(const char *a)
{
        unsigned char *c = (char *) a;
        unsigned long int uiPINtemp = atoi(a);
        unsigned long int uiAccum = 0;
        int i = 0;

        for (;;) {
                if (*c>0x39 || *c<0x30)
                        break;
                else
                        i++;
                if (!*c++ || i == 8)
                        break;
        }
        if(i == 8) {
                uiAccum += 3 * ((uiPINtemp / 10000000) % 10);
                uiAccum += 1 * ((uiPINtemp / 1000000) % 10);
                uiAccum += 3 * ((uiPINtemp / 100000) % 10);
                uiAccum += 1 * ((uiPINtemp / 10000) % 10);
                uiAccum += 3 * ((uiPINtemp / 1000) % 10);
                uiAccum += 1 * ((uiPINtemp / 100) % 10);
                uiAccum += 3 * ((uiPINtemp / 10) % 10);
                uiAccum += 1 * ((uiPINtemp / 1) % 10);
                if (0 != (uiAccum % 10)){
                        return 0;
                }
                return 1;
        }
        else
                return 0;
}

int asus_ate_command(const char *command, const char *value, const char *value2){
	_dprintf("===[ATE %s %s]===\n", command, value);
	if(!strcmp(command, "Set_StartATEMode")) {
                nvram_set("asus_mfg", "1");
                if(nvram_match("asus_mfg", "1")) {
			puts("1");
#ifdef RTCONFIG_FANCTRL
			stop_phy_tempsense();
#endif
#ifdef TODO
	                stop_wsc();
#endif
        	        stop_lltd();    
                	stop_wanduck();
	                stop_logger();
        	        stop_wanduck();
#ifdef RTCONFIG_DNSMASQ
                	stop_dnsmasq();
#else
	                stop_dns();
	                stop_dhcpd();
#endif
        	        stop_ots();
                	stop_networkmap();
#ifdef RTCONFIG_USB
	                stop_usbled();
#endif
		}
		else
			puts("ATE_ERROR");
                return 0;
        }
        else if (!strcmp(command, "Set_AllLedOn")) {
                return setAllLedOn();
        }
        else if (!strcmp(command, "Set_AllLedOff")) {
                return setAllLedOff();
        }
        else if (!strcmp(command, "Set_AllLedOn_Half")) {
                puts("Not support"); //Need to implement for EA-N66U
                return 0;
        }
        else if (!strcmp(command, "Set_MacAddr_2G")) {
                if( !setMAC_2G(value) )
			puts("ATE_ERROR_INCORRECT_PARAMETER");
		return 0;
        }
        else if (!strcmp(command, "Set_MacAddr_5G")) {
                if( !setMAC_5G(value))
                        puts("ATE_ERROR_INCORRECT_PARAMETER");
                return 0;
        }
        else if (!strcmp(command, "Set_RegulationDomain_2G")) {
                if ( !setCountryCode_2G(value))
			puts("ATE_ERROR_INCORRECT_PARAMETER");
                return 0;
        }
#ifdef CONFIG_BCMWL5
        else if (!strcmp(command, "Set_RegulationDomain_5G")) {
                if ( !setCountryCode_5G(value))
                        puts("ATE_ERROR_INCORRECT_PARAMETER");
                return 0;
        }
        else if (!strcmp(command, "Set_Regrev_2G")) {
                if( !setRegrev_2G(value) )
                        puts("ATE_ERROR_INCORRECT_PARAMETER");
                return 0;
        }
        else if (!strcmp(command, "Set_Regrev_5G")) {
                if( !setRegrev_5G(value))
                        puts("ATE_ERROR_INCORRECT_PARAMETER");
                return 0;
        }
#endif
        else if (!strcmp(command, "Set_PINCode")) {
                if (!setPIN(value))
			puts("ATE_ERROR_INCORRECT_PARAMETER");
                return 0;
        }
        else if (!strcmp(command, "Set_40M_Channel_2G")) {
                if(!set40M_Channel_2G(value))
			puts("ATE_ERROR_INCORRECT_PARAMETER");
                return 0;
        }
        else if (!strcmp(command, "Set_40M_Channel_5G")) {
                if(!set40M_Channel_5G(value))
			puts("ATE_ERROR_INCORRECT_PARAMETER");
                return 0;
        }
#ifdef RTCONFIG_FANCTRL
        else if (!strcmp(command, "Set_FanOn")) {
                setFanOn();
                return 0;
        }
        else if (!strcmp(command, "Set_FanOff")) {
                setFanOff();
                return 0;
        }
#endif
	else if (!strcmp(command, "Set_RestoreDefault")) {
		ResetDefault();
                return 0;
        }
#ifdef CONFIG_BCMWL5
        else if (!strcmp(command, "Set_Commit")) {
                setCommit();
                return 0;
        }
#endif
        else if (!strcmp(command, "Get_FWVersion")) {
	        char fwver[12];
        	sprintf(fwver, "%s.%s", nvram_safe_get("firmver"), nvram_safe_get("buildno"));
	        puts(fwver);
                return 0;
        }
        else if (!strcmp(command, "Get_BootLoaderVersion")) {
                getBootVer();
                return 0;
        }
        else if (!strcmp(command, "Get_ResetButtonStatus")) {
                puts(nvram_safe_get("btn_rst"));
                return 0;
        }
        else if (!strcmp(command, "Get_WpsButtonStatus")) {
                puts(nvram_safe_get("btn_ez"));
                return 0;
        }
        else if (!strcmp(command, "Get_SWMode")) {
                puts(nvram_safe_get("sw_mode"));
                return 0;
        }
        else if (!strcmp(command, "Get_MacAddr_2G")) {
                getMAC_2G();
                return 0;
        }
        else if (!strcmp(command, "Get_MacAddr_5G")) {
                getMAC_5G();
                return 0;
        }
        else if (!strcmp(command, "Get_Usb2p0_Port1_Infor")) {
		Get_USB_Port_Info(1);
                return 0;
        }
        else if (!strcmp(command, "Get_Usb2p0_Port1_Folder")) {
                Get_USB_Port_Folder(1);
                return 0;
        }
        else if (!strcmp(command, "Get_Usb2p0_Port2_Infor")) {
                Get_USB_Port_Info(2);
                return 0;
        }
        else if (!strcmp(command, "Get_Usb2p0_Port2_Folder")) {
                Get_USB_Port_Folder(2);
                return 0;
        }
        else if (!strcmp(command, "Get_Usb3p0_Port1_Infor")) {
                puts("Not support"); //Need to implement
                return 0;
        }
        else if (!strcmp(command, "Get_Usb3p0_Port2_Infor")) {
                puts("Not support"); //Need to implement
                return 0;
        }
        else if (!strcmp(command, "Get_Usb3p0_Port3_Infor")) {
                puts("Not support"); //Need to implement
                return 0;
        }
        else if (!strcmp(command, "Get_SD_Infor")) {
		Get_SD_Card_Info();
                return 0;
        }
        else if (!strcmp(command, "Get_SD_Folder")) {
                Get_SD_Card_Folder();
                return 0;
        }
        else if (!strcmp(command, "Get_RegulationDomain_2G")) {
		getCountryCode_2G();
                return 0;
        }
#ifdef CONFIG_BCMWL5
        else if (!strcmp(command, "Get_RegulationDomain_5G")) {
           	getCountryCode_5G();
                return 0;
        }
	else if (!strcmp(command, "Get_Regrev_2G")) {
		getRegrev_2G();
		return 0;
	}
        else if (!strcmp(command, "Get_Regrev_5G")) {
		getRegrev_5G();
                return 0;
        }
#endif
        else if (!strcmp(command, "Get_PINCode")) {
                getPIN();
                return 0;
        }
        else if (!strcmp(command, "Get_WanLanStatus")) {
                if( !GetPhyStatus())
			puts("ATE_ERROR");
		return 0;
        }
        else if (!strcmp(command, "Get_FwReadyStatus")) {
                puts(nvram_safe_get("success_start_service"));
                return 0;
        }
	else if (!strcmp(command, "Get_Build_Info")) {
		puts(nvram_safe_get("buildinfo"));
		return 0;
	}
#ifdef RTCONFIG_RALINK
        else if (!strcmp(command, "Get_RSSI_2G")) {
                getrssi(0);
                return 0;
        }
        else if (!strcmp(command, "Get_RSSI_5G")) {
                getrssi(1);
                return 0;
        }
#endif
        else if (!strcmp(command, "Get_ChannelList_2G")) {
                puts("Not support"); //Need to implement
                return 0;
        }
        else if (!strcmp(command, "Get_ChannelList_5G")) {
                puts("Not support"); //Need to implement
                return 0;
        }
        else if (!strcmp(command, "Get_Usb3p0_Port1_Folder")) {
                puts("Not support"); //Need to implement
                return 0;
        }
        else if (!strcmp(command, "Get_Usb3p0_Port2_Folder")) {
                puts("Not support"); //Need to implement
                return 0;
        }
        else if (!strcmp(command, "Get_Usb3p0_Port3_Folder")) {
                puts("Not support"); //Need to implement
                return 0;
        }
        else if (!strcmp(command, "Get_Usb3p0_Port1_DataRate")) {
                puts("Not support"); //Need to implement
                return 0;
        }
        else if (!strcmp(command, "Get_Usb3p0_Port2_DataRate")) {
                puts("Not support"); //Need to implement
                return 0;
        }
        else if (!strcmp(command, "Get_Usb3p0_Port3_DataRate")) {
                puts("Not support"); //Need to implement
                return 0;
        }
#ifdef RTCONFIG_RALINK
	else if (!strcmp(command, "Ra_FWRITE")) {
		return FWRITE(value, value2);
	}
        else if (!strcmp(command, "Ra_Asuscfe_2G")) {
                return asuscfe(value, WIF_2G);
        }
        else if (!strcmp(command, "Ra_Asuscfe_5G")) {
		return asuscfe(value, WIF_5G);
        }

#endif
        else 
	{
		puts("ATE_ERROR");
                return EINVAL;
	}
}
