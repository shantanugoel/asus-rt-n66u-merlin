/*
 * Enrollee state machine
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: enr_reg_sm.c 287325 2011-10-03 03:13:33Z kenlo $
 */

#ifdef WIN32
#include <windows.h>
#endif

#ifndef UNDER_CE

#ifdef __ECOS
#include <sys/types.h>
#include <sys/socket.h>
#endif

#if defined(__linux__) || defined(__ECOS)
#include <net/if.h>
#endif

#endif /* UNDER_CE */

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
#include <wps_staeapsm.h>
#include <info.h>
#include <wpsapi.h>
#include <wpscommon.h>

#ifdef WFA_WPS_20_TESTBED
#define STR_ATTR_LEN(str, max) \
	((uint16)((b_zpadding && strlen(str) < max) ? strlen(str) + 1 : strlen(str)))
#else /* !WFA_WPS_20_TESTBED */
#define STR_ATTR_LEN(str, max)	((uint16)(strlen(str)))
#endif /* WFA_WPS_20_TESTBED */


EnrSM *
enr_sm_new(void *g_mc, CRegProtocol *pc_regProt)
{
	EnrSM *e = (EnrSM *)malloc(sizeof(EnrSM));

	if (!e)
		return NULL;

	memset(e, 0, sizeof(EnrSM));

	TUTRACE((TUTRACE_INFO, "Enrollee constructor\n"));
	e->g_mc = g_mc;
	e->m_sm = state_machine_new(g_mc, pc_regProt, MODE_ENROLLEE);
	if (!e->m_sm) {
		free(e);
		return NULL;
	}

	WpsSyncCreate(&e->mp_m2dLock);
	/* e->m_stophandlemsg_enr = 0; */
	WpsSyncCreate(&e->mp_ptimerLock_enr);
	return e;
}

void
enr_sm_delete(EnrSM *e)
{
	if (e) {
		if (e->mp_peerEncrSettings) {
			reg_msg_es_del(e->mp_peerEncrSettings, 0);
			e->mp_peerEncrSettings = NULL;
		}

		state_machine_delete(e->m_sm);
		WpsSyncDestroy(e->mp_m2dLock);
		WpsSyncDestroy(e->mp_ptimerLock_enr);

		TUTRACE((TUTRACE_INFO, "Free EnrSM %p\n", e));
		free(e);
	}
}

uint32
enr_sm_initializeSM(EnrSM *e, IN DevInfo *p_registrarInfo, void * p_StaEncrSettings,
	void * p_ApEncrSettings, char *p_devicePasswd, uint32 passwdLength)
{
	uint32 err;

	TUTRACE((TUTRACE_INFO, "Enrollee Initialize SM\n"));

	err = state_machine_init_sm(e->m_sm);
	if (WPS_SUCCESS != err) {
		return err;
	}

	e->m_sm->mps_regData->p_enrolleeInfo = e->m_sm->mps_localInfo;
	e->m_sm->mps_regData->p_registrarInfo = p_registrarInfo;
	state_machine_set_passwd(e->m_sm, p_devicePasswd, passwdLength);
	state_machine_set_encrypt(e->m_sm, p_StaEncrSettings, p_ApEncrSettings);

	return WPS_SUCCESS;
}

void
enr_sm_restartSM(EnrSM *e)
{
	BufferObj *passwd = buffobj_new();
	void *apEncrSettings, *staEncrSettings;
	RegData *mps_regData;
	uint32 err;

	TUTRACE((TUTRACE_INFO, "SM: Restarting the State Machine\n"));

	if (!passwd)
		return;
	/* first extract the all info we need to save */
	mps_regData = e->m_sm->mps_regData;
	buffobj_Append(passwd, mps_regData->password->m_dataLength,
		mps_regData->password->pBase);
	staEncrSettings = mps_regData->staEncrSettings;
	apEncrSettings = mps_regData->apEncrSettings;

	if (mps_regData->p_registrarInfo) {
		free(mps_regData->p_registrarInfo);
		mps_regData->p_registrarInfo = NULL;
	}

	/* next, reinitialize the base SM class */
	err = state_machine_init_sm(e->m_sm);
	if (WPS_SUCCESS != err) {
		buffobj_del(passwd);
		return;
	}

	/* Finally, store the data back into the regData struct */
	e->m_sm->mps_regData->p_enrolleeInfo = e->m_sm->mps_localInfo;

	state_machine_set_passwd(e->m_sm, (char *)passwd->pBase, passwd->m_dataLength);
	buffobj_del(passwd);
	state_machine_set_encrypt(e->m_sm, staEncrSettings, apEncrSettings);

	e->m_sm->m_initialized = true;
}

uint32
enr_sm_step(EnrSM *e, uint32 msgLen, uint8 *p_msg, uint8 *outbuffer, uint32 *out_len)
{
	BufferObj *inMsg, *outMsg;
	uint32 retVal = WPS_CONT;

	TUTRACE((TUTRACE_INFO, "ENRSM: Entering Step, , length = %d\n", msgLen));

	if (false == e->m_sm->m_initialized) {
		TUTRACE((TUTRACE_ERR, "ENRSM: Not yet initialized.\n"));
		return WPS_ERR_NOT_INITIALIZED;
	}

	if (START == e->m_sm->mps_regData->e_smState) {
		TUTRACE((TUTRACE_INFO, "ENRSM: Step 1\n"));
		/* No special processing here */
		inMsg = buffobj_new();
		if (!inMsg)
			return WPS_ERR_OUTOFMEMORY;
		outMsg = buffobj_setbuf(outbuffer, *out_len);
		if (!outMsg) {
			buffobj_del(inMsg);
			return WPS_ERR_OUTOFMEMORY;
		}
		buffobj_dserial(inMsg, p_msg, msgLen);

		retVal = enr_sm_handleMessage(e, inMsg, outMsg);
		*out_len = buffobj_Length(outMsg);

		TUTRACE((TUTRACE_INFO, "ENRSM: Step 2\n"));

		buffobj_del(inMsg);
		buffobj_del(outMsg);
	}
	else {
		/* do the regular processing */
		if (!p_msg || !msgLen) {
			/* Preferential treatment for UPnP */
			if (e->m_sm->mps_regData->e_lastMsgSent == M1) {
				/*
				 * If we have already sent M1 and we get here, assume that it is
				 * another request for M1 rather than an error.
				 * Send the bufferred M1 message
				 */
				TUTRACE((TUTRACE_INFO, "ENRSM: Got another request for M1. "
					"Resending the earlier M1\n"));

				/* copy buffered M1 to msg_to_send buffer */
				if (*out_len < e->m_sm->mps_regData->outMsg->m_dataLength) {
					e->m_sm->mps_regData->e_smState = FAILURE;
					TUTRACE((TUTRACE_ERR, "output message buffer to small\n"));
					return WPS_MESSAGE_PROCESSING_ERROR;
				}
				memcpy(outbuffer, (char *)e->m_sm->mps_regData->outMsg->pBase,
					e->m_sm->mps_regData->outMsg->m_dataLength);
				*out_len = buffobj_Length(e->m_sm->mps_regData->outMsg);
				return WPS_SEND_MSG_CONT;
			}
			return WPS_CONT;
		}

		inMsg = buffobj_new();
		if (!inMsg)
			return WPS_ERR_OUTOFMEMORY;
		outMsg = buffobj_setbuf(outbuffer, *out_len);
		if (!outMsg) {
			buffobj_del(inMsg);
			return WPS_ERR_OUTOFMEMORY;
		}

		buffobj_dserial(inMsg, p_msg, msgLen);

		retVal = enr_sm_handleMessage(e, inMsg, outMsg);
		*out_len = buffobj_Length(outMsg);

		buffobj_del(inMsg);
		buffobj_del(outMsg);
	}

	/* now check the state so we can act accordingly */
	switch (e->m_sm->mps_regData->e_smState) {
	case START:
	case CONTINUE:
		/* do nothing. */
		break;

	case SUCCESS:
		/* reset the SM */
		enr_sm_restartSM(e);

		if (retVal == WPS_SEND_MSG_CONT)
			return WPS_SEND_MSG_SUCCESS;
		else
			return WPS_SUCCESS;

	case FAILURE:
		/* reset the SM */
		enr_sm_restartSM(e);

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
enr_sm_handleMessage(EnrSM *e, BufferObj *msg, BufferObj *outmsg)
{
	uint32 err, retVal = WPS_CONT;
	void *encrSettings = NULL;
	uint32 msgType = 0;
	uint16 configError = WPS_ERROR_MSG_TIMEOUT;
	RegData *regInfo = e->m_sm->mps_regData;
	uint8 regNonce[SIZE_128_BITS] = { 0 };

	err = reg_proto_get_msg_type(&msgType, msg);
	if (WPS_SUCCESS != err) {
		TUTRACE((TUTRACE_ERR, "ENRSM: Expected M1, received %d\n", msgType));
		/* For DTM testing */
		/* goto out; */
	}

	/* If we get a valid message, extract the message type received.  */
	if (MNONE != e->m_sm->mps_regData->e_lastMsgSent) {
		/*
		 * If this is a late-arriving M2D, ACK it.  Otherwise, ignore it.
		 * Should probably also NACK an M2 at this point.
		 */
		if ((SM_RECVD_M2 == e->m_m2dStatus) &&
			(msgType <= WPS_ID_MESSAGE_M2D)) {
			if (WPS_ID_MESSAGE_M2D == msgType) {
				reg_proto_get_nonce(regNonce, msg, WPS_ID_REGISTRAR_NONCE);
				retVal = state_machine_send_ack(e->m_sm, outmsg, regNonce);
				TUTRACE((TUTRACE_INFO, "ENRSM: Possible late M2D received.  "
					"Sending ACK.\n"));
				goto out;
			}
		}
	}

	switch (e->m_sm->mps_regData->e_lastMsgSent) {
	case MNONE:
		if (WPS_ID_MESSAGE_M2 == msgType || WPS_ID_MESSAGE_M4 == msgType ||
			WPS_ID_MESSAGE_M6 == msgType || WPS_ID_MESSAGE_M8 == msgType ||
			WPS_ID_MESSAGE_NACK == msgType) {
			TUTRACE((TUTRACE_ERR, "ENRSM: msgType error type=%d, "
				"stay silent \n", msgType));
			goto out;
		}

		err = reg_proto_create_m1(e->m_sm->mps_regData, outmsg);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "ENRSM: Expected M1, received %d\n", msgType));
			goto out;
		}
		e->m_sm->mps_regData->e_lastMsgSent = M1;

		/* Set the m2dstatus. */
		e->m_m2dStatus = SM_AWAIT_M2;

		/* set the message state to CONTINUE */
		e->m_sm->mps_regData->e_smState = CONTINUE;

		/* msg_to_send is ready */
		retVal = WPS_SEND_MSG_CONT;
		break;

	case M1:
		e->m_ptimerStatus_enr = 0;
		/* for DTM 1.1 test */
		if (e->m_m2dStatus == SM_RECVD_M2D && WPS_ID_MESSAGE_NACK == msgType) {
			err = reg_proto_process_nack(e->m_sm->mps_regData, msg,	&configError,
				e->m_sm->mps_localInfo->version2 >= WPS_VERSION2 ? true : false);
			if (configError != WPS_ERROR_NO_ERROR) {
				TUTRACE((TUTRACE_ERR, "ENRSM: Recvd NACK with config err: "
					"%x\n", configError));
			}
			/* for DTM 1.3 push button test */
			retVal = WPS_MESSAGE_PROCESSING_ERROR;

			enr_sm_restartSM(e);
			goto out;
		}
/* brcm added for message timeout */
		if (SM_RECVD_M2 != e->m_m2dStatus) {
			TUTRACE((TUTRACE_INFO, "ENRSM: Starting wps ProtocolTimerThread\n"));
		}
/* brcm added for message timeou --	*/

		/* Check whether this is M2D */
		if (WPS_ID_MESSAGE_M2D == msgType) {
			err = reg_proto_process_m2d(e->m_sm->mps_regData, msg, regNonce);
			if (WPS_SUCCESS != err) {
				TUTRACE((TUTRACE_ERR, "ENRSM: Expected M2D, err %x\n", msgType));
				goto out;
			}

			/* Send NACK if enrollee is AP */
			if (regInfo->p_enrolleeInfo->b_ap) {
				/* Send an NACK to the registrar */
				/* for DTM1.1 test */
				retVal = state_machine_send_nack(e->m_sm,
					WPS_ERROR_NO_ERROR, outmsg, regNonce);
				if (WPS_SEND_MSG_CONT != retVal) {
					TUTRACE((TUTRACE_ERR, "ENRSM: SendNack err %x\n", msgType));
					goto out;
				}
			}
			else {
				/* Send an ACK to the registrar */
				retVal = state_machine_send_ack(e->m_sm, outmsg, regNonce);
				if (WPS_SEND_MSG_CONT != retVal) {
					TUTRACE((TUTRACE_ERR, "ENRSM: SendAck err %x\n", msgType));
					goto out;
				}
			}

			/*
			 * Now, schedule a thread to sleep for some time to allow other
			 * registrars to send M2 or M2D messages.
			 */
			WpsLock(e->mp_m2dLock);
			if (SM_AWAIT_M2 == e->m_m2dStatus) {
				/*
				 * if the M2D status is 'await', set the timer. For all
				 * other cases, don't do anything, because we've either
				 * already received an M2 or M2D, and possibly, the SM reset
				 * process has already been initiated
				 */
				TUTRACE((TUTRACE_INFO, "ENRSM: Starting M2DThread\n"));

				e->m_m2dStatus = SM_RECVD_M2D;

				WpsSleep(1);

				/* set the message state to CONTINUE */
				e->m_sm->mps_regData->e_smState = CONTINUE;
				WpsUnlock(e->mp_m2dLock);
				goto out;
			}
			else {
				TUTRACE((TUTRACE_INFO, "ENRSM: Did not start M2DThread. "
					"status = %d\n", e->m_m2dStatus));
			}
			WpsUnlock(e->mp_m2dLock);

			goto out; /* done processing for M2D, return */
		}
		/* message wasn't M2D, do processing for M2 */
		else {
			WpsLock(e->mp_m2dLock);
			if (SM_M2D_RESET == e->m_m2dStatus) {
				WpsUnlock(e->mp_m2dLock);
				/* a SM reset has been initiated. Don't process any M2s */
				goto out;
			}
			else {
				e->m_m2dStatus = SM_RECVD_M2;
			}
			WpsUnlock(e->mp_m2dLock);

			err = reg_proto_process_m2(e->m_sm->mps_regData, msg, NULL, regNonce);

			if (WPS_SUCCESS != err) {
				if (err == RPROT_ERR_AUTH_ENC_FLAG) {
					TUTRACE((TUTRACE_ERR, "ENRSM: Check auth or encry flag "
						"error: SendNack! %x\n", err));
					retVal = state_machine_send_nack(e->m_sm,
						WPS_ERROR_NW_AUTH_FAIL, outmsg, regNonce);
					/* send msg_to_send message immedicate */
					enr_sm_restartSM(e);
					goto out;
				}
				else if (err == RPROT_ERR_MULTIPLE_M2) {
					TUTRACE((TUTRACE_ERR, "ENRSM: receive multiple M2 "
						"error: SendNack! %x\n", err));
					retVal = state_machine_send_nack(e->m_sm,
						WPS_ERROR_DEVICE_BUSY, outmsg, regNonce);
					goto out;
				} /* WSC 2.0 */
				else if (err == RPROT_ERR_ROGUE_SUSPECTED) {
					TUTRACE((TUTRACE_ERR, "ENRSM: receive rogue suspected MAC "
						"error: SendNack! %x\n", err));
					retVal = state_machine_send_nack(e->m_sm,
						WPS_ERROR_ROGUE_SUSPECTED, outmsg, regNonce);
					goto out;
				}
				else { /* for DTM1.1 test */
					TUTRACE((TUTRACE_ERR, "ENRSM: HMAC "
						"error: SendNack! %x\n", err));
					retVal = state_machine_send_nack(e->m_sm,
						WPS_ERROR_DECRYPT_CRC_FAIL, outmsg, regNonce);
					/* send msg_to_send message immedicate */
					enr_sm_restartSM(e);
					goto out;
				}
			}
			e->m_sm->mps_regData->e_lastMsgRecd = M2;

			err = reg_proto_create_m3(e->m_sm->mps_regData, outmsg);

			if (WPS_SUCCESS != err) {
				TUTRACE((TUTRACE_ERR, "ENRSM: Build M3 failed %x\n", err));
				retVal = state_machine_send_nack(e->m_sm,
					WPS_ERROR_DEVICE_BUSY, outmsg, regNonce);
				goto out;
			}
			e->m_sm->mps_regData->e_lastMsgSent = M3;

			/* set the message state to CONTINUE */
			e->m_sm->mps_regData->e_smState = CONTINUE;

			/* msg_to_send is ready */
			retVal = WPS_SEND_MSG_CONT;
		}
		break;

	case M3:
		err = reg_proto_process_m4(e->m_sm->mps_regData, msg);

		if (WPS_SUCCESS != err) {
			/* for DTM testing */
			if (WPS_ID_MESSAGE_M2 == msgType) {
				TUTRACE((TUTRACE_ERR, "ENRSM: Receive multiple M2 "
					"error! %x\n", err));
				goto out;
			}

			TUTRACE((TUTRACE_ERR, "ENRSM: Process M4 failed %x\n", err));
			/* Send a NACK to the registrar */
			if (err == RPROT_ERR_CRYPTO) {
				retVal = state_machine_send_nack(e->m_sm,
					WPS_ERROR_DEV_PWD_AUTH_FAIL, outmsg, NULL);
				retVal = WPS_ERR_ENROLLMENT_PINFAIL;
			} else if (err == RPROT_ERR_WRONG_MSGTYPE &&
				msgType == WPS_ID_MESSAGE_NACK) {
				/* set the message state to failure */
				e->m_sm->mps_regData->e_smState = FAILURE;
				retVal = WPS_MESSAGE_PROCESSING_ERROR;
				break;
			} else {
				retVal = state_machine_send_nack(e->m_sm,
					WPS_ERROR_DEVICE_BUSY, outmsg, NULL);
			}
			enr_sm_restartSM(e);
			goto out;
		}
		e->m_sm->mps_regData->e_lastMsgRecd = M4;

		err = reg_proto_create_m5(e->m_sm->mps_regData, outmsg);

		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "ENRSM: Build M5 failed %x\n", err));
			retVal = state_machine_send_nack(e->m_sm,
				WPS_ERROR_DEVICE_BUSY, outmsg, NULL);
			goto out;
		}
		e->m_sm->mps_regData->e_lastMsgSent = M5;

		/* set the message state to CONTINUE */
		e->m_sm->mps_regData->e_smState = CONTINUE;

		/* msg_to_send is ready */
		retVal = WPS_SEND_MSG_CONT;
		break;

	case M5:
		err = reg_proto_process_m6(e->m_sm->mps_regData, msg);

		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "ENRSM: Process M6 failed %x\n", err));
			/* Send a NACK to the registrar */
			if (err == RPROT_ERR_CRYPTO) {
				retVal = state_machine_send_nack(e->m_sm,
					WPS_ERROR_DEV_PWD_AUTH_FAIL, outmsg, NULL);
				/* Add in PF #3 */
				retVal = WPS_ERR_ENROLLMENT_PINFAIL;
			} else if (err == RPROT_ERR_WRONG_MSGTYPE &&
				msgType == WPS_ID_MESSAGE_NACK) {
				/* set the message state to failure */
				e->m_sm->mps_regData->e_smState = FAILURE;
				retVal = WPS_MESSAGE_PROCESSING_ERROR;
				break;
			} else {
				retVal = state_machine_send_nack(e->m_sm,
					WPS_ERROR_DEVICE_BUSY, outmsg, NULL);
			}
			enr_sm_restartSM(e);
			goto out;
		}
		e->m_sm->mps_regData->e_lastMsgRecd = M6;

		/* Build message 7 with the appropriate encrypted settings */
		if (e->m_sm->mps_regData->p_enrolleeInfo->b_ap) {
			err = reg_proto_create_m7(e->m_sm->mps_regData, outmsg,
				e->m_sm->mps_regData->apEncrSettings);
		}
		else {
			err = reg_proto_create_m7(e->m_sm->mps_regData, outmsg,
				e->m_sm->mps_regData->staEncrSettings);
		}

		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "ENRSM: Build M7 failed %x\n", err));
			retVal = state_machine_send_nack(e->m_sm,
				WPS_ERROR_DEVICE_BUSY, outmsg, NULL);
			goto out;
		}

		e->m_sm->mps_regData->e_lastMsgSent = M7;

		/* set the message state to CONTINUE */
		e->m_sm->mps_regData->e_smState = CONTINUE;

		/* msg_to_send is ready */
		retVal = WPS_SEND_MSG_CONT;
		break;

	case M7:
		/* for DTM 1.1 test, if enrollee is AP */
		if (regInfo->p_enrolleeInfo->b_ap && WPS_ID_MESSAGE_NACK == msgType) {
			err = reg_proto_process_nack(e->m_sm->mps_regData, msg, &configError,
				e->m_sm->mps_localInfo->version2 >= WPS_VERSION2 ? true : false);
			if (configError != WPS_ERROR_NO_ERROR) {
				TUTRACE((TUTRACE_ERR, "ENRSM: Recvd NACK with config err: "
					"%d\n", configError));
			}
			e->m_ptimerStatus_enr = 1;
			/* set the message state to success */
			e->m_sm->mps_regData->e_smState = SUCCESS;
			retVal = WPS_SUCCESS;
			goto out;
		}

		err = reg_proto_process_m8(e->m_sm->mps_regData, msg, &encrSettings);

		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "ENRSM: Process M8 failed %x\n", err));

			/* Record the real error code for further use */
			e->err_code = err;

			configError = WPS_ERROR_DEVICE_BUSY;

			if (err == RPROT_ERR_CRYPTO)
				configError = WPS_ERROR_DEV_PWD_AUTH_FAIL;
			if (err == RPROT_ERR_ROGUE_SUSPECTED)
				configError = WPS_ERROR_ROGUE_SUSPECTED;

			/* send fail in all case */
			retVal = state_machine_send_nack(e->m_sm, configError, outmsg, NULL);
			enr_sm_restartSM(e);
			goto out;
		}

		e->m_sm->mps_regData->e_lastMsgRecd = M8;
		e->mp_peerEncrSettings = encrSettings;

		/* Send a Done message */
		retVal = state_machine_send_done(e->m_sm, outmsg);
		e->m_sm->mps_regData->e_lastMsgSent = DONE;
		e->m_ptimerStatus_enr = 1;
		/* Decide if we need to wait for an ACK
			Wait only if we're an AP AND we're running EAP
		*/
		if ((!e->m_sm->mps_regData->p_enrolleeInfo->b_ap) ||
			(e->m_sm->m_transportType != TRANSPORT_TYPE_EAP)) {
			/* set the message state to success */
			e->m_sm->mps_regData->e_smState = SUCCESS;
			e->m_ptimerStatus_enr = 1;
		}
		else {
			/* Wait for ACK. set the message state to continue */
			e->m_sm->mps_regData->e_smState = CONTINUE;
		}
		break;

	case DONE:
		err = reg_proto_process_ack(e->m_sm->mps_regData, msg,
			e->m_sm->mps_localInfo->version2 >= WPS_VERSION2 ? true : false);

		if (RPROT_ERR_NONCE_MISMATCH == err) {
			 /* ignore nonce mismatches */
			e->m_sm->mps_regData->e_smState = CONTINUE;
		}
		else if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "ENRSM: Expected M1, received %d\n", msgType));
			goto out;
		}

		/* set the message state to success */
		e->m_sm->mps_regData->e_smState = SUCCESS;
		e->m_ptimerStatus_enr = 1;
		break;

	default:
		TUTRACE((TUTRACE_ERR, "ENRSM: Expected M1, received %d\n", msgType));
		goto out;
	}

out:

	return retVal;
}

/* WSC 2.0 */
int
reg_proto_MacCheck(DevInfo *enrollee, void *encrSettings)
{
	CTlvEsM8Ap *esAP;
	CTlvEsM8Sta *esSta;
	CTlvCredential *pCredential;
	LISTITR m_itr;
	LPLISTITR itr;
	bool b_ap;
	uint8 *mac;

	if (!enrollee || !encrSettings)
		return WPS_ERR_INVALID_PARAMETERS;

	b_ap = enrollee->b_ap;
	mac = enrollee->macAddr;

	if (b_ap) {
		esAP = (CTlvEsM8Ap *)encrSettings;
		if (esAP->macAddr.m_data == NULL ||
		    memcmp(mac, esAP->macAddr.m_data, SIZE_MAC_ADDR) != 0) {
			TUTRACE((TUTRACE_ERR, "Rogue suspected\n"));
			TUTRACE((TUTRACE_ERR, "AP enrollee mac %02x:%02x:%02x:%02x:%02x:%02x\n",
				mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]));
			if (esAP->macAddr.m_data == NULL) {
				TUTRACE((TUTRACE_ERR, "No AP's mac address!."));
			}
			else {
				TUTRACE((TUTRACE_ERR, "AP's mac %02x:%02x:%02x:%02x:%02x:%02x\n",
					esAP->macAddr.m_data[0], esAP->macAddr.m_data[1],
					esAP->macAddr.m_data[2], esAP->macAddr.m_data[3],
					esAP->macAddr.m_data[4], esAP->macAddr.m_data[5]));
			}
			return RPROT_ERR_ROGUE_SUSPECTED;
		}
	}
	else {
		esSta = (CTlvEsM8Sta *)encrSettings;
		if (esSta->credential == NULL ||
		    list_getCount(esSta->credential) == 0) {
			TUTRACE((TUTRACE_ERR, "Illegal credential\n"));
			return RPROT_ERR_ROGUE_SUSPECTED;
		}

		itr = list_itrFirst(esSta->credential, &m_itr);
		while ((pCredential = (CTlvCredential *)list_itrGetNext(itr))) {
			if (pCredential->macAddr.m_data == NULL ||
			    memcmp(mac, pCredential->macAddr.m_data, SIZE_MAC_ADDR) != 0) {
				TUTRACE((TUTRACE_ERR, "Rogue suspected\n"));
				TUTRACE((TUTRACE_ERR, "STA enrollee mac "
					"%02x:%02x:%02x:%02x:%02x:%02x\n",
					mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]));
				if (pCredential->macAddr.m_data == NULL) {
					TUTRACE((TUTRACE_ERR, "No device's mac address!."));
				}
				else {
					TUTRACE((TUTRACE_ERR, "Device's mac "
						"%02x:%02x:%02x:%02x:%02x:%02x\n",
						pCredential->macAddr.m_data[0],
						pCredential->macAddr.m_data[1],
						pCredential->macAddr.m_data[2],
						pCredential->macAddr.m_data[3],
						pCredential->macAddr.m_data[4],
						pCredential->macAddr.m_data[5]));
				}
				return RPROT_ERR_ROGUE_SUSPECTED;
			}
		}
	}

	return WPS_SUCCESS;
}

/* WSC 2.0 */
int
reg_proto_EncrSettingsCheck(DevInfo *enrollee, void *encrSettings)
{
	CTlvEsM8Ap *esAP;
	CTlvEsM8Sta *esSta;
	CTlvCredential *pCredential;
	LISTITR m_itr;
	LPLISTITR itr;
	bool b_ap;

	if (!enrollee || !encrSettings)
		return WPS_ERR_INVALID_PARAMETERS;

	b_ap = enrollee->b_ap;
	if (b_ap) {
		esAP = (CTlvEsM8Ap *)encrSettings;
		if (esAP->encrType.m_data == WPS_ENCRTYPE_WEP) {
			TUTRACE((TUTRACE_ERR, "Deprecated WEP encryption detected\n"));
			return RPROT_ERR_INCOMPATIBLE_WEP;
		}
	}
	else {
		esSta = (CTlvEsM8Sta *)encrSettings;
		if (esSta->credential == NULL ||
		    list_getCount(esSta->credential) == 0) {
			TUTRACE((TUTRACE_ERR, "Illegal credential\n"));
			return RPROT_ERR_INCOMPATIBLE;
		}

		itr = list_itrFirst(esSta->credential, &m_itr);
		while ((pCredential = (CTlvCredential *)list_itrGetNext(itr))) {
			if (pCredential->encrType.m_data == WPS_ENCRTYPE_WEP) {
				TUTRACE((TUTRACE_ERR, "Deprecated WEP encryption detected\n"));
				return RPROT_ERR_INCOMPATIBLE_WEP;
			}
		}
	}

	return WPS_SUCCESS;
}

int
reg_proto_verCheck(TlvObj_uint8 *version, TlvObj_uint8 *msgType, uint8 msgId, BufferObj *msg)
{
	uint8 reg_proto_version = WPS_VERSION;
	int err = 0;

	err |= tlv_dserialize(version, WPS_ID_VERSION, msg, 0, 0);
	err |= tlv_dserialize(msgType, WPS_ID_MSG_TYPE, msg, 0, 0);

	if (err)
		return RPROT_ERR_REQD_TLV_MISSING; /* for DTM 1.1 */

	/* If the major version number matches, assume we can parse it successfully. */
	if ((reg_proto_version & 0xF0) != (version->m_data & 0xF0)) {
		TUTRACE((TUTRACE_ERR, "message version %d differs from protocol version %d\n",
			version->m_data, reg_proto_version));
		return RPROT_ERR_INCOMPATIBLE;
	}
	if (msgId != msgType->m_data) {
		TUTRACE((TUTRACE_ERR, "message type %d differs from protocol version %d\n",
			msgType->m_data, msgId));
		return RPROT_ERR_WRONG_MSGTYPE;
	}

	return WPS_SUCCESS;
}

uint32
reg_proto_create_m1(RegData *regInfo, BufferObj *msg)
{
	uint32 ret = WPS_SUCCESS;
	uint8 message;
	CTlvPrimDeviceType primDev;
#ifdef WFA_WPS_20_TESTBED
	/* For internal testing purpose */
	bool b_zpadding = regInfo->p_enrolleeInfo->b_zpadding;
#endif /* WFA_WPS_20_TESTBED */
	uint16 configMethods;

	/* First generate/gather all the required data. */
	message = WPS_ID_MESSAGE_M1;

	/* Enrollee nonce */
	/*
	 * Hacking, do not generate new random enrollee nonce
	 * in case of we have prebuild enrollee nonce.
	 */
	if (regInfo->e_lastMsgSent == MNONE) {
		RAND_bytes(regInfo->enrolleeNonce, SIZE_128_BITS);
		TUTRACE((TUTRACE_ERR, "RPROTO: create enrolleeNonce\n"));
	}
	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM1 \n"));

	/* It should not generate new key pair if we have prebuild enrollee nonce */
	if (!regInfo->DHSecret) {
		BufferObj *pubKey = buffobj_new();
		if (pubKey == NULL)
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

		TUTRACE((TUTRACE_ERR, "RPROTO: create DH Secret\n"));
	}

	/* Extract the DH public key */
	if (reg_proto_BN_bn2bin(regInfo->DHSecret->pub_key, regInfo->pke) != SIZE_PUB_KEY)
		return RPROT_ERR_CRYPTO;

	/* Now start composing the message */
	tlv_serialize(WPS_ID_VERSION, msg, &regInfo->p_enrolleeInfo->version, WPS_ID_VERSION_S);
	tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
	tlv_serialize(WPS_ID_UUID_E, msg, regInfo->p_enrolleeInfo->uuid, SIZE_UUID);
	tlv_serialize(WPS_ID_MAC_ADDR, msg, regInfo->p_enrolleeInfo->macAddr, SIZE_MAC_ADDR);
	tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_PUBLIC_KEY, msg, regInfo->pke, SIZE_PUB_KEY);
	tlv_serialize(WPS_ID_AUTH_TYPE_FLAGS, msg, &regInfo->p_enrolleeInfo->authTypeFlags,
		WPS_ID_AUTH_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_ENCR_TYPE_FLAGS, msg, &regInfo->p_enrolleeInfo->encrTypeFlags,
		WPS_ID_ENCR_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_CONN_TYPE_FLAGS, msg, &regInfo->p_enrolleeInfo->connTypeFlags,
		WPS_ID_CONN_TYPE_FLAGS_S);
	/*
	 * For DTM1.5 /1.6 test case "WCN Wireless Compare Probe Response and M1 messages",
	 * Configuration Methods in M1 must matched in Probe Response
	 */
	configMethods = regInfo->p_enrolleeInfo->configMethods;
	/*
	 * 1.In WPS Test Plan 2.0.3, case 4.1.2 step 4. APUT cannot use push button to add ER,
	 * here we make Config Methods in both Probe Resp and M1 consistently.
	 * 2.DMT PBC testing check the PBC method from CONFIG_METHODS field (for now, DMT
	 * testing only avaliable on WPS 1.0
	 */
	if (regInfo->p_enrolleeInfo->b_ap) {
		if (regInfo->p_enrolleeInfo->version2 >= WPS_VERSION2) {
			/* configMethods &= ~(WPS_CONFMET_VIRT_PBC | WPS_CONFMET_PHY_PBC); */
		}
		else {
			/* WPS 1.0 */
			if (regInfo->p_enrolleeInfo->scState == WPS_SCSTATE_UNCONFIGURED)
				configMethods &= ~WPS_CONFMET_PBC;
		}
	}
	tlv_serialize(WPS_ID_CONFIG_METHODS, msg, &configMethods, WPS_ID_CONFIG_METHODS_S);
	tlv_serialize(WPS_ID_SC_STATE, msg, &regInfo->p_enrolleeInfo->scState, WPS_ID_SC_STATE_S);
	tlv_serialize(WPS_ID_MANUFACTURER, msg, regInfo->p_enrolleeInfo->manufacturer,
		STR_ATTR_LEN(regInfo->p_enrolleeInfo->manufacturer, SIZE_64_BYTES));
	tlv_serialize(WPS_ID_MODEL_NAME, msg, regInfo->p_enrolleeInfo->modelName,
		STR_ATTR_LEN(regInfo->p_enrolleeInfo->modelName, SIZE_32_BYTES));
	tlv_serialize(WPS_ID_MODEL_NUMBER, msg, regInfo->p_enrolleeInfo->modelNumber,
		STR_ATTR_LEN(regInfo->p_enrolleeInfo->modelNumber, SIZE_32_BYTES));
	tlv_serialize(WPS_ID_SERIAL_NUM, msg, regInfo->p_enrolleeInfo->serialNumber,
		STR_ATTR_LEN(regInfo->p_enrolleeInfo->serialNumber, SIZE_32_BYTES));

	primDev.categoryId = regInfo->p_enrolleeInfo->primDeviceCategory;
	primDev.oui = regInfo->p_enrolleeInfo->primDeviceOui;
	primDev.subCategoryId = regInfo->p_enrolleeInfo->primDeviceSubCategory;
	tlv_primDeviceTypeWrite(&primDev, msg);
	tlv_serialize(WPS_ID_DEVICE_NAME, msg, regInfo->p_enrolleeInfo->deviceName,
		STR_ATTR_LEN(regInfo->p_enrolleeInfo->deviceName, SIZE_32_BYTES));

	/* set real RF band */
	tlv_serialize(WPS_ID_RF_BAND, msg, &regInfo->p_enrolleeInfo->rfBand, WPS_ID_RF_BAND_S);
	tlv_serialize(WPS_ID_ASSOC_STATE, msg, &regInfo->p_enrolleeInfo->assocState,
		WPS_ID_ASSOC_STATE_S);

	tlv_serialize(WPS_ID_DEVICE_PWD_ID, msg, &regInfo->p_enrolleeInfo->devPwdId,
		WPS_ID_DEVICE_PWD_ID_S);
	tlv_serialize(WPS_ID_CONFIG_ERROR, msg, &regInfo->p_enrolleeInfo->configError,
		WPS_ID_CONFIG_ERROR_S);
	tlv_serialize(WPS_ID_OS_VERSION, msg, &regInfo->p_enrolleeInfo->osVersion,
		WPS_ID_OS_VERSION_S);

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (regInfo->p_enrolleeInfo->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		BufferObj *vendorExt_bufObj = buffobj_new();

		if (vendorExt_bufObj == NULL)
			return WPS_ERR_OUTOFMEMORY;

		/* Add Version2 subelement to vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&regInfo->p_enrolleeInfo->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Add reqToEnroll subelement to vendorExt_bufObj */
		if (regInfo->p_enrolleeInfo->b_reqToEnroll)
			subtlv_serialize(WPS_WFA_SUBID_REQ_TO_ENROLL, vendorExt_bufObj,
				&regInfo->p_enrolleeInfo->b_reqToEnroll,
				WPS_WFA_SUBID_REQ_TO_ENROLL_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->p_enrolleeInfo->nattr_len)
			buffobj_Append(msg, regInfo->p_enrolleeInfo->nattr_len,
				(uint8 *)regInfo->p_enrolleeInfo->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Add support for Windows Rally Vertical Pairing */
	reg_proto_vendor_ext_vp(regInfo->p_enrolleeInfo, msg);

	/* skip other... optional attributes */

	/* copy message to outMsg buffer */
	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM1 copying to output %d bytes\n",
		buffobj_Length(msg)));

	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, buffobj_Length(msg), buffobj_GetBuf(msg));

	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM1 built: %d bytes\n",
		buffobj_Length(msg)));

	return WPS_SUCCESS;
}

uint32
reg_proto_process_m2(RegData *regInfo, BufferObj *msg, void **encrSettings, uint8 *regNonce)
{
	WpsM2 m;
	uint8 *Pos;
	uint32 err, tlv_err = 0;
	uint8 DHKey[SIZE_256_BITS];
	BufferObj *buf1, *buf1_dser, *buf2_dser;
	uint8 secret[SIZE_PUB_KEY];
	int secretLen;
	BufferObj *kdkBuf;
	BufferObj *kdkData;
	BufferObj *pString;
	BufferObj *keys;
	BufferObj *hmacData;
	uint8 kdk[SIZE_256_BITS];
	uint8 null_uuid[SIZE_16_BYTES] = {0};


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

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM2: %d byte message\n",
		buffobj_Length(msg)));

	/* First, deserialize (parse) the message. */

	/* First and foremost, check the version and message number */
	if ((err = reg_proto_verCheck(&m.version, &m.msgType,
		WPS_ID_MESSAGE_M2, msg)) != WPS_SUCCESS)
		goto error;

	reg_msg_init(&m, WPS_ID_MESSAGE_M2);

	tlv_err |= tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE,
		msg, SIZE_128_BITS, 0);
	tlv_err |= tlv_dserialize(&m.registrarNonce, WPS_ID_REGISTRAR_NONCE,
		msg, SIZE_128_BITS, 0);
	tlv_err |= tlv_dserialize(&m.uuid, WPS_ID_UUID_R, msg, SIZE_UUID, 0);
	tlv_err |= tlv_dserialize(&m.publicKey, WPS_ID_PUBLIC_KEY, msg, SIZE_PUB_KEY, 0);
	tlv_err |= tlv_dserialize(&m.authTypeFlags, WPS_ID_AUTH_TYPE_FLAGS, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.encrTypeFlags, WPS_ID_ENCR_TYPE_FLAGS, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.connTypeFlags, WPS_ID_CONN_TYPE_FLAGS, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.configMethods, WPS_ID_CONFIG_METHODS, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.manufacturer, WPS_ID_MANUFACTURER,
		msg, SIZE_64_BYTES, 0);
	tlv_err |= tlv_dserialize(&m.modelName, WPS_ID_MODEL_NAME, msg, SIZE_32_BYTES, 0);
	tlv_err |= tlv_dserialize(&m.modelNumber, WPS_ID_MODEL_NUMBER,
		msg, SIZE_32_BYTES, 0);
	tlv_err |= tlv_dserialize(&m.serialNumber, WPS_ID_SERIAL_NUM, msg, SIZE_32_BYTES, 0);

	tlv_err |= tlv_primDeviceTypeParse(&m.primDeviceType, msg);
	tlv_err |= tlv_dserialize(&m.deviceName, WPS_ID_DEVICE_NAME,
		msg, SIZE_32_BYTES, 0);
	tlv_err |= tlv_dserialize(&m.rfBand, WPS_ID_RF_BAND, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.assocState, WPS_ID_ASSOC_STATE, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.configError, WPS_ID_CONFIG_ERROR, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.devPwdId, WPS_ID_DEVICE_PWD_ID, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.osVersion, WPS_ID_OS_VERSION, msg, 0, 0);

	/* WSC 2.0, parse WFA vendor id and subelements from vendor extension attribute  */
	if (regInfo->p_enrolleeInfo->version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			/* Reuse buf1 */
			BufferObj *vendorExt_bufObj = buf1;

			/* Deserialize subelement */
			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* WSC 2.0, is next Version 2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				tlv_err |= subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);
		}
		else {
			/* No WFA vendor extension, rewind theBuf to get Authenticator */
			buffobj_Rewind(msg);
		}
	}

	/*
	 * For DTM Error Handling Multiple M2 Testing.
	 * We provide the coming M2 R-NONCE in regNonce if any.
	 * It may used by reg_proto_create_nack() when multiple M2 error happen.
	 */
	if (regNonce)
		memcpy(regNonce, m.registrarNonce.m_data, m.registrarNonce.tlvbase.m_len);

	TUTRACE((TUTRACE_INFO, "RPROTO: m2.authTypeFlags.m_data()=%x, authTypeFlags=%x, "
		"m2.encrTypeFlags.m_data()=%x, encrTypeFlags=%x  \n",
		m.authTypeFlags.m_data, regInfo->p_enrolleeInfo->authTypeFlags,
		m.encrTypeFlags.m_data, regInfo->p_enrolleeInfo->encrTypeFlags));

	if (((m.authTypeFlags.m_data & regInfo->p_enrolleeInfo->authTypeFlags) == 0) ||
		((m.encrTypeFlags.m_data & regInfo->p_enrolleeInfo->encrTypeFlags) == 0)) {
		err = RPROT_ERR_AUTH_ENC_FLAG;
		TUTRACE((TUTRACE_ERR, "RPROTO: M2 Auth/Encr type not acceptable\n"));
		goto error;
	}

	/* AP have received M2,it must ignor other M2 */
	if (regInfo->p_registrarInfo &&
	    memcmp(regInfo->p_registrarInfo->uuid, null_uuid, SIZE_16_BYTES) != 0 &&
	    memcmp(regInfo->p_registrarInfo->uuid, m.uuid.m_data, m.uuid.tlvbase.m_len) != 0) {
		TUTRACE((TUTRACE_ERR, "\n***********  multiple M2 !!!! *********\n"));
		err = RPROT_ERR_MULTIPLE_M2;
		goto error;
	}

	/* for VISTA TEST -- */
	if (tlv_err) {
		err =  RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/*
	 * skip the other vendor extensions and any other optional TLVs until
	 * we get to the authenticator
	 */
	while (WPS_ID_AUTHENTICATOR != buffobj_NextType(msg)) {
		/* optional encrypted settings */
		if (WPS_ID_ENCR_SETTINGS == buffobj_NextType(msg) &&
			m.encrSettings.encrDataLength == 0) {
			/* only process the first encrypted settings attribute encountered */
			tlv_err |= tlv_encrSettingsParse(&m.encrSettings, msg);
			Pos = buffobj_Pos(msg);
		}
		else {
			/* advance past the TLV */
			Pos = buffobj_Advance(msg, sizeof(WpsTlvHdr) +
				WpsNtohs((buffobj_Pos(msg)+sizeof(uint16))));
		}

		/*
		 * If Advance returned NULL, it means there's no more data in the
		 * buffer. This is an error.
		 */
		if (Pos == NULL) {
			err = RPROT_ERR_REQD_TLV_MISSING;
			goto error;
		}
	}

	tlv_err |= tlv_dserialize(&m.authenticator, WPS_ID_AUTHENTICATOR,
		msg, SIZE_64_BITS, 0);
	if (tlv_err) {
		err =  RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* Now start processing the message */

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
		err = RPROT_ERR_NONCE_MISMATCH;
		goto error;
	}

	/*
	  * to verify the hmac, we need to process the nonces, generate
	  * the DH secret, the KDK and finally the auth key
	  */
	memcpy(regInfo->registrarNonce, m.registrarNonce.m_data, m.registrarNonce.tlvbase.m_len);

	/*
	 * read the registrar's public key
	 * First store the raw public key (to be used for e/rhash computation)
	 */
	memcpy(regInfo->pkr, m.publicKey.m_data, SIZE_PUB_KEY);

	/* Next, allocate memory for the pub key */
	regInfo->DH_PubKey_Peer = BN_new();
	if (!regInfo->DH_PubKey_Peer) {
		err = WPS_ERR_OUTOFMEMORY;
		goto error;
	}

	/* Finally, import the raw key into the bignum datastructure */
	if (BN_bin2bn(regInfo->pkr, SIZE_PUB_KEY,
		regInfo->DH_PubKey_Peer) == NULL) {
		err =  RPROT_ERR_CRYPTO;
		goto error;
	}

	/* KDK generation */
	/* 1. generate the DH shared secret */
	secretLen = DH_compute_key_bn(secret, regInfo->DH_PubKey_Peer,
		regInfo->DHSecret);
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

	/* 3. Append the enrollee nonce(N1), enrollee mac and registrar nonce(N2) */
	kdkData = buf1;
	buffobj_Reset(kdkData);
	buffobj_Append(kdkData, SIZE_128_BITS, regInfo->enrolleeNonce);
	buffobj_Append(kdkData, SIZE_MAC_ADDR, regInfo->p_enrolleeInfo->macAddr);
	buffobj_Append(kdkData, SIZE_128_BITS, regInfo->registrarNonce);

	/* 4. now generate the KDK */
	hmac_sha256(DHKey, SIZE_256_BITS,
		buffobj_GetBuf(kdkData), buffobj_Length(kdkData), kdk, NULL);

	/* KDK generation */
	/* Derivation of AuthKey, KeyWrapKey and EMSK */
	/* 1. declare and initialize the appropriate buffer objects */
	kdkBuf = buf1_dser;
	buffobj_dserial(kdkBuf, kdk, SIZE_256_BITS);
	pString = buf2_dser;
	buffobj_dserial(pString, (uint8 *)PERSONALIZATION_STRING,
		(uint32)strlen(PERSONALIZATION_STRING));
	keys = buf1;
	buffobj_Reset(keys);

	/* 2. call the key derivation function */
	reg_proto_derivekey(kdkBuf, pString, KDF_KEY_BITS, keys);

	/* 3. split the key into the component keys and store them */
	buffobj_RewindLength(keys, buffobj_Length(keys));
	buffobj_Append(regInfo->authKey, SIZE_256_BITS, buffobj_Pos(keys));
	buffobj_Advance(keys, SIZE_256_BITS);

	buffobj_Append(regInfo->keyWrapKey, SIZE_128_BITS, buffobj_Pos(keys));
	buffobj_Advance(keys, SIZE_128_BITS);

	buffobj_Append(regInfo->emsk, SIZE_256_BITS, buffobj_Pos(keys));
	/* Derivation of AuthKey, KeyWrapKey and EMSK */

	/* HMAC validation */
	hmacData = buf1;
	buffobj_Reset(hmacData);
	/* append the last message sent */
	buffobj_Append(hmacData, buffobj_Length(regInfo->outMsg), buffobj_GetBuf(regInfo->outMsg));

	/* append the current message. Don't append the last TLV (auth) */
	buffobj_Append(hmacData,
	        msg->m_dataLength -(sizeof(WpsTlvHdr)+ m.authenticator.tlvbase.m_len),
	        msg->pBase);

	if (!reg_proto_validate_mac(hmacData, m.authenticator.m_data, regInfo->authKey)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: HMAC validation failed\n"));
		err = RPROT_ERR_CRYPTO;
		goto error;
	}
	/* HMAC validation */

	/* Save Registrar's Informations */
	/* Now we can proceed with copying out and processing the other data */
	if (!regInfo->p_registrarInfo) {
		regInfo->p_registrarInfo = (DevInfo *)alloc_init(sizeof(DevInfo));
		if (!regInfo->p_registrarInfo) {
			err = WPS_ERR_OUTOFMEMORY;
			goto error;
		}
	}
	memcpy(regInfo->p_registrarInfo->uuid, m.uuid.m_data, m.uuid.tlvbase.m_len);

	regInfo->p_registrarInfo->authTypeFlags = m.authTypeFlags.m_data;
	regInfo->p_registrarInfo->encrTypeFlags = m.encrTypeFlags.m_data;
	regInfo->p_registrarInfo->connTypeFlags = m.connTypeFlags.m_data;
	regInfo->p_registrarInfo->configMethods = m.configMethods.m_data;
	wps_strncpy(regInfo->p_registrarInfo->manufacturer, m.manufacturer.m_data,
		sizeof(regInfo->p_registrarInfo->manufacturer));
	wps_strncpy(regInfo->p_registrarInfo->modelName, m.modelName.m_data,
		sizeof(regInfo->p_registrarInfo->modelName));
	wps_strncpy(regInfo->p_registrarInfo->serialNumber, m.serialNumber.m_data,
		sizeof(regInfo->p_registrarInfo->serialNumber));
	regInfo->p_registrarInfo->primDeviceCategory = m.primDeviceType.categoryId;
	regInfo->p_registrarInfo->primDeviceOui = m.primDeviceType.oui;
	regInfo->p_registrarInfo->primDeviceSubCategory = m.primDeviceType.subCategoryId;
	wps_strncpy(regInfo->p_registrarInfo->deviceName, m.deviceName.m_data,
		sizeof(regInfo->p_registrarInfo->deviceName));
	regInfo->p_registrarInfo->rfBand = m.rfBand.m_data;
	regInfo->p_registrarInfo->assocState = m.assocState.m_data;
	regInfo->p_registrarInfo->configError = m.configError.m_data;
	regInfo->p_registrarInfo->devPwdId = m.devPwdId.m_data;
	regInfo->p_registrarInfo->osVersion = m.osVersion.m_data;
	/* WSC 2.0 */
	regInfo->p_registrarInfo->version2 = m.version2.m_data;

	/* extract encrypted settings */
	if (m.encrSettings.encrDataLength) {
		BufferObj *cipherText = buf1_dser;
		BufferObj *plainText = 0;
		BufferObj *iv = 0;
		buffobj_dserial(cipherText, m.encrSettings.ip_encryptedData,
			m.encrSettings.encrDataLength);
		iv = buf2_dser;
		buffobj_dserial(iv, m.encrSettings.iv, SIZE_128_BITS);
		plainText = buf1;
		buffobj_Reset(plainText);
		reg_proto_decrypt_data(cipherText, iv, regInfo->keyWrapKey,
			regInfo->authKey, plainText);

		/* Usually, only out of band method gets enrSettings at M2 stage */
		if (encrSettings) {
			if (regInfo->p_enrolleeInfo->b_ap) {
				CTlvEsM8Ap *esAP = reg_msg_m8ap_new();
				reg_msg_m8ap_parse(esAP, plainText, regInfo->authKey, true);
				*encrSettings = (void *)esAP;
			}
			else {
				CTlvEsM8Sta *esSta = reg_msg_m8sta_new();
				reg_msg_m8sta_parse(esSta, plainText, regInfo->authKey, true);
				*encrSettings = (void *)esSta;
			}
			/*
			 * WSC 2.0, check MAC attribute only when registrar
			 * and enrollee are both WSC 2.0
			 */
			if (regInfo->p_registrarInfo->version2 >= WPS_VERSION2 &&
				reg_proto_MacCheck(regInfo->p_enrolleeInfo, *encrSettings)
					!= WPS_SUCCESS) {
				err = RPROT_ERR_ROGUE_SUSPECTED;
				goto error;
			}
		}
	}
	/* extract encrypted settings */

	/*
	 * now set the registrar's b_ap flag. If the local enrollee is an ap,
	 * the registrar shouldn't be one
	 */
	if (regInfo->p_enrolleeInfo->b_ap)
		regInfo->p_registrarInfo->b_ap = true;

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, msg->m_dataLength, msg->pBase);
	err = WPS_SUCCESS;

error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "ProcessMessageM2 : got error %x\n", err));
	buffobj_del(buf1);
	buffobj_del(buf1_dser);
	buffobj_del(buf2_dser);

	return err;
}

uint32
reg_proto_process_m2d(RegData *regInfo, BufferObj *msg, uint8 *regNonce)
{
	WpsM2 m;
	uint32 err;

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM2D: %d byte message\n",
		msg->m_dataLength));

	/* First, deserialize (parse) the message. */

	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	if ((err = reg_proto_verCheck(&m.version, &m.msgType,
		WPS_ID_MESSAGE_M2D, msg)) != WPS_SUCCESS)
		goto error;

	reg_msg_init(&m, WPS_ID_MESSAGE_M2D);

	tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE, msg, SIZE_128_BITS, 0);

	tlv_dserialize(&m.registrarNonce, WPS_ID_REGISTRAR_NONCE, msg, SIZE_128_BITS, 0);
	tlv_dserialize(&m.uuid, WPS_ID_UUID_R, msg, SIZE_UUID, 0);
	/* No Public Key in M2D */
	tlv_dserialize(&m.authTypeFlags, WPS_ID_AUTH_TYPE_FLAGS, msg, 0, 0);
	tlv_dserialize(&m.encrTypeFlags, WPS_ID_ENCR_TYPE_FLAGS, msg, 0, 0);
	tlv_dserialize(&m.connTypeFlags, WPS_ID_CONN_TYPE_FLAGS, msg, 0, 0);
	tlv_dserialize(&m.configMethods, WPS_ID_CONFIG_METHODS, msg, 0, 0);
	tlv_dserialize(&m.manufacturer, WPS_ID_MANUFACTURER, msg, SIZE_64_BYTES, 0);
	tlv_dserialize(&m.modelName, WPS_ID_MODEL_NAME, msg, SIZE_32_BYTES, 0);
	tlv_dserialize(&m.modelNumber, WPS_ID_MODEL_NUMBER, msg, SIZE_32_BYTES, 0);
	tlv_dserialize(&m.serialNumber, WPS_ID_SERIAL_NUM, msg, SIZE_32_BYTES, 0);

	tlv_primDeviceTypeParse(&m.primDeviceType, msg);

	tlv_dserialize(&m.deviceName, WPS_ID_DEVICE_NAME, msg, SIZE_32_BYTES, 0);
	tlv_dserialize(&m.rfBand, WPS_ID_RF_BAND, msg, 0, 0);
	tlv_dserialize(&m.assocState, WPS_ID_ASSOC_STATE, msg, 0, 0);
	tlv_dserialize(&m.configError, WPS_ID_CONFIG_ERROR, msg, 0, 0);
	tlv_dserialize(&m.osVersion, WPS_ID_OS_VERSION, msg, 0, 0);

	/* WSC 2.0, parse WFA vendor id and subelements from vendor extension attribute  */
	if (regInfo->p_enrolleeInfo->version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			BufferObj *vendorExt_bufObj = buffobj_new();

			/* Deserialize subelement */
			if (!vendorExt_bufObj) {
				err = WPS_ERR_OUTOFMEMORY;
				goto error;
			}

			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* WSC 2.0, is next Version 2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);

			buffobj_del(vendorExt_bufObj);
		}
	}

	/* ignore any other TLVs in the message */

	/* Now start processing the message */

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
		err = RPROT_ERR_NONCE_MISMATCH;
		goto error;
	}

	/*
	 * For DTM Error Handling Multiple M2 Testing.
	 * We provide the coming M2D R-NONCE in regNonce if any.
	 * It may used by reg_proto_create_nack() when multiple M2 error happen.
	 */
	if (regNonce)
		memcpy(regNonce, m.registrarNonce.m_data, m.registrarNonce.tlvbase.m_len);

	if (!regInfo->p_registrarInfo) {
		regInfo->p_registrarInfo = (DevInfo *)alloc_init(sizeof(DevInfo));
		if (!regInfo->p_registrarInfo) {
			err = WPS_ERR_OUTOFMEMORY;
			goto error;
		}
	}

	/* Now we can proceed with copying out and processing the other data */
	/*
	memcpy(regInfo->p_registrarInfo->uuid, m.uuid.m_data, m.uuid.tlvbase.m_len);
	*/

	/* No public key in M2D */
	if ((m.authTypeFlags.m_data & 0x3F) == 0) {
		err =  RPROT_ERR_INCOMPATIBLE;
		goto error;
	}
	regInfo->p_registrarInfo->authTypeFlags = m.authTypeFlags.m_data;

	if ((m.encrTypeFlags.m_data & 0x0F) == 0) {
		err = RPROT_ERR_INCOMPATIBLE;
		goto error;
	}
	regInfo->p_registrarInfo->encrTypeFlags = m.encrTypeFlags.m_data;
	regInfo->p_registrarInfo->connTypeFlags = m.connTypeFlags.m_data;
	regInfo->p_registrarInfo->configMethods = m.configMethods.m_data;
	wps_strncpy(regInfo->p_registrarInfo->manufacturer, m.manufacturer.m_data,
		sizeof(regInfo->p_registrarInfo->manufacturer));
	wps_strncpy(regInfo->p_registrarInfo->modelName, m.modelName.m_data,
		sizeof(regInfo->p_registrarInfo->modelName));
	wps_strncpy(regInfo->p_registrarInfo->serialNumber, m.serialNumber.m_data,
		sizeof(regInfo->p_registrarInfo->serialNumber));
	regInfo->p_registrarInfo->primDeviceCategory = m.primDeviceType.categoryId;
	regInfo->p_registrarInfo->primDeviceOui = m.primDeviceType.oui;
	regInfo->p_registrarInfo->primDeviceSubCategory = m.primDeviceType.subCategoryId;
	wps_strncpy(regInfo->p_registrarInfo->deviceName, m.deviceName.m_data,
		sizeof(regInfo->p_registrarInfo->deviceName));
	regInfo->p_registrarInfo->rfBand = m.rfBand.m_data;
	regInfo->p_registrarInfo->assocState = m.assocState.m_data;
	regInfo->p_registrarInfo->configError = m.configError.m_data;
	/* No Dev Pwd Id in this message: regInfo->p_registrarInfo->devPwdId = m.devPwdId.m_data; */
	/* WSC 2.0, is next Version 2 */
	regInfo->p_registrarInfo->version2 = m.version2.m_data;

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, msg->m_dataLength, msg->pBase);
	err = WPS_SUCCESS;

error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "ProcessMessageM2D : got error %x", err));

	return err;
}

uint32
reg_proto_create_m3(RegData *regInfo, BufferObj *msg)
{
	uint8 message;
	uint8 hashBuf[SIZE_256_BITS];
	uint32 err;
	BufferObj *hmacData;
	uint8 hmac[SIZE_256_BITS];
	BufferObj *ehashBuf;
	uint8 *pwdPtr = 0;
	int pwdLen;

	BufferObj *buf1 = buffobj_new();

	if (buf1 == NULL)
		return WPS_ERR_OUTOFMEMORY;

	message = WPS_ID_MESSAGE_M3;

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

	/* EHash1 and EHash2 generation */
	RAND_bytes(regInfo->es1, SIZE_128_BITS);
	RAND_bytes(regInfo->es2, SIZE_128_BITS);

	ehashBuf = buf1;
	buffobj_Reset(ehashBuf);
	buffobj_Append(ehashBuf, SIZE_128_BITS, regInfo->es1);
	buffobj_Append(ehashBuf, SIZE_128_BITS, regInfo->psk1);
	buffobj_Append(ehashBuf, SIZE_PUB_KEY, regInfo->pke);
	buffobj_Append(ehashBuf, SIZE_PUB_KEY, regInfo->pkr);
	hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
		ehashBuf->pBase, ehashBuf->m_dataLength, hashBuf, NULL);

	memcpy(regInfo->eHash1, hashBuf, SIZE_256_BITS);

	buffobj_Reset(ehashBuf);
	buffobj_Append(ehashBuf, SIZE_128_BITS, regInfo->es2);
	buffobj_Append(ehashBuf, SIZE_128_BITS, regInfo->psk2);
	buffobj_Append(ehashBuf, SIZE_PUB_KEY, regInfo->pke);
	buffobj_Append(ehashBuf, SIZE_PUB_KEY, regInfo->pkr);
	hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
		ehashBuf->pBase, ehashBuf->m_dataLength, hashBuf, NULL);

	memcpy(regInfo->eHash2, hashBuf, SIZE_256_BITS);
	/* EHash1 and EHash2 generation */

	/* Now assemble the message */
	tlv_serialize(WPS_ID_VERSION, msg, &regInfo->p_enrolleeInfo->version, WPS_ID_VERSION_S);
	tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
	tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, regInfo->registrarNonce, SIZE_128_BITS);

	tlv_serialize(WPS_ID_E_HASH1, msg, regInfo->eHash1, SIZE_256_BITS);
	tlv_serialize(WPS_ID_E_HASH2, msg, regInfo->eHash2, SIZE_256_BITS);

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (regInfo->p_enrolleeInfo->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		/* Reuse buf1 */
		BufferObj *vendorExt_bufObj = buf1;

		buffobj_Reset(vendorExt_bufObj);

		/* Add Version2 subelement to vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&regInfo->p_enrolleeInfo->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->p_enrolleeInfo->nattr_len)
			buffobj_Append(msg, regInfo->p_enrolleeInfo->nattr_len,
				(uint8*)regInfo->p_enrolleeInfo->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Calculate the hmac */
	hmacData = buf1;
	buffobj_Reset(hmacData);
	buffobj_Append(hmacData, regInfo->inMsg->m_dataLength, regInfo->inMsg->pBase);
	buffobj_Append(hmacData, msg->m_dataLength, msg->pBase);

	hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
		hmacData->pBase, hmacData->m_dataLength, hmac, NULL);
	tlv_serialize(WPS_ID_AUTHENTICATOR, msg, hmac, SIZE_64_BITS);

	/* Store the outgoing message  */
	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, msg->m_dataLength, msg->pBase);

	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM3 built: %d bytes\n",
		msg->m_dataLength));
	err = WPS_SUCCESS;

	buffobj_del(buf1);
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "BuildMessageM3 : got error %x", err));

	return err;
}

uint32
reg_proto_process_m4(RegData *regInfo, BufferObj *msg)
{
	uint8 *Pos;
	WpsM4 m;
	uint32 err, tlv_err = 0;
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

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM4: %d byte message\n",
		msg->m_dataLength));
	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	if ((err = reg_proto_verCheck(&m.version, &m.msgType,
		WPS_ID_MESSAGE_M4, msg)) != WPS_SUCCESS)
		goto error;

	reg_msg_init(&m, WPS_ID_MESSAGE_M4);

	tlv_err |= tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE,
		msg, SIZE_128_BITS, 0);

	tlv_err |= tlv_dserialize(&m.rHash1, WPS_ID_R_HASH1, msg, SIZE_256_BITS, 0);
	tlv_err |= tlv_dserialize(&m.rHash2, WPS_ID_R_HASH2, msg, SIZE_256_BITS, 0);

	tlv_err |= tlv_encrSettingsParse(&m.encrSettings, msg);
	if (tlv_err) {
		err = RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* WSC 2.0, parse WFA vendor id and subelements from vendor extension attribute  */
	if (regInfo->p_enrolleeInfo->version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			/* Reuse buf1 */
			BufferObj *vendorExt_bufObj = buf1;

			buffobj_Reset(vendorExt_bufObj);

			/* Deserialize subelement */
			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* Get Version2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				tlv_err |= subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);
		}
		else {
			/* No WFA vendor extension, rewind theBuf to get Authenticator */
			buffobj_Rewind(msg);
		}
	}

	/* skip the other optional attributes until we get to the authenticator */
	while (WPS_ID_AUTHENTICATOR != buffobj_NextType(msg)) {
		/* advance past the TLV */
		Pos = buffobj_Advance(msg, sizeof(WpsTlvHdr) +
			WpsNtohs((msg->pCurrent+sizeof(uint16))));

		/*
		 * If Advance returned NULL, it means there's no more data in the
		 * buffer. This is an error.
		 */
		if (Pos == NULL) {
			err =  RPROT_ERR_REQD_TLV_MISSING;
			goto error;
		}
	}

	tlv_err |= tlv_dserialize(&m.authenticator, WPS_ID_AUTHENTICATOR,
		msg, SIZE_64_BITS, 0);
	if (tlv_err) {
		err = RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* Now start validating the message */

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
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

	/* Now copy the relevant data */
	memcpy(regInfo->rHash1, m.rHash1.m_data, SIZE_256_BITS);
	memcpy(regInfo->rHash2, m.rHash2.m_data, SIZE_256_BITS);

	/* extract encrypted settings */
	{
		BufferObj *cipherText = buf1_dser;
		BufferObj *iv = 0;
		BufferObj *plainText = 0;
		TlvEsNonce rNonce;

		buffobj_dserial(cipherText, m.encrSettings.ip_encryptedData,
		                  m.encrSettings.encrDataLength);
		iv = buf2_dser;
		buffobj_dserial(iv, m.encrSettings.iv, SIZE_128_BITS);
		plainText = buf1;
		buffobj_Reset(plainText);
		reg_proto_decrypt_data(cipherText, iv, regInfo->keyWrapKey,
			regInfo->authKey, plainText);
		reg_msg_nonce_parse(&rNonce, WPS_ID_R_SNONCE1, plainText, regInfo->authKey);

		/* extract encrypted settings */

		/* RHash1 validation */
		/* 1. Save RS1 */
		memcpy(regInfo->rs1, rNonce.nonce.m_data, rNonce.nonce.tlvbase.m_len);
	}

	/* 2. prepare the buffer */
	{
		uint8 hashBuf[SIZE_256_BITS];
		BufferObj *rhashBuf = buf1;
		buffobj_Reset(rhashBuf);
		buffobj_Append(rhashBuf, SIZE_128_BITS, regInfo->rs1);
		buffobj_Append(rhashBuf, SIZE_128_BITS, regInfo->psk1);
		buffobj_Append(rhashBuf, SIZE_PUB_KEY, regInfo->pke);
		buffobj_Append(rhashBuf, SIZE_PUB_KEY, regInfo->pkr);

		/* 3. generate the mac */
		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			rhashBuf->pBase, rhashBuf->m_dataLength, hashBuf, NULL);

		/* 4. compare the mac to rhash1 */
		if (memcmp(regInfo->rHash1, hashBuf, SIZE_256_BITS)) {
			TUTRACE((TUTRACE_ERR, "RPROTO: RS1 hash doesn't match RHash1\n"));
			err =  RPROT_ERR_CRYPTO;
			goto error;
		}
	}
	/* 5. Instead of steps 3 & 4, we could have called ValidateMac */
	/* RHash1 validation */

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, msg->m_dataLength, msg->pBase);
	err = WPS_SUCCESS;
	TUTRACE((TUTRACE_INFO, "RPROTO: ProcessMessageM4 done: %d bytes\n",
		msg->m_dataLength));
error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "ProcessMessageM4: got error %x\n", err));

	buffobj_del(buf1);
	buffobj_del(buf1_dser);
	buffobj_del(buf2_dser);

	return err;
}

uint32
reg_proto_create_m5(RegData *regInfo, BufferObj *msg)
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
	message = WPS_ID_MESSAGE_M5;

	/* encrypted settings. */
	{
		BufferObj *encData = buf1;
		BufferObj *cipherText = buf2;
		BufferObj *iv = buf3;
		TlvEsNonce esNonce;
		buffobj_Reset(encData);
		buffobj_Reset(cipherText);
		buffobj_Reset(iv);
		tlv_set(&esNonce.nonce, WPS_ID_E_SNONCE1, regInfo->es1, SIZE_128_BITS);
		reg_msg_nonce_write(&esNonce, encData, regInfo->authKey);

		reg_proto_encrypt_data(encData, regInfo->keyWrapKey,
			regInfo->authKey, cipherText, iv);

		/* Now assemble the message */
		tlv_serialize(WPS_ID_VERSION, msg, &regInfo->p_enrolleeInfo->version,
			WPS_ID_VERSION_S);
		tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
		tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, regInfo->registrarNonce, SIZE_128_BITS);
		{
			CTlvEncrSettings encrSettings;
			encrSettings.iv = iv->pBase;
			encrSettings.ip_encryptedData = cipherText->pBase;
			encrSettings.encrDataLength = cipherText->m_dataLength;
			tlv_encrSettingsWrite(&encrSettings, msg);
		}
	}

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (regInfo->p_enrolleeInfo->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		BufferObj *vendorExt_bufObj = buffobj_new();

		if (vendorExt_bufObj == NULL) {
			err = WPS_ERR_OUTOFMEMORY;
			goto error;
		}

		/* Add Version2 subelement to vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&regInfo->p_enrolleeInfo->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->p_enrolleeInfo->nattr_len)
			buffobj_Append(msg, regInfo->p_enrolleeInfo->nattr_len,
				(uint8*)regInfo->p_enrolleeInfo->nattr_tlv);
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


	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM5 built: %d bytes\n",
		msg->m_dataLength));
	err = WPS_SUCCESS;

error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "BuildMessageM5 : got error %x", err));
	buffobj_del(buf1);
	buffobj_del(buf2);
	buffobj_del(buf3);

	return err;
}

uint32
reg_proto_process_m6(RegData *regInfo, BufferObj *msg)
{
	uint8 *Pos;
	WpsM6 m;
	uint32 err, tlv_err = 0;

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

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM6: %d byte message\n",
		msg->m_dataLength));

	/*
	 * First, deserialize (parse) the message.
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	if ((err = reg_proto_verCheck(&m.version, &m.msgType,
		WPS_ID_MESSAGE_M6, msg)) != WPS_SUCCESS)
		goto error;

	reg_msg_init(&m, WPS_ID_MESSAGE_M6);
	tlv_err |= tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE,
		msg, SIZE_128_BITS, 0);

	tlv_err |= tlv_encrSettingsParse(&m.encrSettings, msg);
	if (tlv_err) {
		err = RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* WSC 2.0, parse WFA vendor id and subelements from vendor extension attribute  */
	if (regInfo->p_enrolleeInfo->version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			/* Reuse buf1 */
			BufferObj *vendorExt_bufObj = buf1;

			buffobj_Reset(vendorExt_bufObj);

			/* Deserialize subelement */
			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* Get Version2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				tlv_err |= subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);
		}
		else {
			/* No WFA vendor extension, rewind theBuf to get Authenticator */
			buffobj_Rewind(msg);
		}
	}

	/* skip the other optional attributes until we get to the authenticator */
	while (WPS_ID_AUTHENTICATOR != buffobj_NextType(msg)) {
		/* advance past the TLV */
		Pos = buffobj_Advance(msg, sizeof(WpsTlvHdr) +
			WpsNtohs((msg->pCurrent+sizeof(uint16))));

		/*
		 * If Advance returned NULL, it means there's no more data in the
		 * buffer. This is an error.
		 */
		if (Pos == NULL) {
			err =  RPROT_ERR_REQD_TLV_MISSING;
			goto error;
		}
	}

	tlv_err |= tlv_dserialize(&m.authenticator, WPS_ID_AUTHENTICATOR,
		msg, SIZE_64_BITS, 0);
	if (tlv_err) {
		err = RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* Now start validating the message */

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
		err = RPROT_ERR_NONCE_MISMATCH;
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
			goto error;
		}
	}
	/* HMAC validation */

	/* extract encrypted settings */
	{
		BufferObj *cipherText = buf1_dser;
		BufferObj *iv = 0;
		BufferObj *plainText = 0;
		TlvEsNonce rNonce;

		buffobj_dserial(cipherText, m.encrSettings.ip_encryptedData,
			m.encrSettings.encrDataLength);
		iv = buf2_dser;
		buffobj_dserial(iv, m.encrSettings.iv, SIZE_128_BITS);
		plainText = buf1;
		buffobj_Reset(plainText);
		reg_proto_decrypt_data(cipherText, iv, regInfo->keyWrapKey,
			regInfo->authKey, plainText);
		reg_msg_nonce_parse(&rNonce, WPS_ID_R_SNONCE2, plainText, regInfo->authKey);

		/* extract encrypted settings */

		/* RHash2 validation */
		/* 1. Save RS2 */
		memcpy(regInfo->rs2, rNonce.nonce.m_data, rNonce.nonce.tlvbase.m_len);
	}

	/* 2. prepare the buffer */
	{
		uint8 hashBuf[SIZE_256_BITS];

		BufferObj *rhashBuf = buf1;
		buffobj_Reset(rhashBuf);
		buffobj_Append(rhashBuf, SIZE_128_BITS, regInfo->rs2);
		buffobj_Append(rhashBuf, SIZE_128_BITS, regInfo->psk2);
		buffobj_Append(rhashBuf, SIZE_PUB_KEY, regInfo->pke);
		buffobj_Append(rhashBuf, SIZE_PUB_KEY, regInfo->pkr);

		/* 3. generate the mac */
		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			rhashBuf->pBase, rhashBuf->m_dataLength, hashBuf, NULL);

		/* 4. compare the mac to rhash2 */
		if (memcmp(regInfo->rHash2, hashBuf, SIZE_256_BITS)) {
			TUTRACE((TUTRACE_ERR, "RPROTO: RS2 hash doesn't match RHash2\n"));
			err = RPROT_ERR_CRYPTO;
			goto error;
		}

		/* 5. Instead of steps 3 & 4, we could have called ValidateMac */
		/* RHash2 validation */
	}

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, msg->m_dataLength, msg->pBase);

	err = WPS_SUCCESS;
	TUTRACE((TUTRACE_INFO, "RPROTO: ProcessMessageM6 done: %d bytes\n",
		msg->m_dataLength));
error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "ProcessMessageM6: got error %x", err));
	buffobj_del(buf1);
	buffobj_del(buf1_dser);
	buffobj_del(buf2_dser);

	return err;
}

uint32
reg_proto_create_m7(RegData *regInfo, BufferObj *msg, void *encrSettings)
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
	message = WPS_ID_MESSAGE_M7;

	/* encrypted settings. */
	{
		CTlvEsM7Ap *apEs = 0;
		BufferObj *esBuf = buf1;
		BufferObj *cipherText = buf2;
		BufferObj *iv = buf3;
		buffobj_Reset(esBuf);
		buffobj_Reset(cipherText);
		buffobj_Reset(iv);

		if (regInfo->p_enrolleeInfo->b_ap) {
			if (!encrSettings) {
				TUTRACE((TUTRACE_ERR, "RPROTO: AP Encr settings are NULL\n"));
				err =  WPS_ERR_INVALID_PARAMETERS;
				goto error;
			}

			apEs = (CTlvEsM7Ap *)encrSettings;
			/* Set ES Nonce2 */
			tlv_set(&apEs->nonce, WPS_ID_E_SNONCE2, regInfo->es2, SIZE_128_BITS);

			if (regInfo->p_enrolleeInfo->version2 >= WPS_VERSION2) {
				/*
				 * WSC 2.0, (WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK)
				 * allowed only when both the Registrar and the Enrollee
				 * are using WSC 2.0 or newer.
				 * Use WPS_AUTHTYPE_WPA2PSK when enrollee is WSC 1.0 only.
				 */
				if (regInfo->p_registrarInfo->version2 < WPS_VERSION2)
					apEs->authType.m_data &= ~WPS_AUTHTYPE_WPAPSK;
			}

			reg_msg_m7ap_write(apEs, esBuf, regInfo->authKey);
		}
		else {
			TlvEsM7Enr *staEs;
			if (!encrSettings) {
				TUTRACE((TUTRACE_INFO, "RPROTO: NULL STA Encrypted settings."
					" Allocating memory...\n"));
				if ((staEs = reg_msg_m7enr_new()) == NULL) {
					err = WPS_ERR_OUTOFMEMORY;
					goto error;
				}

				/* Set ES Nonce2 */
				tlv_set(&staEs->nonce, WPS_ID_E_SNONCE2, regInfo->es2,
					SIZE_128_BITS);
				reg_msg_m7enr_write(staEs, esBuf, regInfo->authKey);

				/* Free local staEs */
				reg_msg_m7enr_del(staEs, 0);
			}
			else {
				staEs = (TlvEsM7Enr *)encrSettings;

				/* Set ES Nonce2 */
				tlv_set(&staEs->nonce, WPS_ID_E_SNONCE2, regInfo->es2,
					SIZE_128_BITS);
				reg_msg_m7enr_write(staEs, esBuf, regInfo->authKey);
			}
		}

		/* Now encrypt the serialize Encrypted settings buffer */

		reg_proto_encrypt_data(esBuf, regInfo->keyWrapKey,
			regInfo->authKey, cipherText, iv);

		/* Now assemble the message */
		tlv_serialize(WPS_ID_VERSION, msg, &regInfo->p_enrolleeInfo->version,
			WPS_ID_VERSION_S);
		tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
		tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, regInfo->registrarNonce, SIZE_128_BITS);
		{
			CTlvEncrSettings settings;
			settings.iv = iv->pBase;
			settings.ip_encryptedData = cipherText->pBase;
			settings.encrDataLength = cipherText->m_dataLength;
			tlv_encrSettingsWrite(&settings, msg);
		}

		if (regInfo->x509csr->m_dataLength) {
			tlv_serialize(WPS_ID_X509_CERT_REQ, msg, regInfo->x509csr->pBase,
				regInfo->x509csr->m_dataLength);
		}
	}

	/* WSC 2.0, add WFA vendor id and subelements to vendor extension attribute  */
	if (regInfo->p_enrolleeInfo->version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		BufferObj *vendorExt_bufObj = buffobj_new();

		if (vendorExt_bufObj == NULL) {
			err = WPS_ERR_OUTOFMEMORY;
			goto error;
		}

		/* Add settingsDelayTime vendorExt_bufObj */
		if (regInfo->p_enrolleeInfo->settingsDelayTime) {
			subtlv_serialize(WPS_WFA_SUBID_SETTINGS_DELAY_TIME, vendorExt_bufObj,
				&regInfo->p_enrolleeInfo->settingsDelayTime,
				WPS_WFA_SUBID_SETTINGS_DELAY_TIME_S);
		}

		/* Add Version2 vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&regInfo->p_enrolleeInfo->version2, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->p_enrolleeInfo->nattr_len)
			buffobj_Append(msg, regInfo->p_enrolleeInfo->nattr_len,
				(uint8*)regInfo->p_enrolleeInfo->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	/* Calculate the hmac */
	{
		uint8 hmac[SIZE_256_BITS];

		BufferObj *hmacData = buf1;
		buffobj_Reset(hmacData);
		buffobj_Append(hmacData, regInfo->inMsg->m_dataLength, regInfo->inMsg->pBase);
		buffobj_Append(hmacData, msg->m_dataLength, msg->pBase);

		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			hmacData->pBase, hmacData->m_dataLength, hmac, NULL);

		tlv_serialize(WPS_ID_AUTHENTICATOR, msg, hmac, SIZE_64_BITS);
	}

	/* Store the outgoing message  */
	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, msg->m_dataLength, msg->pBase);
	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM7 built: %d bytes\n",
		msg->m_dataLength));
	err = WPS_SUCCESS;
error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "BuildMessageM7 : got error %x", err));
	buffobj_del(buf1);
	buffobj_del(buf2);
	buffobj_del(buf3);

	return err;
}

uint32
reg_proto_process_m8(RegData *regInfo, BufferObj *msg, void **encrSettings)
{
	uint8 *Pos;
	WpsM8 m;
	uint32 err, tlv_err = 0;

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

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM8: %d byte message\n",
		msg->m_dataLength));

	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	if ((err = reg_proto_verCheck(&m.version, &m.msgType,
		WPS_ID_MESSAGE_M8, msg)) != WPS_SUCCESS)
		goto error;

	reg_msg_init(&m, WPS_ID_MESSAGE_M8);
	tlv_err |= tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE,
		msg, SIZE_128_BITS, 0);

	tlv_err |= tlv_encrSettingsParse(&m.encrSettings, msg);

	if (WPS_ID_X509_CERT == buffobj_NextType(msg))
		tlv_err |= tlv_dserialize(&m.x509Cert, WPS_ID_X509_CERT, msg, 0, 0);

	if (tlv_err) {
		err = RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* WSC 2.0, parse WFA vendor id and subelements from vendor extension attribute  */
	if (regInfo->p_enrolleeInfo->version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			/* Reuse buf1 */
			BufferObj *vendorExt_bufObj = buf1;

			buffobj_Reset(vendorExt_bufObj);

			/* Deserialize subelement */
			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* Get Version2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				tlv_err |= subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);
		}
		else {
			/* No WFA vendor extension, rewind theBuf to get Authenticator */
			buffobj_Rewind(msg);
		}
	}

	/* skip all optional attributes until we get to the authenticator */
	while (WPS_ID_AUTHENTICATOR != buffobj_NextType(msg)) {
		/* advance past the TLV */
		Pos = buffobj_Advance(msg, sizeof(WpsTlvHdr) +
			WpsNtohs((msg->pCurrent+sizeof(uint16))));

		/*
		 * If Advance returned NULL, it means there's no more data in the
		 * buffer. This is an error.
		 */
		if (Pos == NULL) {
			err =  RPROT_ERR_REQD_TLV_MISSING;
			goto error;
		}
	}

	tlv_err |= tlv_dserialize(&m.authenticator, WPS_ID_AUTHENTICATOR, msg, SIZE_64_BITS, 0);
	if (tlv_err) {
		err = RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* Now start validating the message */

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
		err = RPROT_ERR_NONCE_MISMATCH;
		goto error;
	}

	/* HMAC validation */
	{
		BufferObj *hmacData = buf1;
		buffobj_Reset(hmacData);
		/* append the last message sent */
		buffobj_Append(hmacData,  regInfo->outMsg->m_dataLength, regInfo->outMsg->pBase);
		/* append the current message. Don't append the last TLV (auth) */
		buffobj_Append(hmacData,
		        msg->m_dataLength-(sizeof(WpsTlvHdr)+m.authenticator.tlvbase.m_len),
		        msg->pBase);

		if (!reg_proto_validate_mac(hmacData, m.authenticator.m_data, regInfo->authKey)) {
			TUTRACE((TUTRACE_ERR, "RPROTO: HMAC validation failed\n"));
			err = RPROT_ERR_CRYPTO;
			goto error;
		}
	}
	/* HMAC validation */

	/* extract encrypted settings */
	{
		BufferObj *cipherText = buf1_dser;
		BufferObj *iv = 0;
		BufferObj *plainText = 0;
		buffobj_dserial(cipherText, m.encrSettings.ip_encryptedData,
			m.encrSettings.encrDataLength);
		iv = buf2_dser;
		buffobj_dserial(iv, m.encrSettings.iv, SIZE_128_BITS);
		plainText = buf1;
		buffobj_Reset(plainText);
		reg_proto_decrypt_data(cipherText, iv, regInfo->keyWrapKey,
			regInfo->authKey, plainText);

		if (regInfo->p_enrolleeInfo->b_ap) {
			CTlvEsM8Ap *esAP = reg_msg_m8ap_new();
			reg_msg_m8ap_parse(esAP, plainText, regInfo->authKey, true);
			*encrSettings = (void *)esAP;
		}
		else {
			CTlvEsM8Sta *esSta = reg_msg_m8sta_new();
			reg_msg_m8sta_parse(esSta, plainText, regInfo->authKey, true);
			*encrSettings = (void *)esSta;
		}

		if (regInfo->p_enrolleeInfo->version2 >= WPS_VERSION2) {
			/*
			 * WSC 2.0, check MAC attribute only when registrar
			 * and enrollee are both WSC 2.0
			 */
			if (regInfo->p_registrarInfo->version2 >= WPS_VERSION2 &&
				reg_proto_MacCheck(regInfo->p_enrolleeInfo, *encrSettings)
					!= WPS_SUCCESS) {
				err = RPROT_ERR_ROGUE_SUSPECTED;
				goto error;
			}

			/* WSC 2.0, test case 4.1.10 */
			err = reg_proto_EncrSettingsCheck(regInfo->p_enrolleeInfo, *encrSettings);
			if (err != WPS_SUCCESS)
				goto error;
		}
	}
	/* extract encrypted settings */

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, msg->m_dataLength, msg->pBase);
	err = WPS_SUCCESS;
	TUTRACE((TUTRACE_INFO, "RPROTO: ProcessMessageM8 done: %d bytes\n",
		msg->m_dataLength));
error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "ProcessMessageM8: got error %x", err));
	buffobj_del(buf1);
	buffobj_del(buf1_dser);
	buffobj_del(buf2_dser);

	return err;
}

uint32
reg_proto_create_ack(RegData *regInfo, BufferObj *msg, uint8 wps_version2, uint8 *regNonce)
{
	uint8 data8;
	uint8 *R_Nonce = regNonce ? regNonce : regInfo->registrarNonce;

	TUTRACE((TUTRACE_INFO, "RPROTO: In BuildMessageAck\n"));

	data8 = WPS_VERSION;
	tlv_serialize(WPS_ID_VERSION, msg, &data8, WPS_ID_VERSION_S);

	data8 = WPS_ID_MESSAGE_ACK;
	tlv_serialize(WPS_ID_MSG_TYPE, msg, &data8, WPS_ID_MSG_TYPE_S);

	tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, R_Nonce, SIZE_128_BITS);

	/* WSC 2.0 */
	if (wps_version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		BufferObj *vendorExt_bufObj = buffobj_new();

		if (vendorExt_bufObj == NULL)
			return WPS_ERR_OUTOFMEMORY;

		data8 = wps_version2;

		/* Add Version2 vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&data8, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->p_enrolleeInfo->nattr_len)
			buffobj_Append(msg, regInfo->p_enrolleeInfo->nattr_len,
				(uint8*)regInfo->p_enrolleeInfo->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageAck built: %d bytes\n",
		msg->m_dataLength));

	return WPS_SUCCESS;
}

uint32
reg_proto_process_ack(RegData *regInfo, BufferObj *msg, uint8 wps_version2)
{
	WpsACK m;
	int err;

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageAck: %d byte message\n",
		msg->m_dataLength));

	/* First, deserialize (parse) the message. */

	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	if ((err = reg_proto_verCheck(&m.version, &m.msgType,
		WPS_ID_MESSAGE_ACK, msg)) != WPS_SUCCESS)
	    return err;

	reg_msg_init(&m, WPS_ID_MESSAGE_ACK);
	tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE, msg, SIZE_128_BITS, 0);
	tlv_dserialize(&m.registrarNonce, WPS_ID_REGISTRAR_NONCE, msg, SIZE_128_BITS, 0);

	if (wps_version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			BufferObj *vendorExt_bufObj = buffobj_new();

			if (vendorExt_bufObj == NULL)
				return WPS_ERR_OUTOFMEMORY;

			/* Deserialize subelement */
			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* Get Version2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);
		}
	}

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
		return RPROT_ERR_NONCE_MISMATCH;
	}

	/* confirm the registrar nonce */
	if (memcmp(regInfo->registrarNonce, m.registrarNonce.m_data,
		m.registrarNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect registrar nonce\n"));
		return RPROT_ERR_NONCE_MISMATCH;
	}

	/* ignore any other TLVs */

	return WPS_SUCCESS;
}

uint32
reg_proto_create_nack(RegData *regInfo, BufferObj *msg, uint16 configError,
	uint8 wps_version2, uint8 *regNonce)
{
	uint8 data8;
	uint8 *R_Nonce = regNonce ? regNonce : regInfo->registrarNonce;

	TUTRACE((TUTRACE_INFO, "RPROTO: In BuildMessageNack\n"));

	data8 = WPS_VERSION;
	tlv_serialize(WPS_ID_VERSION, msg, &data8, WPS_ID_VERSION_S);

	data8 = WPS_ID_MESSAGE_NACK;
	tlv_serialize(WPS_ID_MSG_TYPE, msg, &data8, WPS_ID_MSG_TYPE_S);

	tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, R_Nonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_CONFIG_ERROR, msg, &configError, WPS_ID_CONFIG_ERROR_S);

	/* WSC 2.0 */
	if (wps_version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		BufferObj *vendorExt_bufObj = buffobj_new();

		if (vendorExt_bufObj == NULL)
			return WPS_ERR_OUTOFMEMORY;

		data8 = wps_version2;

		/* Add Version2 vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&data8, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->p_enrolleeInfo->nattr_len)
			buffobj_Append(msg, regInfo->p_enrolleeInfo->nattr_len,
				(uint8*)regInfo->p_enrolleeInfo->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	return WPS_SUCCESS;
}

uint32
reg_proto_process_nack(RegData *regInfo, BufferObj *msg, uint16 *configError,
	uint8 wps_version2)
{
	WpsNACK m;
	int err;

	/* First, deserialize (parse) the message. */

	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageNack: %d byte message\n",
		msg->m_dataLength));
	if ((err = reg_proto_verCheck(&m.version, &m.msgType,
		WPS_ID_MESSAGE_NACK, msg)) != WPS_SUCCESS)
		return err;

	reg_msg_init(&m, WPS_ID_MESSAGE_NACK);
	tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE, msg, SIZE_128_BITS, 0);
	tlv_dserialize(&m.registrarNonce, WPS_ID_REGISTRAR_NONCE, msg, SIZE_128_BITS, 0);
	tlv_dserialize(&m.configError, WPS_ID_CONFIG_ERROR, msg, 0, 0);

	if (wps_version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			BufferObj *vendorExt_bufObj = buffobj_new();

			if (vendorExt_bufObj == NULL)
				return WPS_ERR_OUTOFMEMORY;

			/* Deserialize subelement */
			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* Get Version2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);
		}
	}

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
		return  RPROT_ERR_NONCE_MISMATCH;
	}

	/* confirm the registrar nonce */
	if (memcmp(regInfo->registrarNonce, m.registrarNonce.m_data,
		m.registrarNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect registrar nonce\n"));
		return  RPROT_ERR_NONCE_MISMATCH;
	}

	/* ignore any other TLVs */

	*configError = m.configError.m_data;

	return WPS_SUCCESS;
}

uint32
reg_proto_create_done(RegData *regInfo, BufferObj *msg, uint8 wps_version2)
{
	uint8 data8;

	TUTRACE((TUTRACE_INFO, "RPROTO: In BuildMessageDone\n"));

	data8 = WPS_VERSION;
	tlv_serialize(WPS_ID_VERSION, msg, &data8, WPS_ID_VERSION_S);

	data8 = WPS_ID_MESSAGE_DONE;
	tlv_serialize(WPS_ID_MSG_TYPE, msg, &data8, WPS_ID_MSG_TYPE_S);

	tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, regInfo->registrarNonce, SIZE_128_BITS);

	/* WSC 2.0 */
	if (wps_version2 >= WPS_VERSION2) {
		CTlvVendorExt vendorExt;
		BufferObj *vendorExt_bufObj = buffobj_new();

		if (vendorExt_bufObj == NULL)
			return WPS_ERR_OUTOFMEMORY;

		data8 = wps_version2;

		/* Add Version2 vendorExt_bufObj */
		subtlv_serialize(WPS_WFA_SUBID_VERSION2, vendorExt_bufObj,
			&data8, WPS_WFA_SUBID_VERSION2_S);

		/* Serialize subelemetns to Vendor Extension */
		vendorExt.vendorId = (uint8 *)WFA_VENDOR_EXT_ID;
		vendorExt.vendorData = buffobj_GetBuf(vendorExt_bufObj);
		vendorExt.dataLength = buffobj_Length(vendorExt_bufObj);
		tlv_vendorExtWrite(&vendorExt, msg);

		buffobj_del(vendorExt_bufObj);
#ifdef WFA_WPS_20_TESTBED
		/* New unknown attribute */
		if (regInfo->p_enrolleeInfo->nattr_len)
			buffobj_Append(msg, regInfo->p_enrolleeInfo->nattr_len,
				(uint8*)regInfo->p_enrolleeInfo->nattr_tlv);
#endif /* WFA_WPS_20_TESTBED */
	}

	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageDone built: %d bytes\n",
		msg->m_dataLength));

	return WPS_SUCCESS;
}

uint32
reg_proto_process_done(RegData *regInfo, BufferObj *msg, uint8 wps_version2)
{
	WpsDone m;
	int err;

	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageDone: %d byte message\n",
		msg->m_dataLength));

	if ((err = reg_proto_verCheck(&m.version, &m.msgType,
		WPS_ID_MESSAGE_DONE, msg)) != WPS_SUCCESS)
		return err;

	reg_msg_init(&m, WPS_ID_MESSAGE_DONE);
	tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE, msg, SIZE_128_BITS, 0);
	tlv_dserialize(&m.registrarNonce, WPS_ID_REGISTRAR_NONCE, msg, SIZE_128_BITS, 0);

	if (wps_version2 >= WPS_VERSION2) {
		/* Check subelement in WFA Vendor Extension */
		if (tlv_find_vendorExtParse(&m.vendorExt, msg, (uint8 *)WFA_VENDOR_EXT_ID) == 0) {
			BufferObj *vendorExt_bufObj = buffobj_new();

			if (vendorExt_bufObj == NULL)
				return WPS_ERR_OUTOFMEMORY;

			/* Deserialize subelement */
			buffobj_dserial(vendorExt_bufObj, m.vendorExt.vendorData,
				m.vendorExt.dataLength);

			/* Get Version2 subelement */
			if (WPS_WFA_SUBID_VERSION2 == buffobj_NextSubId(vendorExt_bufObj))
				subtlv_dserialize(&m.version2, WPS_WFA_SUBID_VERSION2,
					vendorExt_bufObj, 0, 0);
		}
	}

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
		return RPROT_ERR_NONCE_MISMATCH;
	}

	/* confirm the registrar nonce */
	if (memcmp(regInfo->registrarNonce, m.registrarNonce.m_data,
		m.registrarNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect registrar nonce\n"));
		return RPROT_ERR_NONCE_MISMATCH;
	}

	return WPS_SUCCESS;
}
