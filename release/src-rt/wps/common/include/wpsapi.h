/*
 * WPS API
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpsapi.h 286534 2011-09-28 02:27:25Z gracecsm $
 */

#ifndef _WPSAPI_
#define _WPSAPI_

#ifdef __cplusplus
extern "C" {
#endif

#include <reg_protomsg.h>
#include <wps_devinfo.h>

typedef struct {
	void *bcmwps;
	/* pointers to classes */
	CRegProtocol *mp_regProt;
	RegSM *mp_regSM;
	EnrSM *mp_enrSM;

	LPLIST mp_regInfoList; /* store DevInfo pointer */
	LPLIST mp_enrInfoList; /* store DevInfo pointer */

	bool mb_initialized;
	bool mb_stackStarted;
	bool mb_requestedPwd;
	devcfg *mp_info;

	bool m_b_configured;
	bool m_b_selRegistrar;
	uint16 m_devPwdId;
	uint16 m_selRegCfgMethods;

	/*
	 * Data to be stored for app configuration
	 * Will currently only handle one registration instance at
	 * a time. To enable multiple registrations to occur
	 * simultaneously, store each of these settings in a list
	 */
	CTlvEsM7Ap *mp_tlvEsM7Ap;
	TlvEsM7Enr *mp_tlvEsM7Enr;
	CTlvEsM8Ap *mp_tlvEsM8Ap;
	CTlvEsM8Sta *mp_tlvEsM8Sta;
	uint8 m_peerMacAddr[SIZE_6_BYTES];

	bool gb_regDone;
	bool b_UpnpDevGetDeviceInfo;
	uint8 pre_enrolleeNonce[SIZE_128_BITS];
	uint8 pre_privKey[SIZE_PUB_KEY];
} WPSAPI_T;

/* callback threads */
void * wps_StaticCBThreadProc(void *p_data);
void * wps_ActualCBThreadProc(void);
void wps_KillCallbackThread(void);
void * wps_SetSelectedRegistrarTimerThread(IN void *p_data);

uint32 wps_ProcessBeaconIE(char *ssid, uint8 *macAddr, uint8 *p_data, uint32 len);
uint32 wps_ProcessProbeReqIE(uint8 *macAddr, uint8 *p_data, uint32 len);
uint32 wps_ProcessProbeRespIE(uint8 *macAddr, uint8 *p_data, uint32 len);
void wps_printPskValue(char *nwKey, uint32 nwKeyLen);

void * wps_init(void *bcmwps, devcfg *ap_devcfg, DevInfo *ap_devinfo);
uint32 wps_deinit(WPSAPI_T *g_mc);
int wps_getenrState(void *mc_dev);
int wps_getregState(void *mc_dev);

bool wps_IsPinEntered(void);
char *wps_GetDevPwd(uint32 *len);
uint32 wps_sendStartMessage(void *bcmdev, TRANSPORT_TYPE trType);
int wps_get_upnpDevSSR(WPSAPI_T *g_mc, void *p_cbData, uint32 length, CTlvSsrIE *ssrmsg);
int wps_upnpDevSSR(WPSAPI_T *g_mc, CTlvSsrIE *ssrmsg);

void wps_setProcessStates(int state);
void wps_setStaDevName(unsigned char *str);

#ifdef __cplusplus
}
#endif


#endif /* _WPSAPI_ */
