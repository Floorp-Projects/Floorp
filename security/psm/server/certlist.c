/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#include "certlist.h"
#include "nlsutil.h"
#include "base64.h"
#include "cert.h"
#include "certdb.h"
#include "textgen.h"
#include "minihttp.h"
#include "advisor.h"
#include "slist.h"
#include "nspr.h"
#include "nlslayer.h"

enum
{
    CERTLIST_PARAM_KEY = (PRIntn) 0,
    CERTLIST_PARAM_USAGE,
    CERTLIST_PARAM_PREFIX,
    /* CERTLIST_PARAM_WRAPPER,  -jp not used, as we format all certs together */
    CERTLIST_PARAM_SUFFIX,
    CERTLIST_PARAM_COUNT
};

SSMSortedListFn list_functions;

#define WRAPPER "cert_list_item"
#define EMAIL_WRAPPER "cert_list_email_item"

typedef enum
{
    clNick,
    clCertID, /* issuer and serial number */
    clRID,
    clEmail
} ssmCLCertKey;

typedef struct 
{
    SSMTextGenContext *cx;
    char *output;
    char *temp;
    char *fmt;
    char *efmt;
    void *datefmt;
    CERTCertDBHandle * db;
    SSMSortedList **hash; /* Hash to contain list of certs to display */
    ssmCLCertKey key;
    ssmCLCertUsage usage;
} ssmCLState;

static SSMStatus format_raw_cert(CERTCertificate *cert, char *wrapper,
                                 void*dfmt, char **result);
/* Hash table functions */

PRIntn ssm_certhash_rid_compare(const void *v1, const void *v2);
PLHashNumber ssm_certhash_issuersn_hash(const void *v);
PRIntn ssm_certhash_issuersn_compare(const void *v1, const void *v2);
PRIntn ssm_pointer_compare(const void *v1, const void *v2);
PRIntn ssm_certhash_cert_compare(const void *v1, const void *v2);
void * ssm_certhash_alloc_table(void *state, PRSize size);
void ssm_certhash_free_table(void *state, void *item);
PLHashEntry * ssm_certhash_alloc_entry(void *state, const void *key);
void ssm_certhash_free_entry(void *s, PLHashEntry *he, PRUintn flag);
void certlist_free_item(SECItem *item);
void certlist_dereturnify(char *s);
SSMStatus ssm_certlist_get_b64_cert_key(CERTCertificate *cert, char **result);
void * certlist_get_cert_key(ssmCLState *state, CERTCertificate *cert, ssmCLCertUsage usage);
PRBool certlist_include_cert(CERTCertificate * cert, ssmCLState *state, 
                      ssmCLCertUsage * usage);
SECStatus certlist_get_cert(CERTCertificate *cert, SECItem *dbkey, void *arg);
SSMStatus certlist_display_cert(PRIntn index, void * arg, 
                                void * key, void * itemdata);
PRIntn certlist_wrap_cert(PLHashEntry *he, PRIntn index, void *arg);
SSMStatus certlist_get_usage(ssmCLState *state);
SSMStatus certlist_get_key_type(ssmCLState *state);
SSMStatus ssm_populate_key_hash(ssmCLState * state);

PRIntn
certlist_rid_compare(const void *v1, const void *v2)
{
  SSMResourceID rid1 = (SSMResourceID) v1;
    SSMResourceID rid2 = (SSMResourceID) v2;
    PRIntn rv;

    if (rid1 < rid2)
	rv = -1;
    else if (rid1 > rid2)
	rv = 1;
    else
	rv = 0;

    return rv;
}

PRIntn 
certlist_compare_strings(const void * v1, const void *v2)
{
    return PL_strcasecmp((char *)v1, (char *)v2);
}

PRIntn 
certlist_issuersn_compare(const void *v1, const void *v2)
{ 
    PRIntn rv;
    SECComparison cmp = SECLessThan; /* this is arbitrary */

    const CERTIssuerAndSN *i1 = (const CERTIssuerAndSN *) v1;
    const CERTIssuerAndSN *i2 = (const CERTIssuerAndSN *) v2;

    PR_ASSERT(i1);
    PR_ASSERT(i2);
    if (!i1 || !i2)
	goto loser;

    cmp = SECITEM_CompareItem(&(i1->derIssuer), &(i2->derIssuer));
    if (cmp == SECEqual)
	cmp = SECITEM_CompareItem(&(i2->serialNumber), &(i2->serialNumber));

 loser:
    /* Don't need to free i1 and i2, since they are allocated out of
       the certs' arenas. They will be freed when the certs are freed. */

    rv = (PRIntn) cmp;
    return rv;
}

/* Free ops for cert list */
void
certlist_free_data(void * data)
{
    /* Free the cert, then (if asked) free the entry. */
    PR_Free(((ssmCertData *)data)->certEntry);
    PR_Free((ssmCertData *)data);
}

void certlist_none(void * data)
{
    return;
}

void certlist_free_string(void * key)
{
    PR_Free((char *)key);
}

/* Utility routines for cert list keyword handler below */
void
certlist_free_item(SECItem *item)
{
    if (item)
    {
	if (item->data)
	    PR_Free(item->data);
	PR_Free(item);
    }
}

void
certlist_dereturnify(char *s)
{
    char *c = s;

    while(*c)
    {
        if ((*c == '\015') || (*c == '\012'))
        {
            char *c2 = c;
            while ((*c2 == '\015') || (*c2 == '\012'))
                c2++;
            /* move the rest of the string on top of c */
            memmove(c, c2, strlen(c2));
        }
	c++;
    }
}

SSMStatus
ssm_certlist_get_b64_cert_key(CERTCertificate *cert, char **result)
{
    SECItem *certKeyItem;
    CERTIssuerAndSN *isn = NULL;
    SSMStatus rv = SSM_SUCCESS;

    PR_ASSERT(cert && result);
    *result = NULL; /* in case we fail */

    isn = CERT_GetCertIssuerAndSN(NULL, cert);
    if (!isn)
	goto loser;

    certKeyItem = (SECItem *) PR_CALLOC(sizeof(SECItem));
    if (!certKeyItem)
	goto loser;

    certKeyItem->len = isn->serialNumber.len + isn->derIssuer.len;
    certKeyItem->data = (unsigned char*)PR_CALLOC(certKeyItem->len);
    if (certKeyItem->data == NULL)
	goto loser;

    /* copy the serialNumber */
    memcpy(certKeyItem->data, isn->serialNumber.data,
	   isn->serialNumber.len);

    /* copy the issuer */
    memcpy( &(certKeyItem->data[isn->serialNumber.len]),
	    isn->derIssuer.data, isn->derIssuer.len);

    /* b64 the item */
    *result = BTOA_ConvertItemToAscii(certKeyItem);
    certlist_dereturnify(*result);
    goto done;
 loser:
    rv = SSM_FAILURE;
 done:
    certlist_free_item(certKeyItem);
    return rv;
}

/* get the appropriate key depending on what key type we have chosen. */
void *
certlist_get_cert_key(ssmCLState *state, CERTCertificate *cert, ssmCLCertUsage usage)
{
    char *result_ch = NULL;
    void *result = NULL;
    SSMStatus rv = SSM_SUCCESS;

    PR_ASSERT(cert);
    if (!cert) 
        return result;

    switch(state->key)
    {
    case clCertID:
	rv = ssm_certlist_get_b64_cert_key(cert, &result_ch);
	PR_ASSERT(rv == SSM_SUCCESS);
	result = result_ch;
	break;
    case clRID:
	PR_ASSERT(!"Not yet implemented.");
	break;
    case clNick:
        /* duplicate the nickname */
        if (usage == clEmailRecipient) {
            PR_ASSERT(cert && cert->emailAddr);
            result = PR_CALLOC(strlen(cert->emailAddr)+1);
            if (result)
                PL_strcpy((char*)result, cert->emailAddr);
        } else {
            PR_ASSERT(cert && cert->nickname);
            result = PR_CALLOC(strlen(cert->nickname)+1);
            if (result)
                PL_strcpy((char *) result, cert->nickname);
        }
        break;   
    case clEmail:
        PR_ASSERT(cert && cert->emailAddr);
        result = PR_CALLOC(strlen(cert->emailAddr)+1);
        if (result)
            PL_strcpy((char*)result, cert->emailAddr);
        break;
    default:
        PR_ASSERT(0); /* scream if we defaulted here */
    }
    return result;
}


PRBool
certlist_include_cert(CERTCertificate * cert, ssmCLState *state, 
                      ssmCLCertUsage * usage)
{
    PRBool usercert = PR_FALSE;
    char *nickname;
    char *emailAddr;
    CERTCertTrust *trust;
    ssmCLCertUsage thisUsage = clNoUsage;

    /* jsw said: look at the base cert info stuff, not the dbEntry since
     * we may have been called from a PKCS#11 module */
    nickname = cert->nickname;
    trust = cert->trust;
    emailAddr = cert->emailAddr;

    /* Don't display certs that are invisible */
    if ( ( ( trust->sslFlags & CERTDB_INVISIBLE_CA ) ||
           (trust->emailFlags & CERTDB_INVISIBLE_CA ) ||
           (trust->objectSigningFlags & CERTDB_INVISIBLE_CA ) ) ) 
        goto dontInclude;
    
    if (nickname) {
        /* 
           it's a cert I have named. Could be a personal cert of mine,
           an SSL server cert, or a CA cert. Find out what kind.
        */

        if (state->usage == clSSLClient) {
            if (trust->sslFlags & CERTDB_USER) {
                /* It's an SSL cert I own. */
                thisUsage = clSSLClient;
                goto include;
            }
            else {
                goto dontInclude;
            }
        }

        if (state->usage == clEmailSigner) {
            if (trust->emailFlags & CERTDB_USER) {
                /* It's an email cert I own. */
                thisUsage = clEmailSigner;
                goto include;
            }
            else {
                goto dontInclude;
            }
        }

        if (state->usage == clAllMine) {
            if ((trust->sslFlags & CERTDB_USER) ||
                (trust->emailFlags & CERTDB_USER) ||
                (trust->objectSigningFlags & CERTDB_USER)) {
                /* It's a cert I own. */
                usercert = PR_TRUE;
                thisUsage = clAllMine;
                goto include;
            }
            else {
                goto dontInclude;
            }
        }
            
        /* it is an SSL site */
        if (trust->sslFlags & CERTDB_VALID_PEER) {
            thisUsage = clSSLServer;
            goto include;
        }
            
        /* CA certs */
        if ((trust->sslFlags & CERTDB_VALID_CA) ||
            (trust->emailFlags & CERTDB_VALID_CA) ||
            (trust->objectSigningFlags & CERTDB_VALID_CA)) { 
            thisUsage = clAllCA;
            goto include;
        }
    } /* end of if nickname */

    /* Check if this is an email cert that belongs to someone else. */
    if (emailAddr && !usercert && (trust->emailFlags & CERTDB_VALID_PEER)) {
        /* Someone else's email cert */
        thisUsage = clEmailRecipient;
        goto include;
    }
    
dontInclude:
    *usage = thisUsage;
    return PR_FALSE;

include:
    *usage = thisUsage;
    if (state->usage == thisUsage ||
        (state->usage == clAllMine &&
         (thisUsage == clSSLClient || thisUsage == clEmailSigner))) {        
        return PR_TRUE;
    }
    return PR_FALSE;
}


   
/* build/append a cert list string depending on key and usage */
SECStatus 
certlist_get_cert(CERTCertificate *cert, SECItem *dbkey, void *arg)
{
    ssmCLState *state = (ssmCLState *) arg;
    SECStatus rv = SECSuccess;
    void *certHashKey; /* the key we use to insert into the hash */ 
    ssmCLCertUsage thisUsage; 
    ssmCertData * data;
    char * certInfo = NULL, *nick = NULL;
    char * wrapper;
    

    /* check if we want to include cert in our hash table */
    if (!certlist_include_cert(cert, state, &thisUsage))
        goto done;

    /* check if cert is already in the db */
    nick = (cert->nickname)?cert->nickname:cert->emailAddr;
    if (SSMSortedList_Lookup(*state->hash, nick))
        goto done;
        
    /* different wrapper for email certs */
    if (thisUsage == clEmailRecipient)
        wrapper = state->efmt;
    else wrapper = state->fmt;

    /* If we get here, then we should include this cert in the hash table. */
    rv = (SECStatus) 
    	(format_raw_cert(cert, wrapper, state->datefmt, &certInfo) == SSM_SUCCESS) ?
		SECSuccess : SECFailure;
    if (rv != SECSuccess)
        goto done;   
    certHashKey = certlist_get_cert_key(state, cert, thisUsage);
    
    data = (ssmCertData *) PORT_ZAlloc(sizeof(ssmCertData));
    data->usage = thisUsage;
    data->certEntry = certInfo;
    /* XXX the following code is necessary because we want to pick out
     *     email signing certs among "my certs"
     */
    if (cert->trust->emailFlags & CERTDB_USER) {
        data->isEmailSigner = PR_TRUE;
    }
    else {
        data->isEmailSigner = PR_FALSE;
    }

    SSMSortedList_Insert(*state->hash, certHashKey, data);
done:
    return rv;
}

static char * 
escape_quotes(const char * str)
{
    char * outstr = NULL, * ptr = NULL;
    int i=0, length = 0;
    int quotechar = 0;

    if (!str) 
        goto done;
    ptr = str;
    while (*ptr) {
       length++;
       if (*ptr == 39 || *ptr == 34)
           quotechar++;
       ptr++;
    }
    outstr = (char *) PORT_ZAlloc(length+quotechar+1);
    if (!quotechar) {
        memcpy(outstr, str, length);
        goto done;
    }
    ptr = str;
    i = 0;
    while (i < length+quotechar) {
        if (*ptr == 39 || *ptr == 34) 
            outstr[i++] = 92;
        outstr[i++] = *ptr;
        ptr++;
    }
        
 done:
    return outstr;
}


/* Format a raw cert into (wrapper). */
static SSMStatus
format_raw_cert(CERTCertificate *cert, char *wrapper,
                void *dfmt, char **result)
{
    char *nickStr = NULL;
    char *emailStr = NULL;
    char *subjectName = NULL;
    char *validStartStr = NULL;
    char *validEndStr = NULL;
    char * issuerName = NULL;
    void *dateFormat = dfmt;
    char *isn_ch = NULL;
    SSMStatus rv = SSM_SUCCESS;

    PR_ASSERT(wrapper);

    /* If a date format wasn't supplied, make one. */
    if (dateFormat)
    {
		dateFormat = nlsNewDateFormat();
		if (!dateFormat) {
			goto loser;
		}
    }

    /* Get the key from the cert in b64 format */
    rv = ssm_certlist_get_b64_cert_key(cert, &isn_ch);
    if (rv != SSM_SUCCESS)
	goto loser;

    PR_ASSERT(isn_ch);
    nickStr  = (cert->nickname)  ?escape_quotes(cert->nickname):PL_strdup("-");
    emailStr = (cert->emailAddr) ?escape_quotes(cert->emailAddr):PL_strdup("-");
    subjectName =  (cert->subjectName) ? escape_quotes(cert->subjectName) : 
        PL_strdup("-");
    issuerName = CERT_GetOrgName(&cert->issuer);
    issuerName = issuerName ? escape_quotes(issuerName) : PL_strdup("-");
        
    validStartStr = DER_UTCDayToAscii(&cert->validity.notBefore);
    if (!validStartStr) 
        validStartStr = PL_strdup("");
    validEndStr = DER_UTCDayToAscii(&cert->validity.notAfter);
    if (!validEndStr) 
        validEndStr = PL_strdup("");

    SSMTextGen_UTF8StringClear(result);

    *result = PR_smprintf(wrapper, isn_ch, nickStr, emailStr, subjectName, 
                          validStartStr, validEndStr,
                          issuerName, 0);
    SSM_DebugUTF8String("wrapped cert", *result);
    goto done;
 loser:
    SSM_DEBUG("Couldn't format cert!");
    if (rv == SSM_SUCCESS)
        rv = SSM_FAILURE;
 done:
    PR_FREEIF(nickStr);
    PR_FREEIF(emailStr);
    PR_FREEIF(subjectName);
    PR_FREEIF(issuerName);
    PR_FREEIF(isn_ch);
    PR_FREEIF(validStartStr);
    PR_FREEIF(validEndStr);
    if (dateFormat && !dfmt) /* if we made the date format, delete it */
        nlsFreeDateFormat(dateFormat);
    return rv;
}

/* Enumerator for cert hash entries. Embed cert information in
   the wrapper (state->wrapper). */
SSMStatus 
certlist_display_cert(PRIntn index, void * arg, void * key, void * itemdata)
{
    ssmCLState *state = (ssmCLState *) arg;
    SSMStatus rv = SSM_SUCCESS;
    ssmCertData * data = (ssmCertData *) itemdata; 
    
    if (state->usage == data->usage ||
        (state->usage == clAllMine && 
         (data->usage == clSSLClient || data->usage == clEmailSigner)) ||
        (state->usage == clEmailSigner && data->usage == clAllMine &&
         data->isEmailSigner)) { 
        rv = SSM_ConcatenateUTF8String(&state->output, data->certEntry);
        if (rv != SSM_SUCCESS)
            goto loser;
    }
    goto done;
loser:
    SSM_DEBUG("certlist_display_cert: couldn't add cert info to display list!");
done:
    /* keep going */
    return SSM_SUCCESS;
}    

/* get the certType parameter */
SSMStatus
certlist_get_usage(ssmCLState *state)
{
    char *usageStr = (char *) SSM_At(state->cx->m_params, CERTLIST_PARAM_USAGE);
    SSMStatus rv = SSM_SUCCESS;

    PR_ASSERT(usageStr);
    if (!usageStr)
    {
        SSM_HTTPReportSpecificError(state->cx->m_request, 
                                    "certlist_get_usage: "
                                    "usage parameter is NULL");
        goto loser;
    }

    /* Figure out which one it is. */
    if (!MIN_STRCMP(usageStr, "AllMine"))
        state->usage = clAllMine;
    else if (!MIN_STRCMP(usageStr, "SSLC"))
        state->usage = clSSLClient;
    else if (!MIN_STRCMP(usageStr, "EmailS"))
        state->usage = clEmailSigner;
    else if (!MIN_STRCMP(usageStr, "SSLS"))
        state->usage = clSSLServer;
    else if (!MIN_STRCMP(usageStr, "EmailR"))
        state->usage = clEmailRecipient;
    else if (!MIN_STRCMP(usageStr, "CA"))
        state->usage = clAllCA;
    else /* bad value */
    {
        SSM_HTTPReportSpecificError(state->cx->m_request,
                                    "_certList: Bad usage -- must be "
                                    "AllMine|SSLClient|EmailSigner|"
                                    "SSLServer|EmailRecipient|CA");
        goto loser;
    }
    goto done;
loser:
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
done:
    return rv;
}

/* get the certType parameter */
SSMStatus
certlist_get_key_type(ssmCLState *state)
{
    char *keyStr = (char *) SSM_At(state->cx->m_params, CERTLIST_PARAM_KEY);
    SSMStatus rv = SSM_SUCCESS;

    PR_ASSERT(keyStr);
    if (!keyStr)
    {
        SSM_HTTPReportSpecificError(state->cx->m_request, 
                                    "certlist_get_key_type: "
                                    "key parameter is NULL");
        goto loser;
    }

    /* Figure out which one it is. */
    if (!MIN_STRCMP(keyStr, "nick"))
        state->key = clNick;
    else if (!MIN_STRCMP(keyStr, "certID"))
        state->key = clCertID;
    else if ((!MIN_STRCMP(keyStr, "rid"))
             ||(!MIN_STRCMP(keyStr, "RID")))
        state->key = clRID;
    else if (!MIN_STRCMP(keyStr, "email"))
        state->key = clEmail;
    else/* bad value */
    {
        SSM_HTTPReportSpecificError(state->cx->m_request, 
                                    "_certList: Bad cert key -- must be "
                                    "nick|certID|RID");
        goto loser;
    }
    goto done;
loser:
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
done:
    return rv;
}

SSMStatus
ssm_populate_key_hash(ssmCLState * state)
{
    SSMCompare_fn  keyComparer = NULL;
    SSMListFree_fn keyFree = NULL;
    SSMListFree_fn dataFree = certlist_free_data;
    SECStatus srv;
    SSMStatus rv = SSM_SUCCESS;
    char *tempUStr = NULL;
    PermCertCallback callback = certlist_get_cert;

    /* Based on the usage, choose hasher/key comparer functions for
       the hash table we create below. */
    switch(state->key)
        {
        case clRID:
            keyComparer = certlist_rid_compare;
            keyFree = certlist_none;
            break;
        case clEmail:
        case clCertID:
        case clNick:
            keyComparer = certlist_compare_strings;
            keyFree = certlist_free_string;
            break;
        default:
            PR_ASSERT(0); 
        }
    list_functions.keyCompare = keyComparer;
    list_functions.freeListItemData = dataFree;
    list_functions.freeListItemKey = keyFree;
    /* Create the hash table if needed */
    if (!*state->hash)
        *state->hash = SSMSortedList_New(&list_functions);

    if (!*state->hash) 
        goto loser;

    PR_ASSERT(state->db);
    
    /* Get the wrapper text. */
    rv = SSM_GetAndExpandTextKeyedByString(state->cx, WRAPPER, &tempUStr);
    if (rv != SSM_SUCCESS)
        goto loser; /* error string set by the called function */
    SSM_DebugUTF8String("Certlist wrapper", tempUStr);
    state->fmt = tempUStr;
    tempUStr = NULL;
    /* wrapper for email certs */
    rv = SSM_GetAndExpandTextKeyedByString(state->cx, EMAIL_WRAPPER, &tempUStr);
    if (rv != SSM_SUCCESS)
        goto loser; /* error string set by the called function */
    SSM_DebugUTF8String("Certlist email wrapper", tempUStr);
    state->efmt = tempUStr;
    tempUStr = NULL;
    /* 
       Create a DateFormat object which we will use to convert
       cert date/times into displayable text.
     */
	state->datefmt = nlsNewDateFormat();
	if (!state->datefmt) {
		goto loser;
	}

    /* Format each cert into the list. */
    srv = SEC_TraversePermCerts(state->db, callback, state);
    if (srv == SECSuccess) 
        srv = PK11_TraverseSlotCerts(callback, state, 
                                     state->cx->m_request->ctrlconn);   
    goto done;
    
loser:
    if (rv == SSM_SUCCESS)
        rv = SSM_FAILURE;
done: 
    return rv;
}

/* 
   Cert list keyword handler.
   Syntax: {_certList <certKey>,<certUsage>,prefix,wrapper,suffix}

   where <certKey>  ::= "nick" | "certID" | "RID" | "email"
         <certUsage>::= "AllMine"|"SSLClient"|"EmailSigner"|
                        "SSLServer"|"EmailRecipient"|"CA"
                        (for now)

   Generates a list of certificates. The following steps are performed:
   - Send {prefix} to the client.
   - For each cert indicated by <certType> and <certUsage>, format the 
     cert data into {wrapper} and send to the client. For the moment, 
     raw certs are used and passed to NLS_MessageFormat in the 
     following form:
         NLS_FORMAT_TYPE_STRING, (cert issuer+sn base64),
         NLS_FORMAT_TYPE_STRING, (cert nick),
         NLS_FORMAT_TYPE_STRING, (cert email address),
         NLS_FORMAT_TYPE_STRING, (cert subject name),
         NLS_FORMAT_TYPE_LONG,   (cert resource ID, 0 for now)
   - After the certs are processed and sent, send {suffix}.
*/

SSMStatus
SSM_CertListKeywordHandler(SSMTextGenContext *cx)
{
    SSMStatus rv = SSM_SUCCESS;
    char *prefix = NULL, *suffix = NULL;
    char *tempUStr = NULL;
    SSMResource * target = NULL;
    ssmCLState state;
    SSMResourceType targetType = SSM_RESTYPE_NULL;
    PRIntn certType[clAllCA+1];
    SSMSortedList *nullHash = NULL;

    certType[clAllMine] =        0x00000001;
    certType[clEmailRecipient] = 0x00000010;
    certType[clSSLServer] =      0x00000100;
    certType[clAllCA] =          0x00001000;
    certType[clEmailSigner] =    0x00010000;

    /* Check for parameter validity */
    PR_ASSERT(cx);
    PR_ASSERT(cx->m_request);
    PR_ASSERT(cx->m_params);
    PR_ASSERT(cx->m_result);
    if (!cx || !cx->m_request || !cx->m_params || !cx->m_result)
    {
        rv = (SSMStatus) PR_INVALID_ARGUMENT_ERROR;
        goto real_loser; /* really bail here */
    }

    state.cx = cx;
    state.fmt = NULL;
    state.efmt = NULL;
    state.datefmt = NULL;
    state.output = NULL;
    state.temp = NULL;
    state.db = cx->m_request->ctrlconn->m_certdb;
    if (!state.db)
    {
        SSM_HTTPReportSpecificError(state.cx->m_request, 
                                    "_certlist: no cert db available");
        goto user_loser;
    }

    if (SSM_Count(cx->m_params) != CERTLIST_PARAM_COUNT)
    {
        SSM_HTTPReportSpecificError(cx->m_request, "_certList: "
                                    "Incorrect number of parameters "
                                    "(%d supplied, %d needed).\n",
                                    SSM_Count(cx->m_params), 
                                    CERTLIST_PARAM_COUNT);
        goto user_loser;
    }

    /* Convert parameters to something we can use in finding certs. */
    rv = certlist_get_key_type(&state);
    if (rv != SSM_SUCCESS)
        goto user_loser;

    rv = certlist_get_usage(&state);
    if (rv != SSM_SUCCESS)
        goto user_loser;

    prefix = (char *) SSM_At(cx->m_params, CERTLIST_PARAM_PREFIX);
    suffix = (char *) SSM_At(cx->m_params, CERTLIST_PARAM_SUFFIX);
    PR_ASSERT(prefix); /* already did user check, so bomb here if null */
    PR_ASSERT(suffix);

    /* check where we're called from */
    target = SSMTextGen_GetTargetObject(cx);
    if (!target) 
        goto real_loser;
    /* SecurityAdvisor context keeps hash around, otherwise create new one */
    if (SSM_IsAKindOf(target, SSM_RESTYPE_SECADVISOR_CONTEXT)) {
        targetType = SSM_RESTYPE_SECADVISOR_CONTEXT;
        state.hash = &((SSMSecurityAdvisorContext *)target)->m_certhash;
    }
    else 
        state.hash = &nullHash;

    state.output = state.temp = NULL;

    /* Start with the prefix. */
    rv = SSM_GetAndExpandTextKeyedByString(cx, prefix, &tempUStr);
    if (rv != SSM_SUCCESS)
        goto real_loser; /* error string set by the called function */
    SSM_DebugUTF8String("Certlist prefix", tempUStr);
    rv = SSM_ConcatenateUTF8String(&cx->m_result, tempUStr);
    PR_Free(tempUStr);
    tempUStr = NULL;
    if (rv != SSM_SUCCESS) {
        goto real_loser;
    }

    /* initialize a hash if haven't already */
    if (SSM_IsAKindOf(target, SSM_RESTYPE_SECADVISOR_CONTEXT)) {
        SSMSecurityAdvisorContext *sctx = (SSMSecurityAdvisorContext *)target;

        if ( *state.hash == NULL || 
             (sctx->m_certsIncluded & certType[state.usage]) == 0) {
            sctx->m_certsIncluded |= certType[state.usage];
            rv = ssm_populate_key_hash(&state);
            if (rv != SSM_SUCCESS)
                goto user_loser;
        }
    }
        
    /* Already have all certs info, output in a list in state.result */
    SSMSortedList_Enumerate(*state.hash, certlist_display_cert, &state);
    
    if (state.output) {
        rv = SSM_ConcatenateUTF8String(&cx->m_result, state.output);
        /* Finally, add the suffix. */
        rv = SSM_GetAndExpandTextKeyedByString(cx, suffix, &tempUStr);
        if (rv != SSM_SUCCESS)
            goto real_loser; /* error string set by the called function */
        SSM_DebugUTF8String("certlist suffix", tempUStr); 
        
        rv = SSM_ConcatenateUTF8String(&cx->m_result, tempUStr);
        if (rv != SSM_SUCCESS) {
            goto real_loser;
        }
    }
    else /* no certs found */
        {  char * tmpStr = NULL;
        /* release prefix */
        PR_FREEIF(state.output);
        state.output = NULL;
        rv = SSM_GetAndExpandTextKeyedByString(cx, "list_empty", &tmpStr);
        if (rv != SSM_SUCCESS) 
            SSM_DEBUG("SSM_CertListKeywordHandler: could not get filler for empty cert list!\n");
        PR_FREEIF(cx->m_result);
        cx->m_result = NULL;
        rv = SSM_ConcatenateUTF8String(&cx->m_result, tmpStr);
        PR_FREEIF(tmpStr);
        tmpStr = NULL;
        }
    goto done;
user_loser:
    /* If we reach this point, something in the input is wrong, but we
       can still send something back to the client to indicate that a
       problem has occurred. */

    /* If we can't do what we're about to do, really bail. */
    if (!cx->m_request || !cx->m_request->errormsg)
        goto real_loser;

    /* Clear the string we were accumulating. */
    SSMTextGen_UTF8StringClear(&cx->m_result);

    rv = SSM_ConcatenateUTF8String(&cx->m_result, cx->m_request->errormsg);
    /* Clear the result string, since we're sending this inline */
    SSMTextGen_UTF8StringClear(&cx->m_request->errormsg);

    goto done;
real_loser:
    /* If we reach this point, then we are so screwed that we cannot
       send anything vaguely normal back to the client. Bail. */
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
done:
    if (targetType && targetType != SSM_RESTYPE_SECADVISOR_CONTEXT)
        if (*state.hash) 
            SSMSortedList_Destroy(*state.hash);    
    PR_FREEIF(state.fmt);
    PR_FREEIF(state.efmt);
    PR_FREEIF(state.output);
    PR_FREEIF(state.temp);
    if (state.datefmt)
        nlsFreeDateFormat(state.datefmt);
    PR_FREEIF(tempUStr);

    return rv;
}

static int ssm_CertsOnSmartcards(SSMControlConnection* ctrl, char* name)
{
    CERTCertList* list = NULL;
    int numCerts = 0;
    
    list = PK11_FindCertsFromNickname(name, (void*)ctrl);
    if (list == NULL) {
        return 0;
    }
    numCerts = SSM_CertListCount(list);
    CERT_DestroyCertList(list);
    return numCerts;
}

static PRBool ssm_OtherCertsByNicknameOrEmailAddr(SSMControlConnection* ctrl,
                                                  char* name)
{
    int numCerts = 0;

    /* first get the number of certs under the nickname */
    numCerts = CERT_NumPermCertsForNickname(ctrl->m_certdb, name);
    if (numCerts == 0) {
        /* we may not see the cert because it's on external token: check it */
        numCerts = ssm_CertsOnSmartcards(ctrl, name);
    }

    if (numCerts > 1) {
        /* note that the cert in question is still in the DB: it is just
         * marked for deletion, but not deleted yet
         */
        return PR_TRUE;
    }
    else if (numCerts == 1) {
        /* it is the only cert */
        return PR_FALSE;
    }
    else {
        /* nickname may be email address: try the email address */
        CERTCertList* list = NULL;
        int count = 0;
        
        list = CERT_CreateEmailAddrCertList(list, ctrl->m_certdb, name, 
                                            PR_Now(), PR_FALSE);
        if (list == NULL) {
            /* this usually shouldn't happen */
            return PR_FALSE;
        }

        count = SSM_CertListCount(list);
        if (list != NULL) {
            CERT_DestroyCertList(list);
        }
        if (count > 1) {
            return PR_TRUE;
        }
        else {
            return PR_FALSE;
        }
    }
}

SSMStatus SSM_ChangeCertSecAdvisorList(HTTPRequest * req, char * nickname, 
				       ssmCertHashAction action)
{
    SSMStatus rv = SSM_SUCCESS;
    SSMControlConnection * ctrl = req->ctrlconn;
    CERTCertificate * dbCert = NULL; 
    PRIntn i;
    SSMSecurityAdvisorContext * advisor = NULL;
    SSMResourceID resID;
    char * tmp;
    SSMSortedList * hash;

    PR_ASSERT(ctrl && SSM_IsAKindOf((SSMResource *)ctrl, 
                                    SSM_RESTYPE_CONTROL_CONNECTION));
    if (!ctrl->m_secAdvisorList || !ctrl->m_secAdvisorList->len) 
        goto done;

    if (action == certHashRemove) /* check if need to delete */ {   
        PR_ASSERT(nickname);

        /* find out if there are other certs under the same nickname than
         * the one marked for deletion
         */
        if (ssm_OtherCertsByNicknameOrEmailAddr(ctrl, nickname)) {
            goto done;
        }
    }
    /* there might be multiple Security Advisor up */
    for (i=0; i<ctrl->m_secAdvisorList->len; i++){
        resID = ctrl->m_secAdvisorList->data[i];
        
        rv = (SSMStatus) SSMControlConnection_GetResource(ctrl, resID, (SSMResource **)&advisor);
        if (rv != SSM_SUCCESS)
            continue;
        PR_ASSERT(advisor && SSM_IsAKindOf((SSMResource *)advisor,
                                           SSM_RESTYPE_SECADVISOR_CONTEXT));
        if (!advisor->m_certhash)
            continue;
        hash = advisor->m_certhash;
        if (action == certHashRemove) 
            SSMSortedList_Remove(hash, nickname, (void **)&tmp);
        else if (action == certHashAdd) {
            ssmCLState state;
            char * form;
            SSMTextGenContext *cx; 

            state.key = clNick;
            state.hash = &advisor->m_certhash;
            state.db = ctrl->m_certdb;
            
            /* this assumes we're called from Restore certificate or 
             * or some other place that includes form name in HTTP request 
             */
            rv = SSM_HTTPParamValue(req, "formName", &form);
            if (strstr(form, "mine")) 
                state.usage = clAllMine;
            else if (strstr(form, "others"))
                state.usage = clEmailRecipient;
            else if (strstr(form, "websites"))
                state.usage = clSSLServer;
            else if (strstr(form, "authorities"))
                state.usage = clAllCA;
            
            rv = SSMTextGen_NewTopLevelContext(req, &cx);
            if (rv != SSM_SUCCESS) {
                SSM_DEBUG("ChangeCertSecAdvisorList: can't get TextGenContext\n");
                goto done;
            }
            state.cx = cx;
            
            rv = ssm_populate_key_hash(&state);
            if (rv != SSM_SUCCESS) 
                SSM_DEBUG("ChangeCertSecAdvisorList: error reading certs\n");
            SSMTextGen_DestroyContext(cx);
        } else
            SSM_DEBUG("ChangeCertSecAdvisorList: bad change request %d\n", action);

        SSM_FreeResource((SSMResource *)advisor);
        advisor = NULL;
    }

done:
    
    if (dbCert) 
        CERT_DestroyCertificate(dbCert);
    return rv;
}
