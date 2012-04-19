/*
 * WPS State machine
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: state_machine.c 287159 2011-09-30 07:19:20Z kenlo $
 */

#include <bn.h>
#include <wps_dh.h>

#include <wpsheaders.h>
#include <wpscommon.h>
#include <wpserror.h>
#include <portability.h>
#include <tutrace.h>

#include <slist.h>

#include <sminfo.h>
#include <reg_protomsg.h>
#include <reg_proto.h>
#include <statemachine.h>
#include <info.h>
#include <wpsapi.h>


/* public methods */
/*
 * Name        : CStateMachine
 * Description : Class constructor. Initialize member variables, set
 *                    callback function.
 * Arguments   : none
 * Return type : none
 */
CStateMachine *
state_machine_new(void *g_mc, IN CRegProtocol *pc_regProt, IN uint32 operatingMode)
{
	CStateMachine *sm;

	TUTRACE((TUTRACE_INFO, "SM constructor\n"));

	if (!pc_regProt) {
		TUTRACE((TUTRACE_ERR, "SM: pc_regProt not defined"));
		return NULL;
	}

	sm = (CStateMachine *)malloc(sizeof(CStateMachine));
	if (!sm)
		return NULL;

	sm->m_initialized = false;
	sm->mpc_regProt = pc_regProt;
	sm->mps_regData = NULL;
	sm->m_mode = operatingMode;
	sm->g_mc = g_mc;

	return sm;
}

/*
 * Name        : ~CStateMachine
 * Description : Class destructor. Cleanup if necessary.
 * Arguments   : none
 * Return type : none
 */
void
state_machine_delete(CStateMachine *sm)
{
	RegData *mps_regData;

	if (sm) {
		if (sm->mps_regData) {
			mps_regData = sm->mps_regData;
			TUTRACE((TUTRACE_INFO, "SM: Deleting regData\n"));

			buffobj_del(mps_regData->password);
			buffobj_del(mps_regData->authKey);
			buffobj_del(mps_regData->keyWrapKey);
			buffobj_del(mps_regData->emsk);
			buffobj_del(mps_regData->x509csr);
			buffobj_del(mps_regData->x509Cert);
			buffobj_del(mps_regData->inMsg);
			buffobj_del(mps_regData->outMsg);
			free(mps_regData);
		}

		TUTRACE((TUTRACE_INFO, "Free CstateMachine %p\n", sm));
		free(sm);
	}
}

uint32
state_machine_set_local_devinfo(CStateMachine *sm, IN DevInfo *p_localInfo,
	IN uint32 *p_lock, DH *p_dhKeyPair)
{
	if ((!p_localInfo) || (!p_dhKeyPair)) {
		TUTRACE((TUTRACE_ERR, "SM: invalid input parameters\n"));
		return WPS_ERR_INVALID_PARAMETERS;
	}

	sm->mps_localInfo = p_localInfo;
	sm->mp_dhKeyPair = p_dhKeyPair;

	return WPS_SUCCESS;
}

uint32
state_machine_init_sm(CStateMachine *sm)
{
	RegData *mps_regData = sm->mps_regData;

	/* remove old info if any */
	if (mps_regData) {
		TUTRACE((TUTRACE_ERR, "SM: Deleting regData\n"));
		buffobj_del(mps_regData->password);
		buffobj_del(mps_regData->authKey);
		buffobj_del(mps_regData->keyWrapKey);
		buffobj_del(mps_regData->emsk);
		buffobj_del(mps_regData->x509csr);
		buffobj_del(mps_regData->x509Cert);
		buffobj_del(mps_regData->inMsg);
		buffobj_del(mps_regData->outMsg);
		free(mps_regData);
	}
	if (!(sm->mps_regData = (RegData *)alloc_init(sizeof(RegData)))) {
		TUTRACE((TUTRACE_ERR, "SM : out of memory\n"));
		return WPS_ERR_OUTOFMEMORY;
	}

	/* clear it */
	memset(sm->mps_regData, 0, sizeof(RegData));

	mps_regData = sm->mps_regData;
	mps_regData->e_smState = START;
	mps_regData->e_lastMsgRecd = MNONE;
	mps_regData->e_lastMsgSent = MNONE;
	mps_regData->DHSecret = sm->mp_dhKeyPair;
	mps_regData->staEncrSettings = NULL;
	mps_regData->apEncrSettings = NULL;
	mps_regData->p_enrolleeInfo = NULL;
	mps_regData->p_registrarInfo = NULL;

	mps_regData->password = buffobj_new();
	mps_regData->authKey = buffobj_new();
	mps_regData->keyWrapKey = buffobj_new();
	mps_regData->emsk = buffobj_new();
	mps_regData->x509csr = buffobj_new();
	mps_regData->x509Cert = buffobj_new();
	mps_regData->inMsg = buffobj_new();
	mps_regData->outMsg = buffobj_new();

	sm->m_initialized = true;

	return WPS_SUCCESS;
}

void
state_machine_set_enrolleeNonce(CStateMachine *sm, uint8 *p_enrolleeNonce)
{
	RegData *mps_regData = sm->mps_regData;

	memcpy(mps_regData->enrolleeNonce, p_enrolleeNonce, SIZE_128_BITS);
}

void
state_machine_set_passwd(CStateMachine *sm, char *p_devicePasswd,
                     IN uint32 passwdLength)
{
	if (p_devicePasswd && passwdLength) {
		buffobj_Reset(sm->mps_regData->password);
		buffobj_Append(sm->mps_regData->password, passwdLength, (uint8 *)p_devicePasswd);
	}
}

uint32
state_machine_dup_devinfo(DevInfo *inInfo, DevInfo **outInfo)
{
	if (!inInfo)
		return WPS_ERR_INVALID_PARAMETERS;

	*outInfo = (DevInfo *)alloc_init(sizeof(DevInfo));
	if (!*outInfo) {
		TUTRACE((TUTRACE_ERR, "SM: Unable to allocate memory for duplicating"
			" deviceInfo\n"));
		return WPS_ERR_OUTOFMEMORY;
	}

	memcpy(*outInfo, inInfo, sizeof(DevInfo));

	return WPS_SUCCESS;
}

void
state_machine_set_encrypt(CStateMachine *sm, IN void * p_StaEncrSettings,
	IN void * p_ApEncrSettings)
{
	if (!sm->mps_regData)
	return;

	sm->mps_regData->staEncrSettings = p_StaEncrSettings;
	sm->mps_regData->apEncrSettings = p_ApEncrSettings;
}

uint32
state_machine_send_ack(CStateMachine *sm, BufferObj *outmsg, uint8 *regNonce)
{
	uint32 err;

	err = reg_proto_create_ack(sm->mps_regData, outmsg, sm->mps_localInfo->version2, regNonce);
	if (WPS_SUCCESS != err) {
		TUTRACE((TUTRACE_ERR, "SM: reg_proto_create_ack generated an "
			"error: %d\n", err));
		return err;
	}

	return WPS_SEND_MSG_CONT;
}

uint32
state_machine_send_nack(CStateMachine *sm, uint16 configError, BufferObj *outmsg, uint8 *regNonce)
{
	uint32 err;

	err = reg_proto_create_nack(sm->mps_regData, outmsg, configError,
		sm->mps_localInfo->version2, regNonce);
	if (WPS_SUCCESS != err) {
		TUTRACE((TUTRACE_ERR, "SM: reg_proto_create_nack generated an "
			"error: %d\n", err));
		return err;
	}

	return WPS_SEND_MSG_CONT;
}

uint32
state_machine_send_done(CStateMachine *sm, BufferObj *outmsg)
{
	uint32 err;

	err = reg_proto_create_done(sm->mps_regData, outmsg, sm->mps_localInfo->version2);
	if (WPS_SUCCESS != err) {
		TUTRACE((TUTRACE_ERR, "SM: reg_proto_create_done generated an "
			"error: %d\n", err));
		return err;
	}

	return WPS_SEND_MSG_CONT;
}

uint32
state_machine_update_devinfo(CStateMachine *sm, IN DevInfo *p_localInfo)
{
	if (!p_localInfo) {
		TUTRACE((TUTRACE_ERR, "SM: invalid input parameters\n"));
		return WPS_ERR_INVALID_PARAMETERS;
	}

	sm->mps_localInfo = p_localInfo;

	return WPS_SUCCESS;
}
