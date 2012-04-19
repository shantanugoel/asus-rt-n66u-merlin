/*
 * Broadcom WPS inband UPnP interface
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ap_upnp_sm.h 279189 2011-08-23 10:14:21Z kenlo $
 */

#ifndef __AP_UPNP_SM_H__
#define __AP_UPNP_SM_H__

#include <eap_defs.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WPS_ROUTER
#include <security_ipc.h>
#endif /* WPS_ROUTER */


typedef struct {
	void *mc_dev;
	char ifname[16];
	int if_instance;
	bool m_waitForGetDevInfoResp;
	bool m_waitForPutMessageResp;
	char msg_to_send[WPS_EAP_DATA_MAX_LENGTH]; /* message to be sent */
	int msg_len;
	void (*init_wlan_event)(int, char *, int);
	void (*update_wlan_event)(int, unsigned char *, char *, int, int, char);
	uint32 (*send_data)(int, char *, uint32, int);
	char* (*parse_msg)(char *, int, int *, int *, char *);
} UPNP_WPS_AP_STATE;

/*
 * UPnP command between WPS and WFA device
 */

/* Functions */
uint32 ap_upnp_sm_init(void *mc_dev, int pmr_trigger, void *init_wlan_event,
	void *update_wlan_event, void *send_data, void *parse_msg, int instance);
uint32 ap_upnp_sm_deinit(void);
uint32 ap_upnp_sm_sendMsg(char *dataBuffer, uint32 dataLen);
uint32 ap_upnp_sm_process_msg(char *msg, int msg_len, unsigned long time);
int ap_upnp_sm_get_msg_to_send(char **data);


#ifdef __cplusplus
}
#endif

#endif	/* __AP_UPNP_SM_H__ */
