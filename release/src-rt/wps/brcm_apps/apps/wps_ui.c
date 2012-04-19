/*
 * WPS ui
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_ui.c 287339 2011-10-03 05:10:12Z gracecsm $
 */

#include <stdio.h>
#include <tutrace.h>
#include <time.h>
#include <wps_wl.h>
#include <wps_ui.h>
#include <shutils.h>
#include <wlif_utils.h>
#include <wps_ap.h>
#include <wps_enr.h>
#include <security_ipc.h>
#include <wps.h>
#include <wps_pb.h>
#include <wps_led.h>
#include <wps_upnp.h>
#include <wps_ie.h>
#include <ap_ssr.h>

typedef struct {
	int wps_config_command;	/* Config */
	int wps_method;
	int wps_action;
	int wps_pbc_method;
	char wps_ifname[16];
	char wps_sta_pin[SIZE_64_BYTES];
	int wps_aplockdown;
	int wps_proc_status;	/* Status report */
	int wps_pinfail;
	char wps_sta_mac[sizeof("00:00:00:00:00:00")];
	char wps_autho_sta_mac[sizeof("00:00:00:00:00:00")]; /* WSC 2.0 */
	char wps_sta_devname[32];
	char wps_enr_wsec[SIZE_32_BYTES];
	char wps_enr_ssid[SIZE_32_BYTES+1];
	char wps_enr_bssid[SIZE_32_BYTES];
	int wps_enr_scan;
#ifdef __CONFIG_WFI__
	char wps_device_pin[12]; /* For WiFi-Invite session PIN */
#endif /* __CONFIG_WFI__ */
	char wps_stareg_ap_pin[SIZE_64_BYTES];
	char wps_scstate[32];
} wps_ui_t;

static wps_hndl_t ui_hndl;
static int pending_flag = 0;
static unsigned long pending_time = 0;

#ifdef WPS_ADDCLIENT_WWTP
/* WSC 2.0, 
 * One way to define the use of a particular PIN is that it should be valid for the whole
 * Walking Time period.  In other words, as long as the registration is not successful
 * and the process hasn't timed out and no new PIN has been entered, the AP should
 * accept to engage in a new registration process with the same PIN.
 * On some implementation, this is what happens on the enrollee side when it fails
 * registration with one of several AP and keep trying with all of them (actually,
 * I think we should implement this as well in our enrollee code).
 *
 * In our case, that would mean that :
 * - the AP should detect the NACK from the enrollee after failure on M4 (which I am
 * not sure it does) and close the session.
 * - as long as the PIN is valid, the AP should restart its state machine after failure,
 * keep the same PIN,  and answer an M1 with M2 instead of M2D.
 */
static unsigned long addclient_window_start = 0;
#endif /* WPS_ADDCLIENT_WWTP */

static wps_ui_t s_wps_ui;
static wps_ui_t *wps_ui = &s_wps_ui;

char *
wps_ui_get_env(char *name)
{
	static char buf[32];

	if (!strcmp(name, "wps_config_command")) {
		sprintf(buf, "%d", wps_ui->wps_config_command);
		return buf;
	}

	if (!strcmp(name, "wps_action")) {
		sprintf(buf, "%d", wps_ui->wps_action);
		return buf;
	}

	if (!strcmp(name, "wps_method")) {
		sprintf(buf, "%d", wps_ui->wps_method);
		return buf;
	}

	if (!strcmp(name, "wps_pbc_method")) {
		sprintf(buf, "%d", wps_ui->wps_pbc_method);
		return buf;
	}

	if (!strcmp(name, "wps_sta_pin"))
		return wps_ui->wps_sta_pin;

	if (!strcmp(name, "wps_proc_status")) {
		sprintf(buf, "%d", wps_ui->wps_proc_status);
		return buf;
	}

	if (!strcmp(name, "wps_enr_wsec")) {
		return wps_ui->wps_enr_wsec;
	}

	if (!strcmp(name, "wps_enr_ssid")) {
		return wps_ui->wps_enr_ssid;
	}

	if (!strcmp(name, "wps_enr_bssid")) {
		return wps_ui->wps_enr_bssid;
	}

	if (!strcmp(name, "wps_enr_scan")) {
		sprintf(buf, "%d", wps_ui->wps_enr_scan);
		return buf;
	}

	if (!strcmp(name, "wps_stareg_ap_pin"))
		return wps_ui->wps_stareg_ap_pin;

	if (!strcmp(name, "wps_scstate"))
		return wps_ui->wps_scstate;

#ifdef __CONFIG_WFI__
	/* For WiFi-Invite session PIN */
	if (!strcmp(name, "wps_device_pin")) {
		return wps_ui->wps_device_pin;
	}
#endif /* __CONFIG_WFI__ */

	if (!strcmp(name, "wps_autho_sta_mac")) /* WSC 2.0 */
		return wps_ui->wps_autho_sta_mac;

	return "";
}

void
wps_ui_set_env(char *name, char *value)
{
	if (!strcmp(name, "wps_ifname"))
		wps_strncpy(wps_ui->wps_ifname, value, sizeof(wps_ui->wps_ifname));

	if (!strcmp(name, "wps_config_command"))
		wps_ui->wps_config_command = atoi(value);

	if (!strcmp(name, "wps_action"))
		wps_ui->wps_action = atoi(value);

	if (!strcmp(name, "wps_aplockdown"))
		wps_ui->wps_aplockdown = atoi(value);

	if (!strcmp(name, "wps_proc_status"))
		wps_ui->wps_proc_status = atoi(value);

	if (!strcmp(name, "wps_pinfail"))
		wps_ui->wps_pinfail = atoi(value);

	if (!strcmp(name, "wps_sta_mac"))
		wps_strncpy(wps_ui->wps_sta_mac, value, sizeof(wps_ui->wps_sta_mac));

	if (!strcmp(name, "wps_sta_devname"))
		wps_strncpy(wps_ui->wps_sta_devname, value, sizeof(wps_ui->wps_sta_devname));

#ifdef WPS_AP_M2D
	if (!strcmp(name, "wps_sta_pin"))
		wps_strncpy(wps_ui->wps_sta_pin, value, sizeof(wps_ui->wps_sta_pin));
#endif

	if (!strcmp(name, "wps_pbc_method"))
		wps_ui->wps_pbc_method = atoi(value);

	return;
}

void
wps_ui_reset_env()
{
	/* Reset partial wps_ui environment variables */
	wps_ui->wps_config_command = WPS_UI_CMD_NONE;
	wps_ui->wps_action = WPS_UI_ACT_NONE;
	wps_ui->wps_pbc_method = WPS_UI_PBC_NONE;
}

int
wps_ui_is_pending()
{
	return pending_flag;
}

#ifdef BCMWPSAP
/* Start pending for any MAC  */
static void
wps_ui_start_pending(char *wps_ifname)
{
	unsigned long now;
	CTlvSsrIE ssrmsg;

	int i, imax, scb_num;
	char temp[32];
	char ifname[IFNAMSIZ];
	unsigned int wps_uuid[16];
	unsigned char uuid_R[16];
	unsigned char authorizedMacs[SIZE_MAC_ADDR] = {0};
	int authorizedMacs_len = 0;
	char authorizedMacs_buf[SIZE_MAC_ADDR * SIZE_AUTHORIZEDMACS_NUM] = {0};
	int authorizedMacs_buf_len = 0;
	BufferObj *authorizedMacs_bufObj = NULL, *uuid_R_bufObj = NULL;
	char *wlnames, *next, *ipaddr = NULL, *value;
	bool found = FALSE;
	bool b_wps_version2 = FALSE;
	bool b_do_pbc = FALSE;
	char *pc_data;
	uint8 scState = WPS_SCSTATE_UNCONFIGURED;

	(void) time((time_t*)&now);

	if (strcmp(wps_ui_get_env("wps_sta_pin"), "00000000") == 0)
		b_do_pbc = TRUE;

	/* Check push time only when we use PBC method */
	if (b_do_pbc && wps_pb_check_pushtime(now) == PBC_OVERLAP_CNT) {
		TUTRACE((TUTRACE_INFO, "%d PBC station found, ignored PBC!\n", PBC_OVERLAP_CNT));
		wps_ui->wps_config_command = WPS_UI_CMD_NONE;
		wps_setProcessStates(WPS_PBCOVERLAP);

		return;
	}

	/* WSC 2.0,  find ipaddr according to wps_ifname */
	imax = wps_get_ess_num();
	for (i = 0; i < imax; i++) {
		sprintf(temp, "ess%d_wlnames", i);
		wlnames = wps_safe_get_conf(temp);

		foreach(ifname, wlnames, next) {
			if (!strcmp(ifname, wps_ifname)) {
				found = TRUE;
				break;
			}
		}

		if (found) {
			/* Get ipaddr */
			sprintf(temp, "ess%d_ipaddr", i);
			ipaddr = wps_safe_get_conf(temp);

			/* Get builtin register scState */
			sprintf(temp, "ess%d_wps_oob", i);
			pc_data = wps_safe_get_conf(temp);

			/* According to WPS 2.0 section "Wi-Fi Simple Configuration State"
			 * Note: The Internal Registrar waits until successful completion of the
			 * protocol before applying the automatically generated credentials to
			 * avoid an accidental transition from "Not Configured" to "Configured"
			 * in the case that a neighboring device tries to run WSC
			 */
			if (!strcmp(pc_data, "disabled"))
				scState = WPS_SCSTATE_CONFIGURED;
			else
				scState = WPS_SCSTATE_UNCONFIGURED;

			/* Get UUID, convert string to hex */
			value = wps_safe_get_conf("wps_uuid");
			sscanf(value,
				"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
				&wps_uuid[0], &wps_uuid[1], &wps_uuid[2], &wps_uuid[3],
				&wps_uuid[4], &wps_uuid[5], &wps_uuid[6], &wps_uuid[7],
				&wps_uuid[8], &wps_uuid[9], &wps_uuid[10], &wps_uuid[11],
				&wps_uuid[12], &wps_uuid[13], &wps_uuid[14], &wps_uuid[15]);
			for (i = 0; i < 16; i++)
				uuid_R[i] = (wps_uuid[i] & 0xff);

			break;
		}
	}

	if (!found || !ipaddr) {
		TUTRACE((TUTRACE_ERR, "Can't find ipaddr according to %s\n", wps_ifname));
		return;
	}

	/* WSC 2.0,  support WPS V2 or not */
	if (strcmp(wps_safe_get_conf("wps_version2"), "enabled") == 0)
		b_wps_version2 = TRUE;

	/* Set built-in registrar to be selected registrar */
	if (b_wps_version2) {
		if (strlen(wps_ui->wps_autho_sta_mac)) {
			/* For now, only support one authorized mac */
			authorizedMacs_len = SIZE_MAC_ADDR;
			ether_atoe(wps_ui->wps_autho_sta_mac, authorizedMacs);
		}
		else {
			/* WSC 2.0 r44, add wildcard MAC when authorized mac not specified */
			authorizedMacs_len = SIZE_MAC_ADDR;
			memcpy(authorizedMacs, wildcard_authorizedMacs, SIZE_MAC_ADDR);
		}

		/* Prepare authorizedMacs_Obj and uuid_R_Obj */
		authorizedMacs_bufObj = buffobj_new();
		if (!authorizedMacs_bufObj) {
			TUTRACE((TUTRACE_ERR, "Can't allocate authorizedMacs_bufObj\n"));
			return;
		}
		uuid_R_bufObj = buffobj_new();
		if (!uuid_R_bufObj) {
			TUTRACE((TUTRACE_ERR, "Can't allocate uuid_R_bufObj\n"));
			buffobj_del(authorizedMacs_bufObj);
			return;
		}
	}

	wps_ie_default_ssr_info(&ssrmsg, authorizedMacs, authorizedMacs_len,
		authorizedMacs_bufObj, uuid_R, uuid_R_bufObj, scState);

	/* WSC 2.0, add authorizedMACs when runs built-in registrar */
	ap_ssr_set_scb(ipaddr, &ssrmsg, NULL, now);

	if (b_wps_version2) {
		/*
		 * Get union (WSC 2.0 page 79, 8.4.1) of information received from all Registrars,
		 * use recently authorizedMACs list and update union attriabutes to ssrmsg
		 */
		scb_num = ap_ssr_get_union_attributes(&ssrmsg, authorizedMacs_buf,
			&authorizedMacs_buf_len);

		/*
		 * No any SSR exist, no any selreg is TRUE, set UPnP SSR time expired
		 * if it is activing
		 */
		if (scb_num == 0) {
			TUTRACE((TUTRACE_ERR, "No any SSR exist\n"));
			if (authorizedMacs_bufObj)
				buffobj_del(authorizedMacs_bufObj);
			if (uuid_R_bufObj)
				buffobj_del(uuid_R_bufObj);
			return;
		}

		/* Reset ssrmsg.authorizedMacs */
		ssrmsg.authorizedMacs.m_data = NULL;
		ssrmsg.authorizedMacs.subtlvbase.m_len = 0;

		/* Construct ssrmsg.authorizedMacs from authorizedMacs_buf */
		if (authorizedMacs_buf_len) {
			/* Re-use authorizedMacs_bufObj */
			buffobj_Rewind(authorizedMacs_bufObj);
			/* De-serialize the authorizedMacs data to get the TLVs */
			subtlv_serialize(WPS_WFA_SUBID_AUTHORIZED_MACS, authorizedMacs_bufObj,
				authorizedMacs_buf, authorizedMacs_buf_len);
			buffobj_Rewind(authorizedMacs_bufObj);
			subtlv_dserialize(&ssrmsg.authorizedMacs, WPS_WFA_SUBID_AUTHORIZED_MACS,
				authorizedMacs_bufObj, 0, 0);
		}
	}

	wps_ie_set(wps_ifname, &ssrmsg);

	if (authorizedMacs_bufObj)
		buffobj_del(authorizedMacs_bufObj);

	if (uuid_R_bufObj)
		buffobj_del(uuid_R_bufObj);

	pending_flag = 1;
	pending_time = now;

#ifdef WPS_ADDCLIENT_WWTP
	/* Change in PF #3 */
	if (addclient_window_start && !b_do_pbc) {
		pending_time = addclient_window_start;
	}
	else {
		pending_time = now;
		if (!b_do_pbc)
			addclient_window_start = now;
	}
#endif /* WPS_ADDCLIENT_WWTP */

	return;
}
#endif /* ifdef BCMWPSAP */

#ifdef WPS_ADDCLIENT_WWTP
int
wps_ui_is_SET_cmd(char *buf, int buflen)
{
	if (strncmp(buf, "SET ", 4) == 0)
		return 1;

	return 0;
}

void
wps_ui_close_addclient_window()
{
	addclient_window_start = 0;
}
#endif /* WPS_ADDCLIENT_WWTP */

void
wps_ui_clear_pending()
{
	pending_flag = 0;
	pending_time = 0;
}

int
wps_ui_pending_expire()
{
	unsigned long now;

	if (pending_flag && pending_time) {
		(void) time((time_t*)&now);

		if ((now < pending_time) || ((now - pending_time) > WPS_MAX_TIMEOUT)) {
#ifdef WPS_ADDCLIENT_WWTP
			wps_ui_close_addclient_window();
#endif
			return 1;
		}
	}

	return 0;
}

static void
wps_ui_do_get()
{
#ifdef WPS_UPNP_DEVICE
	unsigned char wps_uuid[16];
#endif
	char buf[256];
	int len = 0;

	len += sprintf(buf + len, "wps_config_command=%d ", wps_ui->wps_config_command);
	len += sprintf(buf + len, "wps_action=%d ", wps_ui->wps_action);
	/* Add in PF #3 */
	len += sprintf(buf + len, "wps_method=%d ", wps_ui->wps_method);
	len += sprintf(buf + len, "wps_autho_sta_mac=%s ", wps_ui->wps_autho_sta_mac);
	len += sprintf(buf + len, "wps_ifname=%s ", wps_ui->wps_ifname);
#ifdef WPS_UPNP_DEVICE
	wps_upnp_device_uuid(wps_uuid);
	len += sprintf(buf + len,
		"wps_uuid=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x ",
		wps_uuid[0], wps_uuid[1], wps_uuid[2], wps_uuid[3],
		wps_uuid[4], wps_uuid[5], wps_uuid[6], wps_uuid[7],
		wps_uuid[8], wps_uuid[9], wps_uuid[10], wps_uuid[11],
		wps_uuid[12], wps_uuid[13], wps_uuid[14], wps_uuid[15]);
#endif /* WPS_UPNP_DEVICE */
	len += sprintf(buf + len, "wps_aplockdown=%d ", wps_ui->wps_aplockdown);
	len += sprintf(buf + len, "wps_proc_status=%d ", wps_ui->wps_proc_status);
	len += sprintf(buf + len, "wps_pinfail=%d ", wps_ui->wps_pinfail);
	len += sprintf(buf + len, "wps_sta_mac=%s ", wps_ui->wps_sta_mac);
	len += sprintf(buf + len, "wps_sta_devname=%s ", wps_ui->wps_sta_devname);

	wps_osl_send_uimsg(&ui_hndl, buf, len+1);
	return;
}

static int
wps_ui_parse_env(char *buf, wps_ui_t *ui)
{
	char *argv[32] = {0};
	char *item, *p, *next;
	char *name, *value;
	int i;

	/* Seperate buf into argv[] */
	for (i = 0, item = buf, p = item; item && item[0]; item = next, p = 0, i++) {
		/* Get next token */
		strtok_r(p, " ", &next);
		argv[i] = item;
	}

	for (i = 0; argv[i]; i++) {
		value = argv[i];
		name = strsep(&value, "=");
		if (name) {
			if (!strcmp(name, "wps_config_command"))
				ui->wps_config_command = atoi(value);
			else if (!strcmp(name, "wps_method"))
				ui->wps_method = atoi(value);
			else if (!strcmp(name, "wps_action"))
				ui->wps_action = atoi(value);
			else if (!strcmp(name, "wps_pbc_method"))
				ui->wps_pbc_method = atoi(value);
			else if (!strcmp(name, "wps_ifname"))
				wps_strncpy(ui->wps_ifname, value, sizeof(ui->wps_ifname));
			else if (!strcmp(name, "wps_sta_pin"))
				wps_strncpy(ui->wps_sta_pin, value, sizeof(ui->wps_sta_pin));
			else if (!strcmp(name, "wps_enr_wsec"))
				wps_strncpy(ui->wps_enr_wsec, value, sizeof(ui->wps_enr_wsec));
			else if (!strcmp(name, "wps_enr_ssid"))
				wps_strncpy(ui->wps_enr_ssid, value, sizeof(ui->wps_enr_ssid));
			else if (!strcmp(name, "wps_enr_bssid"))
				wps_strncpy(ui->wps_enr_bssid, value, sizeof(ui->wps_enr_bssid));
			else if (!strcmp(name, "wps_enr_scan"))
				ui->wps_enr_scan = atoi(value);
#ifdef __CONFIG_WFI__
			else if (!strcmp(name, "wps_device_pin"))
				strcpy(ui->wps_device_pin, value);
#endif /* __CONFIG_WFI__ */
			else if (!strcmp(name, "wps_autho_sta_mac")) /* WSC 2.0 */
				wps_strncpy(ui->wps_autho_sta_mac, value,
					sizeof(ui->wps_autho_sta_mac));
			else if (!strcmp(name, "wps_stareg_ap_pin"))
				wps_strncpy(ui->wps_stareg_ap_pin, value,
					sizeof(ui->wps_stareg_ap_pin));
			else if (!strcmp(name, "wps_scstate"))
				wps_strncpy(ui->wps_scstate, value, sizeof(ui->wps_scstate));
		}
	}

	return 0;
}

static int
wps_ui_do_set(char *buf)
{
	int ret = WPS_CONT;
	wps_ui_t a_ui;
	wps_ui_t *ui = &a_ui;
	wps_app_t *wps_app = get_wps_app();

	/* Process set command */
	memset(ui, 0, sizeof(wps_ui_t));
	if (wps_ui_parse_env(buf, ui) != 0)
		return WPS_CONT;

	/* user click STOPWPS button or pending expire */
	if (ui->wps_config_command == WPS_UI_CMD_STOP) {

#ifdef WPS_ADDCLIENT_WWTP
		/* stop add client 2 min. window */
		wps_ui_close_addclient_window();
		wps_close_addclient_window();
#endif /* WPS_ADDCLIENT_WWTP */

		/* state maching in progress, check stop or push button here */
		wps_close_session();
		wps_setProcessStates(WPS_INIT);

		/*
		  * set wps LAN all leds to normal and wps_pb_state
		  * when session timeout or user stop
		  */
		wps_pb_state_reset();
	}
	else {
		/* Add in PF #3 */
		if (ui->wps_config_command == WPS_UI_CMD_START &&
		    ui->wps_pbc_method == WPS_UI_PBC_SW &&
		    (wps_app->wksp || pending_flag)) {
			/* close session if session opend or in pending state */
			int old_action;

			old_action = ui->wps_action;
			wps_close_session();
			ui->wps_config_command = WPS_UI_CMD_START;
			ui->wps_action = old_action;
		}

		/* for WPS_AP_M2D */
		if ((!wps_app->wksp || wps_app->mode == WPSM_PROXY ||
#ifdef WPS_AP_M2D
		    wps_app->mode == WPSM_BUILTINREG_PROXY ||
#endif
		    FALSE) && !pending_flag) {
			/* Fine to accept set command */
			memcpy(wps_ui, ui, sizeof(*ui));

			/* some button in UI is pushed */
			if (wps_ui->wps_config_command == WPS_UI_CMD_START) {
				wps_setProcessStates(WPS_INIT);

				/* if proxy in process, stop it */
				if (wps_app->wksp) {
					wps_close_session();
					wps_ui->wps_config_command = WPS_UI_CMD_START;
				}

				if (wps_is_wps_sta(wps_ui->wps_ifname)) {
					/* set what interface you want */
#ifdef BCMWPSAPSTA
					/* set the process status */
					wps_setProcessStates(WPS_ASSOCIATED);
					ret = wpssta_open_session(wps_app, wps_ui->wps_ifname);
					if (ret != WPS_CONT) {
						/* Normally it cause by associate time out */
						wps_setProcessStates(WPS_TIMEOUT);
					}
#endif /* BCMWPSAPSTA */

				}
#ifdef BCMWPSAP
				/* for wps_ap application */
				else {
					/* set the process status */
					wps_setProcessStates(WPS_ASSOCIATED);

					if (wps_ui->wps_action == WPS_UI_ACT_CONFIGAP) {
						/* TBD - Initial all relative nvram setting */
						char tmptr[] = "wlXXXXXXXXXX_hwaddr";
						char *macaddr;

						sprintf(tmptr, "%s_hwaddr", wps_ui->wps_ifname);
						macaddr = wps_get_conf(tmptr);
						if (macaddr) {
							char ifmac[SIZE_6_BYTES];
							ether_atoe(macaddr, ifmac);

							/* WSC 2.0, Request to Enroll TRUE */
							ret = wpsap_open_session(wps_app,
								WPSM_UNCONFAP, ifmac, NULL,
								wps_ui->wps_ifname, NULL, NULL,
								NULL, 0, 1, 0);
						}
						else {
							TUTRACE((TUTRACE_INFO, "can not find mac "
								"on %s\n", wps_ui->wps_ifname));
						}
					}
					else if (wps_ui->wps_action == WPS_UI_ACT_ADDENROLLEE) {
						/* Do nothing, wait for client connect */
						wps_ui_start_pending(wps_ui->wps_ifname);
					}
				}

#else
				TUTRACE((TUTRACE_INFO, "AP functionality not included.\n"));
#endif /* BCMWPSAP */
			}
		}
	}

	return ret;
}

int
wps_ui_process_msg(char *buf, int buflen)
{
	if (strncmp(buf, "GET", 3) == 0) {
		wps_ui_do_get();
	}

	if (strncmp(buf, "SET ", 4) == 0) {
		return wps_ui_do_set(buf+4);
	}

	return WPS_CONT;
}

void
wps_setStaDevName(unsigned char *str)
{
	if (str) {
		wps_ui_set_env("wps_sta_devname", str);
		wps_set_conf("wps_sta_devname", str);
	}

	return;
}

int
wps_ui_init()
{
	/* Init eap_hndl */
	memset(&ui_hndl, 0, sizeof(ui_hndl));

	ui_hndl.type = WPS_RECEIVE_PKT_UI;
	ui_hndl.handle = wps_osl_ui_handle_init();
	if (ui_hndl.handle == -1)
		return -1;

	wps_hndl_add(&ui_hndl);
	return 0;
}

void
wps_ui_deinit()
{
	wps_hndl_del(&ui_hndl);
	wps_osl_ui_handle_deinit(ui_hndl.handle);
	return;
}
