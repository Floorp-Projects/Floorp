/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/*
 * This file manages Netscape specific PKCS #11 objects (CRLs, Trust objects,
 * etc).
 */

#include "secport.h"
#include "seccomon.h"
#include "secmod.h"
#include "secmodi.h"
#include "secmodti.h"
#include "pkcs11.h"
#include "pk11func.h"
#include "cert.h"
#include "certi.h" 
#include "secitem.h"
#include "sechash.h" 
#include "secoid.h"

#include "certdb.h" 
#include "secerr.h"
#include "sslerr.h"

#ifndef NSS_3_4_CODE
#define NSS_3_4_CODE
#endif /* NSS_3_4_CODE */
#include "pki3hack.h"
#include "dev3hack.h" 

#include "devm.h" 
#include "pki.h"
#include "pkim.h" 

extern const NSSError NSS_ERROR_NOT_FOUND;

CK_TRUST
pk11_GetTrustField(PK11SlotInfo *slot, PRArenaPool *arena, 
                   CK_OBJECT_HANDLE id, CK_ATTRIBUTE_TYPE type)
{
  CK_TRUST rv = 0;
  SECItem item;

  item.data = NULL;
  item.len = 0;

  if( SECSuccess == PK11_ReadAttribute(slot, id, type, arena, &item) ) {
    PORT_Assert(item.len == sizeof(CK_TRUST));
    PORT_Memcpy(&rv, item.data, sizeof(CK_TRUST));
    /* Damn, is there an endian problem here? */
    return rv;
  }

  return 0;
}

PRBool
pk11_HandleTrustObject(PK11SlotInfo *slot, CERTCertificate *cert, CERTCertTrust *trust)
{
  PRArenaPool *arena;

  CK_ATTRIBUTE tobjTemplate[] = {
    { CKA_CLASS, NULL, 0 },
    { CKA_CERT_SHA1_HASH, NULL, 0 },
  };

  CK_OBJECT_CLASS tobjc = CKO_NETSCAPE_TRUST;
  CK_OBJECT_HANDLE tobjID;
  unsigned char sha1_hash[SHA1_LENGTH];

  CK_TRUST serverAuth, codeSigning, emailProtection, clientAuth;

  PK11_HashBuf(SEC_OID_SHA1, sha1_hash, cert->derCert.data, cert->derCert.len);

  PK11_SETATTRS(&tobjTemplate[0], CKA_CLASS, &tobjc, sizeof(tobjc));
  PK11_SETATTRS(&tobjTemplate[1], CKA_CERT_SHA1_HASH, sha1_hash, 
                SHA1_LENGTH);

  tobjID = pk11_FindObjectByTemplate(slot, tobjTemplate, 
                                     sizeof(tobjTemplate)/sizeof(tobjTemplate[0]));
  if( CK_INVALID_HANDLE == tobjID ) {
    return PR_FALSE;
  }

  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if( NULL == arena ) return PR_FALSE;

  /* Unfortunately, it seems that PK11_GetAttributes doesn't deal
   * well with nonexistant attributes.  I guess we have to check 
   * the trust info fields one at a time.
   */

  /* We could verify CKA_CERT_HASH here */

  /* We could verify CKA_EXPIRES here */


  /* "Purpose" trust information */
  serverAuth = pk11_GetTrustField(slot, arena, tobjID, CKA_TRUST_SERVER_AUTH);
  clientAuth = pk11_GetTrustField(slot, arena, tobjID, CKA_TRUST_CLIENT_AUTH);
  codeSigning = pk11_GetTrustField(slot, arena, tobjID, CKA_TRUST_CODE_SIGNING);
  emailProtection = pk11_GetTrustField(slot, arena, tobjID, 
						CKA_TRUST_EMAIL_PROTECTION);
  /* Here's where the fun logic happens.  We have to map back from the 
   * key usage, extended key usage, purpose, and possibly other trust values 
   * into the old trust-flags bits.  */

  /* First implementation: keep it simple for testing.  We can study what other
   * mappings would be appropriate and add them later.. fgmr 20000724 */

  if ( serverAuth ==  CKT_NETSCAPE_TRUSTED ) {
    trust->sslFlags |= CERTDB_VALID_PEER | CERTDB_TRUSTED;
  }

  if ( serverAuth == CKT_NETSCAPE_TRUSTED_DELEGATOR ) {
    trust->sslFlags |= CERTDB_VALID_CA | CERTDB_TRUSTED_CA | 
							CERTDB_NS_TRUSTED_CA;
  }
  if ( clientAuth == CKT_NETSCAPE_TRUSTED_DELEGATOR ) {
    trust->sslFlags |=  CERTDB_TRUSTED_CLIENT_CA ;
  }

  if ( emailProtection == CKT_NETSCAPE_TRUSTED ) {
    trust->emailFlags |= CERTDB_VALID_PEER | CERTDB_TRUSTED;
  }

  if ( emailProtection == CKT_NETSCAPE_TRUSTED_DELEGATOR ) {
    trust->emailFlags |= CERTDB_VALID_CA | CERTDB_TRUSTED_CA | CERTDB_NS_TRUSTED_CA;
  }

  if( codeSigning == CKT_NETSCAPE_TRUSTED ) {
    trust->objectSigningFlags |= CERTDB_VALID_PEER | CERTDB_TRUSTED;
  }

  if( codeSigning == CKT_NETSCAPE_TRUSTED_DELEGATOR ) {
    trust->objectSigningFlags |= CERTDB_VALID_CA | CERTDB_TRUSTED_CA | CERTDB_NS_TRUSTED_CA;
  }

  /* There's certainly a lot more logic that can go here.. */

  PORT_FreeArena(arena, PR_FALSE);

  return PR_TRUE;
}

static SECStatus
pk11_CollectCrls(PK11SlotInfo *slot, CK_OBJECT_HANDLE crlID, void *arg)
{
    SECItem derCrl;
    CERTCrlHeadNode *head = (CERTCrlHeadNode *) arg;
    CERTCrlNode *new_node = NULL;
    CK_ATTRIBUTE fetchCrl[3] = {
	 { CKA_VALUE, NULL, 0},
	 { CKA_NETSCAPE_KRL, NULL, 0},
	 { CKA_NETSCAPE_URL, NULL, 0},
    };
    const int fetchCrlSize = sizeof(fetchCrl)/sizeof(fetchCrl[2]);
    CK_RV crv;
    SECStatus rv = SECFailure;

    crv = PK11_GetAttributes(head->arena,slot,crlID,fetchCrl,fetchCrlSize);
    if (CKR_OK != crv) {
	PORT_SetError(PK11_MapError(crv));
	goto loser;
    }

    if (!fetchCrl[1].pValue) {
	PORT_SetError(SEC_ERROR_CRL_INVALID);
	goto loser;
    }

    new_node = (CERTCrlNode *)PORT_ArenaAlloc(head->arena, sizeof(CERTCrlNode));
    if (new_node == NULL) {
        goto loser;
    }

    if (*((CK_BBOOL *)fetchCrl[1].pValue))
        new_node->type = SEC_KRL_TYPE;
    else
        new_node->type = SEC_CRL_TYPE;

    derCrl.type = siBuffer;
    derCrl.data = (unsigned char *)fetchCrl[0].pValue;
    derCrl.len = fetchCrl[0].ulValueLen;
    new_node->crl=CERT_DecodeDERCrl(head->arena,&derCrl,new_node->type);
    if (new_node->crl == NULL) {
	goto loser;
    }

    if (fetchCrl[2].pValue) {
        int nnlen = fetchCrl[2].ulValueLen;
        new_node->crl->url  = (char *)PORT_ArenaAlloc(head->arena, nnlen+1);
        if ( !new_node->crl->url ) {
            goto loser;
        }
        PORT_Memcpy(new_node->crl->url, fetchCrl[2].pValue, nnlen);
        new_node->crl->url[nnlen] = 0;
    } else {
        new_node->crl->url = NULL;
    }


    new_node->next = NULL;
    if (head->last) {
        head->last->next = new_node;
        head->last = new_node;
    } else {
        head->first = head->last = new_node;
    }
    rv = SECSuccess;

loser:
    return(rv);
}

/*
 * Return a list of all the CRLs 
 */
SECStatus
PK11_LookupCrls(CERTCrlHeadNode *nodes, int type, void *wincx) {
    pk11TraverseSlot creater;
    CK_ATTRIBUTE theTemplate[2];
    CK_ATTRIBUTE *attrs;
    CK_OBJECT_CLASS certClass = CKO_NETSCAPE_CRL;

    attrs = theTemplate;
    PK11_SETATTRS(attrs, CKA_CLASS, &certClass, sizeof(certClass)); attrs++;
    if (type != -1) {
	CK_BBOOL isKrl = (CK_BBOOL) (type == SEC_KRL_TYPE);
        PK11_SETATTRS(attrs, CKA_NETSCAPE_KRL, &isKrl, sizeof(isKrl)); attrs++;
    }

    creater.callback = pk11_CollectCrls;
    creater.callbackArg = (void *) nodes;
    creater.findTemplate = theTemplate;
    creater.templateCount = (attrs - theTemplate);

    return pk11_TraverseAllSlots(PK11_TraverseSlot, &creater, wincx);
}

/*
 * return the crl associated with a derSubjectName 
 */
SECItem *
PK11_FindCrlByName(PK11SlotInfo **slot, CK_OBJECT_HANDLE *crlHandle,
					 SECItem *name, int type, char **url)
{
#ifdef NSS_CLASSIC
    CK_OBJECT_CLASS crlClass = CKO_NETSCAPE_CRL;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_SUBJECT, NULL, 0 },
	{ CKA_CLASS, NULL, 0 },
	{ CKA_NETSCAPE_KRL, NULL, 0 },
    };
    CK_ATTRIBUTE crlData[] = {
	{ CKA_VALUE, NULL, 0 },
	{ CKA_NETSCAPE_URL, NULL, 0 },
    };
    /* if you change the array, change the variable below as well */
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    CK_BBOOL ck_true = CK_TRUE;
    CK_BBOOL ck_false = CK_FALSE;
    CK_OBJECT_HANDLE crlh = CK_INVALID_HANDLE;
    CK_ATTRIBUTE *attrs = theTemplate;
    CK_RV crv;
    SECItem *derCrl = NULL;

    PK11_SETATTRS(attrs, CKA_SUBJECT, name->data, name->len); attrs++;
    PK11_SETATTRS(attrs, CKA_CLASS, &crlClass, sizeof(crlClass)); attrs++;
    PK11_SETATTRS(attrs, CKA_NETSCAPE_KRL, (type == SEC_CRL_TYPE) ? 
			&ck_false : &ck_true, sizeof (CK_BBOOL)); attrs++;

    if (*slot) {
    	crlh = pk11_FindObjectByTemplate(*slot,theTemplate,tsize);
    } else {
	PK11SlotList *list = PK11_GetAllTokens(CKM_INVALID_MECHANISM,
							PR_FALSE,PR_TRUE,NULL);
	PK11SlotListElement *le;

	/* loop through all the fortezza tokens */
	for (le = list->head; le; le = le->next) {
	    crlh = pk11_FindObjectByTemplate(le->slot,theTemplate,tsize);
	    if (crlh != CK_INVALID_HANDLE) {
		*slot = PK11_ReferenceSlot(le->slot);
		break;
	    }
	}
	PK11_FreeSlotList(list);
    }
    
    if (crlh == CK_INVALID_HANDLE) {
	PORT_SetError(SEC_ERROR_NO_KRL);
	return NULL;
    }
    crv = PK11_GetAttributes(NULL,*slot,crlh,crlData,2);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError (crv));
	goto loser;
    }

    derCrl = (SECItem *)PORT_ZAlloc(sizeof(SECItem));    
    if (derCrl == NULL) {
	goto loser;
    }

    derCrl->data = crlData[0].pValue;
    derCrl->len = crlData[0].ulValueLen;

    if (crlHandle) {
        *crlHandle = crlh;
    }

    if ((url) && crlData[1].ulValueLen != 0) {
	/* make sure it's a null terminated string */
	*url = PORT_ZAlloc (crlData[1].ulValueLen+1);
	if (*url) {
	    PORT_Memcpy(*url,crlData[1].pValue,crlData[1].ulValueLen);
	}
    }
	

loser:
    if (!derCrl) {
	if (crlData[0].pValue) PORT_Free(crlData[0].pValue);
    }
    if (crlData[1].pValue) PORT_Free(crlData[1].pValue);
    return derCrl;
#else
    NSSCRL **crls, **crlp, *crl;
    NSSDER subject;
    SECItem *rvItem;
    NSSTrustDomain *td = STAN_GetDefaultTrustDomain();
    NSSITEM_FROM_SECITEM(&subject, name);
    if (*slot) {
	nssCryptokiObject **instances;
	nssPKIObjectCollection *collection;
	nssTokenSearchType tokenOnly = nssTokenSearchType_TokenOnly;
	NSSToken *token = PK11Slot_GetNSSToken(*slot);
	collection = nssCRLCollection_Create(td, NULL);
	if (!collection) {
	    return NULL;
	}
	instances = nssToken_FindCRLsBySubject(token, NULL, &subject, 
	                                       tokenOnly, 0, NULL);
	nssPKIObjectCollection_AddInstances(collection, instances, 0);
	nss_ZFreeIf(instances);
	crls = nssPKIObjectCollection_GetCRLs(collection, NULL, 0, NULL);
	nssPKIObjectCollection_Destroy(collection);
    } else {
	crls = nssTrustDomain_FindCRLsBySubject(td, &subject);
    }
    if ((!crls) || (*crls == NULL)) {
	if (crls) {
	    nssCRLArray_Destroy(crls);
	}
	if (NSS_GetError() == NSS_ERROR_NOT_FOUND) {
	    PORT_SetError(SEC_ERROR_CRL_NOT_FOUND);
	}
	return NULL;
    }
    crl = NULL;
    for (crlp = crls; *crlp; crlp++) {
	if ((!(*crlp)->isKRL && type == SEC_CRL_TYPE) ||
	    ((*crlp)->isKRL && type != SEC_CRL_TYPE)) 
	{
	    crl = nssCRL_AddRef(*crlp);
	    break;
	}
    }
    nssCRLArray_Destroy(crls);
    if (!crl) { 
	/* CRL collection was found, but no interesting CRL's were on it.
	 * Not an error */
	PORT_SetError(SEC_ERROR_CRL_NOT_FOUND);
	return NULL;
    }
    if (crl->url) {
	*url = PORT_Strdup(crl->url);
	if (!*url) {
	    nssCRL_Destroy(crl);
	    return NULL;
	}
    } else {
	*url = NULL;
    }
    rvItem = SECITEM_AllocItem(NULL, NULL, crl->encoding.size);
    if (!rvItem) {
	PORT_Free(*url);
	nssCRL_Destroy(crl);
	return NULL;
    }
    memcpy(rvItem->data, crl->encoding.data, crl->encoding.size);
    *slot = PK11_ReferenceSlot(crl->object.instances[0]->token->pk11slot);
    *crlHandle = crl->object.instances[0]->handle;
    nssCRL_Destroy(crl);
    return rvItem;
#endif
}

CK_OBJECT_HANDLE
PK11_PutCrl(PK11SlotInfo *slot, SECItem *crl, SECItem *name, 
							char *url, int type)
{
#ifdef NSS_CLASSIC
    CK_OBJECT_CLASS crlClass = CKO_NETSCAPE_CRL;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_SUBJECT, NULL, 0 },
	{ CKA_CLASS, NULL, 0 },
	{ CKA_NETSCAPE_KRL, NULL, 0 },
	{ CKA_NETSCAPE_URL, NULL, 0 },
	{ CKA_VALUE, NULL, 0 },
	{ CKA_TOKEN, NULL, 0 }
    };
    /* if you change the array, change the variable below as well */
    int tsize;
    CK_BBOOL ck_true = CK_TRUE;
    CK_BBOOL ck_false = CK_FALSE;
    CK_OBJECT_HANDLE crlh = CK_INVALID_HANDLE;
    CK_ATTRIBUTE *attrs = theTemplate;
    CK_SESSION_HANDLE rwsession;
    CK_RV crv;

    PK11_SETATTRS(attrs, CKA_SUBJECT, name->data, name->len); attrs++;
    PK11_SETATTRS(attrs, CKA_CLASS, &crlClass, sizeof(crlClass)); attrs++;
    PK11_SETATTRS(attrs, CKA_NETSCAPE_KRL, (type == SEC_CRL_TYPE) ? 
			&ck_false : &ck_true, sizeof (CK_BBOOL)); attrs++;
    if (url) {
	PK11_SETATTRS(attrs, CKA_NETSCAPE_URL, url, PORT_Strlen(url)+1); attrs++;
    }
    PK11_SETATTRS(attrs, CKA_VALUE,crl->data,crl->len); attrs++;
    PK11_SETATTRS(attrs, CKA_TOKEN, &ck_true,sizeof(CK_BBOOL)); attrs++;

    tsize = attrs - &theTemplate[0];
    PORT_Assert(tsize <= sizeof(theTemplate)/sizeof(theTemplate[0]));

    rwsession = PK11_GetRWSession(slot);
    if (rwsession == CK_INVALID_SESSION) {
	PORT_SetError(SEC_ERROR_READ_ONLY);
	return crlh;
    }

    crv = PK11_GETTAB(slot)->
                        C_CreateObject(rwsession,theTemplate,tsize,&crlh);
    if (crv != CKR_OK) {
        PORT_SetError( PK11_MapError(crv) );
    }

    PK11_RestoreROSession(slot,rwsession);

    return crlh;
#else
    NSSItem derCRL, derSubject;
    NSSToken *token = PK11Slot_GetNSSToken(slot);
    nssCryptokiObject *object;
    PRBool isKRL = (type == SEC_CRL_TYPE) ? PR_FALSE : PR_TRUE;
    CK_OBJECT_HANDLE rvH;

    NSSITEM_FROM_SECITEM(&derSubject, name);
    NSSITEM_FROM_SECITEM(&derCRL, crl);

    object = nssToken_ImportCRL(token, NULL, 
                                &derSubject, &derCRL, isKRL, url, PR_TRUE);

    if (object) {
	rvH = object->handle;
	nssCryptokiObject_Destroy(object);
    } else {
	rvH = CK_INVALID_HANDLE;
    }
    return rvH;
#endif
}


/*
 * delete a crl.
 */
SECStatus
SEC_DeletePermCRL(CERTSignedCrl *crl)
{
#ifdef NSS_CLASSIC
    PK11SlotInfo *slot = crl->slot;
    CK_RV crv;

    if (slot == NULL) {
	/* shouldn't happen */
	PORT_SetError( SEC_ERROR_CRL_INVALID);
	return SECFailure;
    }

    crv = PK11_DestroyTokenObject(slot,crl->pkcs11ID);
    if (crv != CKR_OK) {
        PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }
    crl->slot = NULL;
    PK11_FreeSlot(slot);
    return SECSuccess;
#else
    PRStatus status;
    NSSToken *token;
    nssCryptokiObject *object;
    PK11SlotInfo *slot = crl->slot;

    if (slot == NULL) {
        PORT_Assert(slot);
	/* shouldn't happen */
	PORT_SetError( SEC_ERROR_CRL_INVALID);
	return SECFailure;
    }
    token = PK11Slot_GetNSSToken(slot);

    object = nss_ZNEW(NULL, nssCryptokiObject);
    object->token = nssToken_AddRef(token);
    object->handle = crl->pkcs11ID;
    object->isTokenObject = PR_TRUE;

    status = nssToken_DeleteStoredObject(object);

    nssCryptokiObject_Destroy(object);
    return (status == PR_SUCCESS) ? SECSuccess : SECFailure;
#endif
}

/*
 * return the certificate associated with a derCert 
 */
SECItem *
PK11_FindSMimeProfile(PK11SlotInfo **slot, char *emailAddr,
				 SECItem *name, SECItem **profileTime)
{
    CK_OBJECT_CLASS smimeClass = CKO_NETSCAPE_SMIME;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_SUBJECT, NULL, 0 },
	{ CKA_CLASS, NULL, 0 },
	{ CKA_NETSCAPE_EMAIL, NULL, 0 },
    };
    CK_ATTRIBUTE smimeData[] =  {
	{ CKA_SUBJECT, NULL, 0 },
	{ CKA_VALUE, NULL, 0 },
    };
    /* if you change the array, change the variable below as well */
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    CK_OBJECT_HANDLE smimeh = CK_INVALID_HANDLE;
    CK_ATTRIBUTE *attrs = theTemplate;
    CK_RV crv;
    SECItem *emailProfile = NULL;

    PK11_SETATTRS(attrs, CKA_SUBJECT, name->data, name->len); attrs++;
    PK11_SETATTRS(attrs, CKA_CLASS, &smimeClass, sizeof(smimeClass)); attrs++;
    PK11_SETATTRS(attrs, CKA_NETSCAPE_EMAIL, emailAddr, strlen(emailAddr)); 
								    attrs++;

    if (*slot) {
    	smimeh = pk11_FindObjectByTemplate(*slot,theTemplate,tsize);
    } else {
	PK11SlotList *list = PK11_GetAllTokens(CKM_INVALID_MECHANISM,
							PR_FALSE,PR_TRUE,NULL);
	PK11SlotListElement *le;

	/* loop through all the fortezza tokens */
	for (le = list->head; le; le = le->next) {
	    smimeh = pk11_FindObjectByTemplate(le->slot,theTemplate,tsize);
	    if (smimeh != CK_INVALID_HANDLE) {
		*slot = PK11_ReferenceSlot(le->slot);
		break;
	    }
	}
	PK11_FreeSlotList(list);
    }
    
    if (smimeh == CK_INVALID_HANDLE) {
	PORT_SetError(SEC_ERROR_NO_KRL);
	return NULL;
    }

    if (profileTime) {
    	PK11_SETATTRS(smimeData, CKA_NETSCAPE_SMIME_TIMESTAMP, NULL, 0);
    } 
    
    crv = PK11_GetAttributes(NULL,*slot,smimeh,smimeData,2);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError (crv));
	goto loser;
    }

    if (!profileTime) {
	SECItem profileSubject;

	profileSubject.data = (unsigned char*) smimeData[0].pValue;
	profileSubject.len = smimeData[0].ulValueLen;
	if (!SECITEM_ItemsAreEqual(&profileSubject,name)) {
	    goto loser;
	}
    }

    emailProfile = (SECItem *)PORT_ZAlloc(sizeof(SECItem));    
    if (emailProfile == NULL) {
	goto loser;
    }

    emailProfile->data = (unsigned char*) smimeData[1].pValue;
    emailProfile->len = smimeData[1].ulValueLen;

    if (profileTime) {
	*profileTime = (SECItem *)PORT_ZAlloc(sizeof(SECItem));    
	if (*profileTime) {
	    (*profileTime)->data = (unsigned char*) smimeData[0].pValue;
	    (*profileTime)->len = smimeData[0].ulValueLen;
	}
    }

loser:
    if (emailProfile == NULL) {
	if (smimeData[1].pValue) {
	    PORT_Free(smimeData[1].pValue);
	}
    }
    if (profileTime == NULL || *profileTime == NULL) {
	if (smimeData[0].pValue) {
	    PORT_Free(smimeData[0].pValue);
	}
    }
    return emailProfile;
}


SECStatus
PK11_SaveSMimeProfile(PK11SlotInfo *slot, char *emailAddr, SECItem *derSubj,
				 SECItem *emailProfile,  SECItem *profileTime)
{
    CK_OBJECT_CLASS smimeClass = CKO_NETSCAPE_SMIME;
    CK_BBOOL ck_true = CK_TRUE;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_CLASS, NULL, 0 },
	{ CKA_TOKEN, NULL, 0 },
	{ CKA_SUBJECT, NULL, 0 },
	{ CKA_NETSCAPE_EMAIL, NULL, 0 },
	{ CKA_NETSCAPE_SMIME_TIMESTAMP, NULL, 0 },
	{ CKA_VALUE, NULL, 0 }
    };
    /* if you change the array, change the variable below as well */
    int realSize = 0;
    CK_OBJECT_HANDLE smimeh = CK_INVALID_HANDLE;
    CK_ATTRIBUTE *attrs = theTemplate;
    CK_SESSION_HANDLE rwsession;
    PK11SlotInfo *free_slot = NULL;
    CK_RV crv;
#ifdef DEBUG
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
#endif

    PK11_SETATTRS(attrs, CKA_CLASS, &smimeClass, sizeof(smimeClass)); attrs++;
    PK11_SETATTRS(attrs, CKA_TOKEN, &ck_true, sizeof(ck_true)); attrs++;
    PK11_SETATTRS(attrs, CKA_SUBJECT, derSubj->data, derSubj->len); attrs++;
    PK11_SETATTRS(attrs, CKA_NETSCAPE_EMAIL, 
				emailAddr, PORT_Strlen(emailAddr)+1); attrs++;
    if (profileTime) {
	PK11_SETATTRS(attrs, CKA_NETSCAPE_SMIME_TIMESTAMP, profileTime->data,
	                                            profileTime->len); attrs++;
	PK11_SETATTRS(attrs, CKA_VALUE,emailProfile->data,
	                                            emailProfile->len); attrs++;
    }
    realSize = attrs - theTemplate;
    PORT_Assert (realSize <= tsize);

    if (slot == NULL) {
	free_slot = slot = PK11_GetInternalKeySlot();
	/* we need to free the key slot in the end!!! */
    }

    rwsession = PK11_GetRWSession(slot);
    if (rwsession == CK_INVALID_SESSION) {
	PORT_SetError(SEC_ERROR_READ_ONLY);
	if (free_slot) {
	    PK11_FreeSlot(free_slot);
	}
	return SECFailure;
    }

    crv = PK11_GETTAB(slot)->
                        C_CreateObject(rwsession,theTemplate,realSize,&smimeh);
    if (crv != CKR_OK) {
        PORT_SetError( PK11_MapError(crv) );
    }

    PK11_RestoreROSession(slot,rwsession);

    if (free_slot) {
	PK11_FreeSlot(free_slot);
    }
    return SECSuccess;
}


CERTSignedCrl * crl_storeCRL (PK11SlotInfo *slot,char *url,
                  CERTSignedCrl *newCrl, SECItem *derCrl, int type);

/* import the CRL into the token */

CERTSignedCrl* PK11_ImportCRL(PK11SlotInfo * slot, SECItem *derCRL, char *url,
    int type, void *wincx, PRInt32 importOptions, PRArenaPool* arena,
    PRInt32 decodeoptions)
{
    CERTSignedCrl *newCrl, *crl;
    SECStatus rv;
    CERTCertificate *caCert = NULL;

    newCrl = crl = NULL;

    do {
        newCrl = CERT_DecodeDERCrlWithFlags(arena, derCRL, type,
                                            decodeoptions);
        if (newCrl == NULL) {
            if (type == SEC_CRL_TYPE) {
                /* only promote error when the error code is too generic */
                if (PORT_GetError () == SEC_ERROR_BAD_DER)
                    PORT_SetError(SEC_ERROR_CRL_INVALID);
	        } else {
                PORT_SetError(SEC_ERROR_KRL_INVALID);
            }
            break;		
        }

        if (0 == (importOptions & CRL_IMPORT_BYPASS_CHECKS)){
            CERTCertDBHandle* handle = CERT_GetDefaultCertDB();
            PR_ASSERT(handle != NULL);
            caCert = CERT_FindCertByName (handle,
                                          &newCrl->crl.derName);
            if (caCert == NULL) {
                PORT_SetError(SEC_ERROR_UNKNOWN_ISSUER);	    
                break;
            }

            /* If caCert is a v3 certificate, make sure that it can be used for
               crl signing purpose */
            rv = CERT_CheckCertUsage (caCert, KU_CRL_SIGN);
            if (rv != SECSuccess) {
                break;
            }

            rv = CERT_VerifySignedData(&newCrl->signatureWrap, caCert,
                                       PR_Now(), wincx);
            if (rv != SECSuccess) {
                if (type == SEC_CRL_TYPE) {
                    PORT_SetError(SEC_ERROR_CRL_BAD_SIGNATURE);
                } else {
                    PORT_SetError(SEC_ERROR_KRL_BAD_SIGNATURE);
                }
                break;
            }
        }

	crl = crl_storeCRL(slot, url, newCrl, derCRL, type);

    } while (0);

    if (crl == NULL) {
	SEC_DestroyCrl (newCrl);
    }
    if (caCert) {
        CERT_DestroyCertificate(caCert);
    }
    return (crl);
}
