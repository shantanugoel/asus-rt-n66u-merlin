/*
 * Registrataion protocol messages
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: reg_proto_msg.c 274160 2011-07-28 10:52:54Z kenlo $
 */

#include <wps_dh.h>
#include <wps_sha256.h>

#include <slist.h>
#include <tutrace.h>
#include <wpstypes.h>
#include <wpscommon.h>
#include <wpserror.h>
#include <reg_protomsg.h>


/*
 * Init messages based on their types. Mainly use for structures including
 * complex tlv's like EncrSettings.
 */
void
reg_msg_init(void *m, int type)
{
	switch (type) {
		case WPS_ID_MESSAGE_M1:
			memset(m, 0, sizeof(WpsM1));
		break;
		case WPS_ID_MESSAGE_M2:
			memset(m, 0, sizeof(WpsM2));
		break;
		case WPS_ID_MESSAGE_M2D:
			memset(m, 0, sizeof(WpsM2D));
		break;
		case WPS_ID_MESSAGE_M3:
			memset(m, 0, sizeof(WpsM3));
		break;
		case WPS_ID_MESSAGE_M4:
			memset(m, 0, sizeof(WpsM4));
		break;
		case WPS_ID_MESSAGE_M5:
			memset(m, 0, sizeof(WpsM5));
		break;
		case WPS_ID_MESSAGE_M6:
			memset(m, 0, sizeof(WpsM6));
		break;
		case WPS_ID_MESSAGE_M7:
			memset(m, 0, sizeof(WpsM7));
		break;
		case WPS_ID_MESSAGE_M8:
			memset(m, 0, sizeof(WpsM8));
		break;
		case WPS_ID_MESSAGE_ACK:
			memset(m, 0, sizeof(WpsACK));
		break;
		case WPS_ID_MESSAGE_NACK:
			memset(m, 0, sizeof(WpsNACK));
		break;
		case WPS_ID_MESSAGE_DONE:
			memset(m, 0, sizeof(WpsDone));
			break;
	}
}

/* Encrypted settings for M4, M5, M6 */
void
reg_msg_nonce_parse(TlvEsNonce *t, uint16 theType, BufferObj *theBuf, BufferObj *authKey)
{
	uint8 dataMac[BUF_SIZE_256_BITS];
	uint8 hmac_len;

	tlv_dserialize(&t->nonce, theType, theBuf, SIZE_128_BITS, 0);

	/* Skip attributes until the KeyWrapAuthenticator */
	while (WPS_ID_KEY_WRAP_AUTH != buffobj_NextType(theBuf)) {
		/* advance past the TLV */
		uint8 *Pos = buffobj_Advance(theBuf, sizeof(WpsTlvHdr) +
			WpsNtohs((theBuf->pCurrent+sizeof(uint16))));

		 if (Pos == NULL)
			return;
	}

	/* save the length deserialized so far */
	hmac_len = (uint8)(theBuf->pCurrent - theBuf->pBase);
	tlv_dserialize(&t->keyWrapAuth, WPS_ID_KEY_WRAP_AUTH, theBuf, SIZE_64_BITS, 0);

	/* calculate the hmac of the data (data only, not the last auth TLV) */
	hmac_sha256(authKey->pBase, SIZE_256_BITS, theBuf->pBase,
		hmac_len, dataMac, NULL);

	/* compare it against the received hmac */
	if (memcmp(dataMac, t->keyWrapAuth.m_data, SIZE_64_BITS) != 0) {
		TUTRACE((TUTRACE_ERR, "RPROTO: HMAC results don't match\n"));
		return;
	}
}

void
reg_msg_nonce_write(TlvEsNonce *t, BufferObj *theBuf, BufferObj *authKey)
{
	uint8 hmac[SIZE_256_BITS];

	tlv_write(&t->nonce, theBuf);

	/* calculate the hmac and append the TLV to the buffer */
	hmac_sha256(authKey->pBase, SIZE_256_BITS,
		theBuf->pBase, theBuf->m_dataLength, hmac, NULL);

	tlv_serialize(WPS_ID_KEY_WRAP_AUTH, theBuf, hmac,
		SIZE_64_BITS); /* Only the first 64 bits are sent */
}

TlvEsM7Enr *
reg_msg_m7enr_new()
{
	/* Allocate extra sizeof(int) bytes in head to store type M7ENR */
	int *es_type = (int *)malloc(sizeof(TlvEsM7Enr) + sizeof(int));
	TlvEsM7Enr *tlv = NULL;

	if (!es_type)
		goto error;

	*es_type = ES_TYPE_M7ENR;

	tlv = (TlvEsM7Enr *)(es_type + 1);
	memset((char *) tlv, 0, sizeof(TlvEsM7Enr));

	tlv->idProof.m_allocated = false;
	tlv->idProof.tlvbase.m_len = 0;
	tlv->nonce.m_allocated = false;
	tlv->nonce.tlvbase.m_len = 0;
	tlv->keyWrapAuth.m_allocated = false;
	tlv->keyWrapAuth.tlvbase.m_len = 0;

	return tlv;

error:
	TUTRACE((TUTRACE_ERR, "reg_msg_m7enr_new : out of memory\n"));
	return NULL;
}

void
reg_msg_m7enr_del(TlvEsM7Enr *t, bool content_only)
{
	int *es_type = (int *)t;

	/* Move to allocated memory real head */
	es_type--;

	tlv_delete(&t->nonce, 1);
	tlv_delete(&t->idProof, 1);
	tlv_delete(&t->keyWrapAuth, 1);

	if (!content_only)
		free(es_type);
}

/* Encrypted settings for M7 ES when M7 is from an enrollee */
uint32
reg_msg_m7enr_parse(TlvEsM7Enr *t, BufferObj *theBuf, BufferObj *authKey, bool allocate)
{
	uint8 dataMac[BUF_SIZE_256_BITS];
	uint8 hmac_len = 0;
	uint32 err;

	err = tlv_dserialize(&t->nonce, WPS_ID_E_SNONCE2, theBuf, SIZE_128_BITS, 0);

	if (WPS_ID_IDENTITY_PROOF == buffobj_NextType(theBuf)) {
		err |= tlv_dserialize(&t->idProof, WPS_ID_IDENTITY_PROOF, theBuf, 0, allocate);
	}

	/* Skip attributes until the KeyWrapAuthenticator */
	while (WPS_ID_KEY_WRAP_AUTH != buffobj_NextType(theBuf)) {
		/* advance past the TLV */
		uint8 *Pos = buffobj_Advance(theBuf, sizeof(WpsTlvHdr) +
			WpsNtohs((theBuf->pCurrent+sizeof(uint16))));
		if (Pos == NULL)
			return RPROT_ERR_REQD_TLV_MISSING;
	}

	/* save the length deserialized so far */
	hmac_len = (uint8)(theBuf->pCurrent - theBuf->pBase);

	err |= tlv_dserialize(&t->keyWrapAuth, WPS_ID_KEY_WRAP_AUTH, theBuf, SIZE_64_BITS, 0);

	/* validate the mac */

	/* calculate the hmac of the data (data only, not the last auth TLV) */
	hmac_sha256(authKey->pBase, SIZE_256_BITS, theBuf->pBase,
		hmac_len, dataMac, NULL);

	/* compare it against the received hmac */
	if (memcmp(dataMac, t->keyWrapAuth.m_data, SIZE_64_BITS) != 0) {
		TUTRACE((TUTRACE_ERR, "RPROTO: HMAC results don't match\n"));
		return RPROT_ERR_CRYPTO;
	}

	if (err)
		return WPS_ERR_GENERIC;
	else
		return WPS_SUCCESS;
}

void
reg_msg_m7enr_write(TlvEsM7Enr *t, BufferObj *theBuf, BufferObj *authKey)
{
	uint8 hmac[SIZE_256_BITS];

	tlv_write(&t->nonce, theBuf);
	if (t->idProof.tlvbase.m_len) {
		tlv_write(&t->idProof, theBuf);
	}

	/* calculate the hmac and append the TLV to the buffer */
	hmac_sha256(authKey->pBase, SIZE_256_BITS,
		theBuf->pBase, theBuf->m_dataLength, hmac, NULL);
	tlv_serialize(WPS_ID_KEY_WRAP_AUTH, theBuf, hmac,
		SIZE_64_BITS);
}

/* ES when M7 is from an AP */
CTlvEsM7Ap *
reg_msg_m7ap_new()
{
	/* Allocate extra sizeof(int) bytes in head to store type M7AP */
	int *es_type = (int *)malloc(sizeof(CTlvEsM7Ap) + sizeof(int));
	CTlvEsM7Ap *tlv = NULL;

	if (!es_type)
		goto error;

	*es_type = ES_TYPE_M7AP;

	tlv = (CTlvEsM7Ap *)(es_type + 1);
	memset((char *) tlv, 0, sizeof(CTlvEsM7Ap));

	tlv->nwKeyIndex = list_create();
	if (!tlv->nwKeyIndex)
		goto error;

	tlv->nwKey = list_create();
	if (!tlv->nwKey)
		goto error;

	tlv->macAddr.m_allocated = false;
	tlv->ssid.m_allocated = false;
	tlv->nonce.m_allocated = false;
	tlv->keyWrapAuth.m_allocated = false;
	tlv->wepIdx.tlvbase.m_len = 0;

	return tlv;
error:
	TUTRACE((TUTRACE_ERR, "reg_msg_m7ap_new : out of memory\n"));
	if (tlv) {
		if (tlv->nwKeyIndex)
			list_delete(tlv->nwKeyIndex);
		if (tlv->nwKey)
			list_delete(tlv->nwKey);
		free(es_type);
	}
	return NULL;
}

void
reg_msg_m7ap_del(CTlvEsM7Ap *tlv, bool content_only)
{
	LISTITR m_itr;
	LPLISTITR itr;
	CTlvNwKeyIndex *keyIndex;
	CTlvNwKey *key;
	int *es_type = (int *)tlv;

	/* Move to allocated memory real head */
	es_type--;

	itr = list_itrFirst(tlv->nwKeyIndex, &m_itr);

	while ((keyIndex = (CTlvNwKeyIndex *)list_itrGetNext(itr))) {
		tlv_delete((tlvbase_s *)keyIndex, 0);
	}

	list_delete(tlv->nwKeyIndex);

	itr = list_itrFirst(tlv->nwKey, &m_itr);

	while ((key = (CTlvNwKey *)list_itrGetNext(itr))) {
		tlv_delete((tlvbase_s *)key, 0);
	}

	list_delete(tlv->nwKey);

	tlv_delete(&tlv->macAddr, 1);
	tlv_delete(&tlv->ssid, 1);
	if (!content_only)
		free(es_type);
	return;
}

uint32
reg_msg_m7ap_parse(CTlvEsM7Ap *tlv, BufferObj *theBuf, BufferObj *authKey, bool allocate)
{
	CTlvNwKeyIndex *keyIndex;
	CTlvNwKey *key;
	uint8 dataMac[BUF_SIZE_256_BITS];
	uint8 hmac_len = 0;
	int err = 0;

	tlv_dserialize(&tlv->nonce, WPS_ID_E_SNONCE2, theBuf, SIZE_128_BITS, 0);
	tlv_dserialize(&tlv->ssid, WPS_ID_SSID, theBuf, SIZE_32_BYTES, allocate);
	tlv_dserialize(&tlv->macAddr, WPS_ID_MAC_ADDR, theBuf, SIZE_6_BYTES, allocate);
	tlv_dserialize(&tlv->authType, WPS_ID_AUTH_TYPE, theBuf, 0, 0);
	tlv_dserialize(&tlv->encrType, WPS_ID_ENCR_TYPE, theBuf, 0, 0);

	/* The next field is network key index. There are two possibilities: */
	/* 1. The TLV is omitted, in which case, there is only 1 network key */
	/* 2. The TLV is present, in which case, there may be 1 or more network keys */

	/* condition 1. If the next field is a network Key, the index TLV was omitted */
	if (WPS_ID_NW_KEY == buffobj_NextType(theBuf)) {
		key = (CTlvNwKey *)tlv_new(WPS_ID_NW_KEY);
		if (!key)
			return WPS_ERR_OUTOFMEMORY;
		err |= tlv_dserialize(key, WPS_ID_NW_KEY, theBuf, SIZE_64_BYTES, allocate);
		if (!list_addItem(tlv->nwKey, key)) {
			tlv_delete(key, 0);
			return WPS_ERR_OUTOFMEMORY;
		}
	}
	else {
		/* condition 2. all other possibities are illegal & will be caught later */
		while (WPS_ID_NW_KEY_INDEX == buffobj_NextType(theBuf)) {
			keyIndex = (CTlvNwKeyIndex *)tlv_new(WPS_ID_NW_KEY_INDEX);
			if (!keyIndex)
				return WPS_ERR_OUTOFMEMORY;
			err |= tlv_dserialize(keyIndex, WPS_ID_NW_KEY_INDEX, theBuf, 0, 0);
			if (!list_addItem(tlv->nwKeyIndex, keyIndex)) {
				tlv_delete(keyIndex, 0);
				return WPS_ERR_OUTOFMEMORY;
			}

			key = (CTlvNwKey *)tlv_new(WPS_ID_NW_KEY);
			if (!key)
				return WPS_ERR_OUTOFMEMORY;
			err |= tlv_dserialize(key, WPS_ID_NW_KEY, theBuf, SIZE_64_BYTES, allocate);
			if (!list_addItem(tlv->nwKey, key)) {
				tlv_delete(key, 0);
				return WPS_ERR_OUTOFMEMORY;
			}
		}
	}

	/* Set wep index default value to index 1 */
	tlv_set(&tlv->wepIdx, WPS_ID_WEP_TRANSMIT_KEY, (void *)1, 0);

	/* Skip attributes until the KeyWrapAuthenticator */
	while (WPS_ID_KEY_WRAP_AUTH != buffobj_NextType(theBuf)) {
		if (WPS_ID_WEP_TRANSMIT_KEY == buffobj_NextType(theBuf)) {
			tlv_dserialize(&tlv->wepIdx, WPS_ID_WEP_TRANSMIT_KEY, theBuf, 0, 0);
			TUTRACE((TUTRACE_ERR, "CTlvEsM7Ap::parse : deserialize wepkeyIndex "
				"%d allocate: %d\n", tlv->wepIdx.m_data, 0));
		}
		else {
			/* advance past the TLV */
			uint8 *Pos = buffobj_Advance(theBuf, sizeof(WpsTlvHdr) +
			WpsNtohs((theBuf->pCurrent+sizeof(uint16))));
			if (Pos == NULL)
				return RPROT_ERR_REQD_TLV_MISSING;
		}
	}

	/* save the length deserialized so far */
	hmac_len = (uint8)(theBuf->pCurrent - theBuf->pBase);

	err |= tlv_dserialize(&tlv->keyWrapAuth, WPS_ID_KEY_WRAP_AUTH, theBuf, SIZE_64_BITS, 0);

	/* validate the mac */

	/* calculate the hmac of the data (data only, not the last auth TLV) */
	hmac_sha256(authKey->pBase, SIZE_256_BITS, theBuf->pBase,
		hmac_len, dataMac, NULL);

	/* compare it against the received hmac */
	if (memcmp(dataMac, tlv->keyWrapAuth.m_data, SIZE_64_BITS) != 0) {
		TUTRACE((TUTRACE_ERR, "RPROTO: HMAC results don't match\n"));
		return RPROT_ERR_CRYPTO;
	}

	if (err)
		return WPS_ERR_GENERIC;
	else
		return WPS_SUCCESS;
}

void
reg_msg_m7ap_write(CTlvEsM7Ap *tlv, BufferObj *theBuf, BufferObj *authKey)
{
	LISTITR m_indexItr, m_keyItr;
	LPLISTITR indexItr = NULL, keyItr = NULL;
	CTlvNwKeyIndex *keyIndex;
	CTlvNwKey *key;
	uint8 hmac[SIZE_256_BITS];

	indexItr = list_itrFirst(tlv->nwKeyIndex, &m_indexItr);

	keyItr = list_itrFirst(tlv->nwKey, &m_keyItr);

	tlv_write(&tlv->nonce, theBuf);
	tlv_write(&tlv->ssid, theBuf);
	tlv_write(&tlv->macAddr, theBuf);
	tlv_write(&tlv->authType, theBuf);
	tlv_write(&tlv->encrType, theBuf);

	/* write the network index and network key to the buffer */
	if (list_getCount(tlv->nwKeyIndex) == 0) {
		/* Condition1. There is no key index, so there can only be 1 nw key */
		if (!(key = (CTlvNwKey *) list_itrGetNext(keyItr))) {
			if (!(key = (CTlvNwKey *) list_itrGetNext(keyItr))) {
				TUTRACE((TUTRACE_ERR, "reg_msg_m7ap_write : out of memory\n"));
				goto done;
			}
		}
		tlv_write(key, theBuf);
	}
	else {
		/* Condition2. There are multiple network keys. */
		while ((keyIndex = (CTlvNwKeyIndex *) list_itrGetNext(indexItr))) {
			if (!(key = (CTlvNwKey *) list_itrGetNext(keyItr))) {
				TUTRACE((TUTRACE_ERR, "reg_msg_m7ap_write : out of memory\n"));
				goto done;
			}
			tlv_write(keyIndex, theBuf);
			tlv_write(key, theBuf);
		}
	}

	if (tlv->wepIdx.tlvbase.m_len && tlv->wepIdx.m_data != 1) {
		tlv_write(&tlv->wepIdx, theBuf);
		TUTRACE((TUTRACE_ERR, "write wep index!! index = %x\n",
			tlv->wepIdx.m_data));
	}

	/* calculate the hmac and append the TLV to the buffer */
	hmac_sha256(authKey->pBase, SIZE_256_BITS,
		theBuf->pBase, theBuf->m_dataLength, hmac, NULL);
	tlv_serialize(WPS_ID_KEY_WRAP_AUTH, theBuf, hmac,
		SIZE_64_BITS);

done:
	return;
}

/* Encrypted settings for M8, ES when M8 is from an AP */
CTlvEsM8Ap *
reg_msg_m8ap_new()
{
	/* Allocate extra 4 bytes in head to store type M8AP */
	int *es_type = (int *)malloc(sizeof(CTlvEsM8Ap) + 4);
	CTlvEsM8Ap *tlv = NULL;

	if (!es_type)
		goto error;

	*es_type = ES_TYPE_M8AP;

	tlv = (CTlvEsM8Ap *)(es_type + 1);
	memset((char *) tlv, 0, sizeof(CTlvEsM8Ap));

	tlv->nwKeyIndex = list_create();
	if (!tlv->nwKeyIndex)
		goto error;

	tlv->nwKey = list_create();
	if (!tlv->nwKey)
		goto error;

	tlv->macAddr.m_allocated = false;
	tlv->ssid.m_allocated = false;
	tlv->new_pwd.m_allocated = false;
	tlv->new_pwd.tlvbase.m_len = 0;
	tlv->nwIndex.tlvbase.m_len = 0;
	tlv->wepIdx.tlvbase.m_len = 0;

	return tlv;

error:
	TUTRACE((TUTRACE_ERR, "reg_msg_m8ap_new : out of memory\n"));
	if (tlv) {
		if (tlv->nwKeyIndex)
			list_delete(tlv->nwKeyIndex);
		if (tlv->nwKey)
			list_delete(tlv->nwKey);
		free(es_type);
	}
	return NULL;
}

void
reg_msg_m8ap_del(CTlvEsM8Ap *t, bool content_only)
{
	LISTITR m_itr;
	LPLISTITR itr;
	CTlvNwKeyIndex *keyIndex;
	CTlvNwKey *key;
	int *es_type = (int *)t;

	/* Move to allocated memory real head */
	es_type--;

	itr = list_itrFirst(t->nwKeyIndex, &m_itr);

	while ((keyIndex = (CTlvNwKeyIndex *)list_itrGetNext(itr))) {
		tlv_delete((tlvbase_s *)keyIndex, 0);
	}

	list_delete(t->nwKeyIndex);

	itr = list_itrFirst(t->nwKey, &m_itr);

	while ((key = (CTlvNwKey *)list_itrGetNext(itr))) {
		tlv_delete((tlvbase_s *)key, 0);
	}

	list_delete(t->nwKey);

	tlv_delete(&t->ssid, 1);
	tlv_delete(&t->macAddr, 1);
	tlv_delete(&t->new_pwd, 1);
	if (!content_only)
		free(es_type);
}

void
reg_msg_m8ap_parse(CTlvEsM8Ap *t, BufferObj *theBuf, BufferObj *authKey, bool allocate)
{
	CTlvNwKeyIndex *keyIndex;
	CTlvNwKey *key;
	uint8 dataMac[BUF_SIZE_256_BITS];
	uint8 hmac_len = 0;

	/* NW Index is optional */
	if (WPS_ID_NW_INDEX == buffobj_NextType(theBuf))
		tlv_dserialize(&t->nwIndex, WPS_ID_NW_INDEX, theBuf, 0, 0);

	tlv_dserialize(&t->ssid, WPS_ID_SSID, theBuf, SIZE_32_BYTES, allocate);
	tlv_dserialize(&t->authType, WPS_ID_AUTH_TYPE, theBuf, 0, 0);
	tlv_dserialize(&t->encrType, WPS_ID_ENCR_TYPE, theBuf, 0, 0);

	/* The next field is network key index. There are two possibilities: */
	/* 1. The TLV is omitted, in which case, there is only 1 network key */
	/* 2. The TLV is present, in which case, there may be 1 or more network keys */

	/* condition 1. If the next field is a network Key, the index TLV was omitted */
	if (WPS_ID_NW_KEY == buffobj_NextType(theBuf)) {
		key = (CTlvNwKey *)malloc(sizeof(CTlvNwKey));
		if (!key)
			return;
		tlv_dserialize(key, WPS_ID_NW_KEY, theBuf, SIZE_64_BYTES, allocate);
		if (!list_addItem(t->nwKey, key)) {
			tlv_delete(key, 0);
			return;
		}
	}
	else {
		/* condition 2. any other possibities are illegal & will be caught later */
		while (WPS_ID_NW_KEY_INDEX == buffobj_NextType(theBuf)) {
			keyIndex = (CTlvNwKeyIndex *)tlv_new(WPS_ID_NW_KEY_INDEX);
			if (!keyIndex)
				return;

			tlv_dserialize(keyIndex, WPS_ID_NW_KEY_INDEX, theBuf, 0, 0);
			if (!list_addItem(t->nwKeyIndex, keyIndex)) {
				tlv_delete(keyIndex, 0);
				return;
			}

			key = (CTlvNwKey *)tlv_new(WPS_ID_NW_KEY);
			if (!key)
				return;
			tlv_dserialize(key, WPS_ID_NW_KEY, theBuf, SIZE_64_BYTES, allocate);
			if (!list_addItem(t->nwKey, key)) {
				tlv_delete(key, 0);
				return;
			}
		}
	}

	tlv_dserialize(&t->macAddr, WPS_ID_MAC_ADDR, theBuf, SIZE_6_BYTES, allocate);
	if (WPS_ID_NEW_PWD == buffobj_NextType(theBuf)) {
		/* If the New Password TLV is included, the Device password ID is required */
		tlv_dserialize(&t->new_pwd, WPS_ID_NEW_PWD, theBuf, SIZE_64_BYTES, allocate);
		tlv_dserialize(&t->pwdId, WPS_ID_DEVICE_PWD_ID, theBuf, 0, 0);
	}

	if (WPS_ID_WEP_TRANSMIT_KEY == buffobj_NextType(theBuf))
		tlv_dserialize(&t->wepIdx, WPS_ID_WEP_TRANSMIT_KEY, theBuf, 0, 0);

	if (t->wepIdx.m_data < 1 || t->wepIdx.m_data > 4)
		tlv_set(&t->wepIdx, WPS_ID_WEP_TRANSMIT_KEY, (void *)1, 0);

	/* skip Permitted Config Methods field. */
	if (WPS_ID_PERM_CFG_METHODS == buffobj_NextType(theBuf)) {
		/* advance past the TLV */
		buffobj_Advance(theBuf,  sizeof(WpsTlvHdr) +
			WpsNtohs((theBuf->pCurrent+sizeof(uint16))));
	}

	/* Skip attributes until the KeyWrapAuthenticator */
	while (WPS_ID_KEY_WRAP_AUTH != buffobj_NextType(theBuf)) {
		/* advance past the TLV */
		uint8 *Pos = buffobj_Advance(theBuf,  sizeof(WpsTlvHdr) +
			WpsNtohs((theBuf->pCurrent+sizeof(uint16))));
		if (Pos == NULL)
			return;
	}

	/* save the length deserialized so far */
	hmac_len = (uint8)(theBuf->pCurrent - theBuf->pBase);

	tlv_dserialize(&t->keyWrapAuth, WPS_ID_KEY_WRAP_AUTH, theBuf, SIZE_64_BITS, 0);

	/* validate the mac */

	/* calculate the hmac of the data (data only, not the last auth TLV) */
	hmac_sha256(authKey->pBase, SIZE_256_BITS, theBuf->pBase,
		hmac_len, dataMac, NULL);

	/* compare it against the received hmac */
	if (memcmp(dataMac, t->keyWrapAuth.m_data, SIZE_64_BITS) != 0) {
		TUTRACE((TUTRACE_ERR, "RPROTO: HMAC results don't match\n"));
		return;
	}
}

void
reg_msg_m8ap_write(CTlvEsM8Ap *t, BufferObj *theBuf, BufferObj *authKey, bool b_wps_version2)
{
	LISTITR m_indexItr, m_keyItr;
	LPLISTITR indexItr = NULL, keyItr = NULL;
	CTlvNwKeyIndex *keyIndex;
	CTlvNwKey *key;
	uint8 hmac[SIZE_256_BITS];

	indexItr = list_itrFirst(t->nwKeyIndex, &m_indexItr);

	keyItr = list_itrFirst(t->nwKey, &m_keyItr);

	/*
	 * Caution: WSC 2.0, Page 77 Table 20, page 113.
	 * Remove "Network Index" in M8ap encrypted settings
	 */
	if (!b_wps_version2) {
		/* nwIndex is an optional field */
		if (t->nwIndex.tlvbase.m_len)
			tlv_write(&t->nwIndex, theBuf);
	}

	tlv_write(&t->ssid, theBuf);
	tlv_write(&t->authType, theBuf);
	tlv_write(&t->encrType, theBuf);

	/* write the network index and network key to the buffer */
	if (list_getCount(t->nwKeyIndex) == 0) {
		/* Condition1. There is no key index, so there can only be 1 nw key */
		if (!(key = (CTlvNwKey *) list_itrGetNext(keyItr)))
			goto done;
		tlv_write(key, theBuf);
	}
	else {
		/* Condition2. There are multiple network keys. */
		while ((keyIndex = (CTlvNwKeyIndex *) list_itrGetNext(indexItr))) {
			if (!(key = (CTlvNwKey *) list_itrGetNext(keyItr)))
				goto done;
			tlv_write(keyIndex, theBuf);
			tlv_write(key, theBuf);
		}
	}

	/* write the mac address */
	tlv_write(&t->macAddr, theBuf);

	/* write the optional new password and device password ID */
	if (t->new_pwd.tlvbase.m_len) {
		tlv_write(&t->new_pwd, theBuf);
		tlv_write(&t->pwdId, theBuf);
	}

	if (t->wepIdx.tlvbase.m_len && t->wepIdx.m_data != 1) {
		tlv_write(&t->wepIdx, theBuf);
		TUTRACE((TUTRACE_ERR, "CTlvEsM8Ap_write : wepIndex != 1, "
			"Optional field added! \n"));
	}

	/* calculate the hmac and append the TLV to the buffer */
	hmac_sha256(authKey->pBase, SIZE_256_BITS,
		theBuf->pBase, theBuf->m_dataLength, hmac, NULL);
	tlv_serialize(WPS_ID_KEY_WRAP_AUTH, theBuf, hmac,
		SIZE_64_BITS);

done:
	return;
}

/* ES when M8 is from a STA */
CTlvEsM8Sta *
reg_msg_m8sta_new()
{
	/* Allocate extra 4 bytes in head to store type M8STA */
	int *es_type = (int *)malloc(sizeof(CTlvEsM8Sta) + 4);
	CTlvEsM8Sta *tlv = NULL;

	if (!es_type) {
		TUTRACE((TUTRACE_ERR, "out of memory\n"));
		return NULL;
	}

	*es_type = ES_TYPE_M8STA;

	tlv = (CTlvEsM8Sta *)(es_type + 1);
	memset((char *) tlv, 0, sizeof(CTlvEsM8Sta));

	if (!(tlv->credential = list_create())) {
		TUTRACE((TUTRACE_ERR, "out of memory\n"));
		free(es_type);
		return NULL;
	}

	tlv->new_pwd.tlvbase.m_len = 0;
	tlv->new_pwd.m_allocated = false;
	tlv->keyWrapAuth.m_allocated = false;

	return tlv;
}

void
reg_msg_m8sta_del(CTlvEsM8Sta *t, bool content_only)
{
	LISTITR m_itr;
	LPLISTITR itr;
	CTlvCredential *pCredential;
	int *es_type = (int *)t;

	/* Move to allocated memory real head */
	es_type--;

	itr = list_itrFirst(t->credential, &m_itr);

	while ((pCredential = (CTlvCredential *)list_itrGetNext(itr)))
		tlv_credentialDelete(pCredential, 0);

	list_delete(t->credential);

	tlv_delete(&t->new_pwd, 1);
	tlv_delete(&t->pwdId, 1);
	tlv_delete(&t->keyWrapAuth, 1);
	if (!content_only)
		free(es_type);
}

void
reg_msg_m8sta_parse(CTlvEsM8Sta *t, BufferObj *theBuf, BufferObj *authKey, bool allocate)
{
	/* There should be at least 1 credential TLV */
	CTlvCredential *pCredential;
	uint8 hmac_len = 0;
	uint8 dataMac[BUF_SIZE_256_BITS];

	pCredential = (CTlvCredential *)malloc(sizeof(CTlvCredential));
	if (!pCredential)
		return;

	memset(pCredential, 0, sizeof(CTlvCredential));

	tlv_credentialParse(pCredential, theBuf, allocate);
	if (!list_addItem(t->credential, pCredential)) {
		tlv_credentialDelete(pCredential, 0);
		return;
	}

	/* now parse any additional credential TLVs */
	while (WPS_ID_CREDENTIAL == buffobj_NextType(theBuf)) {
		pCredential = (CTlvCredential *)malloc(sizeof(CTlvCredential));
		if (!pCredential)
			return;

		memset(pCredential, 0, sizeof(CTlvCredential));
		tlv_credentialParse(pCredential, theBuf, allocate);
		if (!list_addItem(t->credential, pCredential)) {
			tlv_credentialDelete(pCredential, 0);
			return;
		}
	}

	if (WPS_ID_NEW_PWD == buffobj_NextType(theBuf)) {
		/* If the New Password TLV is included, the Device password ID is required */
		tlv_dserialize(&t->new_pwd, WPS_ID_NEW_PWD, theBuf, SIZE_64_BYTES, allocate);
		tlv_dserialize(&t->pwdId, WPS_ID_DEVICE_PWD_ID, theBuf, 0, 0);
	}

	/* Skip attributes until the KeyWrapAuthenticator */
	while (WPS_ID_KEY_WRAP_AUTH != buffobj_NextType(theBuf)) {
		/* advance past the TLV */
		uint8 *Pos = buffobj_Advance(theBuf, sizeof(WpsTlvHdr) +
			WpsNtohs((theBuf->pCurrent+sizeof(uint16))));
		if (Pos == NULL)
			return;
	}

	/* save the length deserialized so far */
	hmac_len = (uint8)(theBuf->pCurrent - theBuf->pBase);

	tlv_dserialize(&t->keyWrapAuth, WPS_ID_KEY_WRAP_AUTH, theBuf, SIZE_64_BITS, 0);

	/* validate the mac */

	/* calculate the hmac of the data (data only, not the last auth TLV) */
	hmac_sha256(authKey->pBase, SIZE_256_BITS, theBuf->pBase,
		hmac_len, dataMac, NULL);

	/* compare it against the received hmac */
	if (memcmp(dataMac, t->keyWrapAuth.m_data, SIZE_64_BITS) != 0) {
		TUTRACE((TUTRACE_ERR, "RPROTO: HMAC results don't match\n"));
		return;
	}
}

void
reg_msg_m8sta_write(CTlvEsM8Sta *t, BufferObj *theBuf)
{
	LISTITR m_itr;
	LPLISTITR itr;
	CTlvCredential *pCredential;

	/* there should be at least one credential TLV */
	if (list_getCount(t->credential) == 0) {
		TUTRACE((TUTRACE_ERR, "Error %d in function %s \n",
			RPROT_ERR_REQD_TLV_MISSING, __FUNCTION__));
		return;
	}

	itr = list_itrFirst(t->credential, &m_itr);

	while ((pCredential = (CTlvCredential *)list_itrGetNext(itr))) {
		tlv_credentialWrite(pCredential, theBuf);
	}
}

void
reg_msg_m8sta_write_cred(CTlvEsM8Sta *t, BufferObj *theBuf)
{
	LISTITR m_itr;
	LPLISTITR itr;
	CTlvCredential *pCredential;
	uint8 macAddr[SIZE_6_BYTES];
	memset(macAddr, 0, sizeof(macAddr));

	if (list_getCount(t->credential) == 0) {
		TUTRACE((TUTRACE_ERR, "Error %d in function %s \n",
			RPROT_ERR_REQD_TLV_MISSING, __FUNCTION__));
		return;
	}

	itr = list_itrFirst(t->credential, &m_itr);

	while ((pCredential = (CTlvCredential *)list_itrGetNext(itr))) {
		uint8 * pos = theBuf->pCurrent; /* pos will point prior to Credential */
		CTlvCredential cr;	/* local Credential to override MAC address pointer */

		tlv_credentialWrite(pCredential, theBuf);
		buffobj_Set(theBuf, pos);	/* rewind prior to Credential */
		tlv_credentialInit(&cr);
		tlv_credentialParse(&cr, theBuf, 0); /* create copy of Credential from buffer */
		tlv_set(&cr.macAddr, WPS_ID_MAC_ADDR, macAddr, SIZE_6_BYTES);
		buffobj_Set(theBuf, pos);
		tlv_credentialWrite(&cr, theBuf); /* overwrite the data in the buffer */
	}
}

void
reg_msg_m8sta_write_key(CTlvEsM8Sta *t, BufferObj *theBuf, BufferObj *authKey)
{
	LISTITR m_itr;
	LPLISTITR itr;
	CTlvCredential *pCredential;
	uint8 hmac[SIZE_256_BITS];

	/* there should be at least one credential TLV */
	if (list_getCount(t->credential) == 0) {
		TUTRACE((TUTRACE_ERR, "Error %d in function %s \n",
			RPROT_ERR_REQD_TLV_MISSING, __FUNCTION__));
		return;
	}

	itr = list_itrFirst(t->credential, &m_itr);

	while ((pCredential = (CTlvCredential *)list_itrGetNext(itr))) {
		tlv_credentialWrite(pCredential, theBuf);
	}

	/* write the optional new password and device password ID */
	if (t->new_pwd.tlvbase.m_len) {
		tlv_write(&t->new_pwd, theBuf);
		tlv_write(&t->pwdId, theBuf);
	}

	/* calculate the hmac and append the TLV to the buffer */
	hmac_sha256(authKey->pBase, SIZE_256_BITS,
		theBuf->pBase, theBuf->m_dataLength, hmac, NULL);

	tlv_serialize(WPS_ID_KEY_WRAP_AUTH, theBuf, hmac,
		SIZE_64_BITS);
}

void
reg_msg_es_del(void *tlv, bool content_only)
{
	int *es_type = (int *)tlv;

	if (tlv == NULL) {
		TUTRACE((TUTRACE_ERR, "Try to delete NULL ES pointer\n"));
		return;
	}

	/* Move to allocated memory start */
	es_type--;

	switch (*es_type) {
	case ES_TYPE_M7ENR:
		reg_msg_m7enr_del((TlvEsM7Enr *)tlv, content_only);
		break;
	case ES_TYPE_M7AP:
		reg_msg_m7ap_del((CTlvEsM7Ap *)tlv, content_only);
			break;
	case ES_TYPE_M8AP:
		reg_msg_m8ap_del((CTlvEsM8Ap *)tlv, content_only);
		break;
	case ES_TYPE_M8STA:
		reg_msg_m8sta_del((CTlvEsM8Sta *)tlv, content_only);
		break;
	default:
		TUTRACE((TUTRACE_ERR, "Unknown ES type %d\n", *es_type));
		break;
	}
}
