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

#include "wanduck.h"

#define NO_DETECT_INTERNET

static void safe_leave(int signo){
	csprintf("\n## wanduck.safeexit ##\n");
	signal(SIGTERM, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGINT, SIG_IGN);

	FD_ZERO(&allset);
	close(http_sock);
	close(dns_sock);

	int i, ret;
	for(i = 0; i < maxfd; ++i){
		ret = close(i);
		csprintf("## close %d: result=%d.\n", i, ret);
	}

	sleep(1);

	if(sw_mode == SW_MODE_AP
#ifdef RTCONFIG_WIRELESSREPEATER
			|| sw_mode == SW_MODE_REPEATER
#endif
			){
		eval("ebtables", "-t", "broute", "-F");
		eval("ebtables", "-t", "filter", "-F");
		f_write_string("/proc/net/dnsmqctrl", "", 0, 0);
	}

	if(rule_setup == 1){
		csprintf("\n# Disable direct rule(exit wanduck)\n");

		rule_setup = 0;
		err_state = CONNED; // for cleaning the redirect rules.

		handle_wan_line(rule_setup);
	}

	remove(WANDUCK_PID_FILE);

csprintf("\n# return(exit wanduck)\n");
	exit(0);
}

void get_related_nvram(){
	if(nvram_match("x_Setting", "1"))
		isFirstUse = 0;
	else
		isFirstUse = 1;

#ifdef RTCONFIG_WIRELESSREPEATER
	if(strlen(nvram_safe_get("wlc_ssid")) > 0)
		setAP = 1;
	else
		setAP = 0;
#endif

	sw_mode = nvram_get_int("sw_mode");

/*csprintf("\n# wanduck: Got System information:\n");
csprintf("# wanduck: isFirstUse=%d.\n", isFirstUse);
csprintf("# wanduck:      setAP=%d.\n", setAP);
csprintf("# wanduck:    sw_mode=%d.\n", sw_mode);//*/
}

void get_lan_nvram(){
	char nvram_name[16];

	current_lan_unit = nvram_get_int("lan_unit");

	memset(prefix_lan, 0, 8);
	if(current_lan_unit < 0)
		strcpy(prefix_lan, "lan_");
	else
		sprintf(prefix_lan, "lan%d_", current_lan_unit);

	memset(current_lan_ifname, 0, 16);
	strcpy(current_lan_ifname, nvram_safe_get(strcat_r(prefix_lan, "ifname", nvram_name)));

	memset(current_lan_proto, 0, 16);
	strcpy(current_lan_proto, nvram_safe_get(strcat_r(prefix_lan, "proto", nvram_name)));

	memset(current_lan_ipaddr, 0, 16);
	strcpy(current_lan_ipaddr, nvram_safe_get(strcat_r(prefix_lan, "ipaddr", nvram_name)));

	memset(current_lan_netmask, 0, 16);
	strcpy(current_lan_netmask, nvram_safe_get(strcat_r(prefix_lan, "netmask", nvram_name)));

	memset(current_lan_gateway, 0, 16);
	strcpy(current_lan_gateway, nvram_safe_get(strcat_r(prefix_lan, "gateway", nvram_name)));

	memset(current_lan_dns, 0, 256);
	strcpy(current_lan_dns, nvram_safe_get(strcat_r(prefix_lan, "dns", nvram_name)));

	memset(current_lan_subnet, 0, 11);
	strcpy(current_lan_subnet, nvram_safe_get(strcat_r(prefix_lan, "subnet", nvram_name)));

#ifdef RTCONFIG_WIRELESSREPEATER
	if(sw_mode == SW_MODE_REPEATER){
		wlc_state = nvram_get_int("wlc_state");
	}
#endif

	csprintf("# wanduck: Got LAN(%d) information:\n", current_lan_unit);
	if(nvram_match("wanduck_debug", "1")){
		csprintf("# wanduck:   ifname=%s.\n", current_lan_ifname);
		csprintf("# wanduck:    proto=%s.\n", current_lan_proto);
		csprintf("# wanduck:   ipaddr=%s.\n", current_lan_ipaddr);
		csprintf("# wanduck:  netmask=%s.\n", current_lan_netmask);
		csprintf("# wanduck:  gateway=%s.\n", current_lan_gateway);
		csprintf("# wanduck:      dns=%s.\n", current_lan_dns);
		csprintf("# wanduck:   subnet=%s.\n", current_lan_subnet);
	}
}

void get_wan_nvram(){
	char nvram_name[16];

#ifdef RTCONFIG_USB_MODEM
	link_modem = is_usb_modem_ready();
#endif

	current_wan_unit = wan_primary_ifunit();

	memset(prefix_wan, 0, 8);
	sprintf(prefix_wan, "wan%d_", current_wan_unit);

	strcat_r(prefix_wan, "auxstate_t", nvram_auxstate);
	strcat_r(prefix_wan, "state_t", nvram_state);
	strcat_r(prefix_wan, "sbstate_t", nvram_sbstate);

	memset(current_wan_ifname, 0, 16);
	strcpy(current_wan_ifname, get_wan_ifname(current_wan_unit));

	memset(current_wan_proto, 0, 16);
	strcpy(current_wan_proto, nvram_safe_get(strcat_r(prefix_wan, "proto", nvram_name)));

	memset(current_wan_ipaddr, 0, 16);
	strcpy(current_wan_ipaddr, nvram_safe_get(strcat_r(prefix_wan, "ipaddr", nvram_name)));

	memset(current_wan_netmask, 0, 16);
	strcpy(current_wan_netmask, nvram_safe_get(strcat_r(prefix_wan, "netmask", nvram_name)));

	memset(current_wan_gateway, 0, 16);
	strcpy(current_wan_gateway, nvram_safe_get(strcat_r(prefix_wan, "gateway", nvram_name)));

	memset(current_wan_dns, 0, 256);
	strcpy(current_wan_dns, nvram_safe_get(strcat_r(prefix_wan, "dns", nvram_name)));

	memset(current_wan_subnet, 0, 11);
	strcpy(current_wan_subnet, nvram_safe_get(strcat_r(prefix_wan, "subnet", nvram_name)));

#ifdef RTCONFIG_WIRELESSREPEATER
	if(sw_mode == SW_MODE_HOTSPOT){
		wlc_state = nvram_get_int("wlc_state");
	}
#endif

	csprintf("# wanduck: Got WAN(%d) information:\n", current_wan_unit);
	if(nvram_match("wanduck_debug", "1")){
		csprintf("# wanduck:   ifname=%s.\n", current_wan_ifname);
		csprintf("# wanduck:    proto=%s.\n", current_wan_proto);
		csprintf("# wanduck:   ipaddr=%s.\n", current_wan_ipaddr);
		csprintf("# wanduck:  netmask=%s.\n", current_wan_netmask);
		csprintf("# wanduck:  gateway=%s.\n", current_wan_gateway);
		csprintf("# wanduck:      dns=%s.\n", current_wan_dns);
		csprintf("# wanduck:   subnet=%s.\n", current_wan_subnet);
#ifdef RTCONFIG_USB_MODEM
		csprintf("# wanduck: link_modem=%d.\n", link_modem);
#endif
	}
}

static void get_remote_network_nvram(int signo){
	if(sw_mode == SW_MODE_ROUTER
#ifdef RTCONFIG_WIRELESSREPEATER
			|| sw_mode == SW_MODE_HOTSPOT
#endif
			)
		get_wan_nvram();
	else
		get_lan_nvram();
}

static void rebuild_redirect_rules(int signo){
	if(sw_mode == SW_MODE_ROUTER){
		// for ppp connections, eth0 would get IP first, and then dial up.
		// When eth0 got IP, the nat rules would be reset.
		// wanduck needed to re-install the redirect rules if the recover happened.
		if((!strcmp(current_wan_proto, "pppoe")
				|| !strcmp(current_wan_proto, "pptp")
				|| !strcmp(current_wan_proto, "l2tp"))
				&& rule_setup){
csprintf("# wanduck: Re-build the redirect rules.\n");
			handle_wan_line(rule_setup);
		}
	}
}

int get_packets_of_net_dev(const char *net_dev, unsigned long *rx_packets, unsigned long *tx_packets){
	FILE *fp;
	char buf[256];
	char *ifname;
	char *ptr;
	int i, got_packets = 0;

	if((fp = fopen(PROC_NET_DEV, "r")) == NULL){
csprintf("get_packets_of_net_dev: Can't open the file: %s.\n", PROC_NET_DEV);
		return got_packets;
	}

	// headers.
	for(i = 0; i < 2; ++i){
		if(fgets(buf, sizeof(buf), fp) == NULL){
csprintf("get_packets_of_net_dev: Can't read out the headers of %s.\n", PROC_NET_DEV);
			fclose(fp);
			return got_packets;
		}
	}

	while(fgets(buf, sizeof(buf), fp) != NULL){
		if((ptr = strchr(buf, ':')) == NULL)
			continue;

		*ptr = 0;
		if((ifname = strrchr(buf, ' ')) == NULL)
			ifname = buf;
		else
			++ifname;

		if(strcmp(ifname, net_dev))
			continue;

		// <rx bytes, packets, errors, dropped, fifo errors, frame errors, compressed, multicast><tx ...>
		if(sscanf(ptr+1, "%*u%lu%*u%*u%*u%*u%*u%*u%*u%lu", rx_packets, tx_packets) != 2){
csprintf("get_packets_of_net_dev: Can't read the packet's number in %s.\n", PROC_NET_DEV);
			fclose(fp);
			return got_packets;
		}

		got_packets = 1;
		break;
	}
	fclose(fp);

	return got_packets;
}

char *organize_tcpcheck_cmd(char *dns_list, char *cmd, int size){
	char buf[256], *next;

	if(cmd == NULL || size <= 0)
		return NULL;

	memset(cmd, 0, size);

	sprintf(cmd, "/sbin/tcpcheck %d", TCPCHECK_TIMEOUT);

	foreach(buf, dns_list, next){
		sprintf(cmd, "%s %s:53", cmd, buf);
	}

	sprintf(cmd, "%s >>%s", cmd, DETECT_FILE);

	return cmd;
}

int do_ping_detect(){
	char cmd[256], buf[256], *next;

	foreach(buf, current_wan_dns, next){
		sprintf(cmd, "ping -c 1 -W %d %s && touch %s", TCPCHECK_TIMEOUT, buf, PING_RESULT_FILE);
		system(cmd);

		if(check_if_file_exist(PING_RESULT_FILE)){
			unlink(PING_RESULT_FILE);
			return 1;
		}
	}

	return 0;
}

int do_dns_detect(){
	const char *test_url = "www.asus.com";

	if(gethostbyname(test_url) != NULL)
		return 1;

	return 0;
}

int do_tcp_dns_detect(){
	FILE *fp = NULL;
	char line[80], cmd[256];

	remove(DETECT_FILE);

	if(organize_tcpcheck_cmd(current_wan_dns, cmd, 256) == NULL){
csprintf("wanduck: No tcpcheck cmd.\n");
		return 0;
	}
	system(cmd);

	if((fp = fopen(DETECT_FILE, "r")) == NULL){
csprintf("wanduck: No file: %s.\n", DETECT_FILE);
		return 0;
	}

	while(fgets(line, sizeof(line), fp) != NULL){
		if(strstr(line, "alive")){
			fclose(fp);
			return 1;
		}
	}
	fclose(fp);

//csprintf("wanduck: No response from: %s.\n", current_wan_dns);
	return 0;
}

int detect_internet(){
	int link_internet;
	unsigned long rx_packets, tx_packets;

	if(!found_default_route())
		link_internet = DISCONN;
#ifndef NO_DETECT_INTERNET
	else if(!get_packets_of_net_dev(current_wan_ifname, &rx_packets, &tx_packets) || rx_packets <= RX_THRESHOLD)
		link_internet = DISCONN;
	else if(!isFirstUse && (!do_dns_detect() && !do_tcp_dns_detect() && !do_ping_detect()))
		link_internet = DISCONN;
#endif
	else
		link_internet = CONNED;

	if(link_internet == DISCONN){
		nvram_set_int("link_internet", 0);
		record_wan_state_nvram(WAN_AUXSTATE_NO_INTERNET_ACTIVITY, -1, -1);

		if(!(nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOINTERNET)) {
			nvram_set_int("link_internet", 1);
			link_internet = CONNED;
		}
	}
	else{
		nvram_set_int("link_internet", 1);
		record_wan_state_nvram(WAN_AUXSTATE_NONE, -1, -1);
	}

	return link_internet;
}

int passivesock(char *service, int protocol_num, int qlen){
	//struct servent *pse;
	struct sockaddr_in sin;
	int s, type, on;
	
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	
	// map service name to port number
	if((sin.sin_port = htons((u_short)atoi(service))) == 0){
		perror("cannot get service entry");
		
		return -1;
	}
	
	if(protocol_num == IPPROTO_UDP)
		type = SOCK_DGRAM;
	else
		type = SOCK_STREAM;
	
	s = socket(PF_INET, type, protocol_num);
	if(s < 0){
		perror("cannot create socket");
		
		return -1;
	}
	
	on = 1;
	if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) < 0){
		perror("cannot set socket's option: SO_REUSEADDR");
		close(s);
		
		return -1;
	}
	
	if(bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0){
		perror("cannot bind port");
		close(s);
		
		return -1;
	}
	
	if(type == SOCK_STREAM && listen(s, qlen) < 0){
		perror("cannot listen to port");
		close(s);
		
		return -1;
	}
	
	return s;
}

int check_ppp_exist(){
	DIR *dir;
	struct dirent *dent;
	char task_file[64], cmdline[64];
	int pid, fd;

	if((dir = opendir("/proc")) == NULL){
		perror("open proc");
		return 0;
	}

	while((dent = readdir(dir)) != NULL){
		if((pid = atoi(dent->d_name)) > 1){
			memset(task_file, 0, 64);
			sprintf(task_file, "/proc/%d/cmdline", pid);
			if((fd = open(task_file, O_RDONLY)) > 0){
				memset(cmdline, 0, 64);
				read(fd, cmdline, 64);
				close(fd);

				if(strstr(cmdline, "pppd")
						|| strstr(cmdline, "l2tpd")
						){
					closedir(dir);
					return 1;
				}
			}
			else
				printf("cannot open %s\n", task_file);
		}
	}
	closedir(dir);
	
	return 0;
}

int chk_proto(){
	if(sw_mode == SW_MODE_AP
#ifdef RTCONFIG_WIRELESSREPEATER
			|| sw_mode == SW_MODE_REPEATER
#endif
			){
		return CONNED;
	}
	else
#ifdef RTCONFIG_WIRELESSREPEATER
	if(sw_mode == SW_MODE_HOTSPOT){
		if(current_state == WAN_STATE_STOPPED) {
			if(current_sbstate == WAN_STOPPED_REASON_INVALID_IPADDR)
				disconn_case = CASE_THESAMESUBNET;
			else disconn_case = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(current_state == WAN_STATE_INITIALIZING){
			disconn_case = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(current_state == WAN_STATE_CONNECTING){
			disconn_case = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(current_state == WAN_STATE_DISCONNECTED){
			disconn_case = CASE_DHCPFAIL;
			return DISCONN;
		}
	}
	else
#endif
	// Start chk_proto() in SW_MODE_ROUTER.
#ifdef RTCONFIG_USB_MODEM
	if(current_wan_unit == WAN_UNIT_USBPORT){
		ppp_fail_state = pppstatus();

		if(current_state == WAN_STATE_INITIALIZING){
			disconn_case = CASE_PPPFAIL;
			return DISCONN;
		}
		else if(current_state == WAN_STATE_CONNECTING){
			if(ppp_fail_state == WAN_STOPPED_REASON_PPP_AUTH_FAIL)
				record_wan_state_nvram(WAN_AUXSTATE_PPP_AUTH_FAIL, -1, -1);

			disconn_case = CASE_PPPFAIL;
			return DISCONN;
		}
		else if(current_state == WAN_STATE_DISCONNECTED){
			disconn_case = CASE_PPPFAIL;
			return DISCONN;
		}
		else if(current_state == WAN_STATE_STOPPED){
			disconn_case = CASE_PPPFAIL;
			return DISCONN;
		}
	}
	else
#endif
	if(!strcmp(current_wan_proto, "dhcp")
			|| !strcmp(current_wan_proto, "static")){
		if(current_state == WAN_STATE_STOPPED) {
			if(current_sbstate == WAN_STOPPED_REASON_INVALID_IPADDR)
				disconn_case = CASE_THESAMESUBNET;
			else disconn_case = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(current_state == WAN_STATE_INITIALIZING){
			disconn_case = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(current_state == WAN_STATE_CONNECTING){
			disconn_case = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(current_state == WAN_STATE_DISCONNECTED){
			disconn_case = CASE_DHCPFAIL;
			return DISCONN;
		}
	}
	else if(!strcmp(current_wan_proto, "pppoe")
			|| !strcmp(current_wan_proto, "pptp")
			|| !strcmp(current_wan_proto, "l2tp")
			){
		ppp_fail_state = pppstatus();

		if(current_state == WAN_STATE_INITIALIZING){
			disconn_case = CASE_PPPFAIL;
			return DISCONN;
		}
		else if(current_state == WAN_STATE_CONNECTING){
			if(ppp_fail_state == WAN_STOPPED_REASON_PPP_AUTH_FAIL)
				record_wan_state_nvram(WAN_AUXSTATE_PPP_AUTH_FAIL, -1, -1);

			disconn_case = CASE_PPPFAIL;
			return DISCONN;
		}
		else if(current_state == WAN_STATE_DISCONNECTED){
			disconn_case = CASE_PPPFAIL;
			return DISCONN;
		}
		else if(current_state == WAN_STATE_STOPPED){
			disconn_case = CASE_PPPFAIL;
			return DISCONN;
		}
	}

	return CONNED;
}

int if_wan_phyconnected(void){
	int link_wan;
#ifdef RTCONFIG_WIRELESSREPEATER
	int link_ap;
#endif
	int link_changed = 0;

	// check wan port.
	link_wan = get_wanports_status();

#ifdef RTCONFIG_LANWAN_LED
	if(link_wan) led_control(LED_WAN, LED_ON);
	else led_control(LED_WAN, LED_OFF);

	if(get_lanports_status()) led_control(LED_LAN, LED_ON);
	else led_control(LED_LAN, LED_OFF);
#endif

	if(link_wan != nvram_get_int("link_wan")){
		nvram_set_int("link_wan", link_wan);
		link_changed = 1;
	}

#ifdef RTCONFIG_USB_MODEM
	if(link_modem != nvram_get_int("link_modem"))
		nvram_set_int("link_modem", link_modem);
#endif

#ifdef RTCONFIG_WIRELESSREPEATER
	// check if set AP.
	link_ap = (wlc_state == WLC_STATE_CONNECTED);
	if(link_ap != nvram_get_int("link_ap"))
		nvram_set_int("link_ap", link_ap);
#endif

	if(sw_mode == SW_MODE_ROUTER){
		if(current_state == WAN_STATE_DISABLED){
			//record_wan_state_nvram(-1, WAN_STATE_STOPPED, WAN_STOPPED_REASON_MANUAL);

			disconn_case = CASE_OTHERS;
			return DISCONN;
		}
		// this means D2C because of reconnecting the WAN port.
		else if (link_changed) {
			// WAN port was disconnected, arm reconnect
			if (!link_wan) {
				link_setup = 1;
			} else
			// WAN port was connected, fire reconnect if armed
			if (link_setup) {
				link_setup = 0;
				// Only handle this case when WAN proto is DHCP or Static
				if (strcmp(current_wan_proto, "static") == 0) {
					disconn_case = CASE_DISWAN;
					return PHY_RECONN;
				} else
				if (strcmp(current_wan_proto, "dhcp") == 0) {
					disconn_case = CASE_DHCPFAIL;
					return PHY_RECONN;
				}
			}
		}
		else if(!link_wan){
#ifdef RTCONFIG_USB_MODEM
			if(!link_modem || current_wan_unit != WAN_UNIT_USBPORT){
#endif
				nvram_set_int("link_internet", 0);

				record_wan_state_nvram(WAN_AUXSTATE_NOPHY, -1, -1);

				if((nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)){
					disconn_case = CASE_DISWAN;
					return DISCONN;
				}
#ifdef RTCONFIG_USB_MODEM
			}
#endif
		}
	}
	else if(sw_mode == SW_MODE_AP){
		if(!link_wan){
			// ?: type error?
			nvram_set_int("link_internet", 0);

			record_wan_state_nvram(WAN_AUXSTATE_NOPHY, -1, -1);

			if((nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)){
				disconn_case = CASE_DISWAN;
				return DISCONN;
			}
		}
	}
#ifdef RTCONFIG_WIRELESSREPEATER
	else{ // sw_mode == SW_MODE_REPEATER, SW_MODE_HOTSPOT.
		if(!link_ap){
			if(sw_mode == SW_MODE_HOTSPOT)
				record_wan_state_nvram(WAN_AUXSTATE_NOPHY, -1, -1);

			disconn_case = CASE_AP_DISCONN;
			return DISCONN;
		}
	}
#endif

	if(chk_proto() == DISCONN)
		return DISCONN;
	else if(sw_mode == SW_MODE_ROUTER) // TODO: temparily let detect_internet() service in SW_MODE_ROUTER.
		return detect_internet();

	return CONNED;
}

void handle_wan_line(int action){
	char cmd[128];

	// Redirect rules.
	if(action){
		notify_rc_and_wait("stop_nat_rules");
	}
	/*
	 * When C2C and remove the redirect rules,
	 * it means dissolve the default state.
	 */
	else if(err_state == D2C || err_state == CONNED){
		notify_rc_and_wait("start_nat_rules");
	}
	else{ // err_state == PHY_RECONN
csprintf("\n# wanduck(%d): Try to restart_wan_if.\n", action);
		memset(cmd, 0, 128);
		sprintf(cmd, "restart_wan_if %d", current_wan_unit);
		notify_rc_and_wait(cmd);
	}
}

void close_socket(int sockfd, char type){
	close(sockfd);
	FD_CLR(sockfd, &allset);
	client[fd_i].sfd = -1;
	client[fd_i].type = 0;
}

int build_socket(char *http_port, char *dns_port, int *hd, int *dd){
	if((*hd = passivesock(http_port, IPPROTO_TCP, 10)) < 0){
		csprintf("Fail to socket for httpd port: %s.\n", http_port);
		return -1;
	}
	
	if((*dd = passivesock(dns_port, IPPROTO_UDP, 10)) < 0){
		csprintf("Fail to socket for DNS port: %s.\n", dns_port);
		return -1;
	}
	
	return 0;
}

void send_page(int sfd, char *file_dest, char *url){
	char buf[2*MAXLINE];
	time_t now;
	char timebuf[100];

	memset(buf, 0, sizeof(buf));
	now = uptime();
	(void)strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

	sprintf(buf, "%s%s%s%s%s%s", buf, "HTTP/1.0 302 Moved Temporarily\r\n", "Server: wanduck\r\n", "Date: ", timebuf, "\r\n");

	if((err_state == C2D || err_state == DISCONN) && disconn_case == CASE_THESAMESUBNET)
		sprintf(buf, "%s%s%s%s%s%d%s%s" ,buf , "Connection: close\r\n", "Location:http://", DUT_DOMAIN_NAME, "/error_page.htm?flag=", disconn_case, "\r\nContent-Type: text/plain\r\n", "\r\n<html></html>\r\n");
	else if(isFirstUse){
#ifdef RTCONFIG_WIRELESSREPEATER
		if(sw_mode == SW_MODE_REPEATER || sw_mode == SW_MODE_HOTSPOT)
			sprintf(buf, "%s%s%s%s%s%s%s" ,buf , "Connection: close\r\n", "Location:http://", DUT_DOMAIN_NAME, "/QIS_wizard.htm?flag=sitesurvey", "\r\nContent-Type: text/plain\r\n", "\r\n<html></html>\r\n");
		else
#endif
			sprintf(buf, "%s%s%s%s%s%s%s" ,buf , "Connection: close\r\n", "Location:http://", DUT_DOMAIN_NAME, "/QIS_wizard.htm?flag=welcome", "\r\nContent-Type: text/plain\r\n", "\r\n<html></html>\r\n");
	}
	else if(err_state == C2D || err_state == DISCONN) {
		sprintf(buf, "%s%s%s%s%s%d%s%s" ,buf , "Connection: close\r\n", "Location:http://", DUT_DOMAIN_NAME, "/error_page.htm?flag=", disconn_case, "\r\nContent-Type: text/plain\r\n", "\r\n<html></html>\r\n");
	}
#ifdef RTCONFIG_WIRELESSREPEATER
	else {
		sprintf(buf, "%s%s%s%s%s", buf, "Connection: close\r\n", "Location:http://", DUT_DOMAIN_NAME, "/index.asp\r\nContent-Type: text/plain\r\n\r\n<html></html>\r\n"); 
	}	
#endif
	write(sfd, buf, strlen(buf));
	close_socket(sfd, T_HTTP);
}

void parse_dst_url(char *page_src){
	int i, j;
	char dest[PATHLEN], host[64];
	char host_strtitle[7], *hp;
	
	j = 0;
	memset(dest, 0, sizeof(dest));
	memset(host, 0, sizeof(host));
	memset(host_strtitle, 0, sizeof(host_strtitle));
	
	for(i = 0; i < strlen(page_src); ++i){
		if(i >= PATHLEN)
			break;
		
		if(page_src[i] == ' ' || page_src[i] == '?'){
			dest[j] = '\0';
			break;
		}
		
		dest[j++] = page_src[i];
	}
	
	host_strtitle[0] = '\n';
	host_strtitle[1] = 'H';
	host_strtitle[2] = 'o';
	host_strtitle[3] = 's';
	host_strtitle[4] = 't';
	host_strtitle[5] = ':';
	host_strtitle[6] = ' ';
	
	if((hp = strstr(page_src, host_strtitle)) != NULL){
		hp += 7;
		j = 0;
		for(i = 0; i < strlen(hp); ++i){
			if(i >= 64)
				break;
			
			if(hp[i] == '\r' || hp[i] == '\n'){
				host[j] = '\0';
				break;
			}
			
			host[j++] = hp[i];
		}
	}
	
	memset(dst_url, 0, sizeof(dst_url));
	sprintf(dst_url, "%s/%s", host, dest);
}

void parse_req_queries(char *content, char *lp, int len, int *reply_size){
	int i, rn;
	
	rn = *(reply_size);
	for(i = 0; i < len; ++i){
		content[rn+i] = lp[i];
		if(lp[i] == 0){
			++i;
			break;
		}
	}
	
	if(i >= len)
		return;
	
	content[rn+i] = lp[i];
	content[rn+i+1] = lp[i+1];
	content[rn+i+2] = lp[i+2];
	content[rn+i+3] = lp[i+3];
	i += 4;
	
	*reply_size += i;
}

void handle_http_req(int sfd, char *line){
	int len;
	
	if(!strncmp(line, "GET /", 5)){
		parse_dst_url(line+5);
		
		len = strlen(dst_url);
		if((dst_url[len-4] == '.') &&
				(dst_url[len-3] == 'i') &&
				(dst_url[len-2] == 'c') &&
				(dst_url[len-1] == 'o')){
			close_socket(sfd, T_HTTP);
			
			return;
		}
		send_page(sfd, NULL, dst_url);
	}
	else
		close_socket(sfd, T_HTTP);
}

void handle_dns_req(int sfd, char *line, int maxlen, struct sockaddr *pcliaddr, int clen){
	dns_query_packet d_req;
	dns_response_packet d_reply;
	int reply_size;
	char reply_content[MAXLINE];
	
	reply_size = 0;
	memset(reply_content, 0, MAXLINE);
	memset(&d_req, 0, sizeof(d_req));
	memcpy(&d_req.header, line, sizeof(d_req.header));
	
	// header
	memcpy(&d_reply.header, &d_req.header, sizeof(dns_header));
	d_reply.header.flag_set.flag_num = htons(0x8580);
	//d_reply.header.flag_set.flag_num = htons(0x8180);
	d_reply.header.answer_rrs = htons(0x0001);
	memcpy(reply_content, &d_reply.header, sizeof(d_reply.header));
	reply_size += sizeof(d_reply.header);
	
	reply_content[5] = 1;	// Questions
	reply_content[7] = 1;	// Answer RRS
	reply_content[9] = 0;	// Authority RRS
	reply_content[11] = 0;	// Additional RRS
	
	// queries
	parse_req_queries(reply_content, line+sizeof(dns_header), maxlen-sizeof(dns_header), &reply_size);
	
	// answers
	d_reply.answers.name = htons(0xc00c);
	d_reply.answers.type = htons(0x0001);
	d_reply.answers.ip_class = htons(0x0001);
	//d_reply.answers.ttl = htonl(0x00000001);
	d_reply.answers.ttl = htonl(0x00000000);
	d_reply.answers.data_len = htons(0x0004);
	
	char query_name[PATHLEN];
	int len, i;
	
	strncpy(query_name, line+sizeof(dns_header)+1, PATHLEN);
	len = strlen(query_name);
	for(i = 0; i < len; ++i)
		if(query_name[i] < 32)
			query_name[i] = '.';
	
	if(!upper_strcmp(query_name, router_name))
		d_reply.answers.addr = inet_addr_(nvram_safe_get("lan_ipaddr"));	// router's ip
	else
		d_reply.answers.addr = htonl(0x0a000001);	// 10.0.0.1
	
	memcpy(reply_content+reply_size, &d_reply.answers, sizeof(d_reply.answers));
	reply_size += sizeof(d_reply.answers);
	
	sendto(sfd, reply_content, reply_size, 0, pcliaddr, clen);
}

void run_http_serv(int sockfd){
	ssize_t n;
	char line[MAXLINE];
	
	memset(line, 0, sizeof(line));
	
	if((n = read(sockfd, line, MAXLINE)) == 0){	// client close
		close_socket(sockfd, T_HTTP);
		
		return;
	}
	else if(n < 0){
		perror("wanduck read");
		return;
	}
	else{
		if(client[fd_i].type == T_HTTP)
			handle_http_req(sockfd, line);
		else
			close_socket(sockfd, T_HTTP);
	}
}

void run_dns_serv(int sockfd){
	int n;
	char line[MAXLINE];
	struct sockaddr_in cliaddr;
	int clilen = sizeof(cliaddr);
	
	memset(line, 0, MAXLINE);
	memset(&cliaddr, 0, clilen);
	
	if((n = recvfrom(sockfd, line, MAXLINE, 0, (struct sockaddr *)&cliaddr, &clilen)) == 0)	// client close
		return;
	else if(n < 0){
		perror("wanduck read");
		return;
	}
	else
		handle_dns_req(sockfd, line, n, (struct sockaddr *)&cliaddr, clilen);
}

void record_wan_state_nvram(int auxstate, int state, int sbstate){
	if(auxstate != -1 && auxstate != nvram_get_int(nvram_auxstate))
		nvram_set_int(nvram_auxstate, auxstate);

	if(state != -1 && state != nvram_get_int(nvram_state))
		nvram_set_int(nvram_state, state);

	if(sbstate != -1 && sbstate != nvram_get_int(nvram_sbstate))
		nvram_set_int(nvram_sbstate, sbstate);
}

void record_conn_status(){
	if(err_state == DISCONN || err_state == C2D){
#ifdef RTCONFIG_WIRELESSREPEATER
		if(disconn_case == CASE_AP_DISCONN){
			if(Dr_Surf_case == CASE_AP_DISCONN)
				return;
			Dr_Surf_case = CASE_AP_DISCONN;

			logmessage("WAN Connection", "Don't connect the AP yet.");
		}
		else
#endif
		if(disconn_case == CASE_DISWAN){
			if(Dr_Surf_case == CASE_DISWAN)
				return;
			Dr_Surf_case = CASE_DISWAN;

			logmessage("WAN Connection", "Ethernet link down.");
		}
		else if(disconn_case == CASE_PPPFAIL){
			if(Dr_Surf_case == CASE_PPPFAIL)
				return;
			Dr_Surf_case = CASE_PPPFAIL;

			if(ppp_fail_state == WAN_STOPPED_REASON_PPP_AUTH_FAIL)
				logmessage("WAN Connection", "VPN authentication failed.");
			else if(ppp_fail_state == WAN_STOPPED_REASON_PPP_NO_ACTIVITY)
				logmessage("WAN Connection", "No response from ISP.");
			else
				logmessage("WAN Connection", "Fail to connect with some issues.");
		}
		else if(disconn_case == CASE_DHCPFAIL){
			if(Dr_Surf_case == CASE_DHCPFAIL)
				return;
			Dr_Surf_case = CASE_DHCPFAIL;

			if(!strcmp(current_wan_proto, "dhcp")
#ifdef RTCONFIG_WIRELESSREPEATER
					|| sw_mode == SW_MODE_HOTSPOT
#endif
					)
				logmessage("WAN Connection", "ISP's DHCP did not function properly.");
			else
				logmessage("WAN Connection", "Detected that the WAN Connection Type was PPPoE. But the PPPoE Setting was not complete.");
		}
		else if(disconn_case == CASE_MISROUTE){
			if(Dr_Surf_case == CASE_MISROUTE)
				return;
			Dr_Surf_case = CASE_MISROUTE;

			logmessage("WAN Connection", "The router's ip was the same as gateway's ip. It led to your packages couldn't dispatch to internet correctly.");
		}
		else if(disconn_case == CASE_THESAMESUBNET){
			if(Dr_Surf_case == CASE_MISROUTE)
				return;
			Dr_Surf_case = CASE_MISROUTE;

			logmessage("WAN Connection", "The LAN's subnet may be the same with the WAN's subnet.");
		}
		else{	// disconn_case == CASE_OTHERS
			if(Dr_Surf_case == CASE_OTHERS)
				return;
			Dr_Surf_case = CASE_OTHERS;

			logmessage("WAN Connection", "WAN was exceptionally disconnected.");
		}
	}
	else if(err_state == D2C){
		if(Dr_Surf_case == 10)
			return;
		Dr_Surf_case = 10;

		logmessage("WAN Connection", "WAN was restored.");
	}
	else if(err_state == PHY_RECONN){
		logmessage("WAN Connection", "Ethernet link up.");
	}
}

int wanduck_main(int argc, const char *argv[]){
	char *http_servport, *dns_servport;
	int clilen;
	struct sockaddr_in cliaddr;
	struct timeval  tval;
	int nready, maxi, sockfd; //Yau move "conn_state" to wanduck.h
#ifdef RTCONFIG_USB_MODEM
	int modem_ready_count = MODEM_IDLE;
#endif
	char domain_mapping[64];

	signal(SIGHUP, SIG_IGN);
	signal(SIGTERM, safe_leave);
	signal(SIGUSR1, get_remote_network_nvram);
	signal(SIGUSR2, rebuild_redirect_rules);
	signal(SIGINT, test_ifconfig);

	if(argc < 3){
		http_servport = DFL_HTTP_SERV_PORT;
		dns_servport = DFL_DNS_SERV_PORT;
	}
	else{
		if(atoi(argv[1]) <= 0)
			http_servport = DFL_HTTP_SERV_PORT;
		else
			http_servport = (char *)argv[1];

		if(atoi(argv[2]) <= 0)
			dns_servport = DFL_DNS_SERV_PORT;
		else
			dns_servport = (char *)argv[2];
	}

	if(build_socket(http_servport, dns_servport, &http_sock, &dns_sock) < 0){
		csprintf("\n*** Fail to build socket! ***\n");
		exit(0);
	}

	FILE *fp = fopen(WANDUCK_PID_FILE, "w");

	if(fp != NULL){
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	maxfd = (http_sock > dns_sock)?http_sock:dns_sock;
	maxi = -1;

	FD_ZERO(&allset);
	FD_SET(http_sock, &allset);
	FD_SET(dns_sock, &allset);

	for(fd_i = 0; fd_i < MAX_USER; ++fd_i){
		client[fd_i].sfd = -1;
		client[fd_i].type = 0;
	}

	link_setup = 0;
	rule_setup = 0;
	disconn_case = 0;
	clilen = sizeof(cliaddr);

	sprintf(router_name, "%s", DUT_DOMAIN_NAME);

	nvram_set_int("link_wan", 0);
#ifdef RTCONFIG_USB_MODEM
	nvram_set_int("link_modem", 0);
#endif
	nvram_set_int("link_internet", 2);
	get_related_nvram(); // initial the System's variables.
	get_lan_nvram(); // initial the LAN's variables.
	get_wan_nvram(); // initial the WAN's variables.

	current_auxstate = nvram_get_int(nvram_auxstate);
	current_state = nvram_get_int(nvram_state);
	current_sbstate = nvram_get_int(nvram_sbstate);

	err_state = if_wan_phyconnected();

	record_conn_status();

	/*
	 * Because start_wanduck() is run early than start_wan()
	 * and the redirect rules is already set before running wanduck,
	 * handle_wan_line() must not be run at the first detect.
	 */
	if(err_state == DISCONN){
csprintf("\n# Enable direct rule\n");
		rule_setup = 1;
	}
	else if(err_state == CONNED && isFirstUse){
csprintf("\n#CONNED : Enable direct rule\n");
		rule_setup = 1;
	}

	for(;;){
		rset = allset;
		tval.tv_sec = SCAN_INTERVAL;
		tval.tv_usec = 0;

		get_related_nvram();

		current_auxstate = nvram_get_int(nvram_auxstate);
		current_state = nvram_get_int(nvram_state);
		current_sbstate = nvram_get_int(nvram_sbstate);

		conn_state = if_wan_phyconnected();

//csprintf("# wanduck:: err_state= %d, conn_state= %d, disc_case= %d\n", err_state, conn_state, disconn_case);
		if(conn_state == CONNED){
			if(err_state == DISCONN)
				err_state = D2C;
			else
				err_state = CONNED;

#ifdef RTCONFIG_USB_MODEM
//csprintf("# wanduck: set MODEM_IDLE: CONNED.\n");
			modem_ready_count = MODEM_IDLE;
#endif
		}
		else if(conn_state == DISCONN){
			if(err_state == CONNED)
				err_state = C2D;
			else
				err_state = DISCONN;

#ifdef RTCONFIG_USB_MODEM
			if(disconn_case == CASE_THESAMESUBNET){
csprintf("# wanduck: set MODEM_IDLE: CASE_THESAMESUBNET.\n");
				modem_ready_count = MODEM_IDLE;
			}
			else if(current_state == WAN_STATE_DISABLED){
csprintf("# wanduck: set MODEM_IDLE: WAN_STATE_DISABLED.\n");
				modem_ready_count = MODEM_IDLE;
			}
			else if(!link_modem){
//csprintf("# wanduck: set MODEM_IDLE: no link_modem.\n");
				modem_ready_count = MODEM_IDLE;
			}
			else if(modem_ready_count == MODEM_IDLE){
csprintf("# wanduck: set MODEM_READY: current_wan_unit=%d, link_modem=%d.\n", current_wan_unit, link_modem);
				modem_ready_count = MODEM_READY;
			}
#endif
		}
		else if(conn_state == PHY_RECONN){
			err_state = PHY_RECONN;

#ifdef RTCONFIG_USB_MODEM
			if(!current_wan_unit && link_modem){ // When the WAN line in the WAN port mode, the count has to be reset.
csprintf("# wanduck: set MODEM_READY: PHY_RECONN.\n");
				modem_ready_count = MODEM_READY;
			}
#endif
		}

#ifdef RTCONFIG_USB_MODEM
		if(modem_ready_count != MODEM_IDLE){
			if(modem_ready_count < MAX_WAIT_COUNT){
				++modem_ready_count;
csprintf("# wanduck: wait time for switching: %d/%d.\n", modem_ready_count*SCAN_INTERVAL, MAX_WAIT_TIME_OF_MODEM);
			}
			else{
csprintf("# wanduck: set MODEM_READY: modem_ready_count >= MAX_WAIT_COUNT.\n");
				modem_ready_count = MODEM_READY;
			}
		}
#endif

		record_conn_status();

		if(sw_mode == SW_MODE_AP
#ifdef RTCONFIG_WIRELESSREPEATER
				|| sw_mode == SW_MODE_REPEATER
#endif
				){
			if(err_state == DISCONN || err_state == C2D || isFirstUse){
				if(rule_setup == 0){
if(conn_state == DISCONN)
	csprintf("\n# mode(%d): Enable direct rule(DISCONN)\n", sw_mode);
else if(conn_state == C2D)
	csprintf("\n# mode(%d): Enable direct rule(C2D)\n", sw_mode);
else
	csprintf("\n# mode(%d): Enable direct rule(isFirstUse)\n", sw_mode);
					rule_setup = 1;

					eval("ebtables", "-t", "broute", "-F");
					eval("ebtables", "-t", "filter", "-F");
					// Drop the DHCP server from PAP.
					eval("ebtables", "-t", "filter", "-I", "FORWARD", "-i", "eth1", "-j", "DROP");
					f_write_string("/proc/net/dnsmqctrl", "", 0, 0);

					notify_rc_and_wait("stop_nat_rules");
				}
			}
			else{
				if(rule_setup == 1 && !isFirstUse){
csprintf("\n# mode(%d): Disable direct rule(CONNED)\n", sw_mode);
					rule_setup = 0;

					eval("ebtables", "-t", "broute", "-F");
					eval("ebtables", "-t", "filter", "-F");
					eval("ebtables", "-t", "broute", "-I", "BROUTING", "-p", "ipv4", "-d", "00:E0:11:22:33:44", "-j", "redirect", "--redirect-target", "DROP");
					sprintf(domain_mapping, "%x %s", inet_addr(nvram_safe_get("lan_ipaddr")), DUT_DOMAIN_NAME);
					f_write_string("/proc/net/dnsmqctrl", domain_mapping, 0, 0);

					notify_rc_and_wait("start_nat_rules");
				}
			}
		}
		else if(err_state == C2D || (err_state == CONNED && isFirstUse)){
			if(rule_setup == 0){
if(err_state == C2D)
	csprintf("\n# Enable direct rule(C2D)\n");
else
	csprintf("\n# Enable direct rule(isFirstUse)\n");
				rule_setup = 1;

				handle_wan_line(rule_setup);

#ifdef RTCONFIG_USB_MODEM
				if(err_state == C2D){
					if(current_wan_unit == WAN_UNIT_USBPORT && !link_modem){ // plug-off the Modem.
csprintf("\n# wanduck(C2D): Try to Switch the WAN line.(link_modem=%d).\n", link_modem);
						switch_usb_modem(link_modem);
					}
					else if(link_modem && !nvram_match("modem_mode", "0")
							&& nvram_get_int("wan1_state_t") != WAN_STATE_CONNECTED){
csprintf("\n# wanduck(C2D): Try to prepare the backup line.\n");
						notify_rc_and_wait("restart_wan_if 1");
					}
				}
				
#endif
			}
		}
		else if(err_state == D2C || err_state == CONNED){
			if(rule_setup == 1 && !isFirstUse){
csprintf("\n# Disable direct rule(D2C)\n");
				rule_setup = 0;

				handle_wan_line(rule_setup);
			}
		}
#ifdef RTCONFIG_USB_MODEM
		// After boot, wait the WAN port to connect.
		else if(err_state == DISCONN){
			if((link_modem && modem_ready_count >= MAX_WAIT_COUNT)
					|| (current_wan_unit == WAN_UNIT_USBPORT && !link_modem)
					)
			{
if(current_wan_unit)
	csprintf("# Switching the connect to WAN port...\n");
else
	csprintf("# Switching the connect to USB Modem...\n");
				switch_usb_modem(!current_wan_unit);
			}
		}
#endif
		// phy connected -> disconnected -> connected
		else if(err_state == PHY_RECONN){
			handle_wan_line(0);
		}

		if((nready = select(maxfd+1, &rset, NULL, NULL, &tval)) <= 0)
			continue;

		if(FD_ISSET(dns_sock, &rset)){
			//printf("# run fake dns service\n");	// tmp test
			run_dns_serv(dns_sock);
			if(--nready <= 0)
				continue;
		}
		else if(FD_ISSET(http_sock, &rset)){
			//printf("# run fake httpd service\n");	// tmp test
			if((cur_sockfd = accept(http_sock, (struct sockaddr *)&cliaddr, &clilen)) <= 0){
				perror("http accept");
				continue;
			}

			for(fd_i = 0; fd_i < MAX_USER; ++fd_i){
				if(client[fd_i].sfd < 0){
					client[fd_i].sfd = cur_sockfd;
					client[fd_i].type = T_HTTP;
					break;
				}
			}

			if(fd_i == MAX_USER){
csprintf("# wanduck servs full\n");
				close(cur_sockfd);

				continue;
			}

			FD_SET(cur_sockfd, &allset);
			if(cur_sockfd > maxfd)
				maxfd = cur_sockfd;
			if(fd_i > maxi)
				maxi = fd_i;
			
			if(--nready <= 0)
				continue;	// no more readable descriptors
		}

		// polling
		for(fd_i = 0; fd_i <= maxi; ++fd_i){
			if((sockfd = client[fd_i].sfd) < 0)
				continue;

			if(FD_ISSET(sockfd, &rset)){
				int nread;
				ioctl(sockfd, FIONREAD, &nread);
				if(nread == 0){
					close_socket(sockfd, T_HTTP);
					continue;
				}

				cur_sockfd = sockfd;

				run_http_serv(sockfd);

				if(--nready <= 0)
					break;
			}
		}
	}

csprintf("# wanduck exit error\n");
	exit(1);
}

void test_ifconfig(){
	unsigned long rx_packets, tx_packets;
	char *tmp_ifname, *tmp_dns;
	char buf[256], *ptr;

	if(sw_mode == SW_MODE_ROUTER
#ifdef RTCONFIG_WIRELESSREPEATER
			|| sw_mode == SW_MODE_HOTSPOT
#endif
			){
		csprintf("# wanduck: Got WAN information:\n");
		csprintf("# wanduck:  ifname=%s.\n", current_wan_ifname);
		csprintf("# wanduck:   proto=%s.\n", current_wan_proto);
		csprintf("# wanduck:  ipaddr=%s.\n", current_wan_ipaddr);
		csprintf("# wanduck: netmask=%s.\n", current_wan_netmask);
		csprintf("# wanduck: gateway=%s.\n", current_wan_gateway);
		csprintf("# wanduck:     dns=%s.\n", current_wan_dns);
		csprintf("# wanduck:  subnet=%s.\n", current_wan_subnet);
#ifdef RTCONFIG_USB_MODEM
		csprintf("# wanduck:    link_modem=%d.\n", link_modem);
#endif
		tmp_ifname = current_wan_ifname;
		tmp_dns = current_wan_dns;
	}
	else{
		csprintf("# wanduck: Got LAN information:\n");
		csprintf("# wanduck:  ifname=%s.\n", current_lan_ifname);
		csprintf("# wanduck:   proto=%s.\n", current_lan_proto);
		csprintf("# wanduck:  ipaddr=%s.\n", current_lan_ipaddr);
		csprintf("# wanduck: netmask=%s.\n", current_lan_netmask);
		csprintf("# wanduck: gateway=%s.\n", current_lan_gateway);
		csprintf("# wanduck:     dns=%s.\n", current_lan_dns);
		csprintf("# wanduck:  subnet=%s.\n", current_lan_subnet);

		tmp_ifname = current_lan_ifname;
		tmp_dns = current_lan_dns;
	}

	if(get_packets_of_net_dev(tmp_ifname, &rx_packets, &tx_packets))
		csprintf("wanduck: rx_packets=%lu, tx_packets=%lu.\n", rx_packets, tx_packets);
	else
		csprintf("wanduck: Can't get %s's packet numbers.\n", tmp_ifname);

	if((ptr = organize_tcpcheck_cmd(tmp_dns, buf, 256)) != NULL)
		csprintf("wanduck: tcpcheck_cmd=%s.\n", buf);
	else
		csprintf("wanduck: Can't get tcpcheck cmd!\n");
}

