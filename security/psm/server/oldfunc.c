/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#include "oldfunc.h"
#include "newproto.h"
#include "messages.h"

/*
 * All possible key size choices.
 */
static SECKeySizeChoiceInfo SECKeySizeChoiceList[] = {
    { NULL, 2048, 0 },
    { NULL, 1024, 1 },
    { NULL, 512,  1 },
    { NULL, 0,    0 }
};

DERTemplate CERTSubjectPublicKeyInfoTemplate[] = {
    { DER_SEQUENCE,
          0, NULL, sizeof(CERTSubjectPublicKeyInfo) },
    { DER_INLINE,
          offsetof(CERTSubjectPublicKeyInfo,algorithm),
          SECAlgorithmIDTemplate, },
    { DER_BIT_STRING,
          offsetof(CERTSubjectPublicKeyInfo,subjectPublicKey), },
    { 0, }
};

DERTemplate CERTPublicKeyAndChallengeTemplate[] =
{
    { DER_SEQUENCE, 0, NULL, sizeof(CERTPublicKeyAndChallenge) },
    { DER_ANY, offsetof(CERTPublicKeyAndChallenge,spki), },
    { DER_IA5_STRING, offsetof(CERTPublicKeyAndChallenge,challenge), },
    { 0, }
};

typedef struct {
  SSMControlConnection * ctrl;
  CERTCertificate * cert;
  CERTDERCerts * derCerts;
} caImportCertArg;

/* Used for accepting CA certificate. */
static PRBool acceptssl, acceptmime, acceptobjectsign, acceptcacert;

static  char *nickFmt=NULL, *nickFmtWithNum = NULL;
  
static int pqg_prime_bits(char *str);
static PQGParams * decode_pqg_params(char *str);
static char * GenKey(SSMControlConnection * ctrl, int keysize, char *challenge,
		     PRUint32 type, PQGParams *pqg, PK11SlotInfo * slot);
static SSMStatus handle_user_cert(SSMControlConnection * ctrl,
                        CERTCertificate *cert, CERTDERCerts *derCerts);
static SSMStatus handle_ca_cert(SSMControlConnection * ctrl,
			       CERTCertificate *cert, CERTDERCerts *derCerts);
static SSMStatus handle_email_cert(SSMControlConnection * ctrl, 
				  CERTCertificate * cert,
                           	  CERTDERCerts * derCerts);
static void SSM_ImportCACert(void * arg);

static int init = 0;

SSMStatus SSM_EnableHighGradeKeyGen(void)
{
  SECKeySizeChoiceList[0].enabled = 1;
  return SSM_SUCCESS;
}

PRBool SSM_KeyGenAllowedForSize(int size)
{
  int i;
  int numChoices = sizeof(SECKeySizeChoiceList)/sizeof(SECKeySizeChoiceInfo);

  for (i=0; i<numChoices; i++) {
    if (SECKeySizeChoiceList[i].size == size && 
	SECKeySizeChoiceList[i].enabled) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

SSMStatus InitChoiceList(void) 
{
  SSMTextGenContext *cx;
  
  if (SSMTextGen_NewTopLevelContext(NULL, &cx) != SSM_SUCCESS) {
    goto loser;
  }
  if (SSM_GetUTF8Text(cx, "high_grade", 
		     &SECKeySizeChoiceList[0].name) != SSM_SUCCESS) {
    goto loser;
  }
  if (SSM_GetUTF8Text(cx, "medium_grade", 
		      &SECKeySizeChoiceList[1].name) != SSM_SUCCESS) {
    goto loser;
  }

  if (SSM_GetUTF8Text(cx, "low_grade", 
		      &SECKeySizeChoiceList[2].name) != SSM_SUCCESS) {
    goto loser;
  }
  SSMTextGen_DestroyContext(cx);
  return SSM_SUCCESS;
  loser:
  if (cx != NULL) {
    SSMTextGen_DestroyContext(cx);
  }
  return SSM_FAILURE;
}


char ** SSM_GetKeyChoiceList(char * type, char * pqgString, int *nchoices)
{
  char **     list, *pqg, *end;
  int         i, j, len, primeBits;
  SSMStatus rv;

  if (!init) {
    rv = InitChoiceList();
    if (rv != SSM_SUCCESS) {
      PR_ASSERT(!"Could not initialize the choice list for key gen layout!!");
      return NULL;
    }
    init = 1;
  }

  len = sizeof (SECKeySizeChoiceList) / sizeof (SECKeySizeChoiceInfo);

  list = SSM_ZNEW_ARRAY(char*, len);

  if (type && (PORT_Strcasecmp(type, "dsa") == 0)) {
        if (pqgString == NULL) {
            list[0] = NULL;
            return list;
        }
        pqg = pqgString;

        j = 0;
        do {
            end = PORT_Strchr(pqg, ',');
            if (end != NULL)
                *end = '\0';
            primeBits = pqg_prime_bits(pqg);
            for (i = 0; SECKeySizeChoiceList[i].size != 0; i++) {
                if (SECKeySizeChoiceList[i].size == primeBits) {
                    list[j++] = strdup(SECKeySizeChoiceList[i].name);
                    break;
                }
            }
            pqg = end + 1;
        } while (end != NULL);
    } else {
        int maxKeyBits = SSM_MAX_KEY_BITS;
        j = 0;
        for (i = (policyType == ssmDomestic) ? 0 : 1; 
	     SECKeySizeChoiceList[i].size != 0; i++) {
            if (SECKeySizeChoiceList[i].size <= maxKeyBits)
                list[j++] = strdup(SECKeySizeChoiceList[i].name);
        }
    }

    if (pqgString != NULL)
        PORT_Free(pqgString);
    *nchoices = j;
    return list;
}


char * SSMControlConnection_GenerateKeyOldStyle(SSMControlConnection * ctrl,
					   char *choiceString, char *challenge,
			                   char *typeString, char *pqgString)
{
  SECKeySizeChoiceInfo *choice = SECKeySizeChoiceList;
  char *end, *str;
  PQGParams *pqg = NULL;
  int primeBits;
  KeyType type; 
  char * keystring = NULL;
  PK11SlotInfo *slot;
  SSMKeyGenContext * ct = NULL;
  SSMKeyGenContextCreateArg arg;
  PRUint32 keyGenMechanism;
  SSMStatus rv;

  if (choiceString == NULL) {
    goto loser;
  }
  while (PORT_Strcmp(choice->name, choiceString) != 0)
    choice++;

  if (PORT_Strcmp(challenge, "null") == 0) {
    challenge = NULL;
  }

  if ((PORT_Strcmp(typeString, "null") == 0) || 
      (PORT_Strcasecmp(typeString, "rsa") == 0)) {
    type = rsaKey;
    keyGenMechanism = CKM_RSA_PKCS_KEY_PAIR_GEN;
  } else if (PORT_Strcasecmp(typeString, "dsa") == 0) {
    type = dsaKey;
    keyGenMechanism = CKM_DSA_KEY_PAIR_GEN;
    if (PORT_Strcmp(pqgString, "null") == 0)
      goto loser;
    str = pqgString;
    do {
      end = PORT_Strchr(str, ',');
      if (end != NULL)
        *end = '\0';
      primeBits = pqg_prime_bits(str);
      if (choice->size == primeBits)
        goto found_match;
      str = end + 1;
    } while (end != NULL);
    goto loser;
found_match:
    pqg = decode_pqg_params(str);
  } else {
    goto loser; 
  }  
 
  /* 
   * To get slot we first create a keygen context, and 
   * use its facilities to find a slot for our key generation
   */   

  arg.parent  = ctrl;
  arg.type = SSM_OLD_STYLE_KEYGEN;
  arg.param = NULL;
  SSM_DEBUG("Creating key gen context.\n");
  rv = SSMKeyGenContext_Create(&arg, ctrl, (SSMResource **)&ct);
  if (rv != PR_SUCCESS) {
    SSM_DEBUG("Could not create KeyGenContext for oldStyleKeyGen.\n");
    goto loser;
  }
  ct->mech = keyGenMechanism;
  rv = SSMKeyGenContext_GetSlot(ct, keyGenMechanism);
  if (rv != SSM_SUCCESS)
    goto loser;

  slot = ct->slot;
  keystring = GenKey(ctrl,choice->size, challenge, keyGenMechanism, pqg, slot);
  /* received a Cancel event from UI */
  if (ct->m_userCancel) {
    PR_Free(keystring);
    keystring = NULL;
  }

loser:
  if (ct) 
    SSMKeyGenContext_Shutdown((SSMResource *)ct, rv);

  return keystring; 
}

static PQGParams *
decode_pqg_params(char *str)
{
    char *buf;
    unsigned int len;
    PRArenaPool *arena;
    PQGParams *params;
    SECStatus status;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL)
        return NULL;

    params = (PQGParams *) PORT_ArenaZAlloc(arena, sizeof(PQGParams));
    if (params == NULL)
        goto loser;
    params->arena = arena;

    buf = (char *)ATOB_AsciiToData(str, &len);
    if ((buf == NULL) || (len == 0))
        goto loser;

    status = SEC_ASN1Decode(arena, params, SECKEY_PQGParamsTemplate, buf, len);
    if (status != SECSuccess)
        goto loser;

    return params;

loser:
    if (arena != NULL)
        PORT_FreeArena(arena, PR_FALSE);
    return NULL;
}

static int
pqg_prime_bits(char *str)
{
    PQGParams *params = NULL;
    int primeBits = 0, i;

    params = decode_pqg_params(str);
    if (params == NULL)
        goto done; /* lose */

    for (i = 0; params->prime.data[i] == 0; i++)
        /* empty */;
    primeBits = (params->prime.len - i) * 8;

done:
    if (params != NULL)
        PQG_DestroyParams(params);
    return primeBits;
}

static char * 
GenKey(SSMControlConnection * ctrl, int keysize, char *challenge, 
       PRUint32 type, PQGParams *pqg, PK11SlotInfo *slot)
{
    SECKEYPrivateKey *privateKey = NULL;
    SECKEYPublicKey *publicKey = NULL;
    CERTSubjectPublicKeyInfo *spkInfo = NULL;
    PRArenaPool *arena = NULL;
    SECStatus rv = SECFailure;
    SECItem spkiItem;
    SECItem pkacItem;
    SECItem signedItem;
    CERTPublicKeyAndChallenge pkac;
    char *keystring = NULL;
    CK_MECHANISM_TYPE mechanism = type;
    PK11RSAGenParams rsaParams;
    void *params;
    SECOidTag algTag;

    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	goto done;
    }

    /*
     * Create a new key pair.
     */
    /* Switch statement goes here for DSA */
    switch (mechanism) {
      case CKM_RSA_PKCS_KEY_PAIR_GEN:
	rsaParams.keySizeInBits = keysize;
	rsaParams.pe = 65537L;
	algTag = SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION;
	params = &rsaParams;
	break;
      case CKM_DSA_KEY_PAIR_GEN:
	params = pqg;
	algTag = SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST;
	break;
      default:
	goto done;
	break;
    }
    privateKey = PK11_GenerateKeyPair(slot, mechanism, params,
				     &publicKey, PR_TRUE, PR_TRUE, NULL);
    if (privateKey == NULL) {
	goto done;
    }
    /* just in case we'll need to authenticate to the db -jp */
    privateKey->wincx = ctrl;
    /*
     * Create a subject public key info from the public key.
     */
    spkInfo = SECKEY_CreateSubjectPublicKeyInfo(publicKey);
    if ( spkInfo == NULL ) {
	goto done;
    }
    
    /*
     * Now DER encode the whole subjectPublicKeyInfo.
     */
    rv=DER_Encode(arena, &spkiItem, CERTSubjectPublicKeyInfoTemplate, spkInfo);
    if (rv != SECSuccess) {
	goto done;
    }

    /*
     * set up the PublicKeyAndChallenge data structure, then DER encode it
     */
    pkac.spki = spkiItem;
    if ( challenge ) {
	pkac.challenge.len = PORT_Strlen(challenge);
    } else {
	pkac.challenge.len = 0;
    }
    pkac.challenge.data = (unsigned char *)challenge;
    
    rv = DER_Encode(arena, &pkacItem, CERTPublicKeyAndChallengeTemplate, &pkac);
    if ( rv != SECSuccess ) {
	goto done;
    }

    /*
     * now sign the DER encoded PublicKeyAndChallenge
     */
    rv = SEC_DerSignData(arena, &signedItem, pkacItem.data, pkacItem.len,
			 privateKey, algTag);
    if ( rv != SECSuccess ) {
	goto done;
    }
    
    /*
     * Convert the signed public key and challenge into base64/ascii.
     */
    keystring = BTOA_DataToAscii(signedItem.data, signedItem.len);
    SSM_DEBUG("Created following string for KEYGEN:\n%s\n", keystring);
    
 done:
    /*
     * Destroy the private key ASAP, so that we don't leave a copy in
     * memory that some evil hacker can get at.
     */
    if ( rv != SECSuccess ) {
	if ( privateKey ) {
	  PK11_DestroyTokenObject(privateKey->pkcs11Slot,privateKey->pkcs11ID);
	  SECKEY_DestroyPrivateKey(privateKey);
	}
	if ( publicKey ) 
	  PK11_DestroyTokenObject(publicKey->pkcs11Slot,publicKey->pkcs11ID);
    }
    if ( spkInfo ) 
      SECKEY_DestroySubjectPublicKeyInfo(spkInfo);
    if ( publicKey ) 
	SECKEY_DestroyPublicKey(publicKey);
    if ( arena ) 
      PORT_FreeArena(arena, PR_TRUE);

    return keystring;
}

static SECStatus
collect_certs(void *arg, SECItem **certs, int numcerts)
{
    CERTDERCerts *collectArgs;
    SECItem *cert;
    SECStatus rv;

    collectArgs = (CERTDERCerts *)arg;

    collectArgs->numcerts = numcerts;
    collectArgs->rawCerts = (SECItem *) PORT_ArenaZAlloc(collectArgs->arena,
                                           sizeof(SECItem) * numcerts);
    if ( collectArgs->rawCerts == NULL ) 
      return(SECFailure);
    cert = collectArgs->rawCerts;

    while ( numcerts-- ) {
        rv = SECITEM_CopyItem(collectArgs->arena, cert, *certs);
        if ( rv == SECFailure ) 
	  return(SECFailure);
	cert++;
	certs++;
    }

    return (SECSuccess);
}

static SSMStatus handle_user_cert(SSMControlConnection * ctrl, 
			CERTCertificate *cert, CERTDERCerts *derCerts)
{
    PK11SlotInfo *slot;
    char * nickname = NULL;
    SECStatus rv;
    int numCACerts;
    SECItem *CACerts;

    slot = PK11_KeyForCertExists(cert, NULL, ctrl);
    if ( slot == NULL ) {
      SSM_DEBUG("Can't find keydb with the key for the new cert!\n");
      goto loser;
    }
    PK11_FreeSlot(slot);
    if (!certificate_conflict(ctrl, &cert->derCert, 
			      ctrl->m_certdb, PR_TRUE)) {
      /*
       * This cert is already in the database, no need to re-import.
       */
      goto loser;
    }

    /* pick a nickname for the cert */
    if (cert->subjectList && cert->subjectList->entry && 
        cert->subjectList->entry->nickname)
      nickname = cert->subjectList->entry->nickname;
    else nickname = default_nickname(cert, ctrl);

    /*
    UI for cert installation goes here - no UI here?
    */
    /* user wants to import the cert */
    slot = PK11_ImportCertForKey(cert, nickname, ctrl);
    if (!slot) 
      goto loser;
    PK11_FreeSlot(slot);
    SSM_UseAsDefaultEmailIfNoneSet(ctrl, cert, PR_FALSE);
    numCACerts = derCerts->numcerts - 1;

    if (numCACerts) {
      CACerts = derCerts->rawCerts+1;
      
      rv = CERT_ImportCAChain(CACerts, numCACerts, certUsageUserCertImport);
      /* We really should send some notice to the user that importing the
       * CA certs failed if rv is SECSuccess.
       */
    }
    
    return PR_SUCCESS;
 loser:
    CERT_DestroyCertificate(cert);
    return PR_FAILURE;

}

static SSMStatus handle_ca_cert(SSMControlConnection * ctrl, 
			       CERTCertificate *cert, CERTDERCerts *derCerts)
{
  caImportCertArg * arg = NULL;
  arg = (caImportCertArg *) PORT_ZAlloc(sizeof(caImportCertArg));
  arg->ctrl = ctrl;
  arg->cert = CERT_DupCertificate(cert);
  arg->derCerts = derCerts;

  SSM_CreateAndRegisterThread(PR_USER_THREAD, SSM_ImportCACert, (void *)arg, 
		  PR_PRIORITY_NORMAL, PR_LOCAL_THREAD, 
		  PR_UNJOINABLE_THREAD, 0);
  return PR_SUCCESS;
}

PRBool
certificate_conflict(SSMControlConnection * cx, SECItem * derCert, 
                     CERTCertDBHandle * handle, PRBool sendUIEvent)
{
   SECStatus rv;
   SECItem key;
   PRArenaPool *arena = NULL;
   PRBool ret;
   CERTCertificate *cert;

   arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
   if (!arena) 
     goto loser;
   
   /* get the key (issuer+cn) from the cert */
   rv = CERT_KeyFromDERCert(arena, derCert, &key);
   if ( rv != SECSuccess ) 
     goto loser;
   
   /* check if the same cert already exists */
   cert = CERT_FindCertByKey(handle, &key);
   if ( cert ) {
       if ( cert->isperm && ( cert->nickname == NULL ) &&
           ( cert->emailAddr == NULL ) ) {
           /* if the cert doesn't have a nickname or email addr, it is
            * bogus cruft, so delete it
            */
           SEC_DeletePermCertificate(cert);
       } else if ( cert->isperm ) {
           SSM_DEBUG("Found existing cert with the same key!\n");
           rv = SECFailure;
           if (sendUIEvent && (SSMControlConnection_SendUIEvent(cx, 
                                                "get", "cert_already_exists",
						NULL, NULL, NULL, PR_TRUE)) == SSM_SUCCESS)
	     rv = SECSuccess;
           ret = PR_FALSE;
           goto done;
       } else 
	 CERT_DestroyCertificate(cert);
   } /* end of same-cert-exists */
   
   ret = PR_TRUE;
   goto done;
   
loser:
    ret = PR_FALSE;
done:
    if ( arena ) 
      PORT_FreeArena(arena, PR_FALSE);
    return ret;
}

typedef struct UserCertImprtArgStr {
  SSMControlConnection *ctrl;
  SECItem *msg;
} UserCertImportArg;

void ssm_import_user_cert_thread(void *arg) {
  UserCertImportArg *impArg = (UserCertImportArg*)arg;
  SSMControlConnection *ctrl = impArg->ctrl;
  SECItem *msg = impArg->msg;
  SSMStatus rv = PR_FAILURE;
  CERTDERCerts * collectArgs;
  PRArenaPool *arena;
  CERTCertificate * cert=NULL;
  SSMResourceID certID;
  SSMResource * certResource;
  PRBool noconflict;
  DecodeAndCreateTempCertRequest request;
  SingleNumMessage reply;
  
  if (!msg || !msg->data)
    goto loser;
 
  SSM_RegisterThread("cert import", NULL);

  if (CMT_DecodeMessage(DecodeAndCreateTempCertRequestTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
      goto loser;
  }

  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if ( arena == NULL ) 
    goto loser;

  collectArgs = (CERTDERCerts *)PORT_ArenaZAlloc(arena, sizeof(CERTDERCerts));
  if ( collectArgs == NULL ) 
    goto loser;

  collectArgs->arena = arena;

  rv = CERT_DecodeCertPackage((char *) request.cert.data, (int) request.cert.len, collect_certs, 
			      (void *)collectArgs);
  if (rv != PR_SUCCESS)
    goto loser;

  /* check for conflicts */
  noconflict = certificate_conflict(ctrl,collectArgs->rawCerts,
                                    ctrl->m_certdb, PR_TRUE);
  if (!noconflict) 
    goto loser;   /* certificate with this key already exists in the db */

  cert = CERT_NewTempCertificate(ctrl->m_certdb, collectArgs->rawCerts,
                	       (char *)NULL, PR_FALSE, PR_TRUE);
  if (!cert)
    goto loser;

  /* create certificate resource so we can return it to client */
  rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE, cert, ctrl, &certID, &certResource);
  if (rv != PR_SUCCESS) {
    SSM_DEBUG("In decode&create tmp cert: can't create certificate resource.\n");
  goto loser;
  }
  /* Get a client reference to it */
  SSM_ClientGetResourceReference(certResource, &certID);

  switch (request.type) {
    case SEC_CERT_CLASS_USER:
      rv = handle_user_cert(ctrl, cert, collectArgs);
      break;
    case SEC_CERT_CLASS_CA:  
      rv = handle_ca_cert(ctrl, cert, collectArgs);
      break;
    case SEC_CERT_CLASS_SERVER:
      /* why destroy class server certs? */
      CERT_DestroyCertificate(cert);
      break;
    case SEC_CERT_CLASS_EMAIL:
      rv = handle_email_cert(ctrl, cert, collectArgs);
      break;
    default:
      rv = PR_FAILURE;
      SSM_DEBUG("ProcessDecodeAndCreateCertRequest: unknown cert class\n");
  }
  if (rv != PR_SUCCESS) 
    goto loser;
 
  /* create reply */
  msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_CERT_ACTION | SSM_DECODE_TEMP_CERT);
  msg->len  = 0;
  reply.value = certID;
  if (CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
      goto loser;
  }
  SSM_DEBUG("queueing reply: type %lx, len %ld.\n", msg->type, msg->len);
  SSM_SendQMessage(ctrl->m_controlOutQ, 
		   SSM_PRIORITY_NORMAL,
		   msg->type, msg->len, 
		   (char *)msg->data, PR_TRUE);
  if (rv == PR_SUCCESS)
    goto done;
 loser:
  {
    SingleNumMessage reply;
    
    msg->type = (SECItemType) SSM_REPLY_ERR_MESSAGE;
    reply.value = rv;
    CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply);
    if (!msg->data || msg->len == 0)
      goto loser;
    else rv = PR_SUCCESS;
  }
  SSM_DEBUG("queueing reply: type %lx, len %ld.\n", msg->type, msg->len);
  SSM_SendQMessage(ctrl->m_controlOutQ, 
		   SSM_PRIORITY_NORMAL,
		   msg->type, msg->len, 
		   (char *)msg->data, PR_TRUE);
  
  if (rv == PR_SUCCESS)
    rv = PR_FAILURE;
  if (arena) 
    PORT_FreeArena(arena, PR_TRUE);
 done:
  SECITEM_FreeItem(msg, PR_TRUE);
  SSM_FreeResource(&ctrl->super.super);
  if (cert != NULL)
    CERT_DestroyCertificate(cert);
  return;

}

SSMStatus
SSMControlConnection_ProcessDecodeAndCreateTempCert(SSMControlConnection * 
						    ctrl, SECItem * msg)
{
  UserCertImportArg *impArg;

  impArg = SSM_ZNEW(UserCertImportArg);
  if (impArg == NULL) {
    return SSM_FAILURE;
  }

  impArg->ctrl = ctrl;
  impArg->msg = SECITEM_DupItem(msg);
  SSMControlConnection_RecycleItem(msg);
  SSM_GetResourceReference(&ctrl->super.super);
  if (SSM_CreateAndRegisterThread(PR_USER_THREAD,
		     ssm_import_user_cert_thread,
		     (void*)impArg,
		     PR_PRIORITY_NORMAL,
		     PR_LOCAL_THREAD,
		     PR_UNJOINABLE_THREAD, 0) == NULL) {
    SSM_DEBUG("Couldn't create thread for importing cert.");
    return SSM_FAILURE;
  }
  return SSM_ERR_DEFER_RESPONSE;
}

static SSMStatus 
handle_email_cert(SSMControlConnection * ctrl, CERTCertificate * cert, 
		  CERTDERCerts * derCerts)
{
  SSMStatus rv = PR_FAILURE;
  SECItem **rawCerts;
  int numcerts;
  int i;
  
  numcerts = derCerts->numcerts;
  
  rawCerts = (SECItem **) PORT_Alloc(sizeof(SECItem *) * numcerts);
  if ( !rawCerts ) 
    goto loser;
  for ( i = 0; i < numcerts; i++ ) {
    rawCerts[i] = &derCerts->rawCerts[i];
  }
  
  rv = CERT_ImportCerts(cert->dbhandle, certUsageEmailSigner,
			numcerts, rawCerts, NULL, PR_TRUE, PR_FALSE,
			NULL);
  if ( rv == SECSuccess ) {
    rv = CERT_SaveSMimeProfile(cert, NULL, NULL);
  }
  
  PORT_Free(rawCerts);
  
 loser:
  return rv;
}



char *
default_nickname(CERTCertificate *cert, SSMControlConnection *conn)
{   
  char *username = NULL;
  char *caname = NULL;
  char *nickname = NULL;
  char *tmp = NULL;
  int count;
  CERTCertificate *dummycert;
  SSMTextGenContext *cx = NULL;
  PK11SlotInfo *slot=NULL;
  CK_OBJECT_HANDLE keyHandle;

  username = CERT_GetCommonName(&cert->subject);
  if ( username == NULL ) 
    username = strdup("");

  if ( username == NULL ) 
    goto loser;
    
  caname = CERT_GetOrgName(&cert->issuer);
  if ( caname == NULL ) 
    caname = strdup("");
  
  if ( caname == NULL ) 
    goto loser;
  
  count = 1;
  if (nickFmt == NULL) {
    if (SSMTextGen_NewTopLevelContext(NULL, &cx) != SSM_SUCCESS) {
      goto loser;
    }
    if (SSM_FindUTF8StringInBundles(cx, "nick_template", 
				    &nickFmt) != SSM_SUCCESS) {
      goto loser;
    }
    if (SSM_FindUTF8StringInBundles(cx, "nick_template_with_num", 
				    &nickFmtWithNum) != SSM_SUCCESS) {
      goto loser;
    }
  }
  nickname = PR_smprintf(nickFmt, username, caname);
  /*
   * We need to see if the private key exists on a token, if it does
   * then we need to check for nicknames that already exist on the smart
   * card.
   */
  slot = PK11_KeyForCertExists(cert, &keyHandle, conn);
  if (slot == NULL) {
    goto loser;
  }
  if (!PK11_IsInternal(slot)) {
    tmp = PR_smprintf("%s:%s", PK11_GetTokenName(slot), nickname);
    PR_Free(nickname);
    nickname = tmp;
    tmp = NULL;
  }
  tmp = nickname;
  while ( 1 ) {	
    if ( count > 1 ) {
      nickname = PR_smprintf("%s #%d", tmp, count);
    }

    
    if ( nickname == NULL ) 
      goto loser;
 
    _ssm_compress_spaces(nickname);
    if (PK11_IsInternal(slot)) {
      /* look up the nickname to make sure it isn't in use already */
      dummycert = CERT_FindCertByNickname(conn->m_certdb, nickname);
      
    } else {
      /*
       * Check the cert against others that already live on the smart 
       * card.
       */
      dummycert = PK11_FindCertFromNickname(nickname, conn);
      if (dummycert != NULL) {
	/*
	 * Make sure the subject names are different.
	 */ 
	if (CERT_CompareName(&cert->subject, &dummycert->subject) == SECEqual)
	{
	  /*
	   * There is another certificate with the same nickname and
	   * the same subject name on the smart card, so let's use this
	   * nickname.
	   */
	  CERT_DestroyCertificate(dummycert);
	  dummycert = NULL;
	}
      }
    }
    if ( dummycert == NULL ) 
      goto done;
    
    /* found a cert, destroy it and loop */
    CERT_DestroyCertificate(dummycert);
    if (tmp != nickname) PR_Free(nickname);
    count++;
  } /* end of while(1) */
    
loser:
    if ( nickname ) 
      PORT_Free(nickname);
    nickname = NULL;
done:
    if ( caname ) 
      PORT_Free(caname);
    if ( username ) 
      PORT_Free(username);
    if ( cx )
      SSMTextGen_DestroyContext(cx);
    if (slot != NULL) {
      PK11_FreeSlot(slot);
      if (nickname != NULL) {
	tmp = nickname;
	nickname = PL_strchr(tmp, ':');
	if (nickname != NULL) {
	  nickname++;
	  nickname = PL_strdup(nickname);
	  PR_Free(tmp);
	} else {
	  nickname = tmp;
	  tmp = NULL;
	}
      }
    }
    PR_FREEIF(tmp);
    return(nickname);
}

SSMStatus SSM_OKButtonCommandHandler(HTTPRequest * req)
{
  char * tmpStr = NULL;
  SSMStatus rv;

  /* check the base ref */
  rv = SSM_HTTPParamValue(req, "baseRef", &tmpStr);
  if (rv != SSM_SUCCESS ||
    PL_strcmp(tmpStr, "windowclose_doclose_js") != 0) {
    goto loser;
  }
  /* close the window */
  rv = SSM_HTTPCloseAndSleep(req);
  
loser:
  return rv;
}

SSMStatus SSM_CertCAImportCommandHandler1(HTTPRequest *req)
{
    SSMStatus rv;
    char * tmpStr = NULL;

    /* make sure you got the right baseRef */
    rv = SSM_HTTPParamValue(req, "baseRef", &tmpStr);
    if (rv != SSM_SUCCESS ||
        PL_strcmp(tmpStr, "windowclose_doclose_js") != 0) {
        goto loser;
    }
    /* close the first window */
    rv = SSM_HTTPCloseAndSleep(req);
    if (rv != SSM_SUCCESS)
      SSM_DEBUG("Errors closing ImportCAStep1 window: %d\n", rv);
 
    /* figure out the buttons */
    rv =  SSM_HTTPParamValue(req, "do_cancel", &tmpStr);
    if (rv == SSM_SUCCESS) {
      /* cancel button was clicked */
      return SSM_SUCCESS;
    }
    rv = SSM_HTTPParamValue(req, "do_ok", &tmpStr);
    if (rv == SSM_SUCCESS) {
      /* user wants to proceed */
      rv=SSMControlConnection_SendUIEvent(req->ctrlconn, 
				          "get", "import_ca_cert2", 
					  req->target, NULL, 
					  &req->target->m_clientContext, 
					  PR_TRUE);
      if (rv != PR_SUCCESS) {
      /* problem! */
        SSM_DEBUG("Cannot fire second dialog for CA cert importation!\n");
	SSM_NotifyUIEvent(req->target);
      }
    }
    else 
      SSM_DEBUG("Cannot identify button pressed in first importCACert dialog!\n");
loser:
    return rv; 
}


SSMStatus SSM_CertCAImportCommandHandler2(HTTPRequest * req)
{
    SSMStatus rv;
    char * tmpStr = NULL;

    /* if we got here, user must have hit OK for import CA cert */
    acceptcacert = PR_TRUE;

    /* check parameters */
    rv = SSM_HTTPParamValue(req, "acceptssl", &tmpStr);
    if (rv == SSM_SUCCESS) 
      acceptssl = PR_TRUE;
    
    rv = SSM_HTTPParamValue(req, "acceptemail", &tmpStr);
    if (rv == SSM_SUCCESS) 
      acceptmime = PR_TRUE;
    
    rv = SSM_HTTPParamValue(req, "acceptobject", &tmpStr);
    if (rv == SSM_SUCCESS) 
      acceptobjectsign = PR_TRUE;
    
 
    SSM_NotifyUIEvent(req->target);
    return rv;
}

SSMStatus SSM_SubmitFormFromButtonAndFreeTarget(HTTPRequest *req)
{
   SSMStatus rv;

   rv = SSM_SubmitFormFromButtonHandler(req);
   if (req->target) {
     SSM_FreeResource(req->target);
   }
   return rv;
}

SSMStatus SSM_SubmitFormFromButtonHandler(HTTPRequest *req)
{
   SSMResource * res = NULL;
   char * value;
   SSMStatus rv = SSM_FAILURE;

   res = (req->target) ? req->target : (SSMResource *) req->ctrlconn;

   rv = SSM_HTTPParamValue(req, "do_cancel", &value);
   if (rv == SSM_SUCCESS) {
     /* user hit "Cancel", exit */
     res->m_buttonType = SSM_BUTTON_CANCEL;
     goto finished; 
   }
   res->m_buttonType = SSM_BUTTON_OK;
   /* set up the stage to process the main form */ 
   rv = SSM_HTTPParamValue(req, "formName", &value);
   if (rv != SSM_SUCCESS) {
     SSM_DEBUG("Error in SubmitFormHandler: no form name given!\n");
     goto finished;
   }
   if (res->m_formName)  /* hmm... will it crash here? */
     PR_Free(res->m_formName);
   res->m_formName = PL_strdup(value);
        
   rv = SSM_HTTPCloseAndSleep(req);
   if (rv != PR_SUCCESS)
     SSM_DEBUG("SubmitForm: Problem closing dialog box!\n");
   
   return rv;
finished:
   /* no more event handlers called on this event, notify ctrlconn */
   SSM_HTTPCloseWindow(req);
   /* if this is a UIEvent, notify owner connection */
   if (res->m_UILock)
     SSM_NotifyUIEvent(res);
   return rv;
}
   

static void SSM_ImportCACert(void * arg)
{
  CERTCertTrust trust;
  char * nickname = NULL;
  caImportCertArg * certArg = (caImportCertArg *)arg;
  CERTCertificate * cert = certArg->cert;
  SSMControlConnection * ctrl = certArg->ctrl;
  CERTDERCerts * derCerts = certArg->derCerts;
  SSMResource * certObj = NULL;
  SSMResourceID certRID;
  SSMStatus rv;
  char * params;
  char * htmlNickname;

  /* UI asks - do you want to import this cert for 
   * 1) email
   * 2) web sites
   * 3) objects
   * UI sets accept, acceptssl, acceptmime, acceptobjectsign
   */
  acceptssl = acceptmime = acceptobjectsign = acceptcacert = PR_FALSE;


  /* create resource for that cert */
  rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE, cert, 
			  ctrl, &certRID, &certObj);
  if (rv != SSM_SUCCESS) {
    SSM_DEBUG("SSM_ImportCACert: can't create certificate resource!\n");
    goto done;
  }
  
  /* pick a nickname for the cert */
  nickname = CERT_GetCommonName(&cert->subject);
  if (nickname == NULL) {
    char *ou = NULL;
    char *orgName = NULL;
    /* Let's find something we can use as a nickname.*/
    ou = CERT_GetOrgUnitName(&cert->subject);
    orgName = CERT_GetOrgName(&cert->subject);
    PR_ASSERT(orgName || ou);
    nickname = PR_smprintf("%s%s%s", (orgName) ? orgName : "",
			             (orgName  && ou) ? " - " : "",
			             (ou) ? ou : "");
    PR_FREEIF(ou);
    PR_FREEIF(orgName);
  }
  htmlNickname = SSM_ConvertStringToHTMLString(nickname);
  PR_ASSERT(htmlNickname);
  params = PR_smprintf("certresource=%d&nickname=%s", certRID, htmlNickname);
  PR_Free(htmlNickname);
  SSM_LockUIEvent(certObj);
  rv = SSMControlConnection_SendUIEvent(ctrl, "get", "import_ca_cert1", 
					certObj, params, 
					&certObj->m_clientContext, PR_TRUE);
  if (rv != PR_SUCCESS)  {
    SSM_DEBUG("Cannot fire up first import CA cert dialog!\n");
    goto done;
  }
  SSM_WaitUIEvent(certObj, PR_INTERVAL_NO_TIMEOUT);

  if (!acceptcacert)
    goto done;

  PORT_Memset((void *)&trust, 0, sizeof(trust));
  if (acceptssl) 
    trust.sslFlags = CERTDB_VALID_CA | CERTDB_TRUSTED_CA;
  else trust.sslFlags = CERTDB_VALID_CA;
  if (acceptmime) 
    trust.emailFlags = CERTDB_VALID_CA | CERTDB_TRUSTED_CA;
  else trust.emailFlags = CERTDB_VALID_CA;
  if (acceptobjectsign)
    trust.objectSigningFlags = CERTDB_VALID_CA | CERTDB_TRUSTED_CA;
  else
    trust.objectSigningFlags = CERTDB_VALID_CA;

  rv = CERT_AddTempCertToPerm(cert, nickname, &trust);
  if (rv != PR_SUCCESS) {
    /* tell user we're having problems */;
    goto done;
  }
  if (derCerts->numcerts > 1) {
    rv = CERT_ImportCAChain(derCerts->rawCerts + 1, derCerts->numcerts - 1,
			    certUsageSSLCA); 
    if (rv != PR_SUCCESS) 
      /* tell user we're having problems */
      SSM_DEBUG("Cannot import CA chain\n");
  }
 done:
  if (certObj) 
    SSM_FreeResource(certObj);
  if (nickname)
    PR_Free(nickname);
  CERT_DestroyCertificate(cert);
  PR_Free(arg);
  return;
}

SSMStatus SSM_CAPolicyKeywordHandler(SSMTextGenContext * cx)
{
  SSMStatus rv = SSM_FAILURE;
  char * policyString = NULL, * certresStr = NULL;
  CERTCertificate * caCert;
  SSMResource * resource;
  SSMResourceID certRID;
  SSMControlConnection * ctrl;
  
  PR_ASSERT(cx != NULL);
  PR_ASSERT(cx->m_request != NULL);
  PR_ASSERT(cx->m_params != NULL);
  PR_ASSERT(cx->m_result != NULL);
  if (cx == NULL || cx->m_request == NULL || cx->m_params == NULL ||
      cx->m_result == NULL) {
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    goto loser;
  }

  /* find the certificate we're talking about */
  rv = SSM_HTTPParamValue(cx->m_request, "action", &certresStr);
  if (rv != SSM_SUCCESS) {
    goto loser;
  }
  certRID = atoi(certresStr);
  resource = SSMTextGen_GetTargetObject(cx);
  if (!resource) {
    SSM_DEBUG("SSM_CAPolicyKeywordHandler: can't get target object!\n");
    goto loser;
  }
  if (SSM_IsAKindOf(resource, SSM_RESTYPE_CONTROL_CONNECTION)) {
    ctrl = (SSMControlConnection *)resource;
    rv = SSMControlConnection_GetResource(ctrl, certRID, &resource);
    if (rv != SSM_SUCCESS) {
      SSM_DEBUG("SSM_CAPolicyKeywordHandler: can't find cert resource %d\n", 
		certRID);
      goto loser;
    }
  } else
    PR_ASSERT(SSM_IsAKindOf(resource, SSM_RESTYPE_CERTIFICATE));
  
  caCert = ((SSMResourceCert *)resource)->cert;
  
  /* For some reason, this function is going to return policy */
  policyString = CERT_GetCertCommentString(caCert);
  if (!policyString) {
    rv = SSM_GetUTF8Text(cx, "no_ca_policy", &policyString);
    if (rv != SSM_SUCCESS) {
      SSM_DEBUG("Could not find text %s in policy files.\n", "no_ca_policy");
      goto loser;
    }
    SSM_DEBUG("No policy is available for new CA cert: %s!\n", policyString);
  }
  PR_FREEIF(cx->m_result);
  cx->m_result = policyString;
  policyString = NULL;
  rv = SSM_SUCCESS;
  goto done;

 loser:
  SSMTextGen_UTF8StringClear(&cx->m_result);
  rv = SSM_FAILURE;
  SSM_DEBUG("Failed formatting CA policy from signers cert\n");
 done: 
  if (policyString) 
    PR_Free(policyString);
  
  return rv;
}

SSMStatus SSM_CACertKeywordHandler(SSMTextGenContext* cx)
{
  SSMStatus rv = SSM_FAILURE;
  char* pattern = NULL;
  char* key = NULL;
  char * style = NULL;
  const PRIntn CERT_FORMAT = (PRIntn)0;
  const PRIntn CERT_WRAPPER = (PRIntn)1;
  const PRIntn CERT_WRAPPER_NO_COMMENT = (PRIntn)2;
  PRIntn wrapper;
  CERTCertificate * caCert = NULL;
  char * certresStr = NULL;
  SSMControlConnection * ctrl;
  SSMResource * resource;
  SSMResourceID certRID;
  
	
  PR_ASSERT(cx != NULL);
  PR_ASSERT(cx->m_request != NULL);
  PR_ASSERT(cx->m_params != NULL);
  PR_ASSERT(cx->m_result != NULL);
  if (cx == NULL || cx->m_request == NULL || cx->m_params == NULL ||
      cx->m_result == NULL) {
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    goto loser;
  }

  /* find the certificate we're talking about */
  rv = SSM_HTTPParamValue(cx->m_request, "action", &certresStr);
  if (rv != SSM_SUCCESS) {
    goto loser;
  }
  certRID = atoi(certresStr);
  resource = SSMTextGen_GetTargetObject(cx);
  if (!resource) {
    SSM_DEBUG("SSM_CAPolicyKeywordHandler: can't get target object!\n");
    goto loser;
  }
  if (SSM_IsAKindOf(resource, SSM_RESTYPE_CONTROL_CONNECTION)) {
    ctrl = (SSMControlConnection *)resource;
    rv = SSMControlConnection_GetResource(ctrl, certRID, &resource);
    if (rv != SSM_SUCCESS) {
      SSM_DEBUG("SSM_CACertKeywordHandler: can't find cert resource %d\n", 
		certRID);
      goto loser;
    }
  } else
    PR_ASSERT(SSM_IsAKindOf(resource, SSM_RESTYPE_CERTIFICATE));

  caCert = ((SSMResourceCert *)resource)->cert;

  /* form the MessageFormat object */
  /* get the correct wrapper */
  if (CERT_GetCertCommentString(caCert))
    wrapper = CERT_WRAPPER;
  else wrapper = CERT_WRAPPER_NO_COMMENT;
  key = (char *) SSM_At(cx->m_params, wrapper);
  
  /* second, grab and expand the keyword objects */
  rv = SSM_GetAndExpandTextKeyedByString(cx, key, &pattern);
  if (rv != SSM_SUCCESS) {
    goto loser;
  }
  SSM_DebugUTF8String("ca cert info pattern <%s>", pattern);

  style = (char *) SSM_At(cx->m_params, CERT_FORMAT);
  PR_FREEIF(cx->m_result);
  if (!strcmp(style, "pretty"))
    rv = SSM_PrettyFormatCert(caCert, pattern, &cx->m_result, PR_FALSE);
  else if (!strcmp(style, "simple"))
    rv = SSM_FormatCert(caCert, pattern, &cx->m_result);
  else {
    SSM_DEBUG("SSM_CACertKeywordHandler: bad formatting parameter!\n");
    rv =  SSM_ERR_INVALID_FUNC;
  }
  goto done;

loser:
  if (rv == SSM_SUCCESS)
    rv = SSM_FAILURE;
 done:
  if (pattern != NULL) {
    PR_Free(pattern);
  }
  return rv;
}
