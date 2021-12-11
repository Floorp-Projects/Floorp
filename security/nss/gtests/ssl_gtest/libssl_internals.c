/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This file contains functions for frobbing the internals of libssl */
#include "libssl_internals.h"

#include "nss.h"
#include "pk11hpke.h"
#include "pk11pub.h"
#include "pk11priv.h"
#include "tls13ech.h"
#include "seccomon.h"
#include "selfencrypt.h"
#include "secmodti.h"
#include "sslproto.h"

SECStatus SSLInt_RemoveServerCertificates(PRFileDesc *fd) {
  if (!fd) {
    return SECFailure;
  }
  sslSocket *ss = ssl_FindSocket(fd);
  if (!ss) {
    return SECFailure;
  }

  PRCList *cursor;
  while (!PR_CLIST_IS_EMPTY(&ss->serverCerts)) {
    cursor = PR_LIST_TAIL(&ss->serverCerts);
    PR_REMOVE_LINK(cursor);
    ssl_FreeServerCert((sslServerCert *)cursor);
  }
  return SECSuccess;
}

SECStatus SSLInt_SetDCAdvertisedSigSchemes(PRFileDesc *fd,
                                           const SSLSignatureScheme *schemes,
                                           uint32_t num_sig_schemes) {
  if (!fd) {
    return SECFailure;
  }
  sslSocket *ss = ssl_FindSocket(fd);
  if (!ss) {
    return SECFailure;
  }

  // Alloc and copy, libssl will free.
  SSLSignatureScheme *dc_schemes =
      PORT_ZNewArray(SSLSignatureScheme, num_sig_schemes);
  if (!dc_schemes) {
    return SECFailure;
  }
  memcpy(dc_schemes, schemes, sizeof(SSLSignatureScheme) * num_sig_schemes);

  if (ss->xtnData.delegCredSigSchemesAdvertised) {
    PORT_Free(ss->xtnData.delegCredSigSchemesAdvertised);
  }
  ss->xtnData.delegCredSigSchemesAdvertised = dc_schemes;
  ss->xtnData.numDelegCredSigSchemesAdvertised = num_sig_schemes;
  return SECSuccess;
}

SECStatus SSLInt_TweakChannelInfoForDC(PRFileDesc *fd, PRBool changeAuthKeyBits,
                                       PRBool changeScheme) {
  if (!fd) {
    return SECFailure;
  }
  sslSocket *ss = ssl_FindSocket(fd);
  if (!ss) {
    return SECFailure;
  }

  // Just toggle so we'll always have a valid value.
  if (changeScheme) {
    ss->sec.signatureScheme = (ss->sec.signatureScheme == ssl_sig_ed25519)
                                  ? ssl_sig_ecdsa_secp256r1_sha256
                                  : ssl_sig_ed25519;
  }
  if (changeAuthKeyBits) {
    ss->sec.authKeyBits = ss->sec.authKeyBits ? ss->sec.authKeyBits * 2 : 384;
  }

  return SECSuccess;
}

SECStatus SSLInt_GetHandshakeRandoms(PRFileDesc *fd, SSL3Random client_random,
                                     SSL3Random server_random) {
  if (!fd) {
    return SECFailure;
  }
  sslSocket *ss = ssl_FindSocket(fd);
  if (!ss) {
    return SECFailure;
  }

  if (client_random) {
    memcpy(client_random, ss->ssl3.hs.client_random, sizeof(SSL3Random));
  }
  if (server_random) {
    memcpy(server_random, ss->ssl3.hs.server_random, sizeof(SSL3Random));
  }
  return SECSuccess;
}

SECStatus SSLInt_IncrementClientHandshakeVersion(PRFileDesc *fd) {
  sslSocket *ss = ssl_FindSocket(fd);
  if (!ss) {
    return SECFailure;
  }

  ++ss->clientHelloVersion;

  return SECSuccess;
}

/* Use this function to update the ClientRandom of a client's handshake state
 * after replacing its ClientHello message. We for example need to do this
 * when replacing an SSLv3 ClientHello with its SSLv2 equivalent. */
SECStatus SSLInt_UpdateSSLv2ClientRandom(PRFileDesc *fd, uint8_t *rnd,
                                         size_t rnd_len, uint8_t *msg,
                                         size_t msg_len) {
  sslSocket *ss = ssl_FindSocket(fd);
  if (!ss) {
    return SECFailure;
  }

  ssl3_RestartHandshakeHashes(ss);

  // Ensure we don't overrun hs.client_random.
  rnd_len = PR_MIN(SSL3_RANDOM_LENGTH, rnd_len);

  // Zero the client_random.
  PORT_Memset(ss->ssl3.hs.client_random, 0, SSL3_RANDOM_LENGTH);

  // Copy over the challenge bytes.
  size_t offset = SSL3_RANDOM_LENGTH - rnd_len;
  PORT_Memcpy(ss->ssl3.hs.client_random + offset, rnd, rnd_len);

  // Rehash the SSLv2 client hello message.
  return ssl3_UpdateHandshakeHashes(ss, msg, msg_len);
}

PRBool SSLInt_ExtensionNegotiated(PRFileDesc *fd, PRUint16 ext) {
  sslSocket *ss = ssl_FindSocket(fd);
  return (PRBool)(ss && ssl3_ExtensionNegotiated(ss, ext));
}

// Tests should not use this function directly, because the keys may
// still be in cache. Instead, use TlsConnectTestBase::ClearServerCache.
void SSLInt_ClearSelfEncryptKey() { ssl_ResetSelfEncryptKeys(); }

sslSelfEncryptKeys *ssl_GetSelfEncryptKeysInt();

void SSLInt_SetSelfEncryptMacKey(PK11SymKey *key) {
  sslSelfEncryptKeys *keys = ssl_GetSelfEncryptKeysInt();

  PK11_FreeSymKey(keys->macKey);
  keys->macKey = key;
}

SECStatus SSLInt_SetMTU(PRFileDesc *fd, PRUint16 mtu) {
  sslSocket *ss = ssl_FindSocket(fd);
  if (!ss) {
    return SECFailure;
  }
  ss->ssl3.mtu = mtu;
  ss->ssl3.hs.rtRetries = 0; /* Avoid DTLS shrinking the MTU any more. */
  return SECSuccess;
}

PRInt32 SSLInt_CountCipherSpecs(PRFileDesc *fd) {
  PRCList *cur_p;
  PRInt32 ct = 0;

  sslSocket *ss = ssl_FindSocket(fd);
  if (!ss) {
    return -1;
  }

  for (cur_p = PR_NEXT_LINK(&ss->ssl3.hs.cipherSpecs);
       cur_p != &ss->ssl3.hs.cipherSpecs; cur_p = PR_NEXT_LINK(cur_p)) {
    ++ct;
  }
  return ct;
}

void SSLInt_PrintCipherSpecs(const char *label, PRFileDesc *fd) {
  PRCList *cur_p;

  sslSocket *ss = ssl_FindSocket(fd);
  if (!ss) {
    return;
  }

  fprintf(stderr, "Cipher specs for %s\n", label);
  for (cur_p = PR_NEXT_LINK(&ss->ssl3.hs.cipherSpecs);
       cur_p != &ss->ssl3.hs.cipherSpecs; cur_p = PR_NEXT_LINK(cur_p)) {
    ssl3CipherSpec *spec = (ssl3CipherSpec *)cur_p;
    fprintf(stderr, "  %s spec epoch=%d (%s) refct=%d\n", SPEC_DIR(spec),
            spec->epoch, spec->phase, spec->refCt);
  }
}

/* DTLS timers are separate from the time that the rest of the stack uses.
 * Force a timer expiry by backdating when all active timers were started.
 * We could set the remaining time to 0 but then backoff would not work properly
 * if we decide to test it. */
SECStatus SSLInt_ShiftDtlsTimers(PRFileDesc *fd, PRIntervalTime shift) {
  size_t i;
  sslSocket *ss = ssl_FindSocket(fd);
  if (!ss) {
    return SECFailure;
  }

  for (i = 0; i < PR_ARRAY_SIZE(ss->ssl3.hs.timers); ++i) {
    if (ss->ssl3.hs.timers[i].cb) {
      ss->ssl3.hs.timers[i].started -= shift;
    }
  }
  return SECSuccess;
}

#define CHECK_SECRET(secret)                  \
  if (ss->ssl3.hs.secret) {                   \
    fprintf(stderr, "%s != NULL\n", #secret); \
    return PR_FALSE;                          \
  }

PRBool SSLInt_CheckSecretsDestroyed(PRFileDesc *fd) {
  sslSocket *ss = ssl_FindSocket(fd);
  if (!ss) {
    return PR_FALSE;
  }

  CHECK_SECRET(currentSecret);
  CHECK_SECRET(dheSecret);
  CHECK_SECRET(clientEarlyTrafficSecret);
  CHECK_SECRET(clientHsTrafficSecret);
  CHECK_SECRET(serverHsTrafficSecret);

  return PR_TRUE;
}

PRBool sslint_DamageTrafficSecret(PRFileDesc *fd, size_t offset) {
  unsigned char data[32] = {0};
  PK11SymKey **keyPtr;
  PK11SlotInfo *slot = PK11_GetInternalSlot();
  SECItem key_item = {siBuffer, data, sizeof(data)};
  sslSocket *ss = ssl_FindSocket(fd);
  if (!ss) {
    return PR_FALSE;
  }
  if (!slot) {
    return PR_FALSE;
  }
  keyPtr = (PK11SymKey **)((char *)&ss->ssl3.hs + offset);
  if (!*keyPtr) {
    return PR_FALSE;
  }
  PK11_FreeSymKey(*keyPtr);
  *keyPtr = PK11_ImportSymKey(slot, CKM_NSS_HKDF_SHA256, PK11_OriginUnwrap,
                              CKA_DERIVE, &key_item, NULL);
  PK11_FreeSlot(slot);
  if (!*keyPtr) {
    return PR_FALSE;
  }

  return PR_TRUE;
}

PRBool SSLInt_DamageClientHsTrafficSecret(PRFileDesc *fd) {
  return sslint_DamageTrafficSecret(
      fd, offsetof(SSL3HandshakeState, clientHsTrafficSecret));
}

PRBool SSLInt_DamageServerHsTrafficSecret(PRFileDesc *fd) {
  return sslint_DamageTrafficSecret(
      fd, offsetof(SSL3HandshakeState, serverHsTrafficSecret));
}

PRBool SSLInt_DamageEarlyTrafficSecret(PRFileDesc *fd) {
  return sslint_DamageTrafficSecret(
      fd, offsetof(SSL3HandshakeState, clientEarlyTrafficSecret));
}

SECStatus SSLInt_Set0RttAlpn(PRFileDesc *fd, PRUint8 *data, unsigned int len) {
  sslSocket *ss = ssl_FindSocket(fd);
  if (!ss) {
    return SECFailure;
  }

  ss->xtnData.nextProtoState = SSL_NEXT_PROTO_EARLY_VALUE;
  if (ss->xtnData.nextProto.data) {
    SECITEM_FreeItem(&ss->xtnData.nextProto, PR_FALSE);
  }
  if (!SECITEM_AllocItem(NULL, &ss->xtnData.nextProto, len)) {
    return SECFailure;
  }
  PORT_Memcpy(ss->xtnData.nextProto.data, data, len);

  return SECSuccess;
}

PRBool SSLInt_HasCertWithAuthType(PRFileDesc *fd, SSLAuthType authType) {
  sslSocket *ss = ssl_FindSocket(fd);
  if (!ss) {
    return PR_FALSE;
  }

  return (PRBool)(!!ssl_FindServerCert(ss, authType, NULL));
}

PRBool SSLInt_SendAlert(PRFileDesc *fd, uint8_t level, uint8_t type) {
  sslSocket *ss = ssl_FindSocket(fd);
  if (!ss) {
    return PR_FALSE;
  }

  SECStatus rv = SSL3_SendAlert(ss, level, type);
  if (rv != SECSuccess) return PR_FALSE;

  return PR_TRUE;
}

SECStatus SSLInt_AdvanceReadSeqNum(PRFileDesc *fd, PRUint64 to) {
  sslSocket *ss;
  ssl3CipherSpec *spec;

  ss = ssl_FindSocket(fd);
  if (!ss) {
    return SECFailure;
  }
  if (to > RECORD_SEQ_MAX) {
    PORT_SetError(SEC_ERROR_INVALID_ARGS);
    return SECFailure;
  }
  ssl_GetSpecWriteLock(ss);
  spec = ss->ssl3.crSpec;
  spec->nextSeqNum = to;

  /* For DTLS, we need to fix the record sequence number.  For this, we can just
   * scrub the entire structure on the assumption that the new sequence number
   * is far enough past the last received sequence number. */
  if (spec->nextSeqNum <=
      spec->recvdRecords.right + DTLS_RECVD_RECORDS_WINDOW) {
    PORT_SetError(SEC_ERROR_INVALID_ARGS);
    return SECFailure;
  }
  dtls_RecordSetRecvd(&spec->recvdRecords, spec->nextSeqNum - 1);

  ssl_ReleaseSpecWriteLock(ss);
  return SECSuccess;
}

SECStatus SSLInt_AdvanceWriteSeqNum(PRFileDesc *fd, PRUint64 to) {
  sslSocket *ss;
  ssl3CipherSpec *spec;
  PK11Context *pk11ctxt;
  const ssl3BulkCipherDef *cipher_def;

  ss = ssl_FindSocket(fd);
  if (!ss) {
    return SECFailure;
  }
  if (to >= RECORD_SEQ_MAX) {
    PORT_SetError(SEC_ERROR_INVALID_ARGS);
    return SECFailure;
  }
  ssl_GetSpecWriteLock(ss);
  spec = ss->ssl3.cwSpec;
  cipher_def = spec->cipherDef;
  spec->nextSeqNum = to;
  if (cipher_def->type != type_aead) {
    ssl_ReleaseSpecWriteLock(ss);
    return SECSuccess;
  }
  /* If we are using aead, we need to advance the counter in the
   * internal IV generator as well.
   * This could be in the token or software. */
  pk11ctxt = spec->cipherContext;
  /* If counter is in the token, we need to switch it to software,
   * since we don't have access to the internal state of the token. We do
   * that by turning on the simulated message interface, then setting up the
   * software IV generator */
  if (pk11ctxt->ivCounter == 0) {
    _PK11_ContextSetAEADSimulation(pk11ctxt);
    pk11ctxt->ivLen = cipher_def->iv_size + cipher_def->explicit_nonce_size;
    pk11ctxt->ivMaxCount = PR_UINT64(0xffffffffffffffff);
    if ((cipher_def->explicit_nonce_size == 0) ||
        (spec->version >= SSL_LIBRARY_VERSION_TLS_1_3)) {
      pk11ctxt->ivFixedBits =
          (pk11ctxt->ivLen - sizeof(sslSequenceNumber)) * BPB;
      pk11ctxt->ivGen = CKG_GENERATE_COUNTER_XOR;
    } else {
      pk11ctxt->ivFixedBits = cipher_def->iv_size * BPB;
      pk11ctxt->ivGen = CKG_GENERATE_COUNTER;
    }
    /* DTLS included the epoch in the fixed portion of the IV */
    if (IS_DTLS(ss)) {
      pk11ctxt->ivFixedBits += 2 * BPB;
    }
  }
  /* now we can update the internal counter (either we are already using
   * the software IV generator, or we just switched to it above */
  pk11ctxt->ivCounter = to;

  ssl_ReleaseSpecWriteLock(ss);
  return SECSuccess;
}

SECStatus SSLInt_AdvanceWriteSeqByAWindow(PRFileDesc *fd, PRInt32 extra) {
  sslSocket *ss;
  sslSequenceNumber to;

  ss = ssl_FindSocket(fd);
  if (!ss) {
    return SECFailure;
  }
  ssl_GetSpecReadLock(ss);
  to = ss->ssl3.cwSpec->nextSeqNum + DTLS_RECVD_RECORDS_WINDOW + extra;
  ssl_ReleaseSpecReadLock(ss);
  return SSLInt_AdvanceWriteSeqNum(fd, to);
}

SECStatus SSLInt_AdvanceDtls13DecryptFailures(PRFileDesc *fd, PRUint64 to) {
  sslSocket *ss = ssl_FindSocket(fd);
  if (!ss) {
    return SECFailure;
  }

  ssl_GetSpecWriteLock(ss);
  ssl3CipherSpec *spec = ss->ssl3.crSpec;
  if (spec->cipherDef->type != type_aead) {
    ssl_ReleaseSpecWriteLock(ss);
    return SECFailure;
  }

  spec->deprotectionFailures = to;
  ssl_ReleaseSpecWriteLock(ss);
  return SECSuccess;
}

SSLKEAType SSLInt_GetKEAType(SSLNamedGroup group) {
  const sslNamedGroupDef *groupDef = ssl_LookupNamedGroup(group);
  if (!groupDef) return ssl_kea_null;

  return groupDef->keaType;
}

SECStatus SSLInt_SetSocketMaxEarlyDataSize(PRFileDesc *fd, uint32_t size) {
  sslSocket *ss;

  ss = ssl_FindSocket(fd);
  if (!ss) {
    return SECFailure;
  }

  /* This only works when resuming. */
  if (!ss->statelessResume) {
    PORT_SetError(SEC_INTERNAL_ONLY);
    return SECFailure;
  }

  /* Modifying both specs allows this to be used on either peer. */
  ssl_GetSpecWriteLock(ss);
  ss->ssl3.crSpec->earlyDataRemaining = size;
  ss->ssl3.cwSpec->earlyDataRemaining = size;
  ssl_ReleaseSpecWriteLock(ss);

  return SECSuccess;
}

SECStatus SSLInt_HasPendingHandshakeData(PRFileDesc *fd, PRBool *pending) {
  sslSocket *ss = ssl_FindSocket(fd);
  if (!ss) {
    return SECFailure;
  }

  ssl_GetSSL3HandshakeLock(ss);
  *pending = ss->ssl3.hs.msg_body.len > 0;
  ssl_ReleaseSSL3HandshakeLock(ss);
  return SECSuccess;
}

SECStatus SSLInt_SetRawEchConfigForRetry(PRFileDesc *fd, const uint8_t *buf,
                                         size_t len) {
  sslSocket *ss = ssl_FindSocket(fd);
  if (!ss) {
    return SECFailure;
  }

  sslEchConfig *cfg = (sslEchConfig *)PR_LIST_HEAD(&ss->echConfigs);
  SECITEM_FreeItem(&cfg->raw, PR_FALSE);
  SECITEM_AllocItem(NULL, &cfg->raw, len);
  PORT_Memcpy(cfg->raw.data, buf, len);
  return SECSuccess;
}

PRBool SSLInt_IsIp(PRUint8 *s, unsigned int len) { return tls13_IsIp(s, len); }
