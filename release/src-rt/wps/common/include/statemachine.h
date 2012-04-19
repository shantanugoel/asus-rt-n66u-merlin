/*
 * State Machine
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: statemachine.h 287159 2011-09-30 07:19:20Z kenlo $
 */

#ifndef _REGISTRAR_SM_
#define _REGISTRAR_SM_


#ifdef __cplusplus
extern "C" {
#endif

#if defined(__linux__) || defined(__ECOS)
	#define stringPrintf snprintf
#else
	#define stringPrintf _snprintf
#endif

#define WPS_PROTOCOL_TIMEOUT 120
#define WPS_MESSAGE_TIMEOUT 25


/* M2D Status values */
#define SM_AWAIT_M2     0
#define SM_RECVD_M2     1
#define SM_RECVD_M2D    2
#define SM_M2D_RESET    3

/* State Machine operating modes */
#define MODE_REGISTRAR 1
#define MODE_ENROLLEE  2

typedef struct {
	CRegProtocol *mpc_regProt;
	TRANSPORT_TYPE m_transportType; /* transport type to use for the next message */
	uint32 m_cbThread;

	/* flag to indicate whether the SM has been initialized */
	bool m_initialized;

	/* registration protocol data is stored here */
	RegData *mps_regData;

	/* info for local registrar */
	DevInfo *mps_localInfo;

	/* lock for accessing the registration info struct */
	uint32 *mp_infoLock;

	/* Diffie Hellman key pair */
	DH *mp_dhKeyPair; /* this is copied into mps_regData */

	/* operating mode - Enrollee or Registrar */
	uint32 m_mode;

	/* PHIL : "user" of the sm. Either RegSM or EnrSM depending on mode  */
	void *m_user;
	void *g_mc;
} CStateMachine;

uint32 state_machine_dup_devinfo(DevInfo *inINfo, DevInfo **outInfo);
uint32 state_machine_send_ack(CStateMachine *sm, BufferObj *outmsg, uint8 *regNonce);
uint32 state_machine_send_nack(CStateMachine *sm, uint16 configError, BufferObj *outmsg,
	uint8 *regNonce);
uint32 state_machine_send_done(CStateMachine *sm, BufferObj *outmsg);
CStateMachine *state_machine_new(void *g_mc, CRegProtocol *pc_regProt,
	uint32 operatingMode);
void state_machine_delete(CStateMachine *sm);

/* callback function for CTransport to call in */
void state_machine_ActualCBProc(CStateMachine *sm, void *p_message);
uint32 state_machine_set_local_devinfo(CStateMachine *sm, DevInfo *p_localInfo,
	uint32 *p_lock, DH *p_dhKeyPair);
uint32 state_machine_update_devinfo(CStateMachine *sm, DevInfo *p_localInfo);
uint32 state_machine_init_sm(CStateMachine *sm);
void state_machine_set_passwd(CStateMachine *sm, char *p_devicePasswd,
	uint32 passwdLength);
void state_machine_set_encrypt(CStateMachine *sm, void * p_StaEncrSettings,
	void * p_ApEncrSettings);
void state_machine_set_enrolleeNonce(CStateMachine *sm, uint8 *p_enrolleeNonce);

typedef struct {
	CStateMachine *m_sm;

	uint32 *mp_ptimerLock;
	uint32 m_ptimerStatus;

	/* The peer's encrypted settings will be stored here */
	void *mp_peerEncrSettings;
	bool m_localSMEnabled;
	bool m_passThruEnabled;

	/* Temporary state variable */
	bool m_sentM2;
	uint32 m_protocoltimerThrdId;
	void *g_mc;

	/* External Registrar variables */
	bool m_er_sentM2;
	uint8 m_er_nonce[SIZE_128_BITS];

	uint32 err_code; /* Real err code, initially used for RPROT_ERR_INCOMPATIBLE_WEP */
} RegSM;

RegSM *reg_sm_new(void *g_mc, CRegProtocol *pc_regProt);
void reg_sm_StaticCallbackProc(void *g_mc, void *p_callbackMsg, void *cookie);
void reg_sm_delete(RegSM *r);
uint32 reg_sm_initsm(RegSM *r, DevInfo *p_enrolleeInfo, bool enableLocalSM, bool enablePassthru);
uint32 reg_sm_step(RegSM *r, uint32 msgLen, uint8 *p_msg, uint8 *outbuffer, uint32 *out_len);
uint32 reg_sm_get_devinfo_step(RegSM *r); /* brcm */
void reg_sm_restartsm(RegSM *r);
uint32 reg_sm_handleMessage(RegSM *r, BufferObj *msg, BufferObj *outmsg);
int reg_proto_verCheck(TlvObj_uint8 *version, TlvObj_uint8 *msgType, uint8 msgId, BufferObj *msg);

typedef struct {
	CStateMachine *m_sm;

	uint32 *mp_m2dLock;
	uint32 m_m2dStatus;
	uint32 *mp_ptimerLock_enr;
	uint32 m_ptimerStatus_enr;
	uint32 m_timerThrdId;

	/* The peer's encrypted settings will be stored here */
	void *mp_peerEncrSettings;
	void *g_mc;

	uint32 err_code; /* Real err code, initially used for RPROT_ERR_INCOMPATIBLE_WEP */
} EnrSM;

uint32 enr_sm_step(EnrSM *e, uint32 msgLen, uint8 *p_msg, uint8 *outbuffer, uint32 *out_len);
EnrSM * enr_sm_new(void *g_mc, CRegProtocol *pc_regProt);
void enr_sm_delete(EnrSM *e);
uint32 enr_sm_initializeSM(EnrSM *e, DevInfo *p_registrarInfo, void * p_StaEncrSettings,
	void * p_ApEncrSettings, char *p_devicePasswd, uint32 passwdLength);
void enr_sm_restartSM(EnrSM *e);
uint32 enr_sm_handleMessage(EnrSM *e, BufferObj *msg, BufferObj *outmsg);

#ifdef __cplusplus
}
#endif


#endif /* _REGISTRAR_SM_ */
