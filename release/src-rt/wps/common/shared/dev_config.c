/*
 * Device config
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: dev_config.c 241182 2011-02-17 21:50:03Z gmo $
 */

#include <string.h>
#include <stdlib.h>

#include <wpsheaders.h>
#include <wpscommon.h>
#include <wpserror.h>
#include <info.h>
#include <portability.h>
#include <tutrace.h>
#include <wps_enrapi.h>

devcfg *
devcfg_new()
{
	devcfg *inf = (devcfg*)alloc_init(sizeof(devcfg));
	if (!inf)
		return NULL;

	inf->mp_deviceInfo = (DevInfo *)alloc_init(sizeof(DevInfo));

	if (!inf->mp_deviceInfo) {
		TUTRACE((TUTRACE_INFO, "Info::Info: Could not create deviceInfo\n"));
		free(inf);
		return NULL;
	}

	/* Initialize other member variables */
	memset(inf->mp_deviceInfo, 0, sizeof(DevInfo));
	inf->mp_deviceInfo->assocState = WPS_ASSOC_NOT_ASSOCIATED;
	inf->mp_deviceInfo->configError = 0; /* No error */
	inf->mp_deviceInfo->devPwdId = WPS_DEVICEPWDID_DEFAULT;
	inf->mb_infoConfigSet = false;
	inf->mb_useUsbKey = false;
	inf->mb_regWireless = false;
	inf->mb_useUpnp = false;
	inf->mb_nwKeySet = false;
	inf->m_nwKeyLen = 0;
	inf->mp_dhKeyPair = NULL;
	inf->mcp_devPwd = NULL;
	memset(inf->m_pubKey, 0, SIZE_PUB_KEY);
	memset(inf->m_sha256Hash, 0, SIZE_256_BITS);

	return inf;
}

void
devcfg_delete(devcfg *inf)
{
	if (inf) {
		if (inf->mcp_devPwd)
			free(inf->mcp_devPwd);
		if (inf->mp_deviceInfo)
			free(inf->mp_deviceInfo);

		free(inf);
	}
	return;
}

DevInfo *
devcfg_getDeviceInfo(devcfg *inf)
{
	return inf->mp_deviceInfo;
}

uint32 *
devcfg_getLock(devcfg *inf)
{
	return inf->mh_lock;
}

EMode
devcfg_getConfiguredMode(devcfg *inf)
{
	return inf->e_mode;
}

uint8 *
devcfg_getUUID(devcfg *inf)
{
	return inf->mp_deviceInfo->uuid;
}

void
devcfg_setUUIDStr(devcfg *info, char *st)
{
	int i;
	char temp[10];
	temp[0] = '0';
	temp[1] = 'x';

	for (i = 0; i <= 15; i++) {
		strncpy(&temp[2], st, 2);
		info->mp_deviceInfo->uuid[i] = (uint8) (strtoul(temp, NULL, 16));
		st += 2;
	}
}

uint8 *
devcfg_getMacAddr(devcfg *inf)
{
	return inf->mp_deviceInfo->macAddr;
}

/* Return the WPS Version that has been configured */
uint8
devcfg_getVersion(devcfg *inf)
{
	return inf->mp_deviceInfo->version;
}

/*
 * Return a pointer to deviceName. Also set the length
 * of the field in len.
 */
char *
devcfg_getDeviceName(devcfg *inf, uint16 *len)
{
	*len = (uint16) strlen(inf->mp_deviceInfo->deviceName);
	return inf->mp_deviceInfo->deviceName;
}

/*
 * Return a pointer to manufacturer. Also set the length
 * of the field in len.
 */
char *
devcfg_getManufacturer(devcfg *inf, uint16 *len)
{
	*len = (uint16) strlen(inf->mp_deviceInfo->manufacturer);
	return inf->mp_deviceInfo->manufacturer;
}

/*
 * Return a pointer to modelName. Also set the length
 * of the field in len.
 */
char *
devcfg_getModelName(devcfg *inf, uint16 *len)
{
	*len = (uint16) strlen(inf->mp_deviceInfo->modelName);
	return inf->mp_deviceInfo->modelName;
}

/*
 * Return a pointer to modelNumber. Also set the length
 * of the field in len.
 */
char *
devcfg_getModelNumber(devcfg *inf, uint16 *len)
{
	*len = (uint16) strlen(inf->mp_deviceInfo->modelNumber);
	return inf->mp_deviceInfo->modelNumber;
}

/*
 *  Return a pointer to serialNumber. Also set the length
 *  of the field in len.
 */
char *
devcfg_getSerialNumber(devcfg *inf, uint16 *len)
{
	*len = (uint16) strlen(inf->mp_deviceInfo->serialNumber);
	return inf->mp_deviceInfo->serialNumber;
}

/* Return the ConfigMethods that has been configured */
uint16
devcfg_getConfigMethods(devcfg *inf)
{
	return inf->mp_deviceInfo->configMethods;
}

/* Return the EncrTypeFlags that has been configured */
uint16
devcfg_getEncrTypeFlags(devcfg *inf)
{
	return inf->mp_deviceInfo->encrTypeFlags;
}

/* Return the connTypeFlags that has been configured */
uint8
devcfg_getConnTypeFlags(devcfg *inf)
{
	return inf->mp_deviceInfo->connTypeFlags;
}

/* Return the value of rfBand */
uint8
devcfg_getRFBand(devcfg *inf)
{
	return inf->mp_deviceInfo->rfBand;
}

/* Return the osVersion that has been configured */
uint32
devcfg_getOsVersion(devcfg *inf)
{
	return inf->mp_deviceInfo->osVersion;
}

/* Return the featureId that has been configured */
uint32
devcfg_getFeatureId(devcfg *inf)
{
	return inf->mp_deviceInfo->featureId;
}

/* Return the assocState that has been configured */
uint16
devcfg_getAssocState(devcfg *inf)
{
	return inf->mp_deviceInfo->assocState;
}

/* Return the devicePwdId that has been configured */
uint16
devcfg_getDevicePwdId(devcfg *inf)
{
	return inf->mp_deviceInfo->devPwdId;
}

uint16
devcfg_getscState(devcfg *inf)
{
	return inf->mp_deviceInfo->scState;
}

/* Return the configError that has been configured */
uint16
devcfg_getConfigError(devcfg *inf)
{
	return inf->mp_deviceInfo->configError;
}

/* Return the value of SSID */
char *
devcfg_getSSID(devcfg *inf, uint16 *len)
{
	*len = (uint16) strlen(inf->mp_deviceInfo->ssid);
	return inf->mp_deviceInfo->ssid;
}

/* Return the value of keyMgmt */
char *
devcfg_getKeyMgmt(devcfg *inf, uint16 *len)
{
	*len = (uint16) strlen(inf->mp_deviceInfo->keyMgmt);
	return inf->mp_deviceInfo->keyMgmt;
}

uint16
devcfg_getKeyMgmtType(devcfg *inf)
{
	char *pmgmt = inf->mp_deviceInfo->keyMgmt;

	if (!strcmp(pmgmt, "WPA-PSK"))
		return WPS_WL_AKM_PSK;
	else if (!strcmp(pmgmt, "WPA2-PSK"))
		return WPS_WL_AKM_PSK2;
	else if (!strcmp(pmgmt, "WPA-PSK WPA2-PSK"))
		return WPS_WL_AKM_BOTH;

	return WPS_WL_AKM_NONE;
}

uint16
devcfg_getAuth(devcfg *inf)
{
	return inf->mp_deviceInfo->auth;
}

uint16
devcfg_getWep(devcfg *inf)
{
	return inf->mp_deviceInfo->wep;
}

uint16
devcfg_getWepIdx(devcfg *inf)
{
	return inf->m_wepKeyIdx;
}

uint16
devcfg_setWepIdx(devcfg *inf, uint8 idx)
{
	if (idx >= 1 && idx <= 4) {
		/* Copy the data over */
		inf->m_wepKeyIdx = idx;
		return WPS_SUCCESS;
	}
	else {
		return WPS_ERR_SYSTEM;
	}
}

uint16
devcfg_getCrypto(devcfg *inf)
{
	return inf->mp_deviceInfo->crypto;
}

/* Return the Diffie-Hellman key pair value */
DH *
devcfg_getDHKeyPair(devcfg *inf)
{
	return inf->mp_dhKeyPair;
}

/* Return the Diffie-Hellman public key */
uint8 *
devcfg_getPubKey(devcfg *inf)
{
	return inf->m_pubKey;
}

/* Return the SHA 256 hash */
uint8 *
devcfg_getSHA256Hash(devcfg *inf)
{
	return inf->m_sha256Hash;
}

/* Return the nwKey */
char *
devcfg_getNwKey(devcfg *inf, uint32 *len)
{
	if (inf->mb_nwKeySet) {
		*len = inf->m_nwKeyLen;
		return inf->m_nwKey;
	}
	else {
		TUTRACE((TUTRACE_ERR, "Info_GetNwKey: NwKeynot set\n"));
		*len = 0;
		return NULL;
	}
}

/* Return the device pwd */
char *
devcfg_getDevPwd(devcfg *inf, uint32 *len)
{
	if (inf->mcp_devPwd) {
		*len = (uint32) strlen(inf->mcp_devPwd);
		return inf->mcp_devPwd;
	}
	else {
		*len = 0;
		return NULL;
	}
}

bool
devcfg_isKeySet(devcfg *inf)
{
	return inf->mb_nwKeySet;
}

uint16
devcfg_getPrimDeviceCategory(devcfg *inf)
{
	return inf->mp_deviceInfo->primDeviceCategory;
}

uint32
devcfg_getPrimDeviceOui(devcfg *inf)
{
	return inf->mp_deviceInfo->primDeviceOui;
}

uint32
devcfg_getPrimDeviceSubCategory(devcfg *inf)
{
	return inf->mp_deviceInfo->primDeviceSubCategory;
}

/* Set the value of the Diffie-Hellman key pair */
uint32
devcfg_setDHKeyPair(devcfg *inf, DH *p_dhKeyPair)
{
	inf->mp_dhKeyPair = p_dhKeyPair;
	return WPS_SUCCESS;
}

/* Set the value of the public key */
uint32
devcfg_setPubKey(devcfg *inf, BufferObj *bo_pubKey)
{
	if (SIZE_PUB_KEY == bo_pubKey->m_dataLength) {
		/* Copy the data over */
		memcpy(inf->m_pubKey, bo_pubKey->pBase, SIZE_PUB_KEY);
		return WPS_SUCCESS;
	}
	else {
		return WPS_ERR_SYSTEM;
	}
}

/* Set the value of the hash */
uint32
devcfg_setSHA256Hash(devcfg *inf, BufferObj *bo_sha256Hash)
{
	if (SIZE_256_BITS == bo_sha256Hash->m_dataLength) {
		/* Copy the data over */
		memcpy(inf->m_sha256Hash, bo_sha256Hash->pBase, SIZE_256_BITS);
		return WPS_SUCCESS;
	}
	else {
		return WPS_ERR_SYSTEM;
	}
}

/* Set the value of the NwKey */
uint32
devcfg_setNwKey(devcfg *inf, char *p_nwKey, uint32 nwKeyLen)
{
	if (nwKeyLen > 0 && nwKeyLen <= SIZE_64_BYTES) {
		/* Copy the data over */
		memset(inf->m_nwKey, 0, SIZE_64_BYTES);
		memcpy(inf->m_nwKey, p_nwKey, nwKeyLen);
		inf->m_nwKeyLen = nwKeyLen;
		inf->mb_nwKeySet = true;
		return WPS_SUCCESS;
	}
	else {
		inf->mb_nwKeySet = false;
		return WPS_ERR_SYSTEM;
	}
}

/* Set the value of the dev pwd */
uint32
devcfg_setDevPwd(devcfg *inf, char *c_devPwd)
{
	uint32 len = (uint32) strlen(c_devPwd);
	if (len <= 0) {
		return WPS_ERR_SYSTEM;
	}
	else {
		if (inf->mcp_devPwd)
			free(inf->mcp_devPwd);

		inf->mcp_devPwd = (char*)malloc(len+1);
		if (!inf->mcp_devPwd)
			return WPS_ERR_OUTOFMEMORY;

		strncpy(inf->mcp_devPwd, c_devPwd, len);
		inf->mcp_devPwd[len] = '\0';
		return WPS_SUCCESS;
	}
}

uint32
devcfg_setDevPwdId(devcfg *inf, uint16 devPwdId)
{
	if (devPwdId == inf->mp_deviceInfo->devPwdId) {
		return MC_ERR_VALUE_UNCHANGED;
	}
	else {
		inf->mp_deviceInfo->devPwdId = devPwdId;
		return WPS_SUCCESS;
	}
}

uint32
devcfg_setConfiguredMode(devcfg *inf, EMode e_ConfigMode)
{
	inf->e_mode = e_ConfigMode;
	return WPS_SUCCESS;
}

uint32
devcfg_setRFBand(devcfg *inf, uint8 rfBand)
{
	inf->mp_deviceInfo->rfBand = rfBand;
	return WPS_SUCCESS;
}

/* WSC 2.0 */
uint8
devcfg_getVersion2(devcfg *inf)
{
	return inf->mp_deviceInfo->version2;
}

bool
devcfg_reqToEnroll(devcfg *inf)
{
	return inf->mp_deviceInfo->b_reqToEnroll;
}

uint8
devcfg_getNwKeyShareable(devcfg *inf)
{
	return inf->mp_deviceInfo->b_nwKeyShareable;
}

#ifdef WFA_WPS_20_TESTBED
/* Do zero padding for testing purpose */
bool
devcfg_getZPadding(devcfg *inf)
{
	return inf->mp_deviceInfo->b_zpadding;
}

/* Multiple Credential Attributes for testing purpose */
bool
devcfg_getMCA(devcfg *inf)
{
	return inf->mp_deviceInfo->b_mca;
}

char *
devcfg_getDummySSID(devcfg *inf, uint16 *len)
{
	*len = (uint16) strlen(inf->mp_deviceInfo->dummy_ssid);
	return inf->mp_deviceInfo->dummy_ssid;
}

char *
devcfg_getNewAttribute(devcfg *inf, uint16 *len)
{
	*len = (uint16)inf->mp_deviceInfo->nattr_len;
	return inf->mp_deviceInfo->nattr_tlv;
}
#endif /* WFA_WPS_20_TESTBED */
