/*
 * Enrollee API
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: enr_api.c 287349 2011-10-03 07:01:27Z kenlo $
 */

#ifdef WIN32
#include <stdio.h>
#include <windows.h>
#else
#include <string.h>
#include <stdint.h>
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
#include <reg_prototlv.h>
#include <statemachine.h>
#include <tutrace.h>

#include <slist.h>

#include <info.h>
#include <wpsapi.h>
#include <wps_staeapsm.h>
#include <wps_enrapi.h>
#include <wps_sta.h>
#include <wlioctl.h>

#define UINT2PTR(val)	((void *)(uintptr_t)(uint32)(val))

#define WPS_IE_BUF_LEN	VNDR_IE_MAX_LEN * 8	/* 2048 */

static WPSAPI_T *g_mc = NULL;
static uint8 enr_api_wps_iebuf[WPS_IE_BUF_LEN];
static WPSAPI_T *wps_new(void);
static uint32 wps_initReg(IN EMode e_currMode, IN EMode e_targetMode,
	IN char *devPwd, IN char *ssid, IN bool b_useIe /* = false */,
	IN uint16 devPwdId /* = WPS_DEVICEPWDID_DEFAULT */);
static uint32 wps_exit(void);
static void wps_delete(void);
static uint32 wps_stopStack(void);
static uint32 wps_startStack(IN EMode e_mode);
static uint32 wps_switchModeOn(IN EMode e_mode);
static uint32 wps_switchModeOff(IN EMode e_mode);


/*
 * Name        : Init
 * Description : Initialize member variables
 * Arguments   : none
 * Return type : uint32 - result of the initialize operation
 */
uint32
wps_enr_config_init(DevInfo *dev_info)
{
	g_mc = wps_new();
	if (g_mc) {
		if ((g_mc->mp_info = devcfg_new())) {
			DevInfo *info =  g_mc->mp_info->mp_deviceInfo;
			memcpy(info, dev_info, sizeof(DevInfo));
			info->scState = WPS_SCSTATE_UNCONFIGURED; /* Unconfigured */
			g_mc->mp_info->mb_regWireless = 0;
		}
		else
			goto error;

		g_mc->mp_regProt = (CRegProtocol *)alloc_init(sizeof(CRegProtocol));
		if (!g_mc->mp_regProt)
			goto error;

		g_mc->mb_initialized = true;
	}
	else {
		TUTRACE((TUTRACE_ERR, "enrApi::Init: MC initialization failed\n"));
		goto error;
	}

	/*
	 * Everything's initialized ok
	 * Transfer control to member variables
	 */
	return WPS_SUCCESS;

error:

	TUTRACE((TUTRACE_ERR, "enrApi::Init failed\n"));
	if (g_mc) {
		wps_cleanup();
	}

	return WPS_ERR_SYSTEM;
}

/*
 * Name        : Init
 * Description : Initialize member variables
 * Arguments   : none
 * Return type : uint32 - result of the initialize operation
 */
uint32
wps_enr_reg_config_init(DevInfo *dev_info, char *nwKey, int nwKeyLen, char *bssid)
{
	g_mc = wps_new();
	if (g_mc) {
		if ((g_mc->mp_info = devcfg_new())) {
			DevInfo *info =  g_mc->mp_info->mp_deviceInfo;
			memcpy(info, dev_info, sizeof(DevInfo));
			info->scState = WPS_SCSTATE_CONFIGURED; /* Configured */
		}
		else
			goto error;

		/* update nwKey */
		if (nwKey) {
			g_mc->mp_info->m_nwKeyLen = nwKeyLen;
			strncpy(g_mc->mp_info->m_nwKey, nwKey, nwKeyLen);
		}
		g_mc->mp_info->mb_nwKeySet = true; /* always true */

		g_mc->mp_info->mb_regWireless = 1;
		g_mc->mp_regProt = (CRegProtocol *)alloc_init(sizeof(CRegProtocol));
		if (!g_mc->mp_regProt)
			goto error;

		if (bssid)
			memcpy(g_mc->m_peerMacAddr, bssid, SIZE_6_BYTES);

		g_mc->mb_initialized = true;
	}
	else {
		TUTRACE((TUTRACE_ERR, "enrApi::Init: MC initialization failed\n"));
		goto error;
	}

	/*
	 * Everything's initialized ok
	 * Transfer control to member variables
	 */
	return WPS_SUCCESS;

error:

	TUTRACE((TUTRACE_ERR, "enrApi::Init failed\n"));
	if (g_mc) {
		wps_cleanup();
	}

	return WPS_ERR_SYSTEM;
}

/* for enrollee only */
bool
wps_is_wep_incompatible(bool role_reg)
{
	if (g_mc == NULL) {
		TUTRACE((TUTRACE_ERR, "enrApi: not initialization\n"));
		return FALSE;
	}

	if (role_reg) {
		if (g_mc->mp_regSM == NULL) {
			TUTRACE((TUTRACE_ERR, "enrApi: Registrar not initialization\n"));
			return FALSE;
		}

		if (g_mc->mp_regSM->err_code == RPROT_ERR_INCOMPATIBLE_WEP)
			return TRUE;
	}
	else {
		if (g_mc->mp_enrSM == NULL) {
			TUTRACE((TUTRACE_ERR, "enrApi: Enrollee not initialization\n"));
			return FALSE;
		}

		if (g_mc->mp_enrSM->err_code == RPROT_ERR_INCOMPATIBLE_WEP)
			return TRUE;
	}

	return FALSE;
}

/*
 * Name        : DeInit
 * Description : Deinitialize member variables
 * Arguments   : none
 * Return type : uint32
 */
void
wps_cleanup()
{
	if (g_mc) {
		wps_exit();
		wps_delete();
	}
	TUTRACE((TUTRACE_INFO, "enrApi::MC deinitialized\n"));
}

uint32
wps_start_enrollment(char *pin, unsigned long time)
{
	uint devicepwid = WPS_DEVICEPWDID_DEFAULT;
	char *PBC_PIN = "00000000\0";
	char *dev_pin = pin;
	uint32 ret;

	ret = wps_startStack(EModeClient);

	/* for enrollment again case */
	if (ret == MC_ERR_STACK_ALREADY_STARTED)
		ret = WPS_SUCCESS;

	if (ret != WPS_SUCCESS) {
		TUTRACE((TUTRACE_ERR, "Start EModeClient failed\n"));
		return ret;
	}

	if (!pin) {
		dev_pin = PBC_PIN;
		devicepwid = WPS_DEVICEPWDID_PUSH_BTN;
	}

	return wps_initReg(EModeClient, EModeRegistrar, dev_pin, NULL, false, devicepwid);
}

uint32
wps_start_registration(char *pin, unsigned long time)
{
	uint devicepwid = WPS_DEVICEPWDID_DEFAULT;
	char *dev_pin = pin;
	uint32 ret;


	ret = wps_startStack(EModeRegistrar);

	/* for registrar again case */
	if (ret == MC_ERR_STACK_ALREADY_STARTED)
		ret = WPS_SUCCESS;

	if (ret != WPS_SUCCESS) {
		TUTRACE((TUTRACE_ERR, "Start EModeRegistrar failed\n"));
		return ret;
	}

	return wps_initReg(EModeRegistrar, EModeRegistrar, dev_pin, NULL, false, devicepwid);
}

void
wps_get_credentials(WpsEnrCred* credential, const char *ssid, int len)
{
	CTlvEsM8Sta *p_tlvEncr = (CTlvEsM8Sta *)(g_mc->mp_enrSM->mp_peerEncrSettings);
	CTlvCredential *p_tlvCred = (CTlvCredential *)(list_getFirst(p_tlvEncr->credential));
	char *cp_data;
	uint16 data16;
	uint16 mgmt_data;

	if (len > 0 && (list_getCount(p_tlvEncr->credential) > 1)) {
		/* try to find same ssid */
		while (p_tlvCred) {
			if (p_tlvCred->ssid.tlvbase.m_len == len &&
				!strncmp((char*)p_tlvCred->ssid.m_data, ssid, len))
				break;
			/* get next one */
			p_tlvCred = (CTlvCredential *)(list_getNext(p_tlvEncr->credential,
				p_tlvCred));
		}

		/* no anyone match, use first one */
		if (p_tlvCred == NULL) {
			p_tlvCred = (CTlvCredential *)(list_getFirst(p_tlvEncr->credential));
		}
	}

	if (p_tlvCred == NULL)
		return;


	/* Fill in SSID */
	cp_data = (char *)(p_tlvCred->ssid.m_data);
	data16 = credential->ssidLen = p_tlvCred->ssid.tlvbase.m_len;
	strncpy(credential->ssid, cp_data, data16);
	credential->ssid[data16] = '\0';

	/* Fill in keyMgmt */
	mgmt_data = p_tlvCred->authType.m_data;

	if (mgmt_data == WPS_AUTHTYPE_SHARED) {
		strncpy(credential->keyMgmt, "SHARED", 6);
		credential->keyMgmt[6] = '\0';
	}
	else  if (mgmt_data == WPS_AUTHTYPE_WPAPSK) {
		strncpy(credential->keyMgmt, "WPA-PSK", 7);
		credential->keyMgmt[7] = '\0';
	}
	else if (mgmt_data == WPS_AUTHTYPE_WPA2PSK) {
		strncpy(credential->keyMgmt, "WPA2-PSK", 8);
		credential->keyMgmt[8] = '\0';
	}
	else if (mgmt_data == (WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK)) {
		strncpy(credential->keyMgmt, "WPA-PSK WPA2-PSK", 16);
		credential->keyMgmt[16] = '\0';
	}
	else {
		strncpy(credential->keyMgmt, "OPEN", 4);
		credential->keyMgmt[4] = '\0';
	}

	/* get the real cypher */
	mgmt_data = p_tlvCred->encrType.m_data;
	TUTRACE((TUTRACE_INFO, "Get Encr type is %d\n", mgmt_data));

	credential->encrType = mgmt_data;

	/* Fill in WEP index, no matter it's WEP type or not */
	credential->wepIndex = p_tlvCred->WEPKeyIndex.m_data;

	/* Fill in PSK */
	data16 = p_tlvCred->nwKey.tlvbase.m_len;
	memset(credential->nwKey, 0, SIZE_64_BYTES);
	memcpy(credential->nwKey, p_tlvCred->nwKey.m_data, data16);
	/* we have to set key length here */
	credential->nwKeyLen = data16;

	/* WSC 2.0,  nwKeyShareable */
	credential->nwKeyShareable = p_tlvCred->nwKeyShareable.m_data;

}

void
wps_get_reg_M7credentials(WpsEnrCred* credential)
{
	CTlvEsM7Ap *p_tlvEncr = (CTlvEsM7Ap *)(g_mc->mp_regSM->mp_peerEncrSettings);
	CTlvNwKey *nwKey;
	char *cp_data;
	uint16 data16;
	uint16 mgmt_data;
	uint16 m7keystr_len, offset;

	if (!p_tlvEncr) {
		TUTRACE((TUTRACE_INFO, "wps_get_reg_M7credentials: No peer Encrypt settings\n"));
		return;
	}
	TUTRACE((TUTRACE_INFO, "wps_get_reg_M7credentials: p_tlvEncr=0x%p\n", p_tlvEncr));

	/* Fill in SSID */
	cp_data = (char *)(p_tlvEncr->ssid.m_data);
	data16 = credential->ssidLen = p_tlvEncr->ssid.tlvbase.m_len;
	strncpy(credential->ssid, cp_data, data16);
	credential->ssid[data16] = '\0';

	/* Fill in keyMgmt */
	mgmt_data = p_tlvEncr->authType.m_data;

	if (mgmt_data == WPS_AUTHTYPE_SHARED) {
		strncpy(credential->keyMgmt, "SHARED", 6);
		credential->keyMgmt[6] = '\0';
	}
	else  if (mgmt_data == WPS_AUTHTYPE_WPAPSK) {
		strncpy(credential->keyMgmt, "WPA-PSK", 7);
		credential->keyMgmt[7] = '\0';
	}
	else if (mgmt_data == WPS_AUTHTYPE_WPA2PSK) {
		strncpy(credential->keyMgmt, "WPA2-PSK", 8);
		credential->keyMgmt[8] = '\0';
	}
	else if (mgmt_data == (WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK)) {
		strncpy(credential->keyMgmt, "WPA-PSK WPA2-PSK", 16);
		credential->keyMgmt[16] = '\0';
	}
	else {
		strncpy(credential->keyMgmt, "OPEN", 4);
		credential->keyMgmt[4] = '\0';
	}

	/* get the real cypher */

	mgmt_data = p_tlvEncr->encrType.m_data;
	TUTRACE((TUTRACE_INFO, "Get Encr type is %d\n", mgmt_data));

	credential->encrType = mgmt_data;

	/* Fill in WEP index, no matter it's WEP type or not */
	credential->wepIndex = p_tlvEncr->wepIdx.m_data;

	TUTRACE((TUTRACE_INFO, "wps_get_reg_credentials: p_tlvEncr->nwKey=0x%p\n",
		p_tlvEncr->nwKey));
	/* Fill in PSK */
	nwKey = (CTlvNwKey *)(list_getFirst(p_tlvEncr->nwKey));
	TUTRACE((TUTRACE_INFO, "wps_get_reg_credentials: nwKey=0x%p\n", nwKey));

	data16 = nwKey->tlvbase.m_len;
	memset(credential->nwKey, 0, SIZE_64_BYTES);

	if (nwKey->m_data && nwKey->tlvbase.m_len) {
		memcpy(credential->nwKey, nwKey->m_data, data16);

		m7keystr_len = (uint16)strlen(nwKey->m_data);
		if (m7keystr_len < data16) {
			for (offset = m7keystr_len; offset < data16; offset++) {
				if (nwKey->m_data[offset] != 0)
					break;
			}

			if (offset == data16) {
				/* Adjust nwKeyLen */
				nwKey->tlvbase.m_len = m7keystr_len;
				data16 = m7keystr_len;
			}
		}
	}

	/* we have to set key length here */
	credential->nwKeyLen = data16;
}

void
wps_get_reg_M8credentials(WpsEnrCred* credential)
{
	CTlvEsM8Sta *p_tlvEncr = (CTlvEsM8Sta *)
		(g_mc->mp_regSM->m_sm->mps_regData->staEncrSettings);
	CTlvCredential *p_tlvCred = (CTlvCredential *)(list_getFirst(p_tlvEncr->credential));
	char *cp_data;
	uint16 data16;
	uint16 mgmt_data;

	/* Fill in SSID */
	cp_data = (char *)(p_tlvCred->ssid.m_data);
	data16 = credential->ssidLen = p_tlvCred->ssid.tlvbase.m_len;
	if (data16 > (sizeof(credential->ssid) - 1)) {
		TUTRACE((TUTRACE_INFO, "M8 SSID length out of range!\n"));
		data16 = sizeof(credential->ssid) - 1;
	}
	memcpy(credential->ssid, cp_data, data16);
	credential->ssid[data16] = '\0';

	/* Fill in keyMgmt */
	mgmt_data = p_tlvCred->authType.m_data;

	if (mgmt_data == WPS_AUTHTYPE_SHARED)
		strcpy(credential->keyMgmt, "SHARED");
	else if (mgmt_data == WPS_AUTHTYPE_WPAPSK)
		strcpy(credential->keyMgmt, "WPA-PSK");
	else if (mgmt_data == WPS_AUTHTYPE_WPA2PSK)
		strcpy(credential->keyMgmt, "WPA2-PSK");
	else if (mgmt_data == (WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK))
		strcpy(credential->keyMgmt, "WPA-PSK WPA2-PSK");
	else
		strcpy(credential->keyMgmt, "OPEN");

	/* get the real cypher */

	mgmt_data = p_tlvCred->encrType.m_data;
	TUTRACE((TUTRACE_INFO, "Get Encr type is %d\n", mgmt_data));

	credential->encrType = mgmt_data;

	/* Fill in PSK */
	data16 = p_tlvCred->nwKey.tlvbase.m_len;
	memset(credential->nwKey, 0, SIZE_64_BYTES);
	memcpy(credential->nwKey, p_tlvCred->nwKey.m_data, data16);
	/* we have to set key length here */
	credential->nwKeyLen = data16;

	/* WSC 2.0,  nwKeyShareable */
	credential->nwKeyShareable = p_tlvCred->nwKeyShareable.m_data;
}

static WPSAPI_T *
wps_new(void)
{
	/* ONLY 1 instance allowed */
	if (g_mc) {
		TUTRACE((TUTRACE_ERR, "WPSAPI has already been created !!\n"));
		return NULL;
	}
	g_mc = (WPSAPI_T *)malloc(sizeof(WPSAPI_T));
	if (!g_mc)
		return NULL;

	TUTRACE((TUTRACE_INFO, "WPSAPI constructor\n"));
	g_mc->mb_initialized = false;
	g_mc->mb_stackStarted = false;
	g_mc->mb_requestedPwd = false;

	g_mc->mp_regProt = NULL;
	g_mc->mp_regSM = NULL;
	g_mc->mp_enrSM = NULL;
	g_mc->mp_info = NULL;

	g_mc->mp_regInfoList = NULL;
	g_mc->mp_enrInfoList = NULL;

	g_mc->mp_tlvEsM7Ap = NULL;
	g_mc->mp_tlvEsM7Enr = NULL;
	g_mc->mp_tlvEsM8Ap = NULL;
	g_mc->mp_tlvEsM8Sta = NULL;

	memset(&g_mc->m_peerMacAddr, 0, sizeof(g_mc->m_peerMacAddr));

	return g_mc;
}

static void
wps_delete(void)
{
	TUTRACE((TUTRACE_INFO, "WPSAPI destructor\n"));

	free(g_mc);
	g_mc = NULL;
}

static uint32
wps_exit(void)
{
	if (!g_mc->mb_initialized) {
		TUTRACE((TUTRACE_INFO, "MC_DeInit: Not initialized\n"));
		return WPS_ERR_NOT_INITIALIZED;
	}

	/* Stop stack anyway to free DH key if existing */
	wps_stopStack();

	if (g_mc->mp_regProt)
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

	return WPS_SUCCESS;
}

static uint32
wps_startStack(IN EMode e_mode)
{
	uint32 ret = WPS_SUCCESS;

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
		wps_stopStack();
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


	if (WPS_SUCCESS != ret) {
		TUTRACE((TUTRACE_INFO, "MC_StartStack: could not create lists\n"));
		if (g_mc->mp_regInfoList)
			list_delete(g_mc->mp_regInfoList);
		if (g_mc->mp_enrInfoList)
			list_delete(g_mc->mp_enrInfoList);
		return ret;
	}

	g_mc->mp_info->e_mode = e_mode;

	/* set b_ap flag */
	if (g_mc->mp_info->mp_deviceInfo->primDeviceCategory == WPS_DEVICE_TYPE_CAT_NW_INFRA &&
	    g_mc->mp_info->mp_deviceInfo->primDeviceSubCategory == WPS_DEVICE_TYPE_SUB_CAT_NW_AP)
		g_mc->mp_info->mp_deviceInfo->b_ap = true;
	else
		g_mc->mp_info->mp_deviceInfo->b_ap = false;

	TUTRACE((TUTRACE_INFO, "MC_StartStack: 3\n"));

	/* Initialize other member variables */
	g_mc->mb_requestedPwd = false;

	/*
	 * Explicitly perform some actions if the stack is configured
	 * in a particular mode
	 */
	e_mode = devcfg_getConfiguredMode(g_mc->mp_info);
	TUTRACE((TUTRACE_INFO, "MC_StartStack: 4\n"));
	if (WPS_SUCCESS != wps_switchModeOn(e_mode)) {
		TUTRACE((TUTRACE_INFO, "MC_StartStack: Could not start trans\n"));
		return MC_ERR_STACK_NOT_STARTED;
	}

	g_mc->mb_stackStarted = true;

	ret = WPS_SUCCESS;

	return ret;
}

static uint32
wps_stopStack(void)
{
	LISTITR m_listItr;
	LPLISTITR listItr;

	if (!g_mc->mb_initialized)
		return WPS_ERR_NOT_INITIALIZED;

	/* TODO: need to purge data from callback threads here as well */

	/*
	 * Explicitly perform some actions if the stack is configured
	 * in a particular mode
	 */
	wps_switchModeOff(devcfg_getConfiguredMode(g_mc->mp_info));

	/* Reset other member variables */

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
wps_initReg(IN EMode e_currMode, IN EMode e_targetMode,
	IN char *devPwd, IN char *ssid, IN bool b_useIe /* = false */,
	IN uint16 devPwdId /* = WPS_DEVICEPWDID_DEFAULT */)
{
	if (WPS_SUCCESS == devcfg_setDevPwdId(g_mc->mp_info, devPwdId)) {
		/* Reset local device info in SM */
		if (e_currMode == EModeClient)
			state_machine_update_devinfo(g_mc->mp_enrSM->m_sm,
				devcfg_getDeviceInfo(g_mc->mp_info));
		else
			state_machine_update_devinfo(g_mc->mp_regSM->m_sm,
				devcfg_getDeviceInfo(g_mc->mp_info));
	}

	/* Pass in a NULL for the encrypted settings TLV */
	if (e_currMode == EModeClient) {
		g_mc->mp_tlvEsM7Enr = NULL;

		/* Call into the SM now */
		if (WPS_SUCCESS != enr_sm_initializeSM(g_mc->mp_enrSM, NULL, /* p_regInfo */
			(void *)g_mc->mp_tlvEsM7Enr, /* M7 Enr encrSettings */
			NULL, /* M7 AP encrSettings */
			devPwd, /* device password */
			(uint32)strlen(devPwd))	/* dev pwd length */) {
			TUTRACE((TUTRACE_ERR, "Can't init enr_sm\n"));
			return WPS_ERR_OUTOFMEMORY;
		}

		/* Start the EAP transport */
		/* PHIL : link eap and wps state machines  */
		wps_eap_sta_init(NULL, g_mc->mp_enrSM, EModeClient);
	}
	else if (e_currMode == EModeRegistrar) {
		/* Check to see if the devPwd from the app is valid */
		if (!devPwd) {
			/* App sent in a NULL, and USB key isn't sent */
			TUTRACE((TUTRACE_ERR, "MC_InitiateRegistration: "
				"No password to use, expecting one\n"));
			return WPS_ERR_SYSTEM;
		}
		else {
			if (strlen(devPwd)) {
				TUTRACE((TUTRACE_ERR, "Using pwd from app\n"));

				/* Pass the device password on to the SM */
				state_machine_set_passwd(g_mc->mp_regSM->m_sm, devPwd,
					(uint32)(strlen(devPwd)));
			}
		}

		/* Fix transportType to EAP */
		g_mc->mp_regSM->m_sm->m_transportType = TRANSPORT_TYPE_EAP;

		/* Start the EAP transport */
		/* PHIL : link eap and wps state machines  */
		wps_eap_sta_init(NULL, g_mc->mp_regSM, EModeRegistrar);
	}

	return WPS_SUCCESS;
}

bool
wps_validateChecksum(IN unsigned long int PIN)
{
	unsigned long int accum = 0;
	accum += 3 * ((PIN / 10000000) % 10);
	accum += 1 * ((PIN / 1000000) % 10);
	accum += 3 * ((PIN / 100000) % 10);
	accum += 1 * ((PIN / 10000) % 10);
	accum += 3 * ((PIN / 1000) % 10);
	accum += 1 * ((PIN / 100) % 10);
	accum += 3 * ((PIN / 10) % 10);
	accum += 1 * ((PIN / 1) % 10);

	return ((accum % 10) == 0);
}

static uint32
wps_createM8AP(WPSAPI_T *g_mc, IN char *cp_ssid)
{
	char *cp_data;
	uint16 data16;
	uint32 nwKeyLen = 0;
	uint8 *p_macAddr;
	CTlvNwKey *p_tlvKey;
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
	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Ap: ssid=%s\n", cp_data));

	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Ap: do REAL sec config here...\n"));
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
	tlv_set(&g_mc->mp_tlvEsM8Ap->authType, WPS_ID_AUTH_TYPE, UINT2PTR(data16), 0);
	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Ap: Auth type = 0x%x...\n", data16));

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
	tlv_set(&g_mc->mp_tlvEsM8Ap->encrType, WPS_ID_ENCR_TYPE, UINT2PTR(data16), 0);
	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Ap: encr type = 0x%x...\n", data16));

	if (data16 == WPS_ENCRTYPE_WEP)
		wep_exist = 1;

	/* nwKey */
	p_tlvKey = (CTlvNwKey *)tlv_new(WPS_ID_NW_KEY);
	if (!p_tlvKey)
		return WPS_ERR_SYSTEM;

	cp_data = devcfg_getNwKey(g_mc->mp_info, &nwKeyLen);
#ifdef WFA_WPS_20_TESTBED
	/* For internal testing purpose */
	if (devcfg_getZPadding(g_mc->mp_info) && cp_data &&
	    nwKeyLen < SIZE_64_BYTES && strlen(cp_data) == nwKeyLen)
		nwKeyLen ++;
#endif /* WFA_WPS_20_TESTBED */
	tlv_set(p_tlvKey, WPS_ID_NW_KEY, cp_data, nwKeyLen);
	if (!list_addItem(g_mc->mp_tlvEsM8Ap->nwKey, p_tlvKey)) {
		tlv_delete(p_tlvKey, 0);
		return WPS_ERR_SYSTEM;
	}

	/* macAddr (Enrollee AP's MAC) */
	p_macAddr = g_mc->m_peerMacAddr;
	data16 = SIZE_MAC_ADDR;
	tlv_set(&g_mc->mp_tlvEsM8Ap->macAddr, WPS_ID_MAC_ADDR, p_macAddr, data16);

	/* WepKeyIdx */
	if (wep_exist)
		tlv_set(&g_mc->mp_tlvEsM8Ap->wepIdx, WPS_ID_WEP_TRANSMIT_KEY,
			UINT2PTR(devcfg_getWepIdx(g_mc->mp_info)), 0);

	return WPS_SUCCESS;
}

static uint32
wps_createM8Sta(WPSAPI_T *g_mc)
{
	char *cp_data;
	uint8 *p_macAddr;
	uint16 data16;
	uint32 nwKeyLen = 0;
	CTlvCredential *p_tlvCred;
	uint8 wep_exist = 0;
	bool b_wps_version2 = true;
#ifdef WFA_WPS_20_TESTBED
	bool dummy_ssid_applied = false;
#endif /* WFA_WPS_20_TESTBED */

	/* WSC 2.0,  support WPS V2 or not */
	if (devcfg_getVersion2(g_mc->mp_info) < WPS_VERSION2)
		b_wps_version2 = false;

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
		if (devcfg_getZPadding(g_mc->mp_info))
			data16 ++;
	}
#endif /* WFA_WPS_20_TESTBED */
	tlv_set(&p_tlvCred->ssid, WPS_ID_SSID, (uint8 *)cp_data, data16);
	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Sta: ssid=%s\n", cp_data));

	/* security settings */
	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Sta: do REAL sec config here...\n"));
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
	tlv_set(&p_tlvCred->authType, WPS_ID_AUTH_TYPE, UINT2PTR(data16), 0);
	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Sta: Auth type = 0x%x...\n", data16));

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
	tlv_set(&p_tlvCred->encrType, WPS_ID_ENCR_TYPE, UINT2PTR(data16), 0);
	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Sta: encr type = 0x%x...\n", data16));

	if (data16 == WPS_ENCRTYPE_WEP)
		wep_exist = 1;

	/* nwKeyIndex */
	/*
	 * WSC 2.0, "Network Key Index" deprecated - only included by WSC 1.0 devices.
	 * Ignored by WSC 2.0 or newer devices.
	 */
	if (!b_wps_version2)
		tlv_set(&p_tlvCred->nwKeyIndex, WPS_ID_NW_KEY_INDEX, (void *)1, 0);

	/* nwKey */
	cp_data = devcfg_getNwKey(g_mc->mp_info, &nwKeyLen);
#ifdef WFA_WPS_20_TESTBED
	/* For internal testing purpose */
	if (devcfg_getZPadding(g_mc->mp_info) && cp_data &&
	    nwKeyLen < SIZE_64_BYTES && strlen(cp_data) == nwKeyLen)
		nwKeyLen ++;
#endif /* WFA_WPS_20_TESTBED */
	tlv_set(&p_tlvCred->nwKey, WPS_ID_NW_KEY, cp_data, nwKeyLen);

	p_macAddr = g_mc->m_peerMacAddr;
	data16 = SIZE_MAC_ADDR;
	tlv_set(&p_tlvCred->macAddr, WPS_ID_MAC_ADDR, (uint8 *)p_macAddr, data16);

	/* WepKeyIdx */
	if (wep_exist)
		tlv_set(&p_tlvCred->WEPKeyIndex, WPS_ID_WEP_TRANSMIT_KEY,
			UINT2PTR(devcfg_getWepIdx(g_mc->mp_info)), 0);

	/* WSC 2.0, Set "Network Key Shareable" subelement for WFA Vendor Extension */
	if (b_wps_version2 && devcfg_getNwKeyShareable(g_mc->mp_info)) {
		data16 = 1;
		subtlv_set(&p_tlvCred->nwKeyShareable, WPS_WFA_SUBID_NW_KEY_SHAREABLE,
			UINT2PTR(data16), 0);
		TUTRACE((TUTRACE_ERR, "wps_createM8Sta: Network Key Shareable is TRUE\n"));

		/* We will do Vendor Extension write at tlv_credentialWrite */
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

/*
 * Name        : SwitchModeOn
 * Description : Switch to the specified mode of operation.
 *                 Turn off other modes, if necessary.
 * Arguments   : IN EMode e_mode - mode of operation to switch to
 * Return type : uint32 - result of the operation
 */
static uint32
wps_switchModeOn(IN EMode e_mode)
{
	uint32 ret = WPS_SUCCESS;
	uint32 dum_lock;

	/* Create DH Key Pair */
	DH *p_dhKeyPair;
	BufferObj *bo_pubKey, *bo_sha256Hash;

	if ((bo_pubKey = buffobj_new()) == NULL)
		return WPS_ERR_SYSTEM;

	if ((bo_sha256Hash = buffobj_new()) == NULL) {
		buffobj_del(bo_pubKey);
		return WPS_ERR_SYSTEM;
	}


	TUTRACE((TUTRACE_INFO, "MC_SwitchModeOn: 1\n"));
	ret = reg_proto_generate_dhkeypair(&p_dhKeyPair, bo_pubKey);
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
		WpsLock(devcfg_getLock(g_mc->mp_info));
		devcfg_setDHKeyPair(g_mc->mp_info, p_dhKeyPair);
		if (WPS_SUCCESS != (devcfg_setPubKey(g_mc->mp_info, bo_pubKey))) {
			TUTRACE((TUTRACE_ERR, "MC_SwitchModeOn: Could not set pubKey\n"));
			ret = WPS_ERR_SYSTEM;
		}
		if (WPS_SUCCESS != (devcfg_setSHA256Hash(g_mc->mp_info, bo_sha256Hash))) {
			TUTRACE((TUTRACE_ERR, "MC_SwitchModeOn: Could not set sha256Hash\n"));
			ret = WPS_ERR_SYSTEM;
		}
		buffobj_del(bo_sha256Hash);
		buffobj_del(bo_pubKey);

		WpsUnlock(devcfg_getLock(g_mc->mp_info));
		if (WPS_ERR_SYSTEM == ret) {
			return ret;
		}
	}

	switch (e_mode) {
	case EModeClient:
	{
		TUTRACE((TUTRACE_INFO, "MC_SwitchModeOn: EModeClient enter\n"));
		/* Instantiate the Enrollee SM */
		g_mc->mp_enrSM = enr_sm_new(g_mc, g_mc->mp_regProt);

		if (!g_mc->mp_enrSM) {
			TUTRACE((TUTRACE_ERR,
				"MC_SwitchModeOn: Could not allocate g_mc->mp_enrSM\n"));
			return WPS_ERR_OUTOFMEMORY;
		}

		state_machine_set_local_devinfo(g_mc->mp_enrSM->m_sm,
			devcfg_getDeviceInfo(g_mc->mp_info),
			&dum_lock, p_dhKeyPair);

		TUTRACE((TUTRACE_INFO, "MC_SwitchModeOn: Exit\n"));
		break;
	}

	case EModeRegistrar:
	{
		TUTRACE((TUTRACE_INFO, "MC_SwitchModeOn: EModeRegistrar enter\n"));
		/* Instantiate the Registrar SM */
		g_mc->mp_regSM = reg_sm_new(g_mc, g_mc->mp_regProt);
		if (!g_mc->mp_regSM) {
			TUTRACE((TUTRACE_ERR,
				"MC_SwitchModeOn: Could not allocate g_mc->mp_regSM\n"));
			return WPS_ERR_OUTOFMEMORY;
		}

		state_machine_set_local_devinfo(g_mc->mp_regSM->m_sm,
			devcfg_getDeviceInfo(g_mc->mp_info),
			&dum_lock, p_dhKeyPair);

		/* Create the encrypted settings in M8 */
		if (g_mc->mp_info->mb_nwKeySet) {
			/* Create the encrypted settings TLV for enrollee is AP */
			if (WPS_SUCCESS != wps_createM8AP(g_mc, NULL))
				return WPS_ERR_OUTOFMEMORY;

			/* Create the encrypted settings TLV for enrollee is STA */
			if (WPS_SUCCESS != wps_createM8Sta(g_mc))
				return WPS_ERR_OUTOFMEMORY;
		}

		/* Initialize the Registrar SM */
		if (WPS_SUCCESS != reg_sm_initsm(g_mc->mp_regSM, NULL, true, false))
			return WPS_ERR_OUTOFMEMORY;

		/* Send the encrypted settings value to the SM */
		if (g_mc->mp_info->mb_nwKeySet)
			state_machine_set_encrypt(g_mc->mp_regSM->m_sm,
				(void *)g_mc->mp_tlvEsM8Sta,
				(void *)g_mc->mp_tlvEsM8Ap);
		else
			state_machine_set_encrypt(g_mc->mp_regSM->m_sm,
				NULL, NULL);

		TUTRACE((TUTRACE_INFO, "MC_SwitchModeOn: Exit\n"));
		break;
	}

	default:
		ret = WPS_ERR_SYSTEM;
		break;
	}

	return ret;
}

/*
 * Name        : SwitchModeOff
 * Description : Switch off the specified mode of operation.
 * Arguments   : IN EMode e_mode - mode of operation to switch off
 * Return type : uint32 - result of the operation
 */
static uint32
wps_switchModeOff(IN EMode e_mode)
{

	if (e_mode == EModeClient) {
		if (g_mc->mp_enrSM) {
			/* TODO: Clear request IEs */
			enr_sm_delete(g_mc->mp_enrSM);
		}
	}
	else if (e_mode == EModeRegistrar) {
		if (g_mc->mp_regSM) {
			/* TODO: Clear request IEs */
			reg_sm_delete(g_mc->mp_regSM);
		}
	}

	if (g_mc->mp_info->mp_dhKeyPair) {
		DH_free(g_mc->mp_info->mp_dhKeyPair);
		g_mc->mp_info->mp_dhKeyPair = 0;

		TUTRACE((TUTRACE_ERR, "Free g_mc->mp_info->mp_dhKeyPair\n"));
	}

	/* Transport module stop now happens in ProcessRegCompleted() */

	return WPS_SUCCESS;
}

/*
 * creates and copy a WPS IE in buff argument.
 * Lenght of the IE is store in buff[0].
 * buff should be a 256 bytes buffer.
 */
uint32
wps_build_probe_req_ie(uint8 *buff, int buflen,  uint8 reqType,
	uint16 assocState, uint16 configError, uint16 passwId)
{
	uint32 ret = WPS_SUCCESS;
	BufferObj *bufObj, *vendorExt_bufObj;
	uint16 data16;
	uint8 data8, *p_data8;
	char *pc_data;
	CTlvPrimDeviceType tlvPrimDeviceType;
	CTlvVendorExt vendorExt;

	if (!buff)
		return WPS_ERR_SYSTEM;

	/*  fill in the WPA-WPS OUI  */
	buff[0] = 4;
	buff[1] = 0x00;
	buff[2] = 0x50;
	buff[3] = 0xf2;
	buff[4] = 0x04;

	/*  use oa BufferObj for the rest */
	bufObj = buffobj_setbuf(buff+5, buflen-5);
	if (!bufObj)
		return WPS_ERR_SYSTEM;

	/* Create the IE */
	/* Version */
	data8 = devcfg_getVersion(g_mc->mp_info);
	tlv_serialize(WPS_ID_VERSION, bufObj, &data8, WPS_ID_VERSION_S);

	/* Request Type */
	tlv_serialize(WPS_ID_REQ_TYPE, bufObj, &reqType, WPS_ID_REQ_TYPE_S);

	/* Config Methods */
	data16 = devcfg_getConfigMethods(g_mc->mp_info);
	tlv_serialize(WPS_ID_CONFIG_METHODS, bufObj, &data16, WPS_ID_CONFIG_METHODS_S);

	/* UUID */
	p_data8 = devcfg_getUUID(g_mc->mp_info);
	if (WPS_MSGTYPE_REGISTRAR == reqType)
		data16 = WPS_ID_UUID_R;
	else
		data16 = WPS_ID_UUID_E;
	tlv_serialize(data16, bufObj, p_data8, SIZE_UUID);

	/*
	 * Primary Device Type
	 *This is a complex TLV, so will be handled differently
	 */
	tlvPrimDeviceType.categoryId = devcfg_getPrimDeviceCategory(g_mc->mp_info);
	tlvPrimDeviceType.oui = devcfg_getPrimDeviceOui(g_mc->mp_info);
	tlvPrimDeviceType.subCategoryId = devcfg_getPrimDeviceSubCategory(g_mc->mp_info);
	tlv_primDeviceTypeWrite(&tlvPrimDeviceType, bufObj);

	/* RF Bands */
	data8 = devcfg_getRFBand(g_mc->mp_info);
	tlv_serialize(WPS_ID_RF_BAND, bufObj, &data8, WPS_ID_RF_BAND_S);

	/* Association State */
	tlv_serialize(WPS_ID_ASSOC_STATE, bufObj, &assocState, WPS_ID_ASSOC_STATE_S);

	/* Configuration Error */
	tlv_serialize(WPS_ID_CONFIG_ERROR, bufObj, &configError, WPS_ID_CONFIG_ERROR_S);

	/* Device Password ID */
	data16 = passwId;
	tlv_serialize(WPS_ID_DEVICE_PWD_ID, bufObj, &data16, WPS_ID_DEVICE_PWD_ID_S);

	/* WSC 2.0 */
	data8 = devcfg_getVersion2(g_mc->mp_info);
	if (data8 >= WPS_VERSION2) {
		/* Manufacturer */
		pc_data = devcfg_getManufacturer(g_mc->mp_info, &data16);
		tlv_serialize(WPS_ID_MANUFACTURER, bufObj, pc_data, data16);

		/* Model Name */
		pc_data = devcfg_getModelName(g_mc->mp_info, &data16);
		tlv_serialize(WPS_ID_MODEL_NAME, bufObj, pc_data, data16);

		/* Model Number */
		pc_data = devcfg_getModelNumber(g_mc->mp_info, &data16);
		tlv_serialize(WPS_ID_MODEL_NUMBER, bufObj, pc_data, data16);

		/* Device Name */
		pc_data = devcfg_getDeviceName(g_mc->mp_info, &data16);
		tlv_serialize(WPS_ID_DEVICE_NAME, bufObj, pc_data, data16);

		/* Add WFA Vendor Extension subelements */
		vendorExt_bufObj = buffobj_new();
		if (vendorExt_bufObj == NULL)
			return WPS_ERR_OUTOFMEMORY;

		/* Version2 subelement */
		data8 = devcfg_getVersion2(g_mc->mp_info);
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj, &data8,
			WPS_WFA_SUBID_VERSION2_S);

		/* Request to Enroll subelement - optional */
		if (devcfg_reqToEnroll(g_mc->mp_info)) {
			data8 = 1;
			subtlv_serialize(WPS_WFA_SUBID_REQ_TO_ENROLL, vendorExt_bufObj, &data8,
				WPS_WFA_SUBID_REQ_TO_ENROLL_S);
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
			buffobj_Append(bufObj, data16, (uint8*)pc_data);
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Portable Device - optional */

	/* other Vendor Extension - optional */

	/* store length */
	buff[0] += bufObj->m_dataLength;

	buffobj_del(bufObj);

	return ret;
}

/*
 * creates and copy a WPS IE in buff argument.
 * Lenght of the IE is store in buff[0].
 * buff should be a 256 bytes buffer.
 */
uint32
wps_build_assoc_req_ie(uint8 *buff, int buflen,  uint8 reqType)
{
	uint32 ret = WPS_SUCCESS;
	BufferObj *bufObj, *vendorExt_bufObj;
	uint8 data8;
	CTlvVendorExt vendorExt;


	if (!buff)
		return WPS_ERR_SYSTEM;

	/*  fill in the WPA-WPS OUI  */
	buff[0] = 4;
	buff[1] = 0x00;
	buff[2] = 0x50;
	buff[3] = 0xf2;
	buff[4] = 0x04;

	/*  use oa BufferObj for the rest */
	bufObj = buffobj_setbuf(buff+5, buflen-5);
	if (!bufObj)
		return WPS_ERR_SYSTEM;

	/* Create the IE */
	/* Version */
	data8 = devcfg_getVersion(g_mc->mp_info);
	tlv_serialize(WPS_ID_VERSION, bufObj, &data8, WPS_ID_VERSION_S);

	/* Request Type */
	tlv_serialize(WPS_ID_REQ_TYPE, bufObj, &reqType, WPS_ID_REQ_TYPE_S);

	/* Add WFA Vendor Extension subelements */
	vendorExt_bufObj = buffobj_new();
	if (vendorExt_bufObj == NULL)
		return WPS_ERR_OUTOFMEMORY;

	/* Version2 subelement */
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
		buffobj_Append(bufObj, data16, (uint8*)pc_data);
}
#endif /* WFA_WPS_20_TESTBED */

	/* Portable Device - optional */

	/* Other Vendor Extension - optional */

	/* store length */
	buff[0] += bufObj->m_dataLength;

	buffobj_del(bufObj);

	return ret;
}

uint32
wps_build_pbc_proberq(uint8 *buff, int len)
{
	return wps_build_probe_req_ie(buff, len, 0, 0, 0, WPS_DEVICEPWDID_PUSH_BTN);
}

uint32
wps_build_def_proberq(uint8 *buff, int len)
{
	return wps_build_probe_req_ie(buff, len, 0, 0, 0, WPS_DEVICEPWDID_DEFAULT);
}

/* WSC 2.0 */
uint32
wps_build_def_assocrq(uint8 *buff, int len)
{
	return wps_build_assoc_req_ie(buff, len, 0);
}

uint32
wps_computeChecksum(IN unsigned long int PIN)
{
	unsigned long int accum = 0;
	int digit;

	PIN *= 10;
	accum += 3 * ((PIN / 10000000) % 10);
	accum += 1 * ((PIN / 1000000) % 10);
	accum += 3 * ((PIN / 100000) % 10);
	accum += 1 * ((PIN / 10000) % 10);
	accum += 3 * ((PIN / 1000) % 10);
	accum += 1 * ((PIN / 100) % 10);
	accum += 3 * ((PIN / 10) % 10);

	digit = (accum % 10);
	return (10 - digit) % 10;
}

uint32
wps_generatePin(char *c_devPwd, int buf_len, IN bool b_display)
{
	BufferObj *bo_devPwd;
	uint8 *devPwd = NULL;
	uint32 val = 0;
	uint32 checksum = 0;
	char local_pwd[20];

	/*
	 * buffer size needs to big enough to hold 8 digits plus the string terminition
	 * character '\0'
	*/
	if (buf_len < 9)
		return WPS_ERR_BUFFER_TOO_SMALL;

	bo_devPwd = buffobj_new();
	if (!bo_devPwd)
		return WPS_ERR_SYSTEM;

	/* Use the GeneratePSK() method in RegProtocol to help with this */
	buffobj_Reset(bo_devPwd);
	if (WPS_SUCCESS != reg_proto_generate_psk(8, bo_devPwd)) {
		TUTRACE((TUTRACE_ERR,
			"MC_GenerateDevPwd: Could not generate enrDevPwd\n"));
		buffobj_del(bo_devPwd);
		return WPS_ERR_SYSTEM;
	}
	devPwd = bo_devPwd->pBase;
	sprintf(local_pwd, "%08u", *(uint32 *)devPwd);

	/* Compute the checksum */
	local_pwd[7] = '\0';
	val = strtoul(local_pwd, NULL, 10);
	if (val == 0) {
		TUTRACE((TUTRACE_ERR, "MC_GenerateDevPwd: DevPwd is not numeric\n"));
		return WPS_ERR_SYSTEM;
	}
	checksum = wps_computeChecksum(val);
	val = val*10 + checksum;
	sprintf(local_pwd, "%08u", val); /* make sure 8 characters are displayed */
	local_pwd[8] = '\0';
	TUTRACE((TUTRACE_ERR, "MC::GenerateDevPwd: wps_device_pin = %s\n", local_pwd));
	buffobj_del(bo_devPwd);

	if (b_display)
		/* Display this pwd */
		TUTRACE((TUTRACE_ERR, "DEVICE PIN: %s\n", local_pwd));

	/* Output result */
	strncpy(c_devPwd, local_pwd, 8);
	c_devPwd[8] = '\0';

	return WPS_SUCCESS;
}

bool
wps_isSRPBC(IN uint8 *p_data, IN uint32 len)
{
	WpsProbrspIE probrspIE;
	/* De-serialize this IE to get to the data */
	BufferObj *bufObj = buffobj_new();

	if (!bufObj)
		return FALSE;

	buffobj_dserial(bufObj, p_data, len);

	tlv_set(&probrspIE.selRegistrar, WPS_ID_SEL_REGISTRAR, (void *)0, 0);
	tlv_set(&probrspIE.pwdId, WPS_ID_DEVICE_PWD_ID, (void *)0, 0);

	if (!tlv_find_dserialize(&probrspIE.selRegistrar, WPS_ID_SEL_REGISTRAR, bufObj, 0, 0) &&
	    probrspIE.selRegistrar.m_data == TRUE)
		tlv_find_dserialize(&probrspIE.pwdId, WPS_ID_DEVICE_PWD_ID, bufObj, 0, 0);

	buffobj_del(bufObj);
	if (probrspIE.pwdId.m_data == 0x04)
		return TRUE;
	else
		return FALSE;
}

/*
 * Return the state of WPS IE attribute "Select Registrar" indicating if the user has
 * recently activated a registrar to add an Enrollee.
*/
bool
wps_isSELR(IN uint8 *p_data, IN uint32 len)
{
	WpsBeaconIE probrspIE;
	/* De-serialize this IE to get to the data */
	BufferObj *bufObj = buffobj_new();

	if (!bufObj)
		return FALSE;

	buffobj_dserial(bufObj, p_data, len);

	tlv_set(&probrspIE.selRegistrar, WPS_ID_SEL_REGISTRAR, (void *)0, 0);

	tlv_find_dserialize(&probrspIE.selRegistrar, WPS_ID_SEL_REGISTRAR, bufObj, 0, 0);

	buffobj_del(bufObj);

	if (probrspIE.selRegistrar.m_data == TRUE)
		return TRUE;
	else
		return FALSE;
}

bool
wps_isWPSS(IN uint8 *p_data, IN uint32 len)
{
	WpsProbrspIE probrspIE;
	/* De-serialize this IE to get to the data */
	BufferObj *bufObj = buffobj_new();

	if (!bufObj)
		return FALSE;

	buffobj_dserial(bufObj, p_data, len);

	tlv_dserialize(&probrspIE.version, WPS_ID_VERSION, bufObj, 0, 0);
	tlv_dserialize(&probrspIE.scState, WPS_ID_SC_STATE, bufObj, 0, 0);

	buffobj_del(bufObj);
	if (probrspIE.scState.m_data == WPS_SCSTATE_CONFIGURED)
		return TRUE; /* configured */
	else
		return FALSE; /* unconfigured */
}

/* WSC 2.0 */
bool
wps_isVersion2(uint8 *p_data, uint32 len, uint8 *version2, uint8 *macs)
{
	bool isv2 = FALSE;
	WpsProbrspIE probrspIE;
	/* De-serialize this IE to get to the data */
	BufferObj *bufObj = NULL;
	BufferObj *vendorExt_bufObj = NULL;


	bufObj = buffobj_new();
	if (!bufObj)
		return FALSE;

	buffobj_dserial(bufObj, p_data, len);

	/* Initial set */
	tlv_vendorExt_set(&probrspIE.vendorExt, NULL, NULL, 0);
	subtlv_set(&probrspIE.version2, WPS_WFA_SUBID_VERSION2, (void *)0, 0);
	subtlv_set(&probrspIE.authorizedMACs, WPS_WFA_SUBID_AUTHORIZED_MACS, (void *)0, 0);

	/* Check subelement in WFA Vendor Extension */
	if (tlv_find_vendorExtParse(&probrspIE.vendorExt, bufObj, (uint8 *)WFA_VENDOR_EXT_ID) != 0)
		goto no_wfa_vendorExt;

	/* Deserialize subelement */
	vendorExt_bufObj = buffobj_new();
	if (!vendorExt_bufObj) {
		buffobj_del(bufObj);
		return FALSE;
	}

	buffobj_dserial(vendorExt_bufObj, probrspIE.vendorExt.vendorData,
		probrspIE.vendorExt.dataLength);

	/* Get Version2 and AuthorizedMACs subelement */
	if (!subtlv_dserialize(&probrspIE.version2, WPS_WFA_SUBID_VERSION2,
		vendorExt_bufObj, 0, 0) && probrspIE.version2.m_data >= WPS_VERSION2) {
		/* Get AuthorizedMACs list */
		subtlv_find_dserialize(&probrspIE.authorizedMACs, WPS_WFA_SUBID_AUTHORIZED_MACS,
			vendorExt_bufObj,  0, 0);
	}

no_wfa_vendorExt:
	/* Copy AuthorizedMACs list back */
	if (version2)
		*version2 = probrspIE.version2.m_data;

	if (probrspIE.authorizedMACs.subtlvbase.m_len && macs) {
		memcpy(macs, probrspIE.authorizedMACs.m_data,
			probrspIE.authorizedMACs.subtlvbase.m_len);
		isv2 = TRUE;
	}

	if (vendorExt_bufObj)
		buffobj_del(vendorExt_bufObj);

	buffobj_del(bufObj);

	return isv2;
}

/* Is mac in AuthorizedMACs list */
bool
wps_isAuthorizedMAC(IN uint8 *p_data, IN uint32 len, IN uint8 *mac)
{
	int i;
	bool found = FALSE;
	uint8 *amac;
	WpsProbrspIE probrspIE;
	/* De-serialize this IE to get to the data */
	BufferObj *bufObj = NULL;
	BufferObj *vendorExt_bufObj = NULL;


	bufObj = buffobj_new();
	if (!bufObj)
		return FALSE;

	buffobj_dserial(bufObj, p_data, len);

	/* Initial set */
	tlv_vendorExt_set(&probrspIE.vendorExt, NULL, NULL, 0);
	subtlv_set(&probrspIE.version2, WPS_WFA_SUBID_VERSION2, (void *)0, 0);
	subtlv_set(&probrspIE.authorizedMACs, WPS_WFA_SUBID_AUTHORIZED_MACS, (void *)0, 0);

	/* Check subelement in WFA Vendor Extension */
	if (tlv_find_vendorExtParse(&probrspIE.vendorExt, bufObj, (uint8 *)WFA_VENDOR_EXT_ID) != 0)
		goto no_wfa_vendorExt;

	/* Deserialize subelement */
	vendorExt_bufObj = buffobj_new();
	if (!vendorExt_bufObj) {
		buffobj_del(bufObj);
		return FALSE;
	}

	buffobj_dserial(vendorExt_bufObj, probrspIE.vendorExt.vendorData,
		probrspIE.vendorExt.dataLength);

	/* Get Version2 and AuthorizedMACs subelement */
	if (!subtlv_dserialize(&probrspIE.version2, WPS_WFA_SUBID_VERSION2,
		vendorExt_bufObj, 0, 0) && probrspIE.version2.m_data >= WPS_VERSION2) {
		/* Get AuthorizedMACs list */
		subtlv_find_dserialize(&probrspIE.authorizedMACs, WPS_WFA_SUBID_AUTHORIZED_MACS,
			vendorExt_bufObj,  0, 0);
	}

no_wfa_vendorExt:
	/* Check Mac in AuthorizedMACs list */
	if (probrspIE.authorizedMACs.subtlvbase.m_len >= SIZE_MAC_ADDR && mac) {
		for (i = 0; i < probrspIE.authorizedMACs.subtlvbase.m_len; i += SIZE_MAC_ADDR) {
			amac = &probrspIE.authorizedMACs.m_data[i];
			if (memcmp(amac, mac, SIZE_MAC_ADDR) == 0) {
				found = TRUE;
				break;
			}
		}
	}

	if (vendorExt_bufObj)
		buffobj_del(vendorExt_bufObj);

	buffobj_del(bufObj);

	return found;
}

bool is_wps_ies(uint8* cp, uint len)
{
	uint8 *parse = cp;
	uint parse_len = len;
	uint8 *wpaie;

	while ((wpaie = wps_parse_tlvs(parse, parse_len, DOT11_MNG_WPA_ID)))
		if (wl_is_wps_ie(&wpaie, &parse, &parse_len))
			break;
	if (wpaie)
		return TRUE;
	else
		return FALSE;
}

uint8 *wps_get_wps_iebuf(uint8* cp, uint len, uint *wps_iebuf_len)
{
	uint8 *parse = cp;
	uint parse_len = len;
	uint8 *wpaie, wps_ie_len;
	uint wps_iebuf_curr_len = 0;

	while ((wpaie = wps_parse_tlvs(parse, parse_len, DOT11_MNG_WPA_ID))) {
		if (wl_is_wps_ie(&wpaie, &parse, &parse_len)) {
			/* Skip abnormal IE */
			wps_ie_len = wpaie[1];

			if (wps_ie_len < parse_len &&
			    (wps_iebuf_curr_len + wps_ie_len <= WPS_IE_BUF_LEN - 4)) {
				memcpy(&enr_api_wps_iebuf[wps_iebuf_curr_len], &wpaie[6],
					(wps_ie_len - 4)); /* ignore 00:50:F2:04 */
				wps_iebuf_curr_len += (wps_ie_len - 4);
			}
			/* point to the next ie */
			parse = wpaie + wps_ie_len + 2;
			parse_len = len - (parse - cp);
		}
	}

	if (wps_iebuf_len)
		*wps_iebuf_len = wps_iebuf_curr_len;

	if (wps_iebuf_curr_len)
		return enr_api_wps_iebuf;
	else
		return NULL;
}

bool is_SelectReg_PBC(uint8* cp, uint len)
{
	uint8 *wps_iebuf;
	uint wps_iebuf_len = 0;

	wps_iebuf = wps_get_wps_iebuf(cp, len, &wps_iebuf_len);
	if (wps_iebuf)
		return wps_isSRPBC(wps_iebuf, wps_iebuf_len);

	return FALSE;
}

bool is_AuthorizedMAC(uint8* cp, uint len, uint8 *mac)
{
	uint8 *wps_iebuf;
	uint wps_iebuf_len = 0;

	wps_iebuf = wps_get_wps_iebuf(cp, len, &wps_iebuf_len);
	if (wps_iebuf)
		return wps_isAuthorizedMAC(wps_iebuf, wps_iebuf_len, mac);

	return FALSE;
}

bool is_ConfiguredState(uint8* cp, uint len)
{
	uint8 *wps_iebuf;
	uint wps_iebuf_len = 0;

	wps_iebuf = wps_get_wps_iebuf(cp, len, &wps_iebuf_len);
	if (wps_iebuf)
		return wps_isWPSS(wps_iebuf, wps_iebuf_len);

	return FALSE;
}

/* WSC 2.0, The WPS IE may fragment */
bool is_wpsVersion2(uint8* cp, uint len, uint8 *version2, uint8 *macs)
{
	uint8 *wps_iebuf;
	uint wps_iebuf_len = 0;

	wps_iebuf = wps_get_wps_iebuf(cp, len, &wps_iebuf_len);
	if (wps_iebuf)
		return wps_isVersion2(wps_iebuf, wps_iebuf_len, version2, macs);

	return FALSE;
}

/* Is this body of this tlvs entry a WPA entry? If */
/* not update the tlvs buffer pointer/length */
bool wl_is_wps_ie(uint8 **wpaie, uint8 **tlvs, uint *tlvs_len)
{
	uint8 *ie = *wpaie;

	/* If the contents match the WPA_OUI and type=1 */
	if ((ie[1] >= 6) && !memcmp(&ie[2], WPA_OUI "\x04", 4)) {
		return TRUE;
	}

	/* point to the next ie */
	ie += ie[1] + 2;
	/* calculate the length of the rest of the buffer */
	*tlvs_len -= (int)(ie - *tlvs);
	/* update the pointer to the start of the buffer */
	*tlvs = ie;

	return FALSE;
}

/*
 * Traverse a string of 1-byte tag/1-byte length/variable-length value
 * triples, returning a pointer to the substring whose first element
 * matches tag
 */
uint8 *
wps_parse_tlvs(uint8 *tlv_buf, int buflen, uint key)
{
	uint8 *cp;
	int totlen;

	cp = tlv_buf;
	totlen = buflen;

	/* find tagged parameter */
	while (totlen >= 2) {
		uint tag;
		int len;

		tag = *cp;
		len = *(cp +1);

		/* validate remaining totlen */
		if ((tag == key) && (totlen >= (len + 2)))
			return (cp);

		cp += (len + 2);
		totlen -= (len + 2);
	}

	return NULL;
}

/*
 * filter wps enabled AP list. list_in and list_out can be the same.
 * return the number of WPS APs.
 */
int
wps_get_aplist(wps_ap_list_info_t *list_in, wps_ap_list_info_t *list_out)
{
	wps_ap_list_info_t *ap_in = &list_in[0];
	wps_ap_list_info_t *ap_out = &list_out[0];
	int i = 0, wps_apcount = 0;

	while (ap_in->used == TRUE && i < WPS_MAX_AP_SCAN_LIST_LEN) {
		if (true == is_wps_ies(ap_in->ie_buf, ap_in->ie_buflen)) {
			if (true == is_ConfiguredState(ap_in->ie_buf, ap_in->ie_buflen))
				ap_in->scstate = WPS_SCSTATE_CONFIGURED;
			/* WSC 2.0 */
			is_wpsVersion2(ap_in->ie_buf, ap_in->ie_buflen,
				&ap_in->version2, ap_in->authorizedMACs);
			memcpy(ap_out, ap_in, sizeof(wps_ap_list_info_t));
			wps_apcount++;
			ap_out = &list_out[wps_apcount];
		}
		i++;
		ap_in = &list_in[i];
	}

	/* in case of on-the-spot filtering, make sure we stop the list  */
	if (wps_apcount < WPS_MAX_AP_SCAN_LIST_LEN)
		ap_out->used = 0;

	return wps_apcount;
}

int
wps_get_pin_aplist(wps_ap_list_info_t *list_in, wps_ap_list_info_t *list_out)
{
	wps_ap_list_info_t *ap_in = &list_in[0];
	wps_ap_list_info_t *ap_out = &list_out[0];
	int i = 0, wps_apcount = 0;

	while (ap_in->used == TRUE && i < WPS_MAX_AP_SCAN_LIST_LEN) {
		if (true == is_ConfiguredState(ap_in->ie_buf, ap_in->ie_buflen) &&
		    true == wps_get_select_reg(ap_in) &&
		    false == is_SelectReg_PBC(ap_in->ie_buf, ap_in->ie_buflen)) {
			ap_in->scstate = WPS_SCSTATE_CONFIGURED;
			/* WSC 2.0 */
			is_wpsVersion2(ap_in->ie_buf, ap_in->ie_buflen,
				&ap_in->version2, ap_in->authorizedMACs);

			memcpy(ap_out, ap_in, sizeof(wps_ap_list_info_t));
			wps_apcount++;
			ap_out = &list_out[wps_apcount];
		}
		i++;
		ap_in = &list_in[i];
	}

	/* in case of on-the-spot filtering, make sure we stop the list  */
	if (wps_apcount < WPS_MAX_AP_SCAN_LIST_LEN)
		ap_out->used = 0;

	return wps_apcount;
}

int
wps_get_pbc_ap(wps_ap_list_info_t *list_in, char *bssid, char *ssid, uint8 *wep,
	unsigned long time, char start)
{
	wps_ap_list_info_t *ap_in = &list_in[0];
	int pbc_found = 0;
	int i = 0;
	int band_2G = 0, band_5G = 0;
	static unsigned long start_time;

	if (start)
		start_time = time;
	if (time  > start_time + PBC_WALK_TIME)
		return PBC_TIMEOUT;

	while (ap_in->used == TRUE && i < WPS_MAX_AP_SCAN_LIST_LEN) {
		if (true == is_SelectReg_PBC(ap_in->ie_buf, ap_in->ie_buflen)) {
			if (ap_in->band == WPS_RFBAND_50GHZ)
				band_5G++;
			else if (ap_in->band == WPS_RFBAND_24GHZ)
				band_2G++;
			TUTRACE((TUTRACE_INFO, "2.4G = %d, 5G = %d, band = 0x%x\n",
				band_2G, band_5G, ap_in->band));
			if (pbc_found) {
				if ((band_5G > 1) || (band_2G > 1))
					return PBC_OVERLAP;
			}
			pbc_found++;
			memcpy(bssid, ap_in->BSSID, 6);
			strcpy(ssid, (char *)ap_in->ssid);
			*wep = ap_in->wep;
		}

		i++;
		ap_in = &list_in[i];
	}

	if (!pbc_found)
		return PBC_NOT_FOUND;

	return PBC_FOUND_OK;
}

int
wps_get_amac_ap(wps_ap_list_info_t *list_in, uint8 *mac, char wildcard, char *bssid,
	char *ssid, uint8 *wep, unsigned long time, char start)
{
	int i = 0;
	uint8 wc_mac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}; /* wildcard mac */
	static unsigned long amac_start_time;
	wps_ap_list_info_t *ap_in = &list_in[0];

	if (start)
		amac_start_time = time;
	if (time  > amac_start_time + PBC_WALK_TIME)
		return AUTHO_MAC_TIMEOUT;

	while (ap_in->used && i < WPS_MAX_AP_SCAN_LIST_LEN) {
		if (is_AuthorizedMAC(ap_in->ie_buf, ap_in->ie_buflen, mac)) {
			memcpy(bssid, ap_in->BSSID, 6);
			strcpy(ssid, (char *)ap_in->ssid);
			*wep = ap_in->wep;

			/* Method? */
			if (is_SelectReg_PBC(ap_in->ie_buf, ap_in->ie_buflen))
				return AUTHO_MAC_PBC_FOUND;

			return AUTHO_MAC_PIN_FOUND;
		}
		else if (wildcard && is_AuthorizedMAC(ap_in->ie_buf, ap_in->ie_buflen, wc_mac)) {
			memcpy(bssid, ap_in->BSSID, 6);
			strcpy(ssid, (char *)ap_in->ssid);
			*wep = ap_in->wep;

			/* Method? */
			if (is_SelectReg_PBC(ap_in->ie_buf, ap_in->ie_buflen))
				return AUTHO_MAC_WC_PBC_FOUND;

			return AUTHO_MAC_WC_PIN_FOUND;
		}

		i++;
		ap_in = &list_in[i];
	}

	return AUTHO_MAC_NOT_FOUND;
}

/*
 * Return whether a registrar is activated or not and the information is carried in WPS IE
 */
/* WSC 2.0, The WPS IE may fragment */
bool
wps_get_select_reg(const wps_ap_list_info_t *ap_in)
{
	uint8 *wps_iebuf;
	uint wps_iebuf_len = 0;

	wps_iebuf = wps_get_wps_iebuf(ap_in->ie_buf, ap_in->ie_buflen, &wps_iebuf_len);
	if (wps_iebuf)
		return wps_isSELR(wps_iebuf, wps_iebuf_len);

	return FALSE;
}

uint32
wps_update_RFBand(uint8 rfBand)
{
	return devcfg_setRFBand(g_mc->mp_info, rfBand);
}

#ifdef WFA_WPS_20_TESTBED
/* NOTE: ie[0] is NOT a length */
uint32
wps_update_partial_ie(uint8 *buff, int buflen, uint8 *ie, uint8 ie_len,
	uint8 *updie, uint8 updie_len)
{
	uint32 ret = WPS_SUCCESS;
	BufferObj *bufObj = NULL, *ie_bufObj = NULL, *updie_bufObj = NULL;
	uint16 theType, m_len;
	int tlvtype;
	TlvObj_uint8 ie_tlv_uint8, updie_tlv_uint8;
	TlvObj_uint16 ie_tlv_uint16, updie_tlv_uint16;
	TlvObj_uint32 ie_tlv_uint32, updie_tlv_uint32;
	TlvObj_ptru ie_tlv_uint8_ptr, updie_tlv_uint8_ptr;
	TlvObj_ptr ie_tlv_char_ptr, updie_tlv_char_ptr;
	CTlvPrimDeviceType ie_primDeviceType, updie_primDeviceType;
	void *data_v;

	if (!buff || !ie || !updie)
		return WPS_ERR_SYSTEM;

	/* De-serialize the embedded ie data to get the TLVs */
	memcpy(enr_api_wps_iebuf, ie, ie_len);
	ie_bufObj = buffobj_new();
	if (!ie_bufObj)
		return WPS_ERR_SYSTEM;
	buffobj_dserial(ie_bufObj, enr_api_wps_iebuf, ie_len);

	/* De-serialize the update partial ie data to get the TLVs */
	updie_bufObj = buffobj_new();
	if (!updie_bufObj) {
		ret = WPS_ERR_SYSTEM;
		goto error;
	}
	buffobj_dserial(updie_bufObj, updie, updie_len);

	/*  fill in the WPA-WPS OUI  */
	buff[0] = 4;
	buff[1] = 0x00;
	buff[2] = 0x50;
	buff[3] = 0xf2;
	buff[4] = 0x04;

	/*  bufObj is final output */
	bufObj = buffobj_setbuf(buff+5, buflen-5);
	if (!bufObj) {
		ret = WPS_ERR_SYSTEM;
		goto error;
	}

	/* Add each TLVs */
	while ((theType = buffobj_NextType(ie_bufObj))) {
		/* Check update ie */
		tlvtype = tlv_gettype(theType);
		switch (tlvtype) {
		case TLV_UINT8:
			if (tlv_dserialize(&ie_tlv_uint8, theType, ie_bufObj, 0, 0) != 0) {
				ret = WPS_ERR_SYSTEM;
				goto error;
			}
			/* Prepare embedded one */
			data_v = &ie_tlv_uint8.m_data;
			m_len = ie_tlv_uint8.tlvbase.m_len;

			/* Check update one */
			if (tlv_find_dserialize(&updie_tlv_uint8, theType, updie_bufObj,
				0, 0) == 0) {
				/* Use update ie */
				data_v = &updie_tlv_uint8.m_data;
				m_len = updie_tlv_uint8.tlvbase.m_len;
			}
			break;
		case TLV_UINT16:
			if (tlv_dserialize(&ie_tlv_uint16, theType, ie_bufObj, 0, 0) != 0) {
				ret = WPS_ERR_SYSTEM;
				goto error;
			}
			/* Prepare embedded one */
			data_v = &ie_tlv_uint16.m_data;
			m_len = ie_tlv_uint16.tlvbase.m_len;

			/* Check update one */
			if (tlv_find_dserialize(&updie_tlv_uint16, theType, updie_bufObj,
				0, 0) == 0) {
				/* Use update ie */
				data_v = &updie_tlv_uint16.m_data;
				m_len = updie_tlv_uint16.tlvbase.m_len;
			}
			break;
		case TLV_UINT32:
			if (tlv_dserialize(&ie_tlv_uint32, theType, ie_bufObj, 0, 0) != 0) {
				ret = WPS_ERR_SYSTEM;
				goto error;
			}
			/* Prepare embedded one */
			data_v = &ie_tlv_uint32.m_data;
			m_len = ie_tlv_uint32.tlvbase.m_len;

			/* Check update one */
			if (tlv_find_dserialize(&updie_tlv_uint32, theType, updie_bufObj,
				0, 0) == 0) {
				/* Use update ie */
				data_v = &updie_tlv_uint32.m_data;
				m_len = updie_tlv_uint32.tlvbase.m_len;
			}
			break;
		case TLV_UINT8_PTR:
			if (tlv_dserialize(&ie_tlv_uint8_ptr, theType, ie_bufObj, 0, 0) != 0) {
				ret = WPS_ERR_SYSTEM;
				goto error;
			}
			/* Prepare embedded one */
			data_v = ie_tlv_uint8_ptr.m_data;
			m_len = ie_tlv_uint8_ptr.tlvbase.m_len;

			/* Check update one */
			if (tlv_find_dserialize(&updie_tlv_uint8_ptr, theType, updie_bufObj,
				0, 0) == 0) {
				/* Use update ie */
				data_v = updie_tlv_uint8_ptr.m_data;
				m_len = updie_tlv_uint8_ptr.tlvbase.m_len;
			}
			break;
		case TLV_CHAR_PTR:
			if (tlv_dserialize(&ie_tlv_char_ptr, theType, ie_bufObj, 0, 0) != 0) {
				ret = WPS_ERR_SYSTEM;
				goto error;
			}
			/* Prepare embedded one */
			data_v = ie_tlv_char_ptr.m_data;
			m_len = ie_tlv_char_ptr.tlvbase.m_len;

			/* Check update one */
			if (tlv_find_dserialize(&updie_tlv_char_ptr, theType, updie_bufObj,
				0, 0) == 0) {
				/* Use update ie */
				data_v = updie_tlv_char_ptr.m_data;
				m_len = updie_tlv_char_ptr.tlvbase.m_len;
			}
			break;
		default:
			if (theType == WPS_ID_PRIM_DEV_TYPE) {
				if (tlv_primDeviceTypeParse(&ie_primDeviceType,	ie_bufObj) != 0) {
					ret = WPS_ERR_SYSTEM;
					goto error;
				}

				/* Check update one */
				if (tlv_find_primDeviceTypeParse(&updie_primDeviceType,
					updie_bufObj) == 0) {
					/* Use update ie */
					tlv_primDeviceTypeWrite(&updie_primDeviceType, bufObj);
				}
				else {
					/* Use embedded one */
					tlv_primDeviceTypeWrite(&ie_primDeviceType, bufObj);
				}
				continue;
			}

			ret = WPS_ERR_SYSTEM;
			goto error;
		}

		tlv_serialize(theType, bufObj, data_v, m_len);
	}

	/* store length */
	buff[0] += bufObj->m_dataLength;

error:
	if (bufObj)
		buffobj_del(bufObj);
	if (ie_bufObj)
		buffobj_del(ie_bufObj);
	if (updie_bufObj)
		buffobj_del(updie_bufObj);

	return ret;
}
#endif /* WFA_WPS_20_TESTBED */
