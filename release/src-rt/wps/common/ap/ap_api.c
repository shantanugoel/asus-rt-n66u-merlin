/*
 * WPS AP API
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ap_api.c 286534 2011-09-28 02:27:25Z gracecsm $
 */

#if !defined(__linux__) && !defined(__ECOS) && !defined(TARGETOS_nucleus) && \
	!defined(_MACOSX_)&& !defined(TARGETOS_symbian)
#include <stdio.h>
#include <windows.h>
#endif

#if defined(__linux__) || defined(__ECOS)
#include <string.h>
#include <ctype.h>
#endif

#include <bn.h>
#include <wps_dh.h>

#include <portability.h>
#include <wpsheaders.h>
#include <wpscommon.h>
#include <sminfo.h>
#include <wpserror.h>
#include <reg_protomsg.h>
#include <reg_proto.h>
#include <statemachine.h>
#include <tutrace.h>

#include <slist.h>

#include <wps_wl.h>
#include <wps_apapi.h>
#include <wpsapi.h>
#include <ap_eap_sm.h>
#ifdef WPS_UPNP_DEVICE
#include <ap_upnp_sm.h>
#endif
#include <ap_ssr.h>

#ifdef __ECOS
#include <sys/socket.h>
#endif


/* under m_xxx in mbuf.h */

int wps_setWpsIE(void *bcmdev, uint8 scState, uint8 selRegistrar,
	uint16 devPwdId, uint16 selRegCfgMethods, uint8 *authorizedMacs_buf,
	int authorizedMacs_len);
#ifndef WPS_ROUTER
static uint32 wps_setBeaconIE(WPSAPI_T *g_mc, bool b_configured,
	bool b_selRegistrar, uint16 devPwdId, uint16 selRegCfgMethods,
	uint8 *authorizedMacs_buf, int authorizedMacs_len);
static uint32 wps_setProbeRespIE(WPSAPI_T *g_mc, uint8 respType, uint8 scState,
	bool b_selRegistrar, uint16 devPwdId, uint16 selRegCfgMethods,
	uint8 *authorizedMacs_buf, int authorizedMacs_len);
#endif /* !WPS_ROUTER */
static uint32 wps_switchModeOn(WPSAPI_T *g_mc, EMode e_mode);
static uint32 wps_switchModeOff(WPSAPI_T *g_mc, EMode e_mode);
static uint32 wps_genPsk(WPSAPI_T *g_mc);
static uint32 wps_createM8AP(WPSAPI_T *g_mc, char *cp_ssid);
static uint32 wps_createM8Sta(WPSAPI_T *g_mc);
static uint32 wps_startStack(WPSAPI_T *g_mc);
static uint32 wps_stopStack(WPSAPI_T *g_mc);
static uint32 wps_sendSetSelReg(WPSAPI_T *g_mc, IN bool b_setSelReg);
static uint32 wps_sendMsg(void *mcdev, TRANSPORT_TYPE trType, char * dataBuffer, uint32 dataLen);

static uint8 empty_enrolleeNonce[SIZE_128_BITS] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

char* wps_msg_type_str(int msgType)
{
	switch (msgType) {
		case 0x4:
			return "M1";
		case 0x5:
			return "M2";
		case 0x6:
			return "M2D";
		case 0x7:
			return "M3";
		case 0x8:
			return "M4";
		case 0x9:
			return "M5";
		case 0xa:
			return "M6";
		case 0xb:
			return "M7";
		case 0xc:
			return "M8";
		case 0xd:
			return "ACK";
		case 0xe:
			return "NACK";
		case 0xf:
			return "DONE";
		default:
			return "UNKNOWN";
	}
}


/*
 * Name        : wps_new
 * Description : Class constructor.
 * Arguments   : none
 * Return type : none
 */
static WPSAPI_T*
wps_new()
{
	WPSAPI_T *g_mc = NULL;
	g_mc = (WPSAPI_T *)malloc(sizeof(WPSAPI_T));

	if (!g_mc) {
		TUTRACE((TUTRACE_INFO, "WPSAPI constructor fail!!!\n"));
		return 0;
	}

	memset(g_mc, 0, sizeof(WPSAPI_T));

	TUTRACE((TUTRACE_INFO, "WPSAPI constructor\n"));
	g_mc->mb_initialized = false;
	g_mc->mb_stackStarted = false;
	g_mc->mb_requestedPwd = false;

	g_mc->mp_regProt  = NULL;
	g_mc->mp_regSM = NULL;
	g_mc->mp_enrSM =  NULL;
	g_mc->mp_info = NULL;

	g_mc->mp_regInfoList = NULL;
	g_mc->mp_enrInfoList = NULL;

	g_mc->mp_tlvEsM7Ap = NULL;
	g_mc->mp_tlvEsM7Enr = NULL;
	g_mc->mp_tlvEsM8Ap = NULL;
	g_mc->mp_tlvEsM8Sta = NULL;

	memset(&g_mc->m_peerMacAddr, 0, sizeof(g_mc->m_peerMacAddr));
	g_mc->m_b_configured = true;
	g_mc->b_UpnpDevGetDeviceInfo = false;

	return g_mc;
}

/*
 * Name        : wps_delete
 * Description : Class destructor. Cleanup if necessary.
 * Arguments   : none
 * Return type : none
 */
static void
wps_delete(WPSAPI_T *g_mc)
{
	TUTRACE((TUTRACE_INFO, "WPSAPI destructor\n"));

	free(g_mc);
}

/*
 * Name        : Init
 * Description : Initialize member variables
 * Arguments   : none
 * Return type : uint32 - result of the initialize operation
 */
void *
wps_init(void *bcmwps, devcfg *ap_devcfg, DevInfo *ap_devinfo)
{
	WPSAPI_T *gp_mc = NULL;
	devcfg *devcfg = NULL;

	/* initialize WPSAPI */
	gp_mc = wps_new();
	if (gp_mc) {
		/* If stack is started, then it must also be initialized */
		if (gp_mc->mb_initialized) {
			TUTRACE((TUTRACE_INFO, "MC_Init: Already initialized\n"));
			goto error;
		}

		/* Instantiate devcfg */
		gp_mc->mp_info = devcfg_new();
		if (gp_mc->mp_info == NULL) {
			TUTRACE((TUTRACE_ERR, "MC_Init::Init: devcfg_new failed\n"));
			goto error;
		}

		gp_mc->mp_regProt = (CRegProtocol *)alloc_init(sizeof(CRegProtocol));
		if (!gp_mc->mp_regProt)
			goto error;

		/* copy user provided DevInfo to mp_deviceInfo */
		memcpy(gp_mc->mp_info->mp_deviceInfo, ap_devinfo, sizeof(DevInfo));

		/* copy user provided devcfg to mp_info */
		devcfg = gp_mc->mp_info;
		devcfg->e_mode = ap_devcfg->e_mode;
		devcfg->mb_regWireless = ap_devcfg->mb_regWireless;
		devcfg->mb_useUpnp = ap_devcfg->mb_useUpnp;
		devcfg->mb_useUsbKey = ap_devcfg->mb_useUsbKey;
		devcfg->m_nwKeyLen = ap_devcfg->m_nwKeyLen;
		wps_strncpy(devcfg->m_nwKey, ap_devcfg->m_nwKey, sizeof(devcfg->m_nwKey));
		devcfg->mb_nwKeySet = ap_devcfg->mb_nwKeySet;
		devcfg->m_wepKeyIdx = ap_devcfg->m_wepKeyIdx;

		TUTRACE((TUTRACE_INFO, "MC_Init: Init complete ok\n"));

		gp_mc->mb_initialized = true;
		gp_mc->gb_regDone = false;

		/* copy prebuild enrollee noce and private key */
		memcpy(gp_mc->pre_enrolleeNonce, ap_devcfg->pre_enrolleeNonce, SIZE_128_BITS);
		if (memcmp(gp_mc->pre_enrolleeNonce, empty_enrolleeNonce, SIZE_128_BITS)) {
			memcpy(gp_mc->pre_privKey, ap_devcfg->pre_privKey, SIZE_PUB_KEY);
		}

		/* WSC 2.0, copy peer MAC address */
		memcpy(gp_mc->m_peerMacAddr, ap_devcfg->m_peerMacAddr, SIZE_6_BYTES);

		TUTRACE((TUTRACE_INFO, "MC_Init::MC intialized ok\n"));
	}
	else {
		TUTRACE((TUTRACE_ERR, "MC_Init::Init: MC initialization failed\n"));
		goto error;
	}

	/* Everything's initialized ok */

	gp_mc->bcmwps = bcmwps;
	if (WPS_SUCCESS == wps_startStack(gp_mc)) {
		TUTRACE((TUTRACE_INFO, "MC_Init:: Stack started ok\n"));
	}
	else {
		TUTRACE((TUTRACE_INFO, "Unable to start the stack\n"));
		goto error;
	}

	return (void *)gp_mc;

error:

	TUTRACE((TUTRACE_ERR, "MC_Init::Init failed\n"));
	if (gp_mc) {
		wps_deinit(gp_mc);
	}

	return 0;
}

/*
 * Name        : DeInit
 * Description : Deinitialize member variables
 * Arguments   : none
 * Return type : uint32
 */
uint32
wps_deinit(WPSAPI_T *g_mc)
{
	if (g_mc) {
		if (!g_mc->mb_initialized) {
			TUTRACE((TUTRACE_INFO, "MC_DeInit: Not initialized\n"));
			return WPS_ERR_NOT_INITIALIZED;
		}

		/* Stop stack anyway to free DH key if existing */
		wps_stopStack(g_mc);

		free(g_mc->mp_regProt);
		devcfg_delete(g_mc->mp_info);

		/*
		 * These data objects should have been already deleted,
		 * but double-check to make sure
		 */
		if (g_mc->mp_tlvEsM7Ap)
			reg_msg_m7ap_del(g_mc->mp_tlvEsM7Ap, 0);
		if (g_mc->mp_tlvEsM7Enr)
			reg_msg_m7enr_del(g_mc->mp_tlvEsM7Enr, 0);
		if (g_mc->mp_tlvEsM8Ap)
			reg_msg_m8ap_del(g_mc->mp_tlvEsM8Ap, 0);
		if (g_mc->mp_tlvEsM8Sta)
			reg_msg_m8sta_del(g_mc->mp_tlvEsM8Sta, 0);

		g_mc->mb_initialized = false;
		wps_delete(g_mc);
		return WPS_SUCCESS;
	}

	TUTRACE((TUTRACE_INFO, "apApi::MC deinitialized\n"));

	return WPS_SUCCESS;
}

/*
 * Name        : StartStack
 * Description : Once stack is initialized, start operations.
 * Arguments   : none
 * Return type : uint32 - result of the operation
 */
static uint32
wps_startStack(WPSAPI_T *g_mc)
{
	uint32 ret = WPS_SUCCESS;
	EMode e_mode;

	if (!g_mc->mb_initialized) {
		TUTRACE((TUTRACE_INFO, "MC_StartStack: Not initialized\n"));
		return WPS_ERR_NOT_INITIALIZED;
	}
	if (g_mc->mb_stackStarted) {
		TUTRACE((TUTRACE_INFO, "MC_StartStack: Already started\n"));
		/*
		 * Restart Stack to make enrollee or registrar SM restart,
		 * otherwise the enrollee or registrar SM will stay in old state and handle
		 * new message.
		 */
		wps_stopStack(g_mc);
	}

	/*
	 * Stack initialized now. Can be started.
	 * Create enrollee and registrar lists
	 */
	TUTRACE((TUTRACE_INFO, "MC_StartStack: 1\n"));
	g_mc->mp_regInfoList = list_create();

	if (!g_mc->mp_regInfoList) {
		TUTRACE((TUTRACE_ERR, "MC_Init: g_mc->mp_regInfoList not created"));
		ret = MC_ERR_STACK_NOT_STARTED;
	}
	g_mc->mp_enrInfoList = list_create();
	if (!g_mc->mp_enrInfoList) {
		TUTRACE((TUTRACE_ERR, "MC_Init: g_mc->mp_enrInfoList not created"));
		ret = MC_ERR_STACK_NOT_STARTED;
	}

	TUTRACE((TUTRACE_INFO, "MC_StartStack: 2\n"));

	if (WPS_SUCCESS != ret) {
		TUTRACE((TUTRACE_INFO, "MC_StartStack: could not create lists\n"));
		if (g_mc->mp_regInfoList)
			list_delete(g_mc->mp_regInfoList);
		if (g_mc->mp_enrInfoList)
			list_delete(g_mc->mp_enrInfoList);
		return ret;
	}
	g_mc->mb_requestedPwd = false;

	e_mode = devcfg_getConfiguredMode(g_mc->mp_info);
	TUTRACE((TUTRACE_INFO, "MC_StartStack: 4\n"));
	if (WPS_SUCCESS != wps_switchModeOn(g_mc, e_mode)) {
		TUTRACE((TUTRACE_INFO, "MC_StartStack: Could not start trans\n"));
		return MC_ERR_STACK_NOT_STARTED;
	}

	g_mc->mb_stackStarted = true;

	ret = WPS_SUCCESS;
	TUTRACE((TUTRACE_INFO, "MC_StartStack: Informing app of mode\n"));

	return ret;
}

/*
 * Name        : StopStack
 * Description : Stop the stack.
 * Arguments   : none
 * Return type : uint32 - result of the operation
 */
static uint32
wps_stopStack(WPSAPI_T *g_mc)
{
	LISTITR m_listItr;
	LPLISTITR listItr;

	if (!g_mc->mb_initialized)
		return WPS_ERR_NOT_INITIALIZED;

	wps_switchModeOff(g_mc, devcfg_getConfiguredMode(g_mc->mp_info));

	/* Delete all old data from Registrar and Enrollee Info lists */
	if (g_mc->mp_regInfoList) {
		DevInfo *p_deviceInfo;
		listItr = list_itrFirst(g_mc->mp_regInfoList, &m_listItr);

		while ((p_deviceInfo = (DevInfo *)list_itrGetNext(listItr))) {
			free(p_deviceInfo);
		}
		list_delete(g_mc->mp_regInfoList);
	}
	if (g_mc->mp_enrInfoList) {
		DevInfo *p_deviceInfo;
		listItr = list_itrFirst(g_mc->mp_enrInfoList, &m_listItr);

		while ((p_deviceInfo = (DevInfo *)list_itrGetNext(listItr))) {
			free(p_deviceInfo);
		}
		list_delete(g_mc->mp_enrInfoList);
	}
	g_mc->mb_stackStarted = false;
	return WPS_SUCCESS;
}

/* Called by the app to initiate registration */
static uint32
wps_initReg(void *mc_dev, IN EMode e_currMode, IN char *devPwd, IN uint16 devPwdId,
	IN uint8 *authorizedMacs, IN uint32 authorizedMacs_len)
{
	uint32 ret = WPS_SUCCESS;
	WPSAPI_T *g_mc = (WPSAPI_T *)mc_dev;
	char *cp_data;
	uint16 data16;
	uint8 data8;
	uint32 len;
	char *cp_devPwd;

	switch (e_currMode) {
	case EModeUnconfAp:
	{
		uint8 *p_macAddr;
		CTlvNwKey *p_tlvKey;
		char *cp_psk;

		/*
		 * Reset the local device info in the SM if it has changed
		 * SetDevPwdId() will return MC_ERR_VALUE_UNCHANGED if the value
		 * is the same, WPS_SUCCESS if the value was different and the set
		 * succeeded
		 */
		if (WPS_SUCCESS == devcfg_setDevPwdId(g_mc->mp_info, devPwdId)) {
			/* Reset local device info in SM */
			state_machine_update_devinfo(g_mc->mp_enrSM->m_sm,
				devcfg_getDeviceInfo(g_mc->mp_info));
		}
		/* g_mc->mp_enrSM->m_stophandlemsg_enr = 0; */
		/*
		 * AP wants to be configured/managed
		 * Let the Enrollee SM know, so that it can be initialized
		 *
		 * Create an Encrypted Settings TLV to be passed to the SM,
		 * so that the SM can pass it to the Registrar in M7
		 * Also store this TLV locally, so we can delete it once the
		 * registration completes. The SM will not delete it.
		 */
		if (g_mc->mp_tlvEsM7Ap)
			reg_msg_m7ap_del(g_mc->mp_tlvEsM7Ap, 0);

		g_mc->mp_tlvEsM7Ap = reg_msg_m7ap_new();
		if (!g_mc->mp_tlvEsM7Ap)
			return WPS_ERR_SYSTEM;

		/* SSID */
		cp_data = devcfg_getSSID(g_mc->mp_info, &data16);
		tlv_set(&g_mc->mp_tlvEsM7Ap->ssid, WPS_ID_SSID, (uint8 *)cp_data, data16);

		/* MAC addr:  should really get this from the NIC driver */
		p_macAddr = devcfg_getMacAddr(g_mc->mp_info);
		data16 = SIZE_MAC_ADDR;
		tlv_set(&g_mc->mp_tlvEsM7Ap->macAddr, WPS_ID_MAC_ADDR, p_macAddr, data16);


		TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM7Ap: "
			"do REAL sec config here...\n"));
		if (devcfg_getAuth(g_mc->mp_info))
			data16 = WPS_AUTHTYPE_SHARED;
		else if (devcfg_getKeyMgmtType(g_mc->mp_info) == WPS_WL_AKM_PSK)
			data16 = WPS_AUTHTYPE_WPAPSK;
		else if (devcfg_getKeyMgmtType(g_mc->mp_info) == WPS_WL_AKM_PSK2)
			data16 = WPS_AUTHTYPE_WPA2PSK;
		else if (devcfg_getKeyMgmtType(g_mc->mp_info) == WPS_WL_AKM_BOTH)
			data16 = WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK;
		else
			data16 = WPS_AUTHTYPE_OPEN;
		tlv_set(&g_mc->mp_tlvEsM7Ap->authType, WPS_ID_AUTH_TYPE,
			(void *)((uint32)data16), 0);

		TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM7Ap: "
			"Auth type = %x...\n", data16));

		/* encrType */
		if (devcfg_getAuth(g_mc->mp_info))
			data16 = WPS_ENCRTYPE_WEP;
		else if (data16 == WPS_AUTHTYPE_OPEN) {
			if (devcfg_getWep(g_mc->mp_info))
				data16 = WPS_ENCRTYPE_WEP;
			else
				data16 = WPS_ENCRTYPE_NONE;
		}
		else
			data16 = devcfg_getCrypto(g_mc->mp_info);

		tlv_set(&g_mc->mp_tlvEsM7Ap->encrType, WPS_ID_ENCR_TYPE,
			(void *)((uint32)data16), 0);

		TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM7Ap: "
			"encr type = %x...\n", data16));

		if (data16 != WPS_ENCRTYPE_NONE) {
			char *dev_key = NULL;
			uint32	KeyLen;

			if (data16 == WPS_ENCRTYPE_WEP)
				tlv_set(&g_mc->mp_tlvEsM7Ap->wepIdx, WPS_ID_WEP_TRANSMIT_KEY,
					(void *)((uint32)devcfg_getWepIdx(g_mc->mp_info)), 0);

			dev_key = devcfg_getNwKey(g_mc->mp_info, &KeyLen);

			/* nwKey */
			p_tlvKey = (CTlvNwKey *)tlv_new(WPS_ID_NW_KEY);
			if (!p_tlvKey)
				return WPS_ERR_SYSTEM;

			if (tlv_allocate(p_tlvKey, WPS_ID_NW_KEY, KeyLen) == -1) {
				tlv_delete(p_tlvKey, 0);
				return WPS_ERR_SYSTEM;
			}

			cp_psk = p_tlvKey->m_data;
			memset(cp_psk, 0, KeyLen);

			memcpy(cp_psk, dev_key, KeyLen);
#ifdef BCMWPS_DEBUG_PRINT_KEY
			TUTRACE((TUTRACE_INFO, 	"InitiateRegistration_NW_KEY get from AP is key "
				"%p %s\n",  p_tlvKey, cp_psk));
#endif
		}
		else {
			/* nwKey */
			p_tlvKey = (CTlvNwKey *)tlv_new(WPS_ID_NW_KEY);
			if (!p_tlvKey)
				return WPS_ERR_SYSTEM;

			if (tlv_allocate(p_tlvKey, WPS_ID_NW_KEY, 0) == -1) {
				tlv_delete(p_tlvKey, 0);
				return WPS_ERR_SYSTEM;
			}
		}
		if (!list_addItem(g_mc->mp_tlvEsM7Ap->nwKey, p_tlvKey)) {
			tlv_delete(p_tlvKey, 0);
			return WPS_ERR_SYSTEM;
		}

		/*
		 * Call into the State Machine
		 * device password is auto-generated for Enrollees - use
		 * locally-stored password
		 */
		cp_devPwd = devPwd;
		len = (uint32) strlen(devPwd);

		TUTRACE((TUTRACE_INFO, "MC_InitiateRegistration: devPwd = %s, Len = %d \n",
			cp_devPwd, len));
		/* Call in to the SM */
		if (WPS_SUCCESS != enr_sm_initializeSM(g_mc->mp_enrSM, NULL, NULL,
			(void *)g_mc->mp_tlvEsM7Ap, cp_devPwd, len)) {
			TUTRACE((TUTRACE_ERR, "Can't init enr_sm\n"));
			return WPS_ERR_OUTOFMEMORY;
		}

		/* update enrolleeNonce with prebuild_enrollee nonce */
		if (memcmp(g_mc->pre_enrolleeNonce, empty_enrolleeNonce, SIZE_128_BITS)) {
			state_machine_set_enrolleeNonce(g_mc->mp_enrSM->m_sm,
				g_mc->pre_enrolleeNonce);
		}

		g_mc->mp_enrSM->m_ptimerStatus_enr = 0;

		/* Send the right IEs down to the transport */
		g_mc->m_b_selRegistrar = false;
		g_mc->m_devPwdId = devPwdId;
		g_mc->m_selRegCfgMethods =
			devcfg_getConfigMethods(g_mc->mp_info);
		data8 = devcfg_getscState(g_mc->mp_info);
		if (data8 == WPS_SCSTATE_CONFIGURED)
			g_mc->m_b_configured = true;
		else
			g_mc->m_b_configured = false;

		/* WSC 2.0, no authorizedMacs */
		wps_setWpsIE(g_mc,
			data8,
			g_mc->m_b_selRegistrar,
			g_mc->m_devPwdId,
			g_mc->m_selRegCfgMethods,
			NULL,
			0);
		break;
	}

	case EModeApProxyRegistrar:
	case EModeRegistrar:
	{
		bool b_setSelRegNeeded;

		/*
		 * Reset the local device info in the SM if it has changed
		 * SetDevPwdId() will return MC_ERR_VALUE_UNCHANGED if the value
		 * is the same, WPS_SUCCESS if the value was different and the set
		 * succeeded
		 */
		if (WPS_SUCCESS == devcfg_setDevPwdId(g_mc->mp_info, devPwdId)) {
			/* Reset local device info in SM */
			state_machine_update_devinfo(g_mc->mp_regSM->m_sm,
				devcfg_getDeviceInfo(g_mc->mp_info));
		}

		/* g_mc->mp_regSM->m_stophandlemsg = 0; */
		/* reset the timer status */
		g_mc->mp_regSM->m_ptimerStatus = 0;
		/* Registrar needs to configure AP or add Enrollee */

		/*
		 * Added to support SelectedRegistrar
		 * Also set g_mc->me_modeTarget, so if the password
		 * comes down later, we can ask the transport to
		 * send a UPnP message to the AP
		 */

		/* [Added to support SelectedRegistrar] */
		b_setSelRegNeeded = false;

		/* Check to see if the devPwd from the app is valid */
		if (!devPwd) {
			/* App sent in a NULL, and USB key isn't sent */
			TUTRACE((TUTRACE_ERR, "MC_InitiateRegistration: "
				"No password to use, expecting one\n"));
			ret = WPS_ERR_SYSTEM;
			break;
		}
		else {
			if (strlen(devPwd)) {
				TUTRACE((TUTRACE_ERR, "Using pwd from app\n"));

				/* Pass the device password on to the SM */
				state_machine_set_passwd(g_mc->mp_regSM->m_sm, devPwd,
					(uint32)(strlen(devPwd)));
				b_setSelRegNeeded = true;
			}
		}

		/*
		 * Added to support SelectedRegistrar for mode where
		 * registrar is configuring a client via a proxy AP
		 */
		if ((EModeRegistrar == e_currMode) &&
			b_setSelRegNeeded) {
			/*
			 * Ask transport to send a UPnP message to the AP
			 * indicating that this registrar has been selected
			 */
			TUTRACE((TUTRACE_INFO, "MC_InitiateRegistration: "
				"Asking transport to inform AP about SSR\n"));

			wps_sendSetSelReg(g_mc, b_setSelRegNeeded);
		}
		/* add for update selected reg */
		else if ((EModeApProxyRegistrar == e_currMode) &&
			b_setSelRegNeeded)
		{
			/* Send the right IEs down to the transport */
			g_mc->m_b_selRegistrar = true;
			g_mc->m_devPwdId = devPwdId;
			g_mc->m_selRegCfgMethods =
				devcfg_getConfigMethods(g_mc->mp_info);
			data8 = devcfg_getscState(g_mc->mp_info);
			if (data8 == WPS_SCSTATE_CONFIGURED)
				g_mc->m_b_configured = true;
			else
				g_mc->m_b_configured = false;

			/* WSC 2.0 AuthorizedMacs */
			wps_setWpsIE(g_mc,
				data8,
				g_mc->m_b_selRegistrar,
				g_mc->m_devPwdId,
				g_mc->m_selRegCfgMethods,
				authorizedMacs,
				authorizedMacs_len);
		}
		break;
	}

	case EModeApProxy:
	{
		/* Nothing to be done here */
		break;
	}

	default:
	{
		break;
	}
	}

	return ret;
}

/*
 * Name        : SwitchModeOn
 * Description : Switch to the specified mode of operation.
 *                 Turn off other modes, if necessary.
 * Arguments   : IN EMode e_mode - mode of operation to switch to
 * Return type : uint32 - result of the operation
 */
static uint32
wps_switchModeOn(WPSAPI_T *g_mc, IN EMode e_mode)
{
	uint32 ret = WPS_SUCCESS;
	/* Create DH Key Pair */
	DH *p_dhKeyPair = NULL;
	BufferObj *bo_pubKey = NULL;
	BufferObj *bo_sha256Hash = NULL;

	if ((bo_pubKey = buffobj_new()) == NULL)
		return WPS_ERR_SYSTEM;

	if ((bo_sha256Hash = buffobj_new()) == NULL) {
		buffobj_del(bo_pubKey);
		return WPS_ERR_SYSTEM;
	}

	TUTRACE((TUTRACE_INFO, "MC_SwitchModeOn: 1\n"));

	if (memcmp(g_mc->pre_enrolleeNonce, empty_enrolleeNonce, SIZE_128_BITS)) {
		ret = reg_proto_generate_prebuild_dhkeypair(&p_dhKeyPair, bo_pubKey,
			g_mc->pre_privKey);

		TUTRACE((TUTRACE_INFO, "Generate public key using pre-build secret\n"));
	}
	else {
		ret = reg_proto_generate_dhkeypair(&p_dhKeyPair, bo_pubKey);
		TUTRACE((TUTRACE_INFO, "generate random public key\n"));
	}

	if (WPS_SUCCESS != ret) {
		buffobj_del(bo_sha256Hash);
		buffobj_del(bo_pubKey);
		if (p_dhKeyPair)
			DH_free(p_dhKeyPair);

		TUTRACE((TUTRACE_ERR,
			"MC_SwitchModeOn: Could not generate DH Key\n"));
		return ret;
	}
	else {
		TUTRACE((TUTRACE_INFO, "MC_SwitchModeOn: 2\n"));
		/* DH Key generated Now generate the SHA 256 hash */
		reg_proto_generate_sha256hash(bo_pubKey, bo_sha256Hash);

		/* Store this info in g_mc->mp_info */
		devcfg_setDHKeyPair(g_mc->mp_info, p_dhKeyPair);

		TUTRACE((TUTRACE_ERR, "g_mc->mp_info->mp_dhKeyPair = %p\n",
			g_mc->mp_info->mp_dhKeyPair));

		if (WPS_SUCCESS != (devcfg_setPubKey(g_mc->mp_info, bo_pubKey))) {
			TUTRACE((TUTRACE_ERR,
				"MC_SwitchModeOn: Could not set pubKey\n"));
			ret = WPS_ERR_SYSTEM;
		}
		if (WPS_SUCCESS != (devcfg_setSHA256Hash(g_mc->mp_info, bo_sha256Hash))) {
			TUTRACE((TUTRACE_ERR,
				"MC_SwitchModeOn: Could not set sha256Hash\n"));
			ret = WPS_ERR_SYSTEM;
		}
		buffobj_del(bo_sha256Hash);
		buffobj_del(bo_pubKey);

		if (WPS_ERR_SYSTEM == ret) {
			return ret;
		}
	}
	TUTRACE((TUTRACE_INFO, "MC_SwitchModeOn: 3\n"));
	/* Now perform mode-specific startup */
	switch (e_mode) {
		/* Make sure we have the config data to work with */
		case EModeUnconfAp:
		{
			TUTRACE((TUTRACE_INFO, "MC_SwitchModeOn: 4\n"));
			/* Instantiate the Enrollee SM */
			g_mc->mp_enrSM = enr_sm_new(g_mc, g_mc->mp_regProt);
			if (!g_mc->mp_enrSM) {
				TUTRACE((TUTRACE_ERR, "MC_SwitchModeOn: Could not allocate "
					"g_mc->mp_enrSM\n"));
				ret = WPS_ERR_OUTOFMEMORY;
				break;
			}
			state_machine_set_local_devinfo(g_mc->mp_enrSM->m_sm,
				devcfg_getDeviceInfo(g_mc->mp_info), NULL, p_dhKeyPair);

			/* Send the right IEs down to the transport */
			if (devcfg_getscState(g_mc->mp_info) == 0x2)
				g_mc->m_b_configured = true;
			else
				g_mc->m_b_configured = false;
			g_mc->m_b_selRegistrar = false;
			g_mc->m_devPwdId = 0;
			g_mc->m_selRegCfgMethods = 0;
			break;
		}

		case EModeApProxyRegistrar:
		{
			TUTRACE((TUTRACE_INFO,
				"MC_SwitchModeOn: EModeApProxyRegistrar enter\n"));

			/* Instantiate the Registrar SM */
			g_mc->mp_regSM = reg_sm_new(g_mc, g_mc->mp_regProt);
			if (!g_mc->mp_regSM)
			{
				TUTRACE((TUTRACE_ERR, "MC_SwitchModeOn: Could not allocate "
					"g_mc->mp_regSM\n"));
				ret = WPS_ERR_OUTOFMEMORY;
				break;
			}

			state_machine_set_local_devinfo(g_mc->mp_regSM->m_sm,
				devcfg_getDeviceInfo(g_mc->mp_info), NULL, p_dhKeyPair);

			/* First check if we have one from the config file */
			if (!(devcfg_isKeySet(g_mc->mp_info)))
			{
				if (WPS_SUCCESS != wps_genPsk(g_mc)) {
					ret = WPS_ERR_SYSTEM;
					break;
				}
			}
			else
			{
				/* Have a PSK to use Print out the value */
				uint32 len;
				char *nwKey = devcfg_getNwKey(g_mc->mp_info, &len);
				wps_printPskValue(nwKey, len);
			}

			/* Create the encrypted settings TLV for AP */
			ret = wps_createM8AP(g_mc, NULL);
			if (ret != WPS_SUCCESS)
				break;

			/* Create the encrypted settings TLV for STA */
			ret = wps_createM8Sta(g_mc);
			if (ret != WPS_SUCCESS)
				break;

			/* Initialize the Registrar SM */
			ret = reg_sm_initsm(g_mc->mp_regSM, NULL, true, true);
			if (ret != WPS_SUCCESS)
				break;

			/* Send the encrypted settings value to the SM */
			state_machine_set_encrypt(g_mc->mp_regSM->m_sm,
				(void *)g_mc->mp_tlvEsM8Sta,
				(void *)g_mc->mp_tlvEsM8Ap);

			/* Send the right IEs down to the transport */
			if (devcfg_getscState(g_mc->mp_info) == 0x2)
				g_mc->m_b_configured = true;
			else
				g_mc->m_b_configured = false;
			g_mc->m_b_selRegistrar = false;
			g_mc->m_devPwdId = 0;
			g_mc->m_selRegCfgMethods = 0;
			break;
		}

		case EModeApProxy:
		{
			TUTRACE((TUTRACE_INFO, "MC_SwitchModeOn: EModeApProxy enter\n"));

			/* Instantiate the Registrar SM */
			g_mc->mp_regSM = reg_sm_new(g_mc, g_mc->mp_regProt);
			if (!g_mc->mp_regSM) {
				TUTRACE((TUTRACE_ERR, "MC_SwitchModeOn: Could not allocate "
					"g_mc->mp_regSM\n"));
				ret = WPS_ERR_OUTOFMEMORY;
				break;
			}

			state_machine_set_local_devinfo(g_mc->mp_regSM->m_sm,
				devcfg_getDeviceInfo(g_mc->mp_info), NULL, p_dhKeyPair);

			if (!(devcfg_isKeySet(g_mc->mp_info))) {
				if (WPS_SUCCESS != wps_genPsk(g_mc)) {
					ret = WPS_ERR_SYSTEM;
					break;
				}
			}
			else {
				/* A PSK exists. Print out the value. */
				uint32 len = 0;
				char *nwKey = devcfg_getNwKey(g_mc->mp_info, &len);
				wps_printPskValue(nwKey, len);
			}

			/* Initialize the Registrar SM */
			ret = reg_sm_initsm(g_mc->mp_regSM, NULL, false, true);
			if (ret != WPS_SUCCESS)
				break;

			TUTRACE((TUTRACE_INFO, "MC_SwitchModeOn:Started WLAN "
						"Transport for AP+Proxy\n"));
			/*
			 * Send the right IEs down to the transport
			 * [added to support SelectedRegistrar]
			 * This will fail if the right drivers are not used
			 */
			if (devcfg_getscState(g_mc->mp_info) == 0x2)
				g_mc->m_b_configured = true;
			else
				g_mc->m_b_configured = false;
			break;
		}

		default:
		{
			TUTRACE((TUTRACE_ERR, "MC_SwitchModeOn: Unknown mode\n"));
			ret = WPS_ERR_GENERIC;
			break;
		}
	}

	TUTRACE((TUTRACE_INFO, "MC_SwitchModeOn: Exit\n"));
	return ret;
}


/*
 * Name        : SwitchModeOff
 * Description : Switch off the specified mode of operation.
 * Arguments   : IN EMode e_mode - mode of operation to switch off
 * Return type : uint32 - result of the operation
 */
static uint32
wps_switchModeOff(WPSAPI_T *g_mc, IN EMode e_mode)
{
	switch (e_mode) {
	case EModeUnconfAp:
		if (g_mc->mp_enrSM)
			enr_sm_delete(g_mc->mp_enrSM);
		break;

	case EModeRegistrar:
		if (g_mc->mp_regSM)
			reg_sm_delete(g_mc->mp_regSM);
		break;

	case EModeApProxyRegistrar:
		if (g_mc->mp_regSM)
			reg_sm_delete(g_mc->mp_regSM);
		break;

	case EModeApProxy:
		if (g_mc->mp_regSM)
			reg_sm_delete(g_mc->mp_regSM);
		break;

	default:
		TUTRACE((TUTRACE_ERR, "MC_SwitchModeOff: Unknown mode\n"));
		break;
	}

	if (g_mc->mp_info->mp_dhKeyPair) {
		DH_free(g_mc->mp_info->mp_dhKeyPair);
		g_mc->mp_info->mp_dhKeyPair = 0;

		TUTRACE((TUTRACE_ERR, "Free g_mc->mp_info->mp_dhKeyPair\n"));
	}

	return WPS_SUCCESS;
}

#ifndef WPS_ROUTER
/*
 * Name        : SetBeaconIE
 * Description : Push Beacon WPS IE information to transport
 * Arguments   : IN bool b_configured - is the AP configured?
 * IN bool b_selRegistrar - is this flag set?
 * IN uint16 devPwdId - valid if b_selRegistrar is true
 * IN uint16 selRegCfgMethods - valid if b_selRegistrar is true
 * Return type : uint32 - result of the operation
 */
static uint32
wps_setBeaconIE(WPSAPI_T *g_mc, IN bool b_configured, IN bool b_selRegistrar,
	IN uint16 devPwdId, IN uint16 selRegCfgMethods, uint8 *authorizedMacs_buf,
	int authorizedMacs_len)
{
	uint32 ret;
	uint8 data8;
	uint8 *p_data8;
	CTlvVendorExt vendorExt;

	BufferObj *bufObj, *vendorExt_bufObj;

	bufObj = buffobj_new();
	if (!bufObj)
		return WPS_ERR_SYSTEM;

	vendorExt_bufObj = buffobj_new();
	if (vendorExt_bufObj == NULL) {
		buffobj_del(bufObj);
		return WPS_ERR_SYSTEM;
	}

	/* Version */
	data8 = devcfg_getVersion(g_mc->mp_info);
	tlv_serialize(WPS_ID_VERSION, bufObj, &data8, WPS_ID_VERSION_S);

	/* Simple Config State */
	if (b_configured)
		data8 = WPS_SCSTATE_CONFIGURED;
	else
		data8 = WPS_SCSTATE_UNCONFIGURED;
	tlv_serialize(WPS_ID_SC_STATE, bufObj, &data8, WPS_ID_SC_STATE_S);

	/*
	 * AP Setup Locked - optional if false.  If implemented and TRUE, include this attribute.
	 * CTlvAPSetupLocked(WPS_ID_AP_SETUP_LOCKED, bufObj, &b_APSetupLocked);
	 *
	 * Selected Registrar - optional
	 * Add this TLV only if b_selRegistrar is true
	 */
	if (b_selRegistrar) {
		tlv_serialize(WPS_ID_SEL_REGISTRAR, bufObj, &b_selRegistrar,
			WPS_ID_SEL_REGISTRAR_S);
		/* Add in other related params as well Device Password ID */
		tlv_serialize(WPS_ID_DEVICE_PWD_ID, bufObj, &devPwdId, WPS_ID_DEVICE_PWD_ID_S);
		/* Selected Registrar Config Methods */
		tlv_serialize(WPS_ID_SEL_REG_CFG_METHODS, bufObj,
			&selRegCfgMethods, WPS_ID_SEL_REG_CFG_METHODS_S);
		/* Enrollee UUID removed */
	}

	data8 = devcfg_getRFBand(g_mc->mp_info);
	/* dual band, it's become require */
	if (data8 == 3) {
		p_data8 = devcfg_getUUID(g_mc->mp_info);
		tlv_serialize(WPS_ID_UUID_E, bufObj, p_data8, SIZE_UUID);
		tlv_serialize(WPS_ID_RF_BAND, bufObj, &data8, WPS_ID_RF_BAND_S);
	}

	/* WSC 2.0,  support WPS V2 or not */
	data8 = devcfg_getVersion2(g_mc->mp_info);
	if (data8 >= WPS_VERSION2) {
		/* WSC 2.0 */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&data8, WPS_WFA_SUBID_VERSION2_S);
		/* AuthorizedMACs */
		if (authorizedMacs_buf && authorizedMacs_len) {
			subtlv_serialize(WPS_WFA_SUBID_AUTHORIZED_MACS, vendorExt_bufObj,
				authorizedMacs_buf, authorizedMacs_len);
		}

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, bufObj);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
{
	char *pc_data;
	uint16 data16;

	/* New unknown attribute */
	pc_data = devcfg_getNewAttribute(g_mc->mp_info, &data16);
	if (data16 && pc_data)
		buffobj_Append(bufObj, data16, pc_data);
}
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Send a pointer to the serialized data to Transport */
	if (0 != wps_set_wps_ie(g_mc->bcmwps, bufObj->pBase, bufObj->m_dataLength,
		1 /* WPS_IE_TYPE_SET_BEACON_IE */ )) {
		TUTRACE((TUTRACE_ERR, "MC_SetBeaconIE: call to trans->SetBeaconIE() failed\n"));
		ret = WPS_ERR_GENERIC;
	}
	else {
		TUTRACE((TUTRACE_ERR, "MC_SetBeaconIE: call to trans->SetBeaconIE() ok\n"));
		ret = WPS_SUCCESS;
	}

	buffobj_del(bufObj);
	return ret;
}

/*
 * Name        : SetProbeRespIE
 * Description : Push WPS Probe Resp WPS IE information to transport
 * Arguments   :
 * Return type : uint32 - result of the operation
 */
static uint32
wps_setProbeRespIE(WPSAPI_T *g_mc, IN uint8 respType, IN uint8 scState,
	IN bool b_selRegistrar, IN uint16 devPwdId, IN uint16 selRegCfgMethods,
	uint8 *authorizedMacs_buf, int authorizedMacs_len)
{
	uint32 ret;
	BufferObj *bufObj, *vendorExt_bufObj;
	uint16 data16;
	uint8 data8, *p_data8;
	char *pc_data;
	CTlvPrimDeviceType tlvPrimDeviceType;
	CTlvVendorExt vendorExt;

	bufObj = buffobj_new();
	if (!bufObj)
		return WPS_ERR_SYSTEM;

	vendorExt_bufObj = buffobj_new();
	if (vendorExt_bufObj == NULL) {
		buffobj_del(bufObj);
		return WPS_ERR_SYSTEM;
	}

	/* Version */
	data8 = devcfg_getVersion(g_mc->mp_info);
	tlv_serialize(WPS_ID_VERSION, bufObj, &data8, WPS_ID_VERSION_S);

	/* Simple Config State */
	tlv_serialize(WPS_ID_SC_STATE, bufObj, &scState, WPS_ID_SC_STATE_S);

	/* AP Setup Locked - optional if false.  If implemented and TRUE, include this attribute. */
	/*
	 * tlv_serialize(WPS_ID_AP_SETUP_LOCKED, bufObj, &b_APSetupLocked,
	 * WPS_ID_AP_SETUP_LOCKED_S);
	 */

	/* Selected Registrar */
	tlv_serialize(WPS_ID_SEL_REGISTRAR, bufObj, &b_selRegistrar, WPS_ID_SEL_REGISTRAR_S);

	/*
	 * Selected Registrar Config Methods - optional, required if b_selRegistrar
	 * is true
	 * Device Password ID - optional, required if b_selRegistrar is true
	 * Enrollee UUID - optional, required if b_selRegistrar is true
	 */
	if (b_selRegistrar) {
		/* Device Password ID */
		tlv_serialize(WPS_ID_DEVICE_PWD_ID, bufObj, &devPwdId, WPS_ID_DEVICE_PWD_ID_S);
		/* Selected Registrar Config Methods */
		tlv_serialize(WPS_ID_SEL_REG_CFG_METHODS, bufObj, &selRegCfgMethods,
			WPS_ID_SEL_REG_CFG_METHODS_S);
		/* Per 1.0b spec, removed Enrollee UUID */
	}

	/* Response Type */
	tlv_serialize(WPS_ID_RESP_TYPE, bufObj, &respType, WPS_ID_RESP_TYPE_S);

	p_data8 = devcfg_getUUID(g_mc->mp_info);
	if (WPS_MSGTYPE_REGISTRAR == respType)
		data16 = WPS_ID_UUID_R; /* This case used for Registrars using ad hoc mode */
	else
		data16 = WPS_ID_UUID_E; /* This is the AP case, so use UUID_E. */
	tlv_serialize(data16, bufObj, p_data8, SIZE_UUID);

	/* Manufacturer */
	pc_data = devcfg_getManufacturer(g_mc->mp_info, &data16);
	tlv_serialize(WPS_ID_MANUFACTURER, bufObj, pc_data, data16);

	/* Model Name */
	pc_data = devcfg_getModelName(g_mc->mp_info, &data16);
	tlv_serialize(WPS_ID_MODEL_NAME, bufObj, pc_data, data16);

	/* Model Number */
	pc_data = devcfg_getModelNumber(g_mc->mp_info, &data16);
	tlv_serialize(WPS_ID_MODEL_NUMBER, bufObj, pc_data, data16);

	/* Serial Number */
	pc_data = devcfg_getSerialNumber(g_mc->mp_info, &data16);
	tlv_serialize(WPS_ID_SERIAL_NUM, bufObj, pc_data, data16);

	/*
	 * Primary Device Type
	 * This is a complex TLV, so will be handled differently
	 */
	tlvPrimDeviceType.categoryId = devcfg_getPrimDeviceCategory(g_mc->mp_info);
	tlvPrimDeviceType.oui = devcfg_getPrimDeviceOui(g_mc->mp_info);
	tlvPrimDeviceType.subCategoryId = devcfg_getPrimDeviceSubCategory(g_mc->mp_info);
	tlv_primDeviceTypeWrite(&tlvPrimDeviceType, bufObj);

	/* Device Name */
	pc_data = devcfg_getDeviceName(g_mc->mp_info, &data16);
	tlv_serialize(WPS_ID_DEVICE_NAME, bufObj, pc_data, data16);

	/* Config Methods */
	data16 = devcfg_getConfigMethods(g_mc->mp_info);
	tlv_serialize(WPS_ID_CONFIG_METHODS, bufObj, &data16, WPS_ID_CONFIG_METHODS_S);

	/* RF Bands - optional */
	data8 = devcfg_getRFBand(g_mc->mp_info);
	if (data8 == 3)
		tlv_serialize(WPS_ID_RF_BAND, bufObj, &data8, WPS_ID_RF_BAND_S);

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	data8 = devcfg_getVersion2(g_mc->mp_info);
	if (data8 >= WPS_VERSION2) {
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&data8, WPS_WFA_SUBID_VERSION2_S);

		/* AuthorizedMACs */
		if (authorizedMacs_buf && authorizedMacs_len) {
			subtlv_serialize(WPS_WFA_SUBID_AUTHORIZED_MACS, vendorExt_bufObj,
				authorizedMacs_buf, authorizedMacs_len);
		}

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, bufObj);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		pc_data = devcfg_getNewAttribute(g_mc->mp_info, &data16);
		if (data16 && pc_data)
			buffobj_Append(bufObj, data16, pc_data);
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Send a pointer to the serialized data to Transport */
	if (0 != wps_set_wps_ie(g_mc->bcmwps, bufObj->pBase, bufObj->m_dataLength,
		WPS_IE_TYPE_SET_PROBE_RESPONSE_IE)) {
		TUTRACE((TUTRACE_ERR, "wps_setProbeRespIE: call wps_set_wps_ie() failed\n"));
		ret = WPS_ERR_GENERIC;
	}
	else {
		TUTRACE((TUTRACE_ERR, "wps_setProbeRespIE: call wps_set_wps_ie() ok\n"));
		ret = WPS_SUCCESS;
	}

	/* bufObj will be destroyed when we exit this function */
	buffobj_del(bufObj);
	return ret;
}

static uint32
wps_setAssocRespIE(WPSAPI_T *g_mc, IN uint8 respType)
{
	uint32 ret;
	uint8 data8;
	CTlvVendorExt vendorExt;

	BufferObj *bufObj, *vendorExt_bufObj;


	bufObj = buffobj_new();
	if (!bufObj)
		return WPS_ERR_SYSTEM;

	vendorExt_bufObj = buffobj_new();
	if (vendorExt_bufObj == NULL) {
		buffobj_del(bufObj);
		return WPS_ERR_SYSTEM;
	}

	/* Version */
	data8 = devcfg_getVersion(g_mc->mp_info);
	tlv_serialize(WPS_ID_VERSION, bufObj, &data8, WPS_ID_VERSION_S);

	/* Response Type */
	tlv_serialize(WPS_ID_RESP_TYPE, bufObj, &respType, WPS_ID_RESP_TYPE_S);

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	data8 = devcfg_getVersion2(g_mc->mp_info);
	subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj, &data8,
		WPS_WFA_SUBID_VERSION2_S);

	/* Serialize subelemetns to Vendor Extension */
	vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
	vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
	vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
	tlv_vendorExtWrite(&vendorExt, bufObj);

	buffobj_del(vendorExt_bufObj);

#ifdef WFA_WPS_20_TESTBED
{
	char *pc_data;
	uint16 data16;

	/* New unknown attribute */
	pc_data = devcfg_getNewAttribute(g_mc->mp_info, &data16);
	if (data16 && pc_data)
		buffobj_Append(bufObj, data16, pc_data);
}
#endif /* WFA_WPS_20_TESTBED */
	/* Send a pointer to the serialized data to Transport */
	if (0 != wps_set_wps_ie(g_mc->bcmwps, bufObj->pBase, bufObj->m_dataLength,
		WPS_IE_TYPE_SET_ASSOC_RESPONSE_IE)) {
		TUTRACE((TUTRACE_ERR, "wps_setAssocRespIE: call wps_set_wps_ie() failed\n"));
		ret = WPS_ERR_GENERIC;
	}
	else {
		TUTRACE((TUTRACE_ERR, "wps_setAssocRespIE: call wps_set_wps_ie() ok\n"));
		ret = WPS_SUCCESS;
	}

	/* bufObj will be destroyed when we exit this function */
	buffobj_del(bufObj);
	return ret;
}

int
wps_setWpsIE(void *bcmdev, uint8 scState, uint8 selRegistrar,
	uint16 devPwdId, uint16 selRegCfgMethods, uint8 *authorizedMacs_buf,
	int authorizedMacs_len)
{
	uint8 data8;
	WPSAPI_T *g_mc = (WPSAPI_T *)bcmdev;


	/* Enable beacon IE */
	wps_setBeaconIE(g_mc,
		scState == WPS_SCSTATE_CONFIGURED,
		selRegistrar,
		devPwdId,
		selRegCfgMethods,
		authorizedMacs_buf,
		authorizedMacs_len);

	/* Enable probe response IE */
	wps_setProbeRespIE(g_mc,
		WPS_MSGTYPE_AP_WLAN_MGR,
		scState,
		selRegistrar,
		devPwdId,
		selRegCfgMethods,
		authorizedMacs_buf,
		authorizedMacs_len);

	/* Enable associate response IE */
	data8 = devcfg_getVersion2(g_mc->mp_info);
	if (data8 >= WPS_VERSION2)
		wps_setAssocRespIE(g_mc, WPS_MSGTYPE_AP_WLAN_MGR);

	return 0;
}
#endif	/* !WPS_ROUTER */


static uint32
wps_genPsk(WPSAPI_T *g_mc)
{
	BufferObj	*bo_psk;
	uint32		ret;
	char nwKey[SIZE_64_BYTES+1];
	unsigned char tmp[SIZE_32_BYTES];
	int i, j = 0;

	bo_psk = buffobj_new();
	if (!bo_psk)
		return WPS_ERR_SYSTEM;

	reg_proto_generate_psk(SIZE_256_BITS, bo_psk);

	/* Set this in our local store */
	memcpy(tmp, bo_psk->pBase, SIZE_32_BYTES);
	for (i = 0; i < 32; i++) {
		sprintf(&(nwKey[j]), "%02X", tmp[i]);
		j += 2;
	}
	ret = devcfg_setNwKey(g_mc->mp_info, nwKey, 64);
	if (WPS_SUCCESS != ret) {
		TUTRACE((TUTRACE_ERR, "MC_GeneratePsk: "
		         "Could not be locally stored\n"));
	}
	else {
		/* Print out the value */
		uint32 len;
		char *p_nwKey = devcfg_getNwKey(g_mc->mp_info, &len);
		wps_printPskValue(p_nwKey, len);
	}
	buffobj_del(bo_psk);
	return ret;
}

static uint32
wps_createM8AP(WPSAPI_T *g_mc, IN char *cp_ssid)
{
	char *cp_data;
	uint16 data16;
	uint32 nwKeyLen = 0;
	char *p_nwKey;
	CTlvNwKey *p_tlvKey;
	uint8 *p_macAddr;
	uint8 wep_exist = 0;

	/*
	 * Create the Encrypted Settings TLV for AP config
	 * Also store this info locally so it can be used
	 * when registration completes for reconfiguration.
	 */
	if (g_mc->mp_tlvEsM8Ap) {
		reg_msg_m8ap_del(g_mc->mp_tlvEsM8Ap, 0);
	}
	g_mc->mp_tlvEsM8Ap = reg_msg_m8ap_new();
	if (!g_mc->mp_tlvEsM8Ap)
		return WPS_ERR_SYSTEM;

	/* ssid */
	if (cp_ssid) {
		/* Use this instead of the one from the config file */
		cp_data = cp_ssid;
		data16 = (uint16)strlen(cp_ssid);
	}
	else {
		/* Use the SSID from the config file */
		cp_data = devcfg_getSSID(g_mc->mp_info, &data16);
	}
#ifdef WFA_WPS_20_TESTBED
	/* For internal testing purpose */
	if (devcfg_getZPadding(g_mc->mp_info))
		data16 ++;
#endif /* WFA_WPS_20_TESTBED */
	tlv_set(&g_mc->mp_tlvEsM8Ap->ssid, WPS_ID_SSID, (uint8 *)cp_data, data16);

	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Ap: "
	         "do REAL sec config here...\n"));
	if (devcfg_getAuth(g_mc->mp_info))
		data16 = WPS_AUTHTYPE_SHARED;
	else if (devcfg_getKeyMgmtType(g_mc->mp_info) == WPS_WL_AKM_PSK)
		data16 = WPS_AUTHTYPE_WPAPSK;
	else if (devcfg_getKeyMgmtType(g_mc->mp_info) == WPS_WL_AKM_PSK2)
		data16 = WPS_AUTHTYPE_WPA2PSK;
	else if (devcfg_getKeyMgmtType(g_mc->mp_info) == WPS_WL_AKM_BOTH)
		data16 = WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK;
	else
		data16 = WPS_AUTHTYPE_OPEN;
	tlv_set(&g_mc->mp_tlvEsM8Ap->authType, WPS_ID_AUTH_TYPE, (void *)((uint32)data16), 0);

	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Ap: "
	         "Auth type = 0x%x...\n", data16));

	/* encrType */
	if (devcfg_getAuth(g_mc->mp_info))
		data16 = WPS_ENCRTYPE_WEP;
	else if (data16 == WPS_AUTHTYPE_OPEN) {
		if (devcfg_getWep(g_mc->mp_info))
			data16 = WPS_ENCRTYPE_WEP;
		else
			data16 = WPS_ENCRTYPE_NONE;
	}
	else
		data16 = devcfg_getCrypto(g_mc->mp_info);

	tlv_set(&g_mc->mp_tlvEsM8Ap->encrType, WPS_ID_ENCR_TYPE, (void *)((uint32)data16), 0);

	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Ap: "
	         "encr type = 0x%x...\n", data16));
	if (data16 == WPS_ENCRTYPE_WEP)
		wep_exist = 1;

	/* nwKey */
	p_tlvKey = (CTlvNwKey *)tlv_new(WPS_ID_NW_KEY);
	if (!p_tlvKey)
		return WPS_ERR_SYSTEM;

	p_nwKey = devcfg_getNwKey(g_mc->mp_info, &nwKeyLen);
#ifdef WFA_WPS_20_TESTBED
	/* For internal testing purpose */
	if (devcfg_getZPadding(g_mc->mp_info) && p_nwKey &&
	    nwKeyLen < SIZE_64_BYTES && strlen(p_nwKey) == nwKeyLen)
		nwKeyLen ++;
#endif /* WFA_WPS_20_TESTBED */
	tlv_set(p_tlvKey, WPS_ID_NW_KEY, p_nwKey, nwKeyLen);
	if (!list_addItem(g_mc->mp_tlvEsM8Ap->nwKey, p_tlvKey)) {
		tlv_delete(p_tlvKey, 0);
		return WPS_ERR_SYSTEM;
	}

	/* macAddr, AP's MAC (Enrollee AP's MAC) */
	p_macAddr = g_mc->m_peerMacAddr;
	data16 = SIZE_MAC_ADDR;
	tlv_set(&g_mc->mp_tlvEsM8Ap->macAddr, WPS_ID_MAC_ADDR, p_macAddr, data16);

	/* WepKeyIdx */
	if (wep_exist)
		tlv_set(&g_mc->mp_tlvEsM8Ap->wepIdx, WPS_ID_WEP_TRANSMIT_KEY,
			(void *)((uint32)devcfg_getWepIdx(g_mc->mp_info)), 0);

	return WPS_SUCCESS;
}

static uint32
wps_createM8Sta(WPSAPI_T *g_mc)
{
	char *cp_data;
	uint16 data16;
	CTlvCredential *p_tlvCred;
	uint32 nwKeyLen = 0;
	char *p_nwKey;
	uint8 *p_macAddr;
	uint8 wep_exist = 0;
	uint8 data8;
#ifdef WFA_WPS_20_TESTBED
	bool dummy_ssid_applied = false;
#endif /* WFA_WPS_20_TESTBED */

	/*
	 * Create the Encrypted Settings TLV for STA config
	 * We will also need to delete this blob eventually, as the
	 * SM will not delete it.
	 */
	if (g_mc->mp_tlvEsM8Sta) {
		reg_msg_m8sta_del(g_mc->mp_tlvEsM8Sta, 0);
	}
	g_mc->mp_tlvEsM8Sta = reg_msg_m8sta_new();
	if (!g_mc->mp_tlvEsM8Sta)
		return WPS_ERR_SYSTEM;

#ifdef WFA_WPS_20_TESTBED
MSS:
#endif
	/* credential */
	p_tlvCred = (CTlvCredential *)malloc(sizeof(CTlvCredential));
	if (!p_tlvCred)
		return WPS_ERR_SYSTEM;

	memset(p_tlvCred, 0, sizeof(CTlvCredential));

	/* Fill in credential items */
	/* nwIndex */
	tlv_set(&p_tlvCred->nwIndex, WPS_ID_NW_INDEX, (void *)1, 0);
	/* ssid */
	cp_data = devcfg_getSSID(g_mc->mp_info, &data16);
#ifdef WFA_WPS_20_TESTBED
	/* For internal testing purpose */
	if (devcfg_getZPadding(g_mc->mp_info))
		data16 ++;

	if (!dummy_ssid_applied && devcfg_getMCA(g_mc->mp_info)) {
		cp_data = devcfg_getDummySSID(g_mc->mp_info, &data16);
		data16 = strlen(cp_data);
		if (devcfg_getZPadding(g_mc->mp_info))
			data16 ++;
	}
#endif /* WFA_WPS_20_TESTBED */
	tlv_set(&p_tlvCred->ssid, WPS_ID_SSID, (uint8 *)cp_data, data16);

	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Sta: "
		"do REAL sec config here...\n"));
	if (devcfg_getAuth(g_mc->mp_info))
		data16 = WPS_AUTHTYPE_SHARED;
	else if (devcfg_getKeyMgmtType(g_mc->mp_info) == WPS_WL_AKM_PSK)
		data16 = WPS_AUTHTYPE_WPAPSK;
	else if (devcfg_getKeyMgmtType(g_mc->mp_info) == WPS_WL_AKM_PSK2)
		data16 = WPS_AUTHTYPE_WPA2PSK;
	else if (devcfg_getKeyMgmtType(g_mc->mp_info) == WPS_WL_AKM_BOTH)
		data16 = WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK;
	else
		data16 = WPS_AUTHTYPE_OPEN;
	tlv_set(&p_tlvCred->authType, WPS_ID_AUTH_TYPE, (void *)((uint32)data16), 0);
	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Ap: "
	         "Auth type = 0x%x...\n", data16));

	/* encrType */
	if (devcfg_getAuth(g_mc->mp_info))
		data16 = WPS_ENCRTYPE_WEP;
	else if (data16 == WPS_AUTHTYPE_OPEN) {
		if (devcfg_getWep(g_mc->mp_info))
			data16 = WPS_ENCRTYPE_WEP;
		else
			data16 = WPS_ENCRTYPE_NONE;
	}
	else
		data16 = devcfg_getCrypto(g_mc->mp_info);

	tlv_set(&p_tlvCred->encrType, WPS_ID_ENCR_TYPE, (void *)((uint32)data16), 0);
	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Sta: "
		"encr type = 0x%x...\n", data16));
	if (data16 == WPS_ENCRTYPE_WEP)
		wep_exist = 1;

	/* nwKeyIndex */
	/*
	 * WSC 2.0, "Network Key Index" deprecated - only included by WSC 1.0 devices.
	 * Ignored by WSC 2.0 or newer devices.
	 */
	data8 = devcfg_getVersion2(g_mc->mp_info);
	if (data8 < WPS_VERSION2) {
		tlv_set(&p_tlvCred->nwKeyIndex, WPS_ID_NW_KEY_INDEX, (void *)1, 0);
	}
	/* nwKey */
	p_nwKey = devcfg_getNwKey(g_mc->mp_info, &nwKeyLen);
#ifdef WFA_WPS_20_TESTBED
	/* For internal testing purpose */
	if (devcfg_getZPadding(g_mc->mp_info) && p_nwKey &&
	    nwKeyLen < SIZE_64_BYTES && strlen(p_nwKey) == nwKeyLen)
		nwKeyLen ++;
#endif /* WFA_WPS_20_TESTBED */
	tlv_set(&p_tlvCred->nwKey, WPS_ID_NW_KEY, p_nwKey, nwKeyLen);

	/* macAddr, Enrollee Station's MAC */
	p_macAddr = g_mc->m_peerMacAddr;
	data16 = SIZE_MAC_ADDR;
	tlv_set(&p_tlvCred->macAddr, WPS_ID_MAC_ADDR,
	        (uint8 *)p_macAddr, data16);

	/* WepKeyIdx */
	if (wep_exist)
		tlv_set(&p_tlvCred->WEPKeyIndex, WPS_ID_WEP_TRANSMIT_KEY,
			(void *)((uint32)devcfg_getWepIdx(g_mc->mp_info)), 0);

	/* WSC 2.0, WFA "Network Key Shareable" subelement */
	data8 = devcfg_getVersion2(g_mc->mp_info);
	if (data8 >= WPS_VERSION2 && devcfg_getNwKeyShareable(g_mc->mp_info)) {
		data16 = 1;
		subtlv_set(&p_tlvCred->nwKeyShareable, WPS_WFA_SUBID_NW_KEY_SHAREABLE,
			(void *)((uint32)data16), 0);
		TUTRACE((TUTRACE_ERR, "wps_createM8Sta: Network Key Shareable is TRUE\n"));
	}

	if (!list_addItem(g_mc->mp_tlvEsM8Sta->credential, p_tlvCred)) {
		tlv_credentialDelete(p_tlvCred, 0);
		return WPS_ERR_SYSTEM;
	}

#ifdef WFA_WPS_20_TESTBED
	/*
	 * Multiple Security Settings for testing purpose
	 * First credential is using dummy SSID, the second one is good for station.
	 */
	if (!dummy_ssid_applied && devcfg_getMCA(g_mc->mp_info)) {
		dummy_ssid_applied = true;
		goto MSS;
	}
#endif /* WFA_WPS_20_TESTBED */

	return WPS_SUCCESS;
}

void
wps_printPskValue(IN char *nwKey, uint32 nwKeyLen)
{
#ifdef BCMWPS_DEBUG_PRINT_KEY
	/* 
	 * Security key is sensitive information. We only print it for special
	   debug build 
	*/
	char line[100];
	sprintf(line, "***** WPA_PSK = ");
	strncat(line, nwKey, nwKeyLen);

	TUTRACE((TUTRACE_INFO, "%s\n", line));
#endif /* BCMWPS_DEBUG_PRINT_KEY */
}

static uint32
wps_sendSetSelReg(WPSAPI_T *g_mc, IN bool b_setSelReg)
{
	uint32 ret = WPS_SUCCESS;

	/* Create the TLVs that need to be sent */
	BufferObj	*bufObj;
	uint8		data8;
	uint16		data16;
	bufObj = buffobj_new();
	if (!bufObj)
		return WPS_ERR_SYSTEM;

	/* Version */
	data8 = devcfg_getVersion(g_mc->mp_info);
	tlv_serialize(WPS_ID_VERSION, bufObj, &data8, WPS_ID_VERSION_S);
	/* Selected Registrar */
	tlv_serialize(WPS_ID_SEL_REGISTRAR, bufObj,
	               &b_setSelReg, WPS_ID_SEL_REGISTRAR_S);
	if (b_setSelReg) { /* only include this data if flag is true */
		/* Device Password ID */
		data16 = devcfg_getDevicePwdId(g_mc->mp_info);
		tlv_serialize(WPS_ID_DEVICE_PWD_ID, bufObj, &data16, WPS_ID_DEVICE_PWD_ID_S);
		/* Selected Registrar Config Methods */
		data16 = devcfg_getConfigMethods(g_mc->mp_info);
		tlv_serialize(WPS_ID_SEL_REG_CFG_METHODS, bufObj,
			&data16, WPS_ID_SEL_REG_CFG_METHODS_S);
	}

	buffobj_del(bufObj);

	return ret;
}

/*
* NOTE: The caller MUST call tlv_delete(ssrmsg->authorizedMacs, 1) to free memory which
* allocated in this fcuntion.
*/
int
wps_get_upnpDevSSR(WPSAPI_T *g_mc, void *p_cbData, uint32 length, CTlvSsrIE *ssrmsg)
{
	int ret = WPS_CONT;
	uint8 data8;
	EMode emode = devcfg_getConfiguredMode(g_mc->mp_info);

	TUTRACE((TUTRACE_INFO, "wps_get_upnpDevSSR\n"));

	if (emode == EModeApProxy ||
#ifdef WPS_AP_M2D
	    emode == EModeApProxyRegistrar ||
#endif
	    FALSE) {
		BufferObj *vendorExt_bufObj = NULL;
		BufferObj *wlidcardMac_bufObj = NULL;

		/* De-serialize the data to get the TLVs */
		BufferObj *bufObj = buffobj_new();
		if (!bufObj)
			return WPS_ERR_SYSTEM;

		buffobj_dserial(bufObj, (uint8 *)p_cbData, length);

		memset(ssrmsg, 0, sizeof(CTlvSsrIE));

		/* Version */
		tlv_dserialize(&ssrmsg->version, WPS_ID_VERSION, bufObj, 0, 0);
		/* Selected Registrar */
		tlv_dserialize(&ssrmsg->selReg, WPS_ID_SEL_REGISTRAR, bufObj, 0, 0);
		/* Device Password ID */
		tlv_dserialize(&ssrmsg->devPwdId, WPS_ID_DEVICE_PWD_ID, bufObj, 0, 0);
		/* Selected Registrar Config Methods */
		tlv_dserialize(&ssrmsg->selRegCfgMethods, WPS_ID_SEL_REG_CFG_METHODS,
			bufObj, 0, 0);

		/* WSC 2.0 */
		data8 = devcfg_getVersion2(g_mc->mp_info);
		if (data8 >= WPS_VERSION2) {
			/* Check subelement in WFA Vendor Extension */
			if (tlv_find_vendorExtParse(&ssrmsg->vendorExt, bufObj,
				(uint8 *)WFA_VENDOR_EXT_ID) != 0)
				goto add_wildcard_mac;

			/* Deserialize subelement */
			vendorExt_bufObj = buffobj_new();
			if (!vendorExt_bufObj) {
				buffobj_del(bufObj);
				return -1;
			}

			buffobj_dserial(vendorExt_bufObj, ssrmsg->vendorExt.vendorData,
				ssrmsg->vendorExt.dataLength);

			/* Get Version2 and AuthorizedMACs subelement */
			if (subtlv_dserialize(&ssrmsg->version2, WPS_WFA_SUBID_VERSION2,
				vendorExt_bufObj, 0, 0) == 0) {
				/* AuthorizedMACs, <= 30B */
				subtlv_dserialize(&ssrmsg->authorizedMacs,
					WPS_WFA_SUBID_AUTHORIZED_MACS,
					vendorExt_bufObj, 0, 0);
			}

			/* Add wildcard MAC when authorized mac not specified */
			if (ssrmsg->authorizedMacs.subtlvbase.m_len == 0) {
add_wildcard_mac:
				/*
				 * If the External Registrar is WSC version 1.0 it
				 * will not have included an AuthorizedMACs subelement.
				 * In this case the AP shall add the wildcard MAC Address
				 * (FF:FF:FF:FF:FF:FF) in an AuthorizedMACs subelement in
				 * Beacon and Probe Response frames
				 */
				wlidcardMac_bufObj = buffobj_new();
				if (!wlidcardMac_bufObj) {
					buffobj_del(vendorExt_bufObj);
					buffobj_del(bufObj);
					return -1;
				}

				/* Serialize the wildcard_authorizedMacs to wlidcardMac_Obj */
				subtlv_serialize(WPS_WFA_SUBID_AUTHORIZED_MACS, wlidcardMac_bufObj,
					(char *)wildcard_authorizedMacs, SIZE_MAC_ADDR);
				buffobj_Rewind(wlidcardMac_bufObj);
				/*
				 * De-serialize the wlidcardMac_Obj data to get the TLVs
				 * Do allocation, because wlidcardMac_bufObj will be freed here but
				 * ssrmsg->authorizedMacs return and used by caller.
				 */
				subtlv_dserialize(&ssrmsg->authorizedMacs,
					WPS_WFA_SUBID_AUTHORIZED_MACS,
					wlidcardMac_bufObj, 0, 1); /* Need to free it */
			}

			/* Get UUID-R after WFA-Vendor Extension (wpa_supplicant use this way) */
			tlv_find_dserialize(&ssrmsg->uuid_R, WPS_ID_UUID_R, bufObj, 0, 0);
		}

		/* Other... */

		if (bufObj)
			buffobj_del(bufObj);

		if (wlidcardMac_bufObj)
			buffobj_del(wlidcardMac_bufObj);

		if (vendorExt_bufObj)
			buffobj_del(vendorExt_bufObj);

		ret = WPS_SUCCESS;
	}

	return ret;
}

int
wps_upnpDevSSR(WPSAPI_T *g_mc, CTlvSsrIE *ssrmsg)
{
	EMode emode = devcfg_getConfiguredMode(g_mc->mp_info);
	uint8 scState = devcfg_getscState(g_mc->mp_info);

	TUTRACE((TUTRACE_INFO, "MC_Recd CB_TRUPNP_DEV_SSR\n"));

	/*
	 * Added to support SetSelectedRegistrar
	 * If we are an AP+Proxy, then add the SelectedRegistrar TLV
	 * to the WPS IE in the Beacon
	 * This call will fail if the right WLAN drivers are not used.
	 */
	if (emode == EModeApProxy ||
#ifdef WPS_AP_M2D
	    emode == EModeApProxyRegistrar ||
#endif
	    FALSE) {
		wps_setWpsIE(g_mc,
			scState,
			ssrmsg->selReg.m_data,
			ssrmsg->devPwdId.m_data,
			ssrmsg->selRegCfgMethods.m_data,
			ssrmsg->authorizedMacs.m_data,
			ssrmsg->authorizedMacs.subtlvbase.m_len);
	}

	return WPS_CONT;
}

uint32
wps_enrUpnpGetDeviceInfoCheck(EnrSM *e, void *inbuffer, uint32 in_len,
	void *outbuffer, uint32 *out_len)
{
	if (!inbuffer || !in_len) {
		if (false == e->m_sm->m_initialized) {
			TUTRACE((TUTRACE_ERR, "ENRSM: Not yet initialized.\n"));
			return WPS_ERR_NOT_INITIALIZED;
		}

		if (START != e->m_sm->mps_regData->e_smState) {
			TUTRACE((TUTRACE_INFO, "\n======e_lastMsgSent != M1, "
				"Step GetDeviceInfo=%d ======\n",
				wps_getUpnpDevGetDeviceInfo(e->g_mc)));
			if (wps_getUpnpDevGetDeviceInfo(e->g_mc)) {
				wps_setUpnpDevGetDeviceInfo(e->g_mc, false);

				/* copy to msg_to_send buffer */
				if (*out_len < e->m_sm->mps_regData->outMsg->m_dataLength) {
					e->m_sm->mps_regData->e_smState = FAILURE;
					TUTRACE((TUTRACE_ERR, "output message buffer to small\n"));
					return WPS_MESSAGE_PROCESSING_ERROR;
				}
				memcpy(outbuffer, (char *)e->m_sm->mps_regData->outMsg->pBase,
					e->m_sm->mps_regData->outMsg->m_dataLength);
				*out_len = e->m_sm->mps_regData->outMsg->m_dataLength;
				return WPS_SEND_MSG_CONT;
			}
			return WPS_SUCCESS;
		}
	}

	return WPS_CONT;
}

/* Add for PF3 */
static uint32
wps_regUpnpERFilter(RegSM *r, BufferObj *msg, uint32 msgType)
{
	uint32 err;


	/*
	 * If AP have received M2 form one External Registrar,
	 * AP must ignore forwarding messages from other
	 * External Registrars.
	 */
	if (r->m_er_sentM2 == false) {
		if (msgType == WPS_ID_MESSAGE_M2) {
			/* Save R-Nonce */
			err = reg_proto_get_nonce(r->m_er_nonce, msg, WPS_ID_REGISTRAR_NONCE);
			if (err != WPS_SUCCESS) {
				TUTRACE((TUTRACE_ERR, "ENRSM: Get R-Nonce error: %d\n", err));
			}
			else {
				r->m_er_sentM2 = true;
			}
		}
	}
	else {
		/* Filter UPnP to EAP messages by R-Nonce */
		err = reg_proto_check_nonce(r->m_er_nonce, msg, WPS_ID_REGISTRAR_NONCE);
		if (err == RPROT_ERR_NONCE_MISMATCH)
			return WPS_CONT;
	}

	return WPS_SUCCESS;
}

uint32
wps_regUpnpForwardingCheck(RegSM *r, void *inbuffer, uint32 in_len,
	void *outbuffer, uint32 *out_len)
{
	uint32 msgType = 0, err = WPS_SUCCESS, retVal = WPS_CONT;
	TRANSPORT_TYPE trType;
	BufferObj *inMsg = NULL, *outMsg = NULL;

	if (false == r->m_sm->m_initialized) {
		TUTRACE((TUTRACE_ERR, "REGSM: Not yet initialized.\n"));
		return WPS_ERR_NOT_INITIALIZED;
	}

	/* Irrespective of whether the local registrar is enabled or whether we're
	 * using an external registrar, we need to send a WPS_Start over EAP to
	 * kickstart the protocol.
	 * If we send a WPS_START msg, we don't need to do anything else i.e.
	 * invoke the local registrar or pass the message over UPnP
	 */
	if ((!in_len) && (START == r->m_sm->mps_regData->e_smState) &&
		((TRANSPORT_TYPE_EAP == r->m_sm->m_transportType) ||
		(TRANSPORT_TYPE_UPNP_CP == r->m_sm->m_transportType))) {
		err = wps_sendStartMessage(r->g_mc, r->m_sm->m_transportType);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "REGSM: SendStartMessage failed. Err = %d\n", err));
		}
		return WPS_CONT;
	}

	/* Now send the message over UPnP. Else, if the message has come over UPnP
	 * send it over EAP.
	 * if(m_passThruEnabled)
	 */
	/* temporarily disable forwarding to/from UPnP if we've already sent M2
	 * this is a stop-gap measure to prevent multiple registrars from cluttering
	 * up the EAP session.
	 */
	if ((r->m_passThruEnabled) && (!r->m_sentM2) &&
	    (wps_get_mode(r->g_mc) == EModeApProxy ||
#ifdef WPS_AP_M2D
	    wps_get_mode(r->g_mc) == EModeApProxyRegistrar ||
#endif
	    FALSE)) {

		if ((inMsg = buffobj_new()) == NULL)
			return WPS_ERR_SYSTEM;

		if (in_len) {
			buffobj_dserial(inMsg, inbuffer, in_len);
			err = reg_proto_get_msg_type(&msgType, inMsg);
		}

		switch (r->m_sm->m_transportType) {
		case TRANSPORT_TYPE_EAP:
			/* EAP to UPnP:
			 * if add client was performed by AP, skip this forward
			 * if (getBuiltInStart())
			 * break;
			 */
			if (r->m_sm->mps_localInfo->b_ap)
				trType = TRANSPORT_TYPE_UPNP_DEV;
			else
				trType = TRANSPORT_TYPE_UPNP_CP;

			/* recvd from EAP, send over UPnP */
			if (msgType != WPS_ID_MESSAGE_ACK) {
				TUTRACE((TUTRACE_INFO, "REGSM: Forwarding message from "
					"EAP to UPnP.\n"));
				TUTRACE((TUTRACE_INFO, "\n###EAP to UPnP msgType=%x (%s)"
					" ###\n", msgType, wps_msg_type_str(msgType)));
				err = wps_sendMsg(r->g_mc, trType, (char *)inbuffer, in_len);
			}

			/* Now check to see if the EAP message is a WPS_NACK.  If so,
			 * terminate the EAP connection with an EAP-Fail.
			 * if (msgType == WPS_ID_MESSAGE_NACK && WPS_SUCCESS == err) {
			 */
			if (msgType == WPS_ID_MESSAGE_NACK) {
				TUTRACE((TUTRACE_ERR, "REGSM: Notifying MC of failure.\n"));

				/* reset the SM */
				reg_sm_restartsm(r);
				retVal = WPS_MESSAGE_PROCESSING_ERROR;
				break;
			}

			if ((WPS_SUCCESS == err) && (msgType == WPS_ID_MESSAGE_DONE)) {
				TUTRACE((TUTRACE_INFO, "REGSM: Send EAP fail when "
					"DONE or ACK (type=%x).\n", msgType));

				retVal = WPS_SUCCESS;
				break;
			}

			break;

		default:
			/* UPnP to EAP: */
			TUTRACE((TUTRACE_INFO, "REGSM: Forwarding message from UPnP to EAP.\n"));

			/* recvd over UPNP, send it out over EAP */
			TUTRACE((TUTRACE_INFO, "\n======== GetDeviceInfo=%d  =======\n",
				wps_getUpnpDevGetDeviceInfo(r->g_mc)));
			if (in_len) {
				TUTRACE((TUTRACE_INFO, "\n###UPnP to EAP msgType=%x (%s) ###\n",
					msgType, wps_msg_type_str(msgType)));

				if (WPS_SUCCESS == err && msgType == WPS_ID_MESSAGE_M2D) {
					TUTRACE((TUTRACE_INFO, "\n ==received M2D ==\n"));
				}
			}

			if (wps_getUpnpDevGetDeviceInfo(r->g_mc)) {
				outMsg = buffobj_setbuf(outbuffer, *out_len);
				if (!outMsg) {
					retVal = WPS_ERR_SYSTEM;
					break;
				}

				wps_setUpnpDevGetDeviceInfo(r->g_mc, false);

				err = reg_proto_create_devinforsp(r->m_sm->mps_regData, outMsg);
				if (WPS_SUCCESS != err) {
					TUTRACE((TUTRACE_ERR, "BuildMessageM1: %d\n", err));
				}
				else {
					/* Now send the message to the transport */
					err = wps_sendMsg(r->g_mc, r->m_sm->m_transportType,
						(char *)outMsg->pBase, outMsg->m_dataLength);
				}

				if (WPS_SUCCESS != err) {
					r->m_sm->mps_regData->e_smState = FAILURE;
					TUTRACE((TUTRACE_ERR, "ENRSM: TrWrite generated an "
						"error: %d\n", err));
				}
			}

			if (in_len) {
				if (wps_regUpnpERFilter(r, inMsg, msgType) == WPS_CONT)
					break;

				err = wps_sendMsg(r->g_mc, TRANSPORT_TYPE_EAP,
					(char *)inbuffer, in_len);
			}

			break;
		}
	}

	if (inMsg)
		buffobj_del(inMsg);
	if (outMsg)
		buffobj_del(outMsg);

	return retVal;
}

int
wps_processMsg(void *mc_dev, void *inbuffer, uint32 in_len, void *outbuffer, uint32 *out_len,
	TRANSPORT_TYPE m_transportType)
{
	WPSAPI_T *g_mc = (WPSAPI_T *)mc_dev;
	RegData *regInfo;
	EnrSM *e;
	int ret = WPS_CONT;

	if (!g_mc)
		return WPS_MESSAGE_PROCESSING_ERROR;

	if (wps_get_mode(g_mc) == EModeUnconfAp) {
		if (g_mc->mp_enrSM) {
			g_mc->mp_enrSM->m_sm->m_transportType = m_transportType;

			/* Pre-process upnp */
			ret  = wps_enrUpnpGetDeviceInfoCheck(g_mc->mp_enrSM, inbuffer,
				in_len, outbuffer, out_len);
			if (ret != WPS_CONT) {
				if (ret == WPS_SUCCESS)
					return WPS_CONT;
				else
					return ret;
			}

			/* Hacking,
			 * set regInfo's e_lastMsgSent to M1,  m_m2dStatus to SM_AWAIT_M2 and
			 * regInfo's e_smState to CONTINUE,
			 * and create M1 to get regInfo's outMsg which needed in M2 process
			 */
			e = g_mc->mp_enrSM;
			regInfo = e->m_sm->mps_regData;

			if (START == regInfo->e_smState &&
				memcmp(g_mc->pre_enrolleeNonce, empty_enrolleeNonce,
				SIZE_128_BITS)) {
				uint32 msgType = 0, err;
				BufferObj *inMsg, *outMsg;

				inMsg = buffobj_new();
				if (!inMsg)
					return WPS_ERR_SYSTEM;

				buffobj_dserial(inMsg, inbuffer, in_len);
				err = reg_proto_get_msg_type(&msgType, inMsg);
				buffobj_del(inMsg);

				if (WPS_SUCCESS == err && WPS_ID_MESSAGE_M2 == msgType) {
					outMsg = buffobj_setbuf((uint8 *)outbuffer, *out_len);
					if (!outMsg)
						return WPS_ERR_SYSTEM;

					memcpy(regInfo->enrolleeNonce,
						g_mc->pre_enrolleeNonce, SIZE_128_BITS);
					e->m_sm->mps_regData->e_lastMsgSent = M1;
					/* Set the m2dstatus. */
					e->m_m2dStatus = SM_AWAIT_M2;
					/* set the message state to CONTINUE */
					e->m_sm->mps_regData->e_smState = CONTINUE;

					reg_proto_create_m1(regInfo, outMsg);

					buffobj_del(outMsg);
				}
			}

			ret = enr_sm_step(g_mc->mp_enrSM, in_len, ((uint8 *)inbuffer),
				((uint8 *)outbuffer), out_len);
		}
	}
	else {
		if (g_mc->mp_regSM) {
			g_mc->mp_regSM->m_sm->m_transportType = m_transportType;

			/* Pre-process upnp */
			ret  = wps_regUpnpForwardingCheck(g_mc->mp_regSM, inbuffer,
				in_len, outbuffer, out_len);
			if (ret != WPS_CONT) {
				return ret;
			}

			ret = reg_sm_step(g_mc->mp_regSM, in_len, ((uint8 *)inbuffer),
				((uint8 *)outbuffer), out_len);
		}
	}

	return ret;
}

int
wps_getenrState(void  *mc_dev)
{
	WPSAPI_T *g_mc = (WPSAPI_T *)mc_dev;
	if (g_mc->mp_enrSM)
		return g_mc->mp_enrSM->m_sm->mps_regData->e_lastMsgRecd;
	else
		return 0;
}

int
wps_getregState(void  *mc_dev)
{
	WPSAPI_T *g_mc = (WPSAPI_T *)mc_dev;
	if (g_mc->mp_regSM)
		return g_mc->mp_regSM->m_sm->mps_regData->e_lastMsgSent;
	else
		return 0;
}

void
wps_setUpnpDevGetDeviceInfo(void  *mc_dev, bool value)
{
	WPSAPI_T *g_mc = (WPSAPI_T *)mc_dev;

	if (g_mc)
	{
		g_mc->b_UpnpDevGetDeviceInfo = value;
	}

	return;
}

bool
wps_getUpnpDevGetDeviceInfo(void  *mc_dev)
{
	WPSAPI_T *g_mc = (WPSAPI_T *)mc_dev;

	if (g_mc)
	{
		return g_mc->b_UpnpDevGetDeviceInfo;
	}

	return false;
}

static uint32
wps_sendMsg(void *mcdev, TRANSPORT_TYPE trType, char * dataBuffer, uint32 dataLen)
{
	uint32 retVal = WPS_SUCCESS;

	TUTRACE((TUTRACE_INFO, "In wps_sendMsg\n"));

	if (trType < 1 || trType >= TRANSPORT_TYPE_MAX) {
		TUTRACE((TUTRACE_ERR, "Transport Type is not within the "
			"accepted range\n"));
		return WPS_ERR_INVALID_PARAMETERS;
	}

	switch (trType) {
		case TRANSPORT_TYPE_EAP:
		{
			retVal = ap_eap_sm_sendMsg(dataBuffer, dataLen);
		}
		break;
#ifdef WPS_UPNP_DEVICE
		case TRANSPORT_TYPE_UPNP_DEV:
		{
#ifndef WPS_AP_M2D
			/* UPnP is not enabled in built-in reg mode */
			if (wps_get_mode(mcdev) != EModeApProxyRegistrar)
#endif /* !WPS_AP_M2D */
				retVal = ap_upnp_sm_sendMsg(dataBuffer, dataLen);
		}
		break;
#endif /* WPS_UPNP_DEVICE */
		default:
		break;
	}

	if (retVal != WPS_SUCCESS) {
		TUTRACE((TUTRACE_ERR,  "WriteData for "
			"trType %d failed.\n", trType));
	}

	return retVal;
}

uint32
wps_sendStartMessage(void *mcdev, TRANSPORT_TYPE trType)
{
	if (trType == TRANSPORT_TYPE_EAP) {
		return (ap_eap_sm_sendWPSStart());
	}
	else {
		TUTRACE((TUTRACE_ERR, "Transport Type invalid\n"));
		return WPS_ERR_INVALID_PARAMETERS;
	}

	/* return WPS_SUCCESS;	unreachable */
}

uint32
wps_start_ap_registration(void *mc_dev, EMode e_currMode,
	char *devPwd, uint16 devPwdId, uint8 *authorizedMacs, uint32 authorizedMacs_len)
{
	return wps_initReg(mc_dev, e_currMode, devPwd, devPwdId,
		authorizedMacs, authorizedMacs_len);
}

int
wps_get_mode(void *mc_dev)
{
	WPSAPI_T *mc = mc_dev;
	if (mc)
		return devcfg_getConfiguredMode(mc->mp_info);
	else
		return EModeUnknown;
}

/* Get Enrollee's MAC address */
unsigned char *
wps_get_mac(void *mc_dev)
{
	int mode;

	if (!mc_dev)
		return NULL;

	mode = wps_get_mode(mc_dev);

	return ap_eap_sm_getMac(mode);
}

unsigned char *
wps_get_mac_income(void *mc_dev)
{
	WPSAPI_T *mc = mc_dev;
	if (!mc_dev)
		return NULL;

	return devcfg_getMacAddr(mc->mp_info);
}

/* WSC 2.0, return 0: support WSC 1.0 only otherwise return WSC V2 version */
uint8
wps_get_version2(void *mc_dev)
{
	WPSAPI_T *mc = mc_dev;
	uint8 data8;

	if (mc) {
		data8 = devcfg_getVersion2(mc->mp_info);
		if (data8 >= WPS_VERSION2)
			return data8;
	}

	return 0;
}
