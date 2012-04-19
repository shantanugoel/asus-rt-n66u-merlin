/*
 * WPS Device info
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: info.h 261558 2011-05-25 05:19:36Z gracecsm $
 */

#ifndef _INFO_
#define _INFO_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __linux__
#include <stdio.h>
#endif

#include <wps_dh.h>


typedef struct {
	DevInfo *mp_deviceInfo;
	DH *mp_dhKeyPair;
	uint8 m_pubKey[SIZE_PUB_KEY];
	uint8 m_sha256Hash[SIZE_256_BITS];
	char m_nwKey[SIZE_64_BYTES+1];
	uint16 m_wepKeyIdx;
	uint32 m_nwKeyLen;
	bool mb_nwKeySet;
	char *mcp_devPwd;
	uint32 *mh_lock;
	bool mb_infoConfigSet;
	EMode e_mode;
	bool mb_useUsbKey;
	bool mb_regWireless;
	bool mb_useUpnp;
	uint8 pre_enrolleeNonce[SIZE_128_BITS];
	uint8 pre_privKey[SIZE_PUB_KEY];
	uint8 m_peerMacAddr[SIZE_6_BYTES];
} devcfg;

devcfg *devcfg_new(void);
void devcfg_delete(devcfg *inf);
DevInfo *devcfg_getDeviceInfo(devcfg *);
uint32 *devcfg_getLock(devcfg *);
EMode devcfg_getConfiguredMode(devcfg *);
uint8 *devcfg_getUUID(devcfg *);
void devcfg_setUUIDStr(devcfg *info, char *st);
uint8 *devcfg_getMacAddr(devcfg *);
uint8 devcfg_getVersion(devcfg *);
char *devcfg_getDeviceName(devcfg *inf, uint16 *len);
char *devcfg_getManufacturer(devcfg *inf, uint16 *len);
char *devcfg_getModelName(devcfg *inf, uint16 *len);
char *devcfg_getModelNumber(devcfg *inf, uint16 *len);
char *devcfg_getSerialNumber(devcfg *inf, uint16 *len);
uint16 devcfg_getConfigMethods(devcfg *inf);
uint16 devcfg_getAuth(devcfg *inf);
uint16 devcfg_getWep(devcfg *inf);
uint16 devcfg_getWepIdx(devcfg *inf);
uint16 devcfg_setWepIdx(devcfg *inf, uint8 idx);
uint16 devcfg_getCrypto(devcfg *inf);
uint16 devcfg_getEncrTypeFlags(devcfg *inf);
uint8 devcfg_getConnTypeFlags(devcfg *inf);
uint8 devcfg_getRFBand(devcfg *inf);
uint32 devcfg_getOsVersion(devcfg *inf);
uint32 devcfg_getFeatureId(devcfg *inf);
uint16 devcfg_getAssocState(devcfg *inf);
uint16 devcfg_getDevicePwdId(devcfg *inf);
uint16 devcfg_getConfigError(devcfg *inf);
char *devcfg_getSSID(devcfg *inf, uint16 *len);
char *devcfg_getKeyMgmt(devcfg *inf, uint16 *len);
uint16 devcfg_getKeyMgmtType(devcfg *inf);
DH *devcfg_getDHKeyPair(devcfg *inf);
uint8 *devcfg_getPubKey(devcfg *inf);
uint8 *devcfg_getSHA256Hash(devcfg *inf);
char *devcfg_getNwKey(devcfg *inf, uint32 *len);
char *devcfg_getDevPwd(devcfg *inf, uint32 *len);
bool devcfg_isKeySet(devcfg *inf);
uint16 devcfg_getPrimDeviceCategory(devcfg *inf);
uint32 devcfg_getPrimDeviceOui(devcfg *inf);
uint32 devcfg_getPrimDeviceSubCategory(devcfg *inf);
uint32 devcfg_setDHKeyPair(devcfg *inf, DH *p_dhKeyPair);
uint32 devcfg_setPubKey(devcfg *inf, BufferObj *bo_pubKey);
uint32 devcfg_setSHA256Hash(devcfg *inf, BufferObj *bo_sha256Hash);
uint32 devcfg_setNwKey(devcfg *inf, char *p_nwKey, uint32 nwKeyLen);
uint32 devcfg_setDevPwd(devcfg *inf, char *c_devPwd);
uint32 devcfg_setDevPwdId(devcfg *inf, uint16 devPwdId);
uint32 devcfg_setConfiguredMode(devcfg *inf, EMode e_ConfigMode);
uint16 devcfg_getscState(devcfg *inf);
uint32 devcfg_setRFBand(devcfg *inf, uint8 rfBand);

/* WSC 2.0 */
uint8 devcfg_getVersion2(devcfg *inf);
bool devcfg_reqToEnroll(devcfg *inf);
uint8 devcfg_getNwKeyShareable(devcfg *inf);

#ifdef WFA_WPS_20_TESTBED
bool devcfg_getZPadding(devcfg *inf);
bool devcfg_getMCA(devcfg *inf);
char *devcfg_getDummySSID(devcfg *inf, uint16 *len);
char *devcfg_getNewAttribute(devcfg *inf, uint16 *len);
#endif /* WFA_WPS_20_TESTBED */

#ifdef __cplusplus
}
#endif


#endif /*  _INFO_ */
