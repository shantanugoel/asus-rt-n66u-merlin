/*
 * Registrataion state machine
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: reg_sm.c 287325 2011-10-03 03:13:33Z kenlo $
 */

#ifdef WIN32
#include <windows.h>
#endif

#include <string.h>

#include <bn.h>
#include <wps_dh.h>
#include <wps_sha256.h>

#include <wpsheaders.h>
#include <wpscommon.h>
#include <wpserror.h>
#include <portability.h>
#include <tutrace.h>
#include <sminfo.h>
#include <reg_protomsg.h>
#include <reg_proto.h>
#include <statemachine.h>
#include <info.h>
#include <wpsapi.h>

#ifdef WFA_WPS_20_TESTBED
#define STR_ATTR_LEN(str, max) \
	((uint16)((b_zpadding && strlen(str) < max) ? strlen(str) + 1 : strlen(str)))
#else /* !WFA_WPS_20_TESTBED */
#define STR_ATTR_LEN(str, max)	((uint16)(strlen(str)))
#endif /* WFA_WPS_20_TESTBED */

/*
 * Include the UPnP Microsostack header file so an AP can send an EAP_FAIL
 * right after sending M2D if there are no currently-subscribed external Registrars.
 */
static uint32 reg_sm_localRegStep(RegSM *r, uint32 msgLen, uint8 *p_msg,
	uint8 *outbuffer, uint32 *out_len);

static uint32 reg_proto_m7ap_EncrSettingsCheck(void *encrSettings);

RegSM *
reg_sm_new(void *g_mc, CRegProtocol *pc_regProt)
{
	RegSM *r = (RegSM *)malloc(sizeof(RegSM));

	if (!r)
		return NULL;

	memset(r, 0, sizeof(RegSM));

	TUTRACE((TUTRACE_INFO, "Registrar constructor\n"));
	r->m_sm = state_machine_new(g_mc, pc_regProt, MODE_REGISTRAR);
	if (!r->m_sm) {
		free(r);
		return NULL;
	}

	r->m_ptimerStatus = 0;
	/* r->m_stophandlemsg = 0; */
	r->m_sentM2 = false;
	r->g_mc = g_mc;

	r->m_er_sentM2 = false;

	return r;
}

void
reg_sm_delete(RegSM *r)
{
	if (r) {
		if (r->mp_peerEncrSettings) {
			reg_msg_es_del(r->mp_peerEncrSettings, 0);
			r->mp_peerEncrSettings = NULL;
		}

		state_machine_delete(r->m_sm);
		TUTRACE((TUTRACE_INFO, "Free RegSM %p\n", r));
		free(r);
	}
}

uint32
reg_sm_initsm(RegSM *r, DevInfo *p_enrolleeInfo,
	bool enableLocalSM, bool enablePassthru)
{
	uint32 err;

	if ((!enableLocalSM) && (!enablePassthru)) {
		TUTRACE((TUTRACE_ERR, "REGSM: enableLocalSM and enablePassthru are "
			"both false.\n"));
		return WPS_ERR_INVALID_PARAMETERS;
	}

	/* if the device is an AP, passthru MUST be enabled */
	if ((r->m_sm->mps_localInfo->b_ap) && (!enablePassthru)) {
		TUTRACE((TUTRACE_ERR, "REGSM: AP Passthru not enabled.\n"));
		return WPS_ERR_INVALID_PARAMETERS;
	}

	r->m_localSMEnabled = enableLocalSM;
	r->m_passThruEnabled = enablePassthru;

	err = state_machine_init_sm(r->m_sm);
	if (WPS_SUCCESS != err)
		return err;

	r->m_sm->mps_regData->p_enrolleeInfo = p_enrolleeInfo;
	r->m_sm->mps_regData->p_registrarInfo = r->m_sm->mps_localInfo;
	r->m_sm->m_user = r;
	r->m_protocoltimerThrdId = 0;

	return WPS_SUCCESS;
}

/*
 * This method is used by the Proxy to send packets to
 * the various registrars. If the local registrar is
 * enabled, this method will call it using RegStepSM
 */
void
reg_sm_restartsm(RegSM *r)
{
	uint32 err;
	void *apEncrSettings, *staEncrSettings;
	RegData *mps_regData;
	BufferObj *passwd = buffobj_new();

	TUTRACE((TUTRACE_INFO, "SM: Restarting the State Machine\n"));

	if (!passwd)
		return;
	/* first extract the all info we need to save */
	mps_regData = r->m_sm->mps_regData;
	buffobj_Append(passwd, mps_regData->password->m_dataLength,
		mps_regData->password->pBase);
	staEncrSettings = mps_regData->staEncrSettings;
	apEncrSettings = mps_regData->apEncrSettings;
	if (mps_regData->p_enrolleeInfo) {
		free(mps_regData->p_enrolleeInfo);
		mps_regData->p_enrolleeInfo = NULL;
	}

	err = state_machine_init_sm(r->m_sm);
	if (WPS_SUCCESS != err) {
		buffobj_del(passwd);
		return;
	}

	r->m_sm->mps_regData->p_registrarInfo = r->m_sm->mps_localInfo;
	state_machine_set_passwd(r->m_sm, (char *)passwd->pBase, passwd->m_dataLength);
	buffobj_del(passwd);
	state_machine_set_encrypt(r->m_sm, staEncrSettings, apEncrSettings);
	r->m_sm->m_initialized = true;
	r->m_sentM2 = false;
	r->m_er_sentM2 = false;
}

uint32
reg_sm_step(RegSM *r, uint32 msgLen, uint8 *p_msg, uint8 *outbuffer, uint32 *out_len)
{
	uint32 err = WPS_SUCCESS;

	TUTRACE((TUTRACE_INFO, "REGSM: Entering Step.\n"));
	if (false == r->m_sm->m_initialized) {
		TUTRACE((TUTRACE_ERR, "REGSM: Not yet initialized.\n"));
		return WPS_ERR_NOT_INITIALIZED;
	}

	/*
	 * Irrespective of whether the local registrar is enabled or whether we're
	 * using an external registrar, we need to send a WPS_Start over EAP to
	 * kickstart the protocol.
	 * If we send a WPS_START msg, we don't need to do anything else i.e.
	 * invoke the local registrar or pass the message over UPnP
	 */
	if ((!msgLen) && (START == r->m_sm->mps_regData->e_smState)&&
		((TRANSPORT_TYPE_EAP == r->m_sm->m_transportType) ||
		(TRANSPORT_TYPE_UPNP_CP == r->m_sm->m_transportType))) {
		/* The outside wps_regUpnpForwardingCheck already handle it for us */
		return WPS_CONT; /* no more processing for WPS_START */
	}

	/*
	 * Send this message to the local registrar only if it hasn't
	 * arrived from UPnP. This is needed on an AP with passthrough
	 * enabled so that the local registrar doesn't
	 * end up processing messages coming over UPnP from a remote registrar
	 */
	if ((r->m_localSMEnabled) && ((! r->m_passThruEnabled) ||
		((r->m_sm->m_transportType != TRANSPORT_TYPE_UPNP_DEV) &&
		(r->m_sm->m_transportType != TRANSPORT_TYPE_UPNP_CP)))) {
			TUTRACE((TUTRACE_INFO, "REGSM: Sending msg to local registrar.\n"));
			err = reg_sm_localRegStep(r, msgLen, p_msg, outbuffer, out_len);
			return err;
	}

	return WPS_CONT;
}

static uint32
reg_sm_localRegStep(RegSM *r, uint32 msgLen, uint8 *p_msg, uint8 *outbuffer, uint32 *out_len)

{
	uint32 err, retVal;
	BufferObj *inMsg = NULL, *outMsg = NULL;

	TUTRACE((TUTRACE_INFO, "REGSM: Entering RegSMStep.\n"));

	TUTRACE((TUTRACE_INFO, "REGSM: Recvd message of length %d\n", msgLen));

	if ((!p_msg || !msgLen) && (START != r->m_sm->mps_regData->e_smState)) {
		TUTRACE((TUTRACE_ERR, "REGSM: Wrong input parameters, smstep = %d\n",
			r->m_sm->mps_regData->e_smState));
		reg_sm_restartsm(r);
		return WPS_CONT;
	}

	if ((MNONE == r->m_sm->mps_regData->e_lastMsgSent) && (msgLen)) {
		/*
		 * We've received a msg from the enrollee, we don't want to send
		 * WPS_Start in this case
		 * Change last message sent from MNONE to MSTART
		 */
		r->m_sm->mps_regData->e_lastMsgSent = MSTART;
	}

	inMsg = buffobj_new();
	if (!inMsg)
		return WPS_ERR_OUTOFMEMORY;

	buffobj_dserial(inMsg, p_msg, msgLen);
	err = reg_proto_check_nonce(r->m_sm->mps_regData->registrarNonce,
		inMsg, WPS_ID_REGISTRAR_NONCE);

	/* make an exception for M1. It won't have the registrar nonce */
	if ((WPS_SUCCESS != err) && (RPROT_ERR_REQD_TLV_MISSING != err)) {
		TUTRACE((TUTRACE_INFO, "REGSM: Recvd message is not meant for"
				" this registrar. Ignoring...\n"));
		buffobj_del(inMsg);
		return WPS_CONT;
	}

	TUTRACE((TUTRACE_INFO, "REGSM: Calling HandleMessage...\n"));
	outMsg = buffobj_setbuf(outbuffer, *out_len);
	if (!outMsg) {
		buffobj_del(inMsg);
		return WPS_ERR_OUTOFMEMORY;
	}

	retVal = reg_sm_handleMessage(r, inMsg, outMsg);
	*out_len = buffobj_Length(outMsg);

	buffobj_del(inMsg);
	buffobj_del(outMsg);

	/* now check the state so we can act accordingly */
	switch (r->m_sm->mps_regData->e_smState) {
	case START:
	case CONTINUE:
		/* do nothing. */
		break;

	case RESTART:
		/* reset the SM */
		reg_sm_restartsm(r);
		break;

	case SUCCESS:
		TUTRACE((TUTRACE_ERR, "REGSM: Notifying MC of success.\n"));

		/* reset the SM */
		reg_sm_restartsm(r);

		if (retVal == WPS_SEND_MSG_CONT)
			return WPS_SEND_MSG_SUCCESS;
		else
			return WPS_SUCCESS;

	case FAILURE:
		TUTRACE((TUTRACE_ERR, "REGSM: Notifying MC of failure.\n"));

		/* reset the SM */
		reg_sm_restartsm(r);

		if (retVal == WPS_SEND_MSG_CONT)
			return WPS_SEND_MSG_ERROR;
		else
			return WPS_MESSAGE_PROCESSING_ERROR;

	default:
		break;
	}

	return retVal;
}

uint32
reg_sm_handleMessage(RegSM *r, BufferObj *msg, BufferObj *outmsg)
{
	uint32 err, retVal = WPS_CONT;
	void *encrSettings = NULL;
	uint32 msgType;
	CTlvEsM8Ap *esM8Ap;
	RegData *regInfo = r->m_sm->mps_regData;

	err = reg_proto_get_msg_type(&msgType, msg);
	if (WPS_SUCCESS != err) {
		TUTRACE((TUTRACE_ERR, "ENRSM: GetMsgType returned error: %d\n", err));
		goto out;
	}

	switch (r->m_sm->mps_regData->e_lastMsgSent) {
	case MSTART:
		if (WPS_ID_MESSAGE_M1 != msgType) {
			TUTRACE((TUTRACE_ERR, "REGSM: Expected M1, received %d\n",
				msgType));
			TUTRACE((TUTRACE_INFO, "REGSM: Sending EAP FAIL\n"));
			/* upper application will send EAP FAIL and continue */
			retVal = WPS_SEND_FAIL_CONT;
			goto out;
		}

		/* Now, schedule a thread to sleep for some time to */
		r->m_ptimerStatus = 0;

		err = reg_proto_process_m1(r->m_sm->mps_regData, msg);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "REGSM: Expected M1, received %d\n",
				msgType));
			goto out;
		}
		r->m_sm->mps_regData->e_lastMsgRecd = M1;

		if (r->m_sm->mps_regData->password->m_dataLength) {
			/* We don't send any encrypted settings currently */
			err = reg_proto_create_m2(r->m_sm->mps_regData, outmsg, NULL);
			if (WPS_SUCCESS != err) {
				TUTRACE((TUTRACE_ERR, "REGSM: BuildMessageM2 err %x\n", err));
				/* upper application will send EAP FAIL and continue */
				retVal = WPS_SEND_FAIL_CONT;
				goto out;
			}
			r->m_sm->mps_regData->e_lastMsgSent = M2;

			/* Indicate M2 send out */
			wps_setProcessStates(WPS_SENDM2);

			/* temporary change */
			r->m_sentM2 = true;
			TUTRACE((TUTRACE_INFO, "REGSM: m_sentM2 = true !!\n"));
		}
		else {
			/* ere is no password present, so send M2D */
			err = reg_proto_create_m2d(r->m_sm->mps_regData, outmsg);
			if (WPS_SUCCESS != err) {
				TUTRACE((TUTRACE_ERR, "REGSM: BuildMessageM2D err %x\n", err));
				/* upper application will send EAP FAIL and continue */
				retVal = WPS_SEND_FAIL_CONT;
				goto out;
			}
			r->m_sm->mps_regData->e_lastMsgSent = M2D;
		}

		/* Set state to CONTINUE */
		r->m_sm->mps_regData->e_smState = CONTINUE;

		/* msg_to_send is ready */
		retVal = WPS_SEND_MSG_CONT;

#ifdef WPS_AP_M2D
		/* For DTM WCN Wireless 463. M1-M2D Proxy Functionality */
		/* Delay send built-in registrar M2D */
		if (r->m_sm->mps_regData->e_lastMsgSent == M2D)
			retVal = WPS_AP_M2D_READY_CONT;
#endif /* WPS_AP_M2D */
		break;

	case M2:
		if (WPS_ID_MESSAGE_M3 != msgType) {
			TUTRACE((TUTRACE_ERR, "REGSM: Expected M3, received %d\n", msgType));
			goto out;
		}

		err = reg_proto_process_m3(r->m_sm->mps_regData, msg);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "REGSM: Process M3 failed %d\n", msgType));
			/* Send a NACK to the Enrollee */
			retVal = state_machine_send_nack(r->m_sm, WPS_ERROR_DEVICE_BUSY,
				outmsg, NULL);
			goto out;
		}
		r->m_sm->mps_regData->e_lastMsgRecd = M3;

		err = reg_proto_create_m4(r->m_sm->mps_regData, outmsg);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "REGSM: Build M4 failed %d\n", msgType));
			/* Send a NACK to the Enrollee */
			retVal = state_machine_send_nack(r->m_sm, WPS_ERROR_DEVICE_BUSY,
				outmsg, NULL);
			goto out;
		}
		r->m_sm->mps_regData->e_lastMsgSent = M4;

		/* set the message state to CONTINUE */
		r->m_sm->mps_regData->e_smState = CONTINUE;

		/* msg_to_send is ready */
		retVal = WPS_SEND_MSG_CONT;
		break;

	case M2D:
		if (WPS_ID_MESSAGE_ACK == msgType) {
			err = reg_proto_process_ack(r->m_sm->mps_regData, msg,
				r->m_sm->mps_localInfo->version2 >= WPS_VERSION2 ? true : false);
			if (WPS_SUCCESS != err) {
				TUTRACE((TUTRACE_ERR, "REGSM: Expected ACK, received %d\n",
					msgType));
				goto out;
			}
		}
		else if (WPS_ID_MESSAGE_NACK == msgType) {
			uint16 configError = 0xffff;
			err = reg_proto_process_nack(r->m_sm->mps_regData, msg,	&configError,
				r->m_sm->mps_localInfo->version2 >= WPS_VERSION2 ? true : false);
			if (configError != WPS_ERROR_NO_ERROR) {
				TUTRACE((TUTRACE_ERR, "REGSM: Recvd NACK with config err: "
					"%d", configError));
			}
		}
		else {
			err = RPROT_ERR_WRONG_MSGTYPE;
		}

		if (err == WPS_SUCCESS)
			r->m_sm->mps_regData->e_smState = RESTART; /* Done processing for now */
		break;

	case M4:
		if (WPS_ID_MESSAGE_M5 != msgType) {
			TUTRACE((TUTRACE_ERR, "REGSM: Expected M5 (0x09), "
					"received %0x\n", msgType));

			/*
			 * WPS spec 1.0h, in 6.10.4 external registrar should send
			 * NACK after receive NACK from AP (802.1X authenticator).
			 */
			if (!regInfo->p_registrarInfo->b_ap && msgType == WPS_ID_MESSAGE_NACK) {
				uint16 configError = 0xffff;
				uint8 wps_version2 = r->m_sm->mps_localInfo->version2;
				err = reg_proto_process_nack(r->m_sm->mps_regData, msg,
					&configError,
					wps_version2 >= WPS_VERSION2 ? true : false);
				if (configError != WPS_ERROR_NO_ERROR) {
					TUTRACE((TUTRACE_ERR, "REGSM: Recvd NACK"
						" with config err: %d", configError));
				}
				retVal = state_machine_send_nack(r->m_sm,
					WPS_ERROR_DEV_PWD_AUTH_FAIL, outmsg, NULL);
			}

			retVal = WPS_ERR_REGISTRATION_PINFAIL;

			goto out;
		}

		err = reg_proto_process_m5(r->m_sm->mps_regData, msg);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "REGSM: Processing M5 faile %d\n",
				msgType));
			/* Send a NACK to the Enrollee */
			if (err == RPROT_ERR_CRYPTO) {
				retVal = state_machine_send_nack(r->m_sm,
					WPS_ERROR_DEV_PWD_AUTH_FAIL, outmsg, NULL);
				retVal = WPS_ERR_REGISTRATION_PINFAIL;
			}
			else {
				retVal = state_machine_send_nack(r->m_sm,
					WPS_ERROR_DEVICE_BUSY, outmsg, NULL);
			}
			goto out;
		}
		r->m_sm->mps_regData->e_lastMsgRecd = M5;

		err = reg_proto_create_m6(r->m_sm->mps_regData, outmsg);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "REGSM: Build M6 failed %d\n",
				msgType));
			/* Send a NACK to the Enrollee */
			retVal = state_machine_send_nack(r->m_sm,
				WPS_ERROR_DEVICE_BUSY, outmsg, NULL);
			goto out;
		}
		r->m_sm->mps_regData->e_lastMsgSent = M6;

		/* set the message state to CONTINUE */
		r->m_sm->mps_regData->e_smState = CONTINUE;

		/* msg_to_send is ready */
		retVal = WPS_SEND_MSG_CONT;
		break;

	case M6:
		if (WPS_ID_MESSAGE_M7 != msgType) {
			TUTRACE((TUTRACE_ERR, "REGSM: Expected M7, received %d\n",
				msgType));
			goto out;
		}

		err = reg_proto_process_m7(r->m_sm->mps_regData, msg, &encrSettings);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "REGSM: Processing M7 failed %d\n",
				msgType));
			/* Send a NACK to the Enrollee */
			if (err == RPROT_ERR_CRYPTO) {
				retVal = state_machine_send_nack(r->m_sm,
					WPS_ERROR_DEV_PWD_AUTH_FAIL, outmsg, NULL);
			}
			else {
				retVal = state_machine_send_nack(r->m_sm,
					WPS_ERROR_DEVICE_BUSY, outmsg, NULL);
			}
			goto out;
		}
		wps_setProcessStates(WPS_SENDM7);
		r->m_sm->mps_regData->e_lastMsgRecd = M7;
		r->mp_peerEncrSettings = encrSettings;

		/* Build message 8 with the appropriate encrypted settings */
		if (r->m_sm->mps_regData->p_enrolleeInfo->b_ap) {
			/*
			 * Send M8 with the same credential get from M7
			 * instead of empty credential
			 */
			esM8Ap = (CTlvEsM8Ap *)r->m_sm->mps_regData->apEncrSettings;
			if (esM8Ap && esM8Ap->ssid.tlvbase.m_len == 0) {
				RegData *regInfo = r->m_sm->mps_regData;

				retVal = state_machine_send_nack(r->m_sm,
					WPS_ERROR_NO_ERROR, outmsg, NULL);

				if (regInfo->p_registrarInfo->version2 >= WPS_VERSION2) {
					/* WSC 2.0, test case 4.1.10 */
					err = reg_proto_m7ap_EncrSettingsCheck(encrSettings);
					if (err != WPS_SUCCESS) {
						/* Record the real error code for further use */
						r->err_code = err;
						r->m_sm->mps_regData->e_smState = FAILURE;
						goto out;
					}
				}

				/* set the message state to success */
				r->m_sm->mps_regData->e_smState = SUCCESS;

				goto out;
			}
			else {
				err = reg_proto_create_m8(r->m_sm->mps_regData,
					outmsg, esM8Ap);
			}
		}
		else {
			err = reg_proto_create_m8(r->m_sm->mps_regData,
				outmsg, r->m_sm->mps_regData->staEncrSettings);
		}

		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "REGSM: build M8 failed %d\n",
				msgType));
			/* Send a NACK to the Enrollee */
			retVal = state_machine_send_nack(r->m_sm,
				WPS_ERROR_DEVICE_BUSY, outmsg, NULL);
			goto out;
		}
		r->m_sm->mps_regData->e_lastMsgSent = M8;

		/* set the message state to continue */
		r->m_sm->mps_regData->e_smState = CONTINUE;

		/* msg_to_send is ready */
		retVal = WPS_SEND_MSG_CONT;
		break;

	case M8:
		if (WPS_ID_MESSAGE_DONE != msgType) {
			TUTRACE((TUTRACE_ERR, "REGSM: Expected DONE, received %d\n",
				msgType));
			/*
			 * WPS spec 1.0h, in 6.10.4 external registrar should send
			 * NACK after receive NACK from AP (802.1X authenticator).
			 */
			if (!regInfo->p_registrarInfo->b_ap && msgType == WPS_ID_MESSAGE_NACK) {
				uint16 configError = 0xffff;
				uint8 wps_version2 = r->m_sm->mps_localInfo->version2;
				err = reg_proto_process_nack(r->m_sm->mps_regData, msg,
					&configError,
					wps_version2 >= WPS_VERSION2 ? true : false);
				if (configError != WPS_ERROR_NO_ERROR) {
					TUTRACE((TUTRACE_ERR, "REGSM: Recvd NACK"
						" with config err: %d", configError));
				}
				retVal = state_machine_send_nack(r->m_sm, configError,
					outmsg, NULL);
			}

			r->m_sm->mps_regData->e_smState = FAILURE;

			goto out;
		}

		err = reg_proto_process_done(r->m_sm->mps_regData, msg,
			r->m_sm->mps_localInfo->version2 >= WPS_VERSION2 ? true : false);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "REGSM: Processing DONE failed %d\n",
				msgType));
			goto out;
		}

		/* Indicate Done received */
		wps_setProcessStates(WPS_MSGDONE);

		if (r->m_sm->mps_regData->p_enrolleeInfo->b_ap &&
			(r->m_sm->m_transportType == TRANSPORT_TYPE_EAP)) {
			retVal = state_machine_send_ack(r->m_sm, outmsg, NULL);
		}

		/* set the message state to success */
		r->m_sm->mps_regData->e_smState = SUCCESS;

		r->m_ptimerStatus = 1;
		break;

	default:
		if (WPS_ID_MESSAGE_NACK == msgType) {
			uint16 configError;

			err = reg_proto_process_nack(r->m_sm->mps_regData, msg, &configError,
				r->m_sm->mps_localInfo->version2 >= WPS_VERSION2 ? true : false);

			if (WPS_SUCCESS != err) {
				TUTRACE((TUTRACE_ERR, "REGSM: ProcessMessageNack err %d\n",
					err));
				goto out;
			}
			TUTRACE((TUTRACE_ERR, "REGSM: Recvd NACK with err code: %d",
				configError));
			r->m_sm->mps_regData->e_smState = FAILURE;
		}
		else {
			TUTRACE((TUTRACE_ERR, "REGSM: Expected M1, received %d\n",
				msgType));
			goto out;
		}
	}

out:

	return retVal;
}

/* WSC 2.0 */
static uint32
reg_proto_m7ap_EncrSettingsCheck(void *encrSettings)
{
	CTlvEsM7Ap *esAP = (CTlvEsM7Ap *)encrSettings;

	if (!esAP)
		return WPS_ERR_INVALID_PARAMETERS;

	if (esAP->encrType.m_data == WPS_ENCRTYPE_WEP) {
		TUTRACE((TUTRACE_ERR, "Deprecated WEP encryption detected\n"));
		return RPROT_ERR_INCOMPATIBLE_WEP;
	}

	return WPS_SUCCESS;
}

uint32
reg_proto_process_m1(RegData *regInfo, BufferObj *msg)
{
	WpsM1 m;
	int ret;
	uint32 tlv_err = 0;
	char *deviceName;

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM1: %d byte message\n",
		msg->m_dataLength));
	/* First, deserialize (parse) the message. */

	/* First and foremost, check the version and message number. */
	if ((ret = reg_proto_verCheck(&m.version, &m.msgType,
		WPS_ID_MESSAGE_M1, msg)) != WPS_SUCCESS)
		return ret;

	reg_msg_init(&m, WPS_ID_MESSAGE_M1);

	/* Add in PF#3, we should not trust what we receive. (Ralink issue) */
	tlv_err |= tlv_dserialize(&m.uuid, WPS_ID_UUID_E, msg, SIZE_UUID, 0);
	tlv_err |= tlv_dserialize(&m.macAddr, WPS_ID_MAC_ADDR, msg, SIZE_MAC_ADDR, 0);
	tlv_err |= tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE, msg, SIZE_128_BITS, 0);
	tlv_err |= tlv_dserialize(&m.publicKey, WPS_ID_PUBLIC_KEY, msg, SIZE_PUB_KEY, 0);
	tlv_err |= tlv_dserialize(&m.authTypeFlags, WPS_ID_AUTH_TYPE_FLAGS, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.encrTypeFlags, WPS_ID_ENCR_TYPE_FLAGS, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.connTypeFlags, WPS_ID_CONN_TYPE_FLAGS, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.configMethods, WPS_ID_CONFIG_METHODS, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.scState, WPS_ID_SC_STATE, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.manufacturer, WPS_ID_MANUFACTURER, msg, SIZE_64_BYTES, 0);
	tlv_err |= tlv_dserialize(&m.modelName, WPS_ID_MODEL_NAME, msg, SIZE_32_BYTES, 0);
	tlv_err |= tlv_dserialize(&m.modelNumber, WPS_ID_MODEL_NUMBER, msg, SIZE_32_BYTES, 0);
	tlv_err |= tlv_dserialize(&m.serialNumber, WPS_ID_SERIAL_NUM, msg, SIZE_32_BYTES, 0);

	tlv_err |= tlv_primDeviceTypeParse(&m.primDeviceType, msg);
	tlv_err |= tlv_dserialize(&m.deviceName, WPS_ID_DEVICE_NAME, msg, SIZE_32_BYTES, 0);
	tlv_err |= tlv_dserialize(&m.rfBand, WPS_ID_RF_BAND, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.assocState, WPS_ID_ASSOC_STATE, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.devPwdId, WPS_ID_DEVICE_PWD_ID, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.configError, WPS_ID_CONFIG_ERROR, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.osVersion, WPS_ID_OS_VERSION, msg, 0, 0);
	if (tlv_err) {
		TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM1: Bad attributes\n"));
		return RPROT_ERR_INVALID_VALUE;
	}

	/* WSC 2.0, parse WFA vendor id and subelements from vendor extension attribute  */
	if (regInfo->p_registrarInfo->version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			BufferObj *vendorExt_bufObj = buffobj_new();

			/* Deserialize subelement */
			if (!vendorExt_bufObj) {
				TUTRACE((TUTRACE_ERR, "Fail to allocate vendor extension buffer,"
					"Out of memory\n"));
				return WPS_ERR_OUTOFMEMORY;
			}

			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* WSC 2.0, is next Version 2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);

			if (WPS_WFA_SUBID_REQ_TO_ENROLL == buffobj_NextSubId(vendorExt_bufObj))
				subtlv_dserialize(&m.reqToEnr, WPS_WFA_SUBID_REQ_TO_ENROLL,
					vendorExt_bufObj, 0, 0);

			buffobj_del(vendorExt_bufObj);
		}
	}

	deviceName = m.deviceName.m_data ? m.deviceName.m_data : "";
	TUTRACE((TUTRACE_INFO, "The device name is %s\n", deviceName));
	wps_setStaDevName((unsigned char*)deviceName);

	/* skip the optional attributes */
	if (!regInfo->p_enrolleeInfo) {
		regInfo->p_enrolleeInfo = (DevInfo *)alloc_init(sizeof(DevInfo));
		if (!regInfo->p_enrolleeInfo)
			return WPS_ERR_OUTOFMEMORY;
	}

	/* Save Enrollee's Informations */
	memcpy(regInfo->p_enrolleeInfo->uuid, m.uuid.m_data, m.uuid.tlvbase.m_len);
	memcpy(regInfo->p_enrolleeInfo->macAddr, m.macAddr.m_data, m.macAddr.tlvbase.m_len);
	regInfo->p_enrolleeInfo->version2 = m.version2.m_data;

	memcpy(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len);

	/* Extract the peer's public key. */
	/* First store the raw public key (to be used for e/rhash computation) */
	memcpy(regInfo->pke, m.publicKey.m_data, SIZE_PUB_KEY);

	/* Next, allocate memory for the pub key */

	regInfo->DH_PubKey_Peer = BN_new();
	if (!regInfo->DH_PubKey_Peer) {
		TUTRACE((TUTRACE_ERR, "ProcessMessageM1 generated WPS_ERR_OUTOFMEMORY\n"));
		return WPS_ERR_OUTOFMEMORY;
	}

	/* Finally, import the raw key into the bignum datastructure */
	if (BN_bin2bn(regInfo->pke, SIZE_PUB_KEY, regInfo->DH_PubKey_Peer) == NULL) {
		TUTRACE((TUTRACE_ERR, "ProcessMessageM1 generated RPROT_ERR_CRYPTO\n"));
		return RPROT_ERR_CRYPTO;
	}

	regInfo->p_enrolleeInfo->authTypeFlags = m.authTypeFlags.m_data;
	regInfo->p_enrolleeInfo->encrTypeFlags = m.encrTypeFlags.m_data;
	regInfo->p_enrolleeInfo->connTypeFlags = m.connTypeFlags.m_data;
	regInfo->p_enrolleeInfo->configMethods = m.configMethods.m_data;
	regInfo->p_enrolleeInfo->scState = m.scState.m_data;
	strncpy(regInfo->p_enrolleeInfo->manufacturer, m.manufacturer.m_data, SIZE_64_BYTES);
	strncpy(regInfo->p_enrolleeInfo->modelName, m.modelName.m_data, SIZE_32_BYTES);
	strncpy(regInfo->p_enrolleeInfo->modelNumber, m.modelNumber.m_data, SIZE_32_BYTES);
	strncpy(regInfo->p_enrolleeInfo->serialNumber, m.serialNumber.m_data, SIZE_32_BYTES);
	regInfo->p_enrolleeInfo->primDeviceCategory = m.primDeviceType.categoryId;
	regInfo->p_enrolleeInfo->primDeviceOui = m.primDeviceType.oui;
	regInfo->p_enrolleeInfo->primDeviceSubCategory = m.primDeviceType.subCategoryId;
	strncpy(regInfo->p_enrolleeInfo->deviceName, m.deviceName.m_data, SIZE_32_BYTES);
	regInfo->p_enrolleeInfo->rfBand = m.rfBand.m_data;
	regInfo->p_enrolleeInfo->assocState = m.assocState.m_data;
	regInfo->p_enrolleeInfo->devPwdId = m.devPwdId.m_data;

	/*
	* Verify that the device password ID indicated by the Enrollee corresponds
	* to a supported password type.  For now, exclude rekey passwords and
	* machine-specified passwords.
	*/
	if (WPS_DEVICEPWDID_MACHINE_SPEC == regInfo->p_enrolleeInfo->devPwdId ||
		WPS_DEVICEPWDID_REKEY == regInfo->p_enrolleeInfo->devPwdId) {
		regInfo->p_enrolleeInfo->configError = m.configError.m_data;
		TUTRACE((TUTRACE_ERR, "ProcessMessageM1 generated WPS_ERR_NOT_IMPLEMENTED\n"));
		/* should probably define a config error for unknown Device Password. */
		return WPS_ERR_NOT_IMPLEMENTED;
	}

	regInfo->p_enrolleeInfo->configError = m.configError.m_data;
	regInfo->p_enrolleeInfo->osVersion = m.osVersion.m_data;

	/*
	 * Check if the enrollee is an AP and set the b_ap flag accordingly
	 * We need to set the flag only if the AP indicates that it is
	 * unconfigured so that we don't send it different configuration
	 */
	if ((m.primDeviceType.categoryId == WPS_DEVICE_TYPE_CAT_NW_INFRA) &&
	(m.primDeviceType.subCategoryId == WPS_DEVICE_TYPE_SUB_CAT_NW_AP)) {
		regInfo->p_enrolleeInfo->b_ap = true;
	}
	else {
		regInfo->p_enrolleeInfo->b_ap = false;
	}

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, buffobj_Length(msg), buffobj_GetBuf(msg));
	return WPS_SUCCESS;
}

uint32
reg_proto_create_m2(RegData *regInfo, BufferObj *msg, void *encrSettings)
{
	uint8 message;
	uint32 err;
	uint8 secret[SIZE_PUB_KEY];
	int secretLen;
	uint8 DHKey[SIZE_256_BITS];
	uint8 kdk[SIZE_256_BITS];
	uint8 hmac[SIZE_256_BITS];
	BufferObj *esBuf;
	BufferObj *cipherText;
	BufferObj *iv;
	BufferObj *hmacData;
	CTlvPrimDeviceType primDev;
	uint8 reg_proto_version = WPS_VERSION;
#ifdef WFA_WPS_20_TESTBED
	/* For internal testing purpose */
	bool b_zpadding = regInfo->p_registrarInfo->b_zpadding;
#endif /* WFA_WPS_20_TESTBED */

	BufferObj *buf1, *buf2, *buf3, *buf1_dser, *buf2_dser;


	buf1 = buffobj_new();
	if (buf1 == NULL)
		return WPS_ERR_OUTOFMEMORY;

	buf2 = buffobj_new();
	if (buf2 == NULL) {
		buffobj_del(buf1);
		return WPS_ERR_OUTOFMEMORY;
	}

	buf3 = buffobj_new();
	if (buf3 == NULL) {
		buffobj_del(buf1);
		buffobj_del(buf2);
		return WPS_ERR_OUTOFMEMORY;
	}

	buf1_dser = buffobj_new();
	if (buf1_dser == NULL) {
		buffobj_del(buf1);
		buffobj_del(buf2);
		buffobj_del(buf3);
		return WPS_ERR_OUTOFMEMORY;
	}

	buf2_dser = buffobj_new();
	if (buf2_dser == NULL) {
		buffobj_del(buf1);
		buffobj_del(buf2);
		buffobj_del(buf3);
		buffobj_del(buf1_dser);
		return WPS_ERR_OUTOFMEMORY;
	}

	/* First, generate or gather the required data */
	message = WPS_ID_MESSAGE_M2;

	/* Registrar nonce */
	RAND_bytes(regInfo->registrarNonce, SIZE_128_BITS);

	if (!regInfo->DHSecret) {
		err = reg_proto_generate_dhkeypair(&regInfo->DHSecret, buf1);
		if (WPS_SUCCESS != err) {
			if (regInfo->DHSecret) {
				DH_free(regInfo->DHSecret);
				regInfo->DHSecret = NULL;
			}

			TUTRACE((TUTRACE_ERR, "RPROTO: Cannot create DH Secret\n"));
			goto error;
		}
	}

	/* extract the DH public key */
	if (reg_proto_BN_bn2bin(regInfo->DHSecret->pub_key, regInfo->pkr) != SIZE_PUB_KEY) {
		err =  RPROT_ERR_CRYPTO;
		goto error;
	}

	/* KDK generation */
	/* 1. generate the DH shared secret */
	secretLen = DH_compute_key_bn(secret, regInfo->DH_PubKey_Peer, regInfo->DHSecret);
	if (secretLen == -1) {
		err = RPROT_ERR_CRYPTO;
		goto error;
	}

	/* 2. compute the DHKey based on the DH secret */
	if (SHA256(secret, secretLen, DHKey) == NULL) {
		TUTRACE((TUTRACE_ERR, "RPROTO: SHA256 calculation failed\n"));
		err = RPROT_ERR_CRYPTO;
		goto error;
	}

	/* 3.Append the enrollee nonce(N1), enrollee mac and registrar nonce(N2) */
	buffobj_Append(buf2, SIZE_128_BITS, regInfo->enrolleeNonce);
	buffobj_Append(buf2, SIZE_MAC_ADDR, regInfo->p_enrolleeInfo->macAddr);
	buffobj_Append(buf2, SIZE_128_BITS, regInfo->registrarNonce);

	/* 4. now generate the KDK */
	hmac_sha256(DHKey, SIZE_256_BITS, buf2->pBase, buf2->m_dataLength, kdk, NULL);

	/* KDK generation */

	/* Derivation of AuthKey, KeyWrapKey and EMSK */
	/* 1. declare and initialize the appropriate buffer objects */
	buffobj_dserial(buf1_dser, kdk, SIZE_256_BITS);
	buffobj_dserial(buf2_dser, (uint8 *)PERSONALIZATION_STRING,
		(uint32)strlen(PERSONALIZATION_STRING));

	/* 2. call the key derivation function */
	reg_proto_derivekey(buf1_dser, buf2_dser, KDF_KEY_BITS, buf1);

	/* 3. split the key into the component keys and store them */
	buffobj_RewindLength(buf1, buf1->m_dataLength);
	buffobj_Append(regInfo->authKey, SIZE_256_BITS, buf1->pCurrent);
	buffobj_Advance(buf1, SIZE_256_BITS);

	buffobj_Append(regInfo->keyWrapKey, SIZE_128_BITS, buf1->pCurrent);
	buffobj_Advance(buf1, SIZE_128_BITS);

	buffobj_Append(regInfo->emsk, SIZE_256_BITS, buf1->pCurrent);

	/* Derivation of AuthKey, KeyWrapKey and EMSK */

	/* Encrypted settings */
	/* encrypted settings. */
	esBuf = buf1;
	cipherText = buf2;
	iv = buf3;

	buffobj_Reset(esBuf);
	buffobj_Reset(cipherText);
	buffobj_Reset(iv);

	if (encrSettings) {
		if (regInfo->p_enrolleeInfo->b_ap) {
			CTlvEsM8Ap *apEs = (CTlvEsM8Ap *)encrSettings;

			if (regInfo->p_registrarInfo->version2 >= WPS_VERSION2) {
				/*
				 * WSC 2.0, (WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK)
				 * allowed only when both the Registrar and the Enrollee
				 * are using WSC 2.0 or newer.
				 * Use WPS_AUTHTYPE_WPA2PSK when enrollee is WSC 1.0 only.
				 */
				if (regInfo->p_enrolleeInfo->version2)
					apEs->authType.m_data &= ~WPS_AUTHTYPE_WPAPSK;
			}
			reg_msg_m8ap_write(apEs, esBuf, regInfo->authKey,
				regInfo->p_registrarInfo->version2 >= WPS_VERSION2 ? TRUE : FALSE);
		}
		else {
			CTlvEsM8Sta *staEs = (CTlvEsM8Sta *)encrSettings;
			reg_msg_m8sta_write_key(staEs, esBuf, regInfo->authKey);
		}
		/* Now encrypt the serialize Encrypted settings buffer */
		reg_proto_encrypt_data(esBuf, regInfo->keyWrapKey,
			regInfo->authKey, cipherText, iv);
	}

	/* Encrypted settings */

	/* start assembling the message */
	tlv_serialize(WPS_ID_VERSION, msg, &reg_proto_version, WPS_ID_VERSION_S);
	tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
	TUTRACE((TUTRACE_INFO, "RPROTO: In BuildMessageM2:  after encrypt 2\n"));
	tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, regInfo->registrarNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_UUID_R, msg, regInfo->p_registrarInfo->uuid, SIZE_UUID);
	tlv_serialize(WPS_ID_PUBLIC_KEY, msg, regInfo->pkr, SIZE_PUB_KEY);
	tlv_serialize(WPS_ID_AUTH_TYPE_FLAGS, msg, &regInfo->p_registrarInfo->authTypeFlags,
		WPS_ID_AUTH_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_ENCR_TYPE_FLAGS, msg, &regInfo->p_registrarInfo->encrTypeFlags,
		WPS_ID_ENCR_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_CONN_TYPE_FLAGS, msg, &regInfo->p_registrarInfo->connTypeFlags,
		WPS_ID_CONN_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_CONFIG_METHODS, msg, &regInfo->p_registrarInfo->configMethods,
		WPS_ID_CONFIG_METHODS_S);
	tlv_serialize(WPS_ID_MANUFACTURER, msg, regInfo->p_registrarInfo->manufacturer,
		STR_ATTR_LEN(regInfo->p_registrarInfo->manufacturer, SIZE_64_BYTES));
	tlv_serialize(WPS_ID_MODEL_NAME, msg, regInfo->p_registrarInfo->modelName,
		STR_ATTR_LEN(regInfo->p_registrarInfo->modelName, SIZE_32_BYTES));
	tlv_serialize(WPS_ID_MODEL_NUMBER, msg, regInfo->p_registrarInfo->modelNumber,
		STR_ATTR_LEN(regInfo->p_registrarInfo->modelNumber, SIZE_32_BYTES));
	tlv_serialize(WPS_ID_SERIAL_NUM, msg, regInfo->p_registrarInfo->serialNumber,
		STR_ATTR_LEN(regInfo->p_registrarInfo->serialNumber, SIZE_32_BYTES));
	primDev.categoryId = regInfo->p_registrarInfo->primDeviceCategory;
	primDev.oui = regInfo->p_registrarInfo->primDeviceOui;
	primDev.subCategoryId = regInfo->p_registrarInfo->primDeviceSubCategory;
	tlv_primDeviceTypeWrite(&primDev, msg);

	tlv_serialize(WPS_ID_DEVICE_NAME, msg, regInfo->p_registrarInfo->deviceName,
		STR_ATTR_LEN(regInfo->p_registrarInfo->deviceName, SIZE_32_BYTES));

	/* set real RF band */
	tlv_serialize(WPS_ID_RF_BAND, msg, &regInfo->p_registrarInfo->rfBand,
		WPS_ID_RF_BAND_S);
	tlv_serialize(WPS_ID_ASSOC_STATE, msg, &regInfo->p_registrarInfo->assocState,
		WPS_ID_ASSOC_STATE_S);
	tlv_serialize(WPS_ID_CONFIG_ERROR, msg, &regInfo->p_registrarInfo->configError,
		WPS_ID_CONFIG_ERROR_S);
	tlv_serialize(WPS_ID_DEVICE_PWD_ID, msg, &regInfo->p_registrarInfo->devPwdId,
		WPS_ID_DEVICE_PWD_ID_S);
	tlv_serialize(WPS_ID_OS_VERSION, msg, &regInfo->p_registrarInfo->osVersion,
		WPS_ID_OS_VERSION_S);
	/* Skip optional attributes */
	/* Encrypted settings */
	if (encrSettings) {
		CTlvEncrSettings encrSettings_tlv;

		encrSettings_tlv.iv = iv->pBase;
		encrSettings_tlv.ip_encryptedData = cipherText->pBase;
		encrSettings_tlv.encrDataLength = cipherText->m_dataLength;
		tlv_encrSettingsWrite(&encrSettings_tlv, msg);
	}

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (regInfo->p_registrarInfo->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		/* Reuse buf1 */
		BufferObj *vendorExt_bufObj = buf1;

		buffobj_Reset(vendorExt_bufObj);

		/* Add Version2 subelement to vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&regInfo->p_registrarInfo->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->p_registrarInfo->nattr_len)
			buffobj_Append(msg, regInfo->p_registrarInfo->nattr_len,
				(uint8*)regInfo->p_registrarInfo->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Add support for Windows Rally Vertical Pairing */
	reg_proto_vendor_ext_vp(regInfo->p_registrarInfo, msg);

	/* Now calculate the hmac */
	hmacData = buf1;
	buffobj_Reset(hmacData);
	buffobj_Append(hmacData, buffobj_Length(regInfo->inMsg), buffobj_GetBuf(regInfo->inMsg));
	buffobj_Append(hmacData, buffobj_Length(msg), buffobj_GetBuf(msg));

	hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
		buffobj_GetBuf(hmacData), buffobj_Length(hmacData), hmac, NULL);

	tlv_serialize(WPS_ID_AUTHENTICATOR, msg, hmac, SIZE_64_BITS);

	/* Store the outgoing message  */
	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, buffobj_Length(msg), buffobj_GetBuf(msg));

	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM2 built: %d bytes\n",
		buffobj_Length(msg)));
	err =  WPS_SUCCESS;

error:
	buffobj_del(buf1);
	buffobj_del(buf2);
	buffobj_del(buf3);
	buffobj_del(buf1_dser);
	buffobj_del(buf2_dser);

	return err;
}

/* M2D generation */
uint32
reg_proto_create_m2d(RegData *regInfo, BufferObj *msg)
{
	uint8 message;
	uint8 reg_proto_version = WPS_VERSION;
	CTlvPrimDeviceType primDev;
#ifdef WFA_WPS_20_TESTBED
	/* For internal testing purpose */
	bool b_zpadding = regInfo->p_registrarInfo->b_zpadding;
#endif /* WFA_WPS_20_TESTBED */

	/* First, generate or gather the required data */
	message = WPS_ID_MESSAGE_M2D;

	/* Registrar nonce */
	RAND_bytes(regInfo->registrarNonce, SIZE_128_BITS);

	/* start assembling the message */

	tlv_serialize(WPS_ID_VERSION, msg, &reg_proto_version, WPS_ID_VERSION_S);
	tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
	tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, regInfo->registrarNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_UUID_R, msg, regInfo->p_registrarInfo->uuid, SIZE_UUID);
	tlv_serialize(WPS_ID_AUTH_TYPE_FLAGS, msg, &regInfo->p_registrarInfo->authTypeFlags,
		WPS_ID_AUTH_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_ENCR_TYPE_FLAGS, msg, &regInfo->p_registrarInfo->encrTypeFlags,
		WPS_ID_ENCR_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_CONN_TYPE_FLAGS, msg, &regInfo->p_registrarInfo->connTypeFlags,
		WPS_ID_CONN_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_CONFIG_METHODS, msg, &regInfo->p_registrarInfo->configMethods,
		WPS_ID_CONFIG_METHODS_S);
	tlv_serialize(WPS_ID_MANUFACTURER, msg, regInfo->p_registrarInfo->manufacturer,
		STR_ATTR_LEN(regInfo->p_registrarInfo->manufacturer, SIZE_64_BYTES));
	tlv_serialize(WPS_ID_MODEL_NAME, msg, regInfo->p_registrarInfo->modelName,
		STR_ATTR_LEN(regInfo->p_registrarInfo->modelName, SIZE_32_BYTES));
	tlv_serialize(WPS_ID_MODEL_NUMBER, msg, regInfo->p_registrarInfo->modelNumber,
		STR_ATTR_LEN(regInfo->p_registrarInfo->modelNumber, SIZE_32_BYTES));
	tlv_serialize(WPS_ID_SERIAL_NUM, msg, regInfo->p_registrarInfo->serialNumber,
		STR_ATTR_LEN(regInfo->p_registrarInfo->serialNumber, SIZE_32_BYTES));

	primDev.categoryId = regInfo->p_registrarInfo->primDeviceCategory;
	primDev.oui = regInfo->p_registrarInfo->primDeviceOui;
	primDev.subCategoryId = regInfo->p_registrarInfo->primDeviceSubCategory;
	tlv_primDeviceTypeWrite(&primDev, msg);

	tlv_serialize(WPS_ID_DEVICE_NAME, msg, regInfo->p_registrarInfo->deviceName,
		STR_ATTR_LEN(regInfo->p_registrarInfo->deviceName, SIZE_32_BYTES));

	/* set real RF band */
	tlv_serialize(WPS_ID_RF_BAND, msg, &regInfo->p_registrarInfo->rfBand, WPS_ID_RF_BAND_S);
	tlv_serialize(WPS_ID_ASSOC_STATE, msg, &regInfo->p_registrarInfo->assocState,
		WPS_ID_ASSOC_STATE_S);
	tlv_serialize(WPS_ID_CONFIG_ERROR, msg, &regInfo->p_registrarInfo->configError,
		WPS_ID_CONFIG_ERROR_S);

	tlv_serialize(WPS_ID_OS_VERSION, msg, &regInfo->p_registrarInfo->osVersion,
		WPS_ID_OS_VERSION_S);

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (regInfo->p_registrarInfo->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		BufferObj *vendorExt_bufObj = buffobj_new();

		if (vendorExt_bufObj == NULL)
			return WPS_ERR_OUTOFMEMORY;

		/* Add Version2 subelement to vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&regInfo->p_registrarInfo->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->p_registrarInfo->nattr_len)
			buffobj_Append(msg, regInfo->p_registrarInfo->nattr_len,
				(uint8*)regInfo->p_registrarInfo->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Store the outgoing message */
	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, msg->m_dataLength, msg->pBase);

	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM2D built: %d bytes\n",
		msg->m_dataLength));

	return WPS_SUCCESS;
}

uint32
reg_proto_process_m3(RegData *regInfo, BufferObj *msg)
{
	WpsM3 m;
	uint32 err;
	BufferObj *hmacData;
	BufferObj *buf1 = buffobj_new();


	if (buf1 == NULL)
		return WPS_ERR_OUTOFMEMORY;

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM3: %d byte message\n",
	         msg->m_dataLength));

	/*
	* First and foremost, check the version and message number.
	* Don't deserialize incompatible messages!
	*/
	if ((err = reg_proto_verCheck(&m.version, &m.msgType,
		WPS_ID_MESSAGE_M3, msg)) != WPS_SUCCESS)
		goto error;

	reg_msg_init(&m, WPS_ID_MESSAGE_M3);

	tlv_dserialize(&m.registrarNonce, WPS_ID_REGISTRAR_NONCE,
		msg, SIZE_128_BITS, 0);
	tlv_dserialize(&m.eHash1, WPS_ID_E_HASH1, msg, SIZE_256_BITS, 0);
	tlv_dserialize(&m.eHash2, WPS_ID_E_HASH2, msg, SIZE_256_BITS, 0);

	/* WSC 2.0, parse WFA vendor id and subelements from vendor extension attribute  */
	if (regInfo->p_registrarInfo->version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			/* Reuse buf1 */
			BufferObj *vendorExt_bufObj = buf1;

			buffobj_Reset(vendorExt_bufObj);

			/* Deserialize subelement */
			if (!vendorExt_bufObj) {
				TUTRACE((TUTRACE_ERR, "Fail to allocate vendor extension buffer,"
					"Out of memory\n"));
				return WPS_ERR_OUTOFMEMORY;
			}

			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* WSC 2.0, is next Version 2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);
		}
		else {
			/* No WFA vendor extension, rewind theBuf to get Authenticator */
			buffobj_Rewind(msg);
		}
	}

	/* skip other optional attributes until we get to the authenticator */
	while (WPS_ID_AUTHENTICATOR != buffobj_NextType(msg)) {
		/* advance past the TLV */
		uint8 *Pos = buffobj_Advance(msg, sizeof(WpsTlvHdr) +
			WpsNtohs(msg->pCurrent+sizeof(uint16)));

		/*
		 * If Advance returned NULL, it means there's no more data in the
		 * buffer. This is an error.
		 */
		if (Pos == NULL) {
			err =  RPROT_ERR_REQD_TLV_MISSING;
			goto error;
		}
	}

	tlv_dserialize(&m.authenticator, WPS_ID_AUTHENTICATOR, msg, SIZE_64_BITS, 0);

	/* Now start validating the message */

	/* confirm the registrar nonce */
	if (memcmp(regInfo->registrarNonce, m.registrarNonce.m_data,
		m.registrarNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect registrar nonce\n"));
		err =  RPROT_ERR_NONCE_MISMATCH;
		goto error;
	}

	/* HMAC validation */
	hmacData = buf1;
	buffobj_Reset(hmacData);
	/* append the last message sent */
	buffobj_Append(hmacData, regInfo->outMsg->m_dataLength, regInfo->outMsg->pBase);
	/* append the current message. Don't append the last TLV (auth) */
	buffobj_Append(hmacData,
		msg->m_dataLength-(sizeof(WpsTlvHdr)+m.authenticator.tlvbase.m_len),
		msg->pBase);
	if (!reg_proto_validate_mac(hmacData, m.authenticator.m_data, regInfo->authKey)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: HMAC validation failed"));
		err =  RPROT_ERR_CRYPTO;
		goto error;
	}

	/* HMAC validation */

	/* Now copy the relevant data */
	memcpy(regInfo->eHash1, m.eHash1.m_data, SIZE_256_BITS);
	memcpy(regInfo->eHash2, m.eHash2.m_data, SIZE_256_BITS);

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, msg->m_dataLength, msg->pBase);

	err = WPS_SUCCESS;

error:
	buffobj_del(buf1);
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "ProcessMessageM3 : got error %x", err));
	return err;
}

uint32
reg_proto_create_m4(RegData *regInfo, BufferObj *msg)
{
	uint8 message;
	uint8 hashBuf[SIZE_256_BITS];
	uint32 err;
	uint8 *pwdPtr;
	int pwdLen;

	BufferObj *buf1, *buf2, *buf3;


	buf1 = buffobj_new();
	if (buf1 == NULL)
		return WPS_ERR_OUTOFMEMORY;

	buf2 = buffobj_new();
	if (buf2 == NULL) {
		buffobj_del(buf1);
		return WPS_ERR_OUTOFMEMORY;
	}

	buf3 = buffobj_new();
	if (buf3 == NULL) {
		buffobj_del(buf1);
		buffobj_del(buf2);
		return WPS_ERR_OUTOFMEMORY;
	}

	/* First, generate or gather the required data */
	message = WPS_ID_MESSAGE_M4;

	/* PSK1 and PSK2 generation */
	pwdPtr = regInfo->password->pBase;
	pwdLen = regInfo->password->m_dataLength;

	/*
	 * Hash 1st half of passwd. If it is an odd length, the extra byte
	 * goes along with the first half
	 */
	hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
		pwdPtr, (pwdLen/2)+(pwdLen%2), hashBuf, NULL);

	/* copy first 128 bits into psk1; */
	memcpy(regInfo->psk1, hashBuf, SIZE_128_BITS);

	/* Hash 2nd half of passwd */
	hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
		pwdPtr+(pwdLen/2)+(pwdLen%2), pwdLen/2, hashBuf, NULL);

	/* copy first 128 bits into psk2; */
	memcpy(regInfo->psk2, hashBuf, SIZE_128_BITS);
	/* PSK1 and PSK2 generation */

	/* RHash1 and RHash2 generation */
	RAND_bytes(regInfo->rs1, SIZE_128_BITS);
	RAND_bytes(regInfo->rs2, SIZE_128_BITS);
	{
		BufferObj *rhashBuf = buf1;
		buffobj_Reset(rhashBuf);
		buffobj_Append(rhashBuf, SIZE_128_BITS, regInfo->rs1);
		buffobj_Append(rhashBuf, SIZE_128_BITS, regInfo->psk1);
		buffobj_Append(rhashBuf, SIZE_PUB_KEY, regInfo->pke);
		buffobj_Append(rhashBuf, SIZE_PUB_KEY, regInfo->pkr);
		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			rhashBuf->pBase, rhashBuf->m_dataLength, hashBuf, NULL);
		memcpy(regInfo->rHash1, hashBuf, SIZE_256_BITS);

		buffobj_Reset(rhashBuf);
		buffobj_Append(rhashBuf, SIZE_128_BITS, regInfo->rs2);
		buffobj_Append(rhashBuf, SIZE_128_BITS, regInfo->psk2);
		buffobj_Append(rhashBuf, SIZE_PUB_KEY, regInfo->pke);
		buffobj_Append(rhashBuf, SIZE_PUB_KEY, regInfo->pkr);
		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
		            rhashBuf->pBase, rhashBuf->m_dataLength, hashBuf, NULL);
	}
	memcpy(regInfo->rHash2, hashBuf, SIZE_256_BITS);

	/* RHash1 and RHash2 generation */
	{
		/* encrypted settings. */
		BufferObj *encData = buf1;
		BufferObj *cipherText = buf2;
		BufferObj *iv = buf3;
		uint8 reg_proto_version = WPS_VERSION;
		TlvEsNonce rsNonce;
		CTlvEncrSettings encrSettings_tlv;

		buffobj_Reset(encData);
		buffobj_Reset(cipherText);
		buffobj_Reset(iv);

		tlv_set(&rsNonce.nonce, WPS_ID_R_SNONCE1, regInfo->rs1, SIZE_128_BITS);
		reg_msg_nonce_write(&rsNonce, encData, regInfo->authKey);

		reg_proto_encrypt_data(encData, regInfo->keyWrapKey,
			regInfo->authKey, cipherText, iv);

		/* Now assemble the message */
		tlv_serialize(WPS_ID_VERSION, msg, &reg_proto_version, WPS_ID_VERSION_S);
		tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
		tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce,
			SIZE_128_BITS);
		tlv_serialize(WPS_ID_R_HASH1, msg, regInfo->rHash1, SIZE_256_BITS);
		tlv_serialize(WPS_ID_R_HASH2, msg, regInfo->rHash2, SIZE_256_BITS);

		encrSettings_tlv.iv = iv->pBase;
		encrSettings_tlv.ip_encryptedData = cipherText->pBase;
		encrSettings_tlv.encrDataLength = cipherText->m_dataLength;
		tlv_encrSettingsWrite(&encrSettings_tlv, msg);
	}

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (regInfo->p_registrarInfo->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		/* Reuse buf1 */
		BufferObj *vendorExt_bufObj = buf1;

		buffobj_Reset(vendorExt_bufObj);

		/* Add Version2 subelement to vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&regInfo->p_registrarInfo->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->p_registrarInfo->nattr_len)
			buffobj_Append(msg, regInfo->p_registrarInfo->nattr_len,
				(uint8*)regInfo->p_registrarInfo->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Calculate the hmac */
	{
		BufferObj *hmacData = buf1;
		uint8 hmac[SIZE_256_BITS];

		buffobj_Reset(hmacData);
		buffobj_Append(hmacData, regInfo->inMsg->m_dataLength, regInfo->inMsg->pBase);
		buffobj_Append(hmacData, msg->m_dataLength, msg->pBase);

		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			hmacData->pBase, hmacData->m_dataLength, hmac, NULL);
		tlv_serialize(WPS_ID_AUTHENTICATOR, msg, hmac, SIZE_64_BITS);
	}
	/* Store the outgoing message */
	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, msg->m_dataLength, msg->pBase);

	err = WPS_SUCCESS;
	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM4 built: %d bytes\n",
		msg->m_dataLength));

	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "BuildMessageM4 : got error %x", err));
	buffobj_del(buf1);
	buffobj_del(buf2);
	buffobj_del(buf3);

	return err;
}

uint32
reg_proto_process_m5(RegData *regInfo, BufferObj *msg)
{
	WpsM5 m;
	uint32 err;

	BufferObj *buf1, *buf2, *buf3, *buf1_dser, *buf2_dser;


	buf1 = buffobj_new();
	if (buf1 == NULL)
		return WPS_ERR_OUTOFMEMORY;

	buf2 = buffobj_new();
	if (buf2 == NULL) {
		buffobj_del(buf1);
		return WPS_ERR_OUTOFMEMORY;
	}

	buf3 = buffobj_new();
	if (buf3 == NULL) {
		buffobj_del(buf1);
		buffobj_del(buf2);
		return WPS_ERR_OUTOFMEMORY;
	}

	buf1_dser = buffobj_new();
	if (buf1_dser == NULL) {
		buffobj_del(buf1);
		buffobj_del(buf2);
		buffobj_del(buf3);
		return WPS_ERR_OUTOFMEMORY;
	}

	buf2_dser = buffobj_new();
	if (buf2_dser == NULL) {
		buffobj_del(buf1);
		buffobj_del(buf2);
		buffobj_del(buf3);
		buffobj_del(buf1_dser);
		return WPS_ERR_OUTOFMEMORY;
	}

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM5: %d byte message\n",
		msg->m_dataLength));

	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	if ((err = reg_proto_verCheck(&m.version, &m.msgType,
		WPS_ID_MESSAGE_M5, msg)) != WPS_SUCCESS)
		goto error;

	reg_msg_init(&m, WPS_ID_MESSAGE_M5);
	tlv_dserialize(&m.registrarNonce, WPS_ID_REGISTRAR_NONCE, msg, SIZE_128_BITS, 0);
	tlv_encrSettingsParse(&m.encrSettings, msg);

	/* WSC 2.0, parse WFA vendor id and subelements from vendor extension attribute  */
	if (regInfo->p_registrarInfo->version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			/* Reuse buf1 */
			BufferObj *vendorExt_bufObj = buf1;

			buffobj_Reset(vendorExt_bufObj);

			/* Deserialize subelement */
			if (!vendorExt_bufObj) {
				TUTRACE((TUTRACE_ERR, "Fail to allocate vendor extension buffer,"
					"Out of memory\n"));
				return WPS_ERR_OUTOFMEMORY;
			}

			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* WSC 2.0, is next Version 2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);
		}
		else {
			/* No WFA vendor extension, rewind theBuf to get Authenticator */
			buffobj_Rewind(msg);
		}
	}

	/* skip other optional attributes until we get to the authenticator */
	while (WPS_ID_AUTHENTICATOR != buffobj_NextType(msg)) {
		/* advance past the TLV */
		uint8 *Pos = buffobj_Advance(msg, sizeof(WpsTlvHdr) +
			WpsNtohs(msg->pCurrent+sizeof(uint16)));

		/*
		 * If Advance returned NULL, it means there's no more data in the
		 * buffer. This is an error.
		 */
		if (Pos == NULL)
			err =  RPROT_ERR_REQD_TLV_MISSING;
	}

	tlv_dserialize(&m.authenticator, WPS_ID_AUTHENTICATOR, msg, SIZE_64_BITS, 0);

	/* Now start validating the message */

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->registrarNonce, m.registrarNonce.m_data,
		m.registrarNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect registrar nonce\n"));
		err =  RPROT_ERR_NONCE_MISMATCH;
		goto error;
	}

	/* HMAC validation */
	{
		BufferObj *hmacData = buf1;
		buffobj_Reset(hmacData);
		/* append the last message sent */
		buffobj_Append(hmacData, regInfo->outMsg->m_dataLength, regInfo->outMsg->pBase);
		/* append the current message. Don't append the last TLV (auth) */
		buffobj_Append(hmacData,
			msg->m_dataLength-(sizeof(WpsTlvHdr)+m.authenticator.tlvbase.m_len),
			msg->pBase);

		if (!reg_proto_validate_mac(hmacData, m.authenticator.m_data, regInfo->authKey)) {
			TUTRACE((TUTRACE_ERR, "RPROTO: HMAC validation failed"));
			err =  RPROT_ERR_CRYPTO;
			goto error;
		}
	}

	/* HMAC validation */

	/* extract encrypted settings */
	{
		BufferObj *cipherText = buf1_dser;
		BufferObj *iv = buf2_dser;
		BufferObj *plainText = buf1;
		TlvEsNonce eNonce;

		buffobj_dserial(cipherText, m.encrSettings.ip_encryptedData,
			m.encrSettings.encrDataLength);
		buffobj_dserial(iv, m.encrSettings.iv, SIZE_128_BITS);
		buffobj_Reset(plainText);
		reg_proto_decrypt_data(cipherText, iv, regInfo->keyWrapKey,
			regInfo->authKey, plainText);

		reg_msg_nonce_parse(&eNonce, WPS_ID_E_SNONCE1, plainText, regInfo->authKey);

	/* extract encrypted settings */

	/* EHash1 validation */
	/* 1. Save ES1 */
		memcpy(regInfo->es1, eNonce.nonce.m_data, eNonce.nonce.tlvbase.m_len);
	}

	{
		/* 2. prepare the buffer */
		BufferObj *ehashBuf = buf1;
		uint8 hashBuf[SIZE_256_BITS];

		buffobj_Reset(ehashBuf);
		buffobj_Append(ehashBuf, SIZE_128_BITS, regInfo->es1);
		buffobj_Append(ehashBuf, SIZE_128_BITS, regInfo->psk1);
		buffobj_Append(ehashBuf, SIZE_PUB_KEY, regInfo->pke);
		buffobj_Append(ehashBuf, SIZE_PUB_KEY, regInfo->pkr);

		/* 3. generate the mac */
		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			ehashBuf->pBase, ehashBuf->m_dataLength, hashBuf, NULL);

		/* 4. compare the mac to ehash1 */
		if (memcmp(regInfo->eHash1, hashBuf, SIZE_256_BITS)) {
			TUTRACE((TUTRACE_ERR, "RPROTO: ES1 hash doesn't match EHash1\n"));
			err =  RPROT_ERR_CRYPTO;
			goto error;
		}

		/* 5. Instead of steps 3 & 4, we could have called ValidateMac */
	}

	/* EHash1 validation */

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, msg->m_dataLength, msg->pBase);
	err = WPS_SUCCESS;
	TUTRACE((TUTRACE_INFO, "RPROTO: ProcessMessageM5 done: %d bytes\n",
		msg->m_dataLength));
error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "ProcessMessageM5: got error %x", err));
	buffobj_del(buf1);
	buffobj_del(buf2);
	buffobj_del(buf3);
	buffobj_del(buf1_dser);
	buffobj_del(buf2_dser);

	return err;
}

uint32
reg_proto_create_m6(RegData *regInfo, BufferObj *msg)
{
	uint8 message;
	uint32 err;

	BufferObj *buf1, *buf2, *buf3;


	buf1 = buffobj_new();
	if (buf1 == NULL)
		return WPS_ERR_OUTOFMEMORY;

	buf2 = buffobj_new();
	if (buf2 == NULL) {
		buffobj_del(buf1);
		return WPS_ERR_OUTOFMEMORY;
	}

	buf3 = buffobj_new();
	if (buf3 == NULL) {
		buffobj_del(buf1);
		buffobj_del(buf2);
		return WPS_ERR_OUTOFMEMORY;
	}

	/* First, generate or gather the required data */
	message = WPS_ID_MESSAGE_M6;

	/* encrypted settings. */
	{
		BufferObj *encData = buf1;
		BufferObj *cipherText = buf2;
		BufferObj *iv = buf3;
		uint8 reg_proto_version = WPS_VERSION;
		TlvEsNonce rsNonce;
		CTlvEncrSettings encrSettings_tlv;

		buffobj_Reset(encData);
		buffobj_Reset(cipherText);
		buffobj_Reset(iv);

		tlv_set(&rsNonce.nonce, WPS_ID_R_SNONCE2, regInfo->rs2, SIZE_128_BITS);
		reg_msg_nonce_write(&rsNonce, encData, regInfo->authKey);

		reg_proto_encrypt_data(encData, regInfo->keyWrapKey,
			regInfo->authKey, cipherText, iv);

		/* Now assemble the message */
		tlv_serialize(WPS_ID_VERSION, msg, &reg_proto_version, WPS_ID_VERSION_S);
		tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
		tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);

		encrSettings_tlv.iv = iv->pBase;
		encrSettings_tlv.ip_encryptedData = cipherText->pBase;
		encrSettings_tlv.encrDataLength = cipherText->m_dataLength;
		tlv_encrSettingsWrite(&encrSettings_tlv, msg);
	}

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (regInfo->p_registrarInfo->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		/* Reuse buf1 */
		BufferObj *vendorExt_bufObj = buf1;

		buffobj_Reset(vendorExt_bufObj);

		/* Add Version2 subelement to vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&regInfo->p_registrarInfo->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->p_registrarInfo->nattr_len)
			buffobj_Append(msg, regInfo->p_registrarInfo->nattr_len,
				(uint8*)regInfo->p_registrarInfo->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Calculate the hmac */
	{
		BufferObj *hmacData = buf1;
		uint8 hmac[SIZE_256_BITS];

		buffobj_Reset(hmacData);
		buffobj_Append(hmacData, regInfo->inMsg->m_dataLength, regInfo->inMsg->pBase);
		buffobj_Append(hmacData, msg->m_dataLength, msg->pBase);

		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			hmacData->pBase, hmacData->m_dataLength, hmac, NULL);
		tlv_serialize(WPS_ID_AUTHENTICATOR, msg, hmac, SIZE_64_BITS);
	}

	/* Store the outgoing message */

	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, msg->m_dataLength, msg->pBase);
	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM6 built: %d bytes\n",
		msg->m_dataLength));
	err = WPS_SUCCESS;

	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "BuildMessageM6 : got error %x", err));
	buffobj_del(buf1);
	buffobj_del(buf2);
	buffobj_del(buf3);

	return err;
}

/*
 * Vic: make sure we don't override the settings of an already-configured AP.
 * This scenario is not yet supported, but if this code is modified to support
 * setting up an already-configured AP, the settings from M7 will need to be
 * carried across to the encrypted settings of M8.
 */
uint32
reg_proto_process_m7(RegData *regInfo, BufferObj *msg, void **encrSettings)
{
	WpsM7 m;
	uint32 err;

	BufferObj *buf1, *buf1_dser, *buf2_dser;


	buf1 = buffobj_new();
	if (buf1 == NULL)
		return WPS_ERR_OUTOFMEMORY;

	buf1_dser = buffobj_new();
	if (buf1_dser == NULL) {
		buffobj_del(buf1);
		return WPS_ERR_OUTOFMEMORY;
	}

	buf2_dser = buffobj_new();
	if (buf2_dser == NULL) {
		buffobj_del(buf1);
		buffobj_del(buf1_dser);
		return WPS_ERR_OUTOFMEMORY;
	}

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM7: %d byte message\n",
		msg->m_dataLength));

	/* First, deserialize (parse) the message. */

	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	if ((err = reg_proto_verCheck(&m.version, &m.msgType,
		WPS_ID_MESSAGE_M7, msg)) != WPS_SUCCESS)
		goto error;

	reg_msg_init(&m, WPS_ID_MESSAGE_M7);
	tlv_dserialize(&m.registrarNonce, WPS_ID_REGISTRAR_NONCE, msg, SIZE_128_BITS, 0);

	tlv_encrSettingsParse(&m.encrSettings, msg);

	if (WPS_ID_X509_CERT_REQ == buffobj_NextType(msg))
		tlv_dserialize(&m.x509CertReq, WPS_ID_X509_CERT_REQ, msg, 0, 0);

	/* WSC 2.0, parse WFA vendor id and subelements from vendor extension attribute  */
	if (regInfo->p_registrarInfo->version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			/* Reuse buf1 */
			BufferObj *vendorExt_bufObj = buf1;

			buffobj_Reset(vendorExt_bufObj);

			/* Deserialize subelement */
			if (!vendorExt_bufObj) {
				TUTRACE((TUTRACE_ERR, "Fail to allocate vendor extension buffer,"
					"Out of memory\n"));
				return WPS_ERR_OUTOFMEMORY;
			}

			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* WSC 2.0, is next Settings delay Time */
			if (WPS_WFA_SUBID_SETTINGS_DELAY_TIME ==
				buffobj_NextSubId(vendorExt_bufObj))
				subtlv_dserialize(&m.settingsDelayTime,
					WPS_WFA_SUBID_SETTINGS_DELAY_TIME,
					vendorExt_bufObj, 0, 0);

			/* WSC 2.0, is next Version 2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);
		}
		else {
			/* No WFA vendor extension, rewind theBuf to get Authenticator */
			buffobj_Rewind(msg);
		}
	}

	/* skip other optional attributes until we get to the authenticator */
	while (WPS_ID_AUTHENTICATOR != buffobj_NextType(msg)) {
		/* advance past the TLV */
		uint8 *Pos = buffobj_Advance(msg, sizeof(WpsTlvHdr) +
			WpsNtohs(msg->pCurrent+sizeof(uint16)));

		/* If Advance returned NULL, it means there's no more data in the
		 * buffer. This is an error.
		 */
		if (Pos == NULL) {
			err =  RPROT_ERR_REQD_TLV_MISSING;
			goto error;
		}
	}

	tlv_dserialize(&m.authenticator, WPS_ID_AUTHENTICATOR, msg, SIZE_64_BITS, 0);

	/* Now start validating the message */

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->registrarNonce, m.registrarNonce.m_data,
		m.registrarNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect registrar nonce\n"));
		err =  RPROT_ERR_NONCE_MISMATCH;
		goto error;
	}

	/* HMAC validation */
	{
		BufferObj *hmacData = buf1;
		buffobj_Reset(hmacData);
		/* append the last message sent */
		buffobj_Append(hmacData, regInfo->outMsg->m_dataLength, regInfo->outMsg->pBase);
		/* append the current message. Don't append the last TLV (auth) */
		buffobj_Append(hmacData,
			msg->m_dataLength-(sizeof(WpsTlvHdr)+m.authenticator.tlvbase.m_len),
			msg->pBase);

		if (!reg_proto_validate_mac(hmacData, m.authenticator.m_data, regInfo->authKey)) {
			TUTRACE((TUTRACE_ERR, "RPROTO: HMAC validation failed"));
			err = RPROT_ERR_CRYPTO;
		}
	}

	/* HMAC validation */

	/* extract encrypted settings */
	{
		BufferObj *cipherText = buf1_dser;
		BufferObj *iv = buf2_dser;
		BufferObj *plainText = buf1;
		CTlvNonce eNonce;

		buffobj_dserial(cipherText, m.encrSettings.ip_encryptedData,
			m.encrSettings.encrDataLength);

		buffobj_dserial(iv, m.encrSettings.iv, SIZE_128_BITS);

		buffobj_Reset(plainText);
		reg_proto_decrypt_data(cipherText, iv, regInfo->keyWrapKey,
			regInfo->authKey, plainText);

		if (regInfo->p_enrolleeInfo->b_ap) {
			CTlvSsid ssid;

			/*
			 * If encrypted settings of M7 does not have SSID attribute,
			 * change p_enrolleeInfo->b_ap to false.
			 */
			if (tlv_find_dserialize(&ssid, WPS_ID_SSID, plainText, SIZE_32_BYTES, 0)
				!= 0)
				regInfo->p_enrolleeInfo->b_ap = false;
			/* rewind plainText buffer OBJ */
			buffobj_Rewind(plainText);
		}

		if (regInfo->p_enrolleeInfo->b_ap) {
			CTlvEsM7Ap *esAP = reg_msg_m7ap_new();

			if (esAP == NULL)
				goto error;

			err = reg_msg_m7ap_parse(esAP, plainText, regInfo->authKey, true);
			if (err != WPS_SUCCESS) {
				reg_msg_m7ap_del(esAP, 0);
				goto error;
			}

			eNonce = esAP->nonce;
			*encrSettings = (void *)esAP;
		}
		else {
			TlvEsM7Enr *esSTA = reg_msg_m7enr_new();

			if (esSTA == NULL)
				goto error;

			err = reg_msg_m7enr_parse(esSTA, plainText, regInfo->authKey, true);
			if (err != WPS_SUCCESS) {
				reg_msg_m7enr_del(esSTA, 0);
				goto error;
			}

			eNonce = esSTA->nonce;
			*encrSettings = (void *)esSTA;
		}

		/* extract encrypted settings */

		/* EHash2 validation */

		/* 1. Save ES2 */
		memcpy(regInfo->es2, eNonce.m_data, eNonce.tlvbase.m_len);
	}

	{
		BufferObj *ehashBuf = buf1;
		uint8 hashBuf[SIZE_256_BITS];

		/* 2. prepare the buffer */
		buffobj_Reset(ehashBuf);
		buffobj_Append(ehashBuf, SIZE_128_BITS, regInfo->es2);
		buffobj_Append(ehashBuf, SIZE_128_BITS, regInfo->psk2);
		buffobj_Append(ehashBuf, SIZE_PUB_KEY, regInfo->pke);
		buffobj_Append(ehashBuf, SIZE_PUB_KEY, regInfo->pkr);

		/* 3. generate the mac */
		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			ehashBuf->pBase, ehashBuf->m_dataLength, hashBuf, NULL);

		/* 4. compare the mac to ehash2 */
		if (memcmp(regInfo->eHash2, hashBuf, SIZE_256_BITS)) {
			TUTRACE((TUTRACE_ERR, "RPROTO: ES2 hash doesn't match EHash2\n"));
			err =  RPROT_ERR_CRYPTO;
			goto error;
		}

		/* 5. Instead of steps 3 & 4, we could have called ValidateMac */
	}

	/* EHash1 validation */

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, msg->m_dataLength, msg->pBase);
	err = WPS_SUCCESS;
	TUTRACE((TUTRACE_INFO, "RPROTO: ProcessMessageM7 done: %d bytes\n",
		msg->m_dataLength));
error:
	if (err != WPS_SUCCESS) {
		*encrSettings = NULL;
		TUTRACE((TUTRACE_ERR, "ProcessMessageM7: got error %x", err));
	}

	buffobj_del(buf1);
	buffobj_del(buf1_dser);
	buffobj_del(buf2_dser);

	return err;
}

uint32
reg_proto_create_m8(RegData *regInfo, BufferObj *msg, void *encrSettings)
{
	uint8 message;
	uint32 err;

	BufferObj *buf1, *buf2, *buf3;


	buf1 = buffobj_new();
	if (buf1 == NULL)
		return WPS_ERR_OUTOFMEMORY;

	buf2 = buffobj_new();
	if (buf2 == NULL) {
		buffobj_del(buf1);
		return WPS_ERR_OUTOFMEMORY;
	}

	buf3 = buffobj_new();
	if (buf3 == NULL) {
		buffobj_del(buf1);
		buffobj_del(buf2);
		return WPS_ERR_OUTOFMEMORY;
	}

	/* First, generate or gather the required data */
	message = WPS_ID_MESSAGE_M8;

	/* encrypted settings. */
	{
		BufferObj *esBuf = buf1;
		BufferObj *cipherText = buf2;
		BufferObj *iv = buf3;
		uint8 reg_proto_version = WPS_VERSION;
		CTlvEncrSettings encrSettings_tlv;

		buffobj_Reset(esBuf);
		buffobj_Reset(cipherText);
		buffobj_Reset(iv);

		if (!encrSettings) {
			TUTRACE((TUTRACE_ERR, "Encrypted settings settings are NULL\n"));
			err =  WPS_ERR_INVALID_PARAMETERS;
			goto error;
		}

		if (regInfo->p_enrolleeInfo->b_ap) {
			CTlvEsM8Ap *apEs = (CTlvEsM8Ap *)encrSettings;

			if (regInfo->p_registrarInfo->version2 >= WPS_VERSION2) {
				/*
				 * WSC 2.0, (WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK)
				 * allowed only when both the Registrar and the Enrollee
				 * are using WSC 2.0 or newer.
				 * Use WPS_AUTHTYPE_WPA2PSK when enrollee is WSC 1.0 only.
				 */
				if (regInfo->p_enrolleeInfo->version2 < WPS_VERSION2)
					apEs->authType.m_data &= ~WPS_AUTHTYPE_WPAPSK;
			}
			reg_msg_m8ap_write(apEs, esBuf, regInfo->authKey,
				regInfo->p_registrarInfo->version2 >= WPS_VERSION2 ? TRUE : FALSE);
		}
		else {
			LISTITR m_itr;
			LPLISTITR itr;
			CTlvCredential *pCred;
			CTlvEsM8Sta *staEs = (CTlvEsM8Sta *)encrSettings;

			if (regInfo->p_registrarInfo->version2 >= WPS_VERSION2) {
				/*
				 * WSC 2.0, (WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK)
				 * allowed only when both the Registrar and the Enrollee
				 * are using WSC 2.0 or newer.
				 * Use WPS_AUTHTYPE_WPA2PSK when enrollee is WSC 1.0 only.
				 */
				if (regInfo->p_enrolleeInfo->version2 < WPS_VERSION2 &&
				    list_getCount(staEs->credential)) {
					itr = list_itrFirst(staEs->credential, &m_itr);
					while ((pCred = (CTlvCredential *)list_itrGetNext(itr))) {
						pCred->authType.m_data &= ~WPS_AUTHTYPE_WPAPSK;
					}
				}
			}
			reg_msg_m8sta_write_key(staEs, esBuf, regInfo->authKey);
		}

		/* Now encrypt the serialize Encrypted settings buffer */
		reg_proto_encrypt_data(esBuf, regInfo->keyWrapKey,
			regInfo->authKey, cipherText, iv);

		/* Now assemble the message */
		tlv_serialize(WPS_ID_VERSION, msg, &reg_proto_version, WPS_ID_VERSION_S);
		tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
		tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);

		encrSettings_tlv.iv = iv->pBase;
		encrSettings_tlv.ip_encryptedData = cipherText->pBase;
		encrSettings_tlv.encrDataLength = cipherText->m_dataLength;
		tlv_encrSettingsWrite(&encrSettings_tlv, msg);

		if (regInfo->x509Cert->m_dataLength) {
			tlv_serialize(WPS_ID_X509_CERT, msg, regInfo->x509Cert->pBase,
				regInfo->x509Cert->m_dataLength);
		}
	}

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (regInfo->p_registrarInfo->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		/* Reuse buf1 */
		BufferObj *vendorExt_bufObj = buf1;

		buffobj_Reset(vendorExt_bufObj);

		/* Add Version2 subelement to vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&regInfo->p_registrarInfo->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->p_registrarInfo->nattr_len)
			buffobj_Append(msg, regInfo->p_registrarInfo->nattr_len,
				(uint8*)regInfo->p_registrarInfo->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Calculate the hmac */
	{
		BufferObj *hmacData = buf1;
		uint8 hmac[SIZE_256_BITS];

		buffobj_Reset(hmacData);
		buffobj_Append(hmacData,  regInfo->inMsg->m_dataLength, regInfo->inMsg->pBase);
		buffobj_Append(hmacData,  msg->m_dataLength, msg->pBase);

		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			hmacData->pBase, hmacData->m_dataLength, hmac, NULL);

		tlv_serialize(WPS_ID_AUTHENTICATOR, msg, hmac, SIZE_64_BITS);
	}

	/* Store the outgoing message */
	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, msg->m_dataLength, msg->pBase);

	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM8 built: %d bytes\n",
		msg->m_dataLength));
	err = WPS_SUCCESS;
error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "BuildMessageM8 : got error %x\n", err));

	buffobj_del(buf1);
	buffobj_del(buf2);
	buffobj_del(buf3);

	return err;
}

uint32
reg_proto_create_devinforsp(RegData *regInfo, BufferObj *msg)
{
	uint32 ret = WPS_SUCCESS;
	uint8 message;
	uint8 reg_proto_version = WPS_VERSION;
	CTlvPrimDeviceType primDev;
#ifdef WFA_WPS_20_TESTBED
	/* For internal testing purpose */
	bool b_zpadding = regInfo->p_registrarInfo->b_zpadding;
#endif /* WFA_WPS_20_TESTBED */

	/* First generate/gather all the required data. */
	message = WPS_ID_MESSAGE_M1;

	/* Enrollee nonce */
	RAND_bytes(regInfo->enrolleeNonce, SIZE_128_BITS);

	if (!regInfo->DHSecret) {
		BufferObj *pubKey = buffobj_new();

		if (!pubKey)
			return WPS_ERR_OUTOFMEMORY;

		ret = reg_proto_generate_dhkeypair(&regInfo->DHSecret, pubKey);
		buffobj_del(pubKey);

		if (WPS_SUCCESS != ret) {
			if (regInfo->DHSecret) {
				DH_free(regInfo->DHSecret);
				regInfo->DHSecret = NULL;
			}

			TUTRACE((TUTRACE_ERR, "RPROTO: Cannot create DH Secret\n"));
			return ret;
		}
	}

	/* Extract the DH public key */
	if (reg_proto_BN_bn2bin(regInfo->DHSecret->pub_key, regInfo->pke) != SIZE_PUB_KEY)
		return RPROT_ERR_CRYPTO;

	/* Now start composing the message */
	tlv_serialize(WPS_ID_VERSION, msg, &reg_proto_version, WPS_ID_VERSION_S);
	tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
	tlv_serialize(WPS_ID_UUID_E, msg, regInfo->p_registrarInfo->uuid, SIZE_UUID);
	tlv_serialize(WPS_ID_MAC_ADDR, msg, regInfo->p_registrarInfo->macAddr, SIZE_MAC_ADDR);
	tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_PUBLIC_KEY, msg, regInfo->pke, SIZE_PUB_KEY);
	tlv_serialize(WPS_ID_AUTH_TYPE_FLAGS, msg, &regInfo->p_registrarInfo->authTypeFlags,
		WPS_ID_AUTH_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_ENCR_TYPE_FLAGS, msg, &regInfo->p_registrarInfo->encrTypeFlags,
		WPS_ID_ENCR_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_CONN_TYPE_FLAGS, msg, &regInfo->p_registrarInfo->connTypeFlags,
		WPS_ID_CONN_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_CONFIG_METHODS, msg, &regInfo->p_registrarInfo->configMethods,
		WPS_ID_CONFIG_METHODS_S);
	tlv_serialize(WPS_ID_SC_STATE, msg, &regInfo->p_registrarInfo->scState,
		WPS_ID_SC_STATE_S);
	tlv_serialize(WPS_ID_MANUFACTURER, msg, regInfo->p_registrarInfo->manufacturer,
		STR_ATTR_LEN(regInfo->p_registrarInfo->manufacturer, SIZE_64_BYTES));
	tlv_serialize(WPS_ID_MODEL_NAME, msg, regInfo->p_registrarInfo->modelName,
		STR_ATTR_LEN(regInfo->p_registrarInfo->modelName, SIZE_32_BYTES));
	tlv_serialize(WPS_ID_MODEL_NUMBER, msg, regInfo->p_registrarInfo->modelNumber,
		STR_ATTR_LEN(regInfo->p_registrarInfo->modelNumber, SIZE_32_BYTES));
	tlv_serialize(WPS_ID_SERIAL_NUM, msg, regInfo->p_registrarInfo->serialNumber,
		STR_ATTR_LEN(regInfo->p_registrarInfo->serialNumber, SIZE_32_BYTES));

	primDev.categoryId = regInfo->p_registrarInfo->primDeviceCategory;
	primDev.oui = regInfo->p_registrarInfo->primDeviceOui;
	primDev.subCategoryId = regInfo->p_registrarInfo->primDeviceSubCategory;
	tlv_primDeviceTypeWrite(&primDev, msg);

	tlv_serialize(WPS_ID_DEVICE_NAME, msg, regInfo->p_registrarInfo->deviceName,
		STR_ATTR_LEN(regInfo->p_registrarInfo->deviceName, SIZE_32_BYTES));

	/* set real RF band */
	tlv_serialize(WPS_ID_RF_BAND, msg, &regInfo->p_registrarInfo->rfBand,
		WPS_ID_RF_BAND_S);
	tlv_serialize(WPS_ID_ASSOC_STATE, msg, &regInfo->p_registrarInfo->assocState,
		WPS_ID_ASSOC_STATE_S);
	tlv_serialize(WPS_ID_DEVICE_PWD_ID, msg, &regInfo->p_registrarInfo->devPwdId,
		WPS_ID_DEVICE_PWD_ID_S);
	tlv_serialize(WPS_ID_CONFIG_ERROR, msg, &regInfo->p_registrarInfo->configError,
		WPS_ID_CONFIG_ERROR_S);
	tlv_serialize(WPS_ID_OS_VERSION, msg, &regInfo->p_registrarInfo->osVersion,
		WPS_ID_OS_VERSION_S);

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (regInfo->p_registrarInfo->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		BufferObj *vendorExt_bufObj = buffobj_new();

		if (vendorExt_bufObj == NULL)
			return WPS_ERR_OUTOFMEMORY;

		/* Add Version2 subelement to vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&regInfo->p_registrarInfo->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Add ReqToEnroll subelement to vendorExt_bufObj */
		if (regInfo->p_registrarInfo->b_reqToEnroll)
			subtlv_serialize(WPS_WFA_SUBID_REQ_TO_ENROLL, vendorExt_bufObj,
				&regInfo->p_registrarInfo->b_reqToEnroll,
				WPS_WFA_SUBID_REQ_TO_ENROLL_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->p_registrarInfo->nattr_len)
			buffobj_Append(msg, regInfo->p_registrarInfo->nattr_len,
				(uint8*)regInfo->p_registrarInfo->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}


	/* skip other optional attributes */

	/* copy message to outMsg buffer */
	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, buffobj_Length(msg), buffobj_GetBuf(msg));
	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM1 built: %d bytes\n", buffobj_Length(msg)));

	return WPS_SUCCESS;
}
