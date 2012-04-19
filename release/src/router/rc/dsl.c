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
#include <bcmnvram.h>
#include <rc.h>
#include <shutils.h>
#ifdef LINUX26
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#endif
#include <shared.h>

extern void get_country_code_from_rc(char* country_code);


void start_dsl()
{
	system("mkdir /tmp/adsl"); 

	if (get_dualwan_secondary()==WANS_DUALWAN_IF_NONE)
	{
		if (get_dualwan_primary()!=WANS_DUALWAN_IF_DSL)
		{
			// it does not need to start dsl driver when using other modes
			// but it still need to run tp_init to have firmware version info
			printf("get modem info\n");
			system("tp_init eth_wan_mode_only");						
			return;
		}
	}
	else
	{
		// DSLTODO
	}


	//dsl_config_num
	int config_num = nvram_get_int("dsltmp_config_num");
	// need to use system()
	system("tp_init &");
	if (config_num == 0) {
		int x;
		char wan_if[9];
		char wan_num[2];		
		char country_value[8];
		char cmd_buf[64];
		for(x=2; x<=8; x++) {
			sprintf(wan_if, "eth2.1.%d", x);
			sprintf(wan_num, "%d", x);
			eval("vconfig", "add", "eth2.1", wan_num);
			eval("ifconfig", wan_if, "up");				
		}
		//
		nvram_set("dsltmp_autodet_state", "Detecting");
		// call auto detection with country code
		// not implemnt yet?
		//get_country_code_from_rc(country_value);			
		strcpy(cmd_buf,"auto_det ");
		strcat(cmd_buf,country_value);			
		strcat(cmd_buf," &");						
		// need to use system()		
		system(cmd_buf);	
	}
}

void stop_dsl()
{
	// dsl service need not to stop
}

//find the wan setting from WAN List and convert to 
//wan_xxx for original rc flow.
void convert_dsl_wan_settings(int argc, char *argv[])
{
	int config_num = 0;
	int i;
	if (nvram_match("dsl0_enable","1")) config_num++;
	if (nvram_match("dsl1_enable","1")) config_num++;
	if (nvram_match("dsl2_enable","1")) config_num++;
	if (nvram_match("dsl3_enable","1")) config_num++;
	if (nvram_match("dsl4_enable","1")) config_num++;
	if (nvram_match("dsl5_enable","1")) config_num++;
	if (nvram_match("dsl6_enable","1")) config_num++;
	if (nvram_match("dsl7_enable","1")) config_num++;
	nvram_set_int("dsltmp_config_num", config_num);

	// DSLTODO
	// talk to cheni, for how to use those variables

	nvram_set("wan_nat_x",nvram_safe_get("dslx_nat"));
	nvram_set("wan_upnp_enable",nvram_safe_get("dslx_upnp_enable"));	
	nvram_set("wan_enable",nvram_safe_get("dslx_link_enable"));	
	nvram_set("wan_dhcpenable_x",nvram_safe_get("dslx_DHCPClient"));	
	nvram_set("wan_ipaddr_x",nvram_safe_get("dslx_ipaddr"));	
	nvram_set("wan_upnp_enable",nvram_safe_get("dslx_upnp_enable"));		
	nvram_set("wan_netmask_x",nvram_safe_get("dslx_netmask"));		
	nvram_set("wan_gateway_x",nvram_safe_get("dslx_gateway"));		
	nvram_set("wan_dnsenable_x",nvram_safe_get("dslx_dnsenable"));		
	nvram_set("wan_dns1_x",nvram_safe_get("dslx_dns1"));			
	nvram_set("wan_dns2_x",nvram_safe_get("dslx_dns2"));			
	nvram_set("wan_pppoe_username",nvram_safe_get("dslx_pppoe_username"));			
	nvram_set("wan_pppoe_passwd",nvram_safe_get("dslx_pppoe_passwd"));				
	nvram_set("wan_pppoe_idletime",nvram_safe_get("dslx_pppoe_idletime"));			
	nvram_set("wan_pppoe_mtu",nvram_safe_get("dslx_pppoe_mtu"));				
	nvram_set("wan_pppoe_mru",nvram_safe_get("dslx_pppoe_mtu"));			
	nvram_set("wan_pppoe_service",nvram_safe_get("dslx_pppoe_service"));				
	nvram_set("wan_pppoe_ac",nvram_safe_get("dslx_pppoe_ac"));				
	nvram_set("wan_pppoe_options_x",nvram_safe_get("dslx_pppoe_options"));			
	nvram_set("wan_pppoe_relay",nvram_safe_get("dslx_pppoe_relay"));				
	nvram_set("wan_dns1_x",nvram_safe_get("dslx_dns1"));			
	nvram_set("wan_dns1_x",nvram_safe_get("dslx_dns1"));				

	nvram_set("wan0_nat_x",nvram_safe_get("dslx_nat"));
	nvram_set("wan0_upnp_enable",nvram_safe_get("dslx_upnp_enable"));	
	nvram_set("wan0_enable",nvram_safe_get("dslx_link_enable"));	
	nvram_set("wan0_dhcpenable_x",nvram_safe_get("dslx_DHCPClient"));	
	nvram_set("wan0_ipaddr_x",nvram_safe_get("dslx_ipaddr"));	
	nvram_set("wan0_upnp_enable",nvram_safe_get("dslx_upnp_enable"));		
	nvram_set("wan0_netmask_x",nvram_safe_get("dslx_netmask"));		
	nvram_set("wan0_gateway_x",nvram_safe_get("dslx_gateway"));		
	nvram_set("wan0_dnsenable_x",nvram_safe_get("dslx_dnsenable"));		
	nvram_set("wan0_dns1_x",nvram_safe_get("dslx_dns1"));			
	nvram_set("wan0_dns2_x",nvram_safe_get("dslx_dns2"));			
	nvram_set("wan0_pppoe_username",nvram_safe_get("dslx_pppoe_username"));			
	nvram_set("wan0_pppoe_passwd",nvram_safe_get("dslx_pppoe_passwd"));				
	nvram_set("wan0_pppoe_idletime",nvram_safe_get("dslx_pppoe_idletime"));			
	nvram_set("wan0_pppoe_mtu",nvram_safe_get("dslx_pppoe_mtu"));				
	nvram_set("wan0_pppoe_mru",nvram_safe_get("dslx_pppoe_mtu"));			
	nvram_set("wan0_pppoe_service",nvram_safe_get("dslx_pppoe_service"));				
	nvram_set("wan0_pppoe_ac",nvram_safe_get("dslx_pppoe_ac"));				
	nvram_set("wan0_pppoe_options_x",nvram_safe_get("dslx_pppoe_options"));			
	nvram_set("wan0_pppoe_relay",nvram_safe_get("dslx_pppoe_relay"));				
	nvram_set("wan0_dns1_x",nvram_safe_get("dslx_dns1"));			
	nvram_set("wan0_dns1_x",nvram_safe_get("dslx_dns1"));				

	// DSLTODO : bridge case
	if (nvram_match("dsl0_proto","pppoe") || nvram_match("dsl0_proto","pppoa")) {
		nvram_set("wan_proto","pppoe");
	}
	else if (nvram_match("dsl0_proto","mer")) {
		if (nvram_match("dslx_DHCPClient","1")) {
			nvram_set("wan_proto","dhcp");
		}
		else {
			nvram_set("wan_proto","static");		
		}
	}

	// test , how to use those 2 vars
	nvram_set("wan0_primary","1");				
	nvram_set("wan_primary","0");					
	nvram_set("wan0_proto",nvram_safe_get("wan_proto"));					
}


