/*
 * State machine infomation
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: sminfo.h 287159 2011-09-30 07:19:20Z kenlo $
 */

#ifndef _SM_INFO_H
#define _SM_INFO_H


/* data structures for each instance of registration protocol */
typedef enum {
	START = 0,
	CONTINUE,
	RESTART,
	SUCCESS,
	FAILURE
} ESMState;

typedef enum {
	MSTART = 0,
	M1,
	M2,
	M2D,
	M3,
	M4,
	M5,
	M6,
	M7,
	M8,
	DONE,
	MNONE = 99
} EMsg;

/*
 * data structure to store info about a particular instance
 * of the Registration protocol
 */
typedef struct {
	ESMState e_smState;
	EMsg e_lastMsgRecd;
	EMsg e_lastMsgSent;

	/* TODO: must store previous message as well to compute hash */

	/* enrollee endpoint - filled in by the Registrar, NULL for Enrollee */
	DevInfo *p_enrolleeInfo;

	/* Registrar endpoint - filled in by the Enrollee, NULL for Registrar */
	DevInfo *p_registrarInfo;

	/* Diffie Hellman parameters */
	BIGNUM *DH_PubKey_Peer; /* peer's pub key stored in bignum format */
	DH *DHSecret; /* local key pair in bignum format */
	uint8 pke[SIZE_PUB_KEY]; /* enrollee's raw pub key */
	uint8 pkr[SIZE_PUB_KEY]; /* registrar's raw pub key */

	BufferObj *password;
	void *staEncrSettings; /* to be sent in M2/M8 by reg & M7 by enrollee */
	void *apEncrSettings;
	uint16 enrolleePwdId;
	uint8 enrolleeNonce[SIZE_128_BITS]; /* N1 */
	uint8 registrarNonce[SIZE_128_BITS]; /* N2 */

	uint8 psk1[SIZE_128_BITS];
	uint8 psk2[SIZE_128_BITS];

	uint8 eHash1[SIZE_256_BITS];
	uint8 eHash2[SIZE_256_BITS];
	uint8 es1[SIZE_128_BITS];
	uint8 es2[SIZE_128_BITS];

	uint8 rHash1[SIZE_256_BITS];
	uint8 rHash2[SIZE_256_BITS];
	uint8 rs1[SIZE_128_BITS];
	uint8 rs2[SIZE_128_BITS];

	BufferObj *authKey;
	BufferObj *keyWrapKey;
	BufferObj *emsk;
	BufferObj *x509csr;
	BufferObj *x509Cert;

	BufferObj *inMsg; /* A recd msg will be stored here */
	BufferObj *outMsg; /* Contains msg to be transmitted */
} RegData;

#endif /* _SM_INFO_H */
