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
#include "nspr.h"
#include "secerr.h"
#include "secasn1.h"
#include "seccomon.h"
#include "pk11func.h"
#include "certdb.h"
#include "certt.h"
#include "cert.h"
#include "certxutl.h"

#define NSS_3_4_CODE
#include "nsspki.h"
#include "pkit.h"
#include "pkitm.h"
#include "pki3hack.h"

/*
 * Find all user certificates that match the given criteria.
 * 
 *	"handle" - database to search
 *	"usage" - certificate usage to match
 *	"oneCertPerName" - if set then only return the "best" cert per
 *			name
 *	"validOnly" - only return certs that are curently valid
 *	"proto_win" - window handle passed to pkcs11
 */
CERTCertList *
CERT_FindUserCertsByUsage(CERTCertDBHandle *handle,
			  SECCertUsage usage,
			  PRBool oneCertPerName,
			  PRBool validOnly,
			  void *proto_win)
{
    CERTCertNicknames *nicknames = NULL;
    char **nnptr;
    int nn;
    CERTCertificate *cert = NULL;
    CERTCertList *certList = NULL;
    SECStatus rv;
    int64 time;
    CERTCertListNode *node = NULL;
    CERTCertListNode *freenode = NULL;
    int n;
    
    time = PR_Now();
    
    nicknames = CERT_GetCertNicknames(handle, SEC_CERT_NICKNAMES_USER,
				      proto_win);
    
    if ( ( nicknames == NULL ) || ( nicknames->numnicknames == 0 ) ) {
	goto loser;
    }

    nnptr = nicknames->nicknames;
    nn = nicknames->numnicknames;

    while ( nn > 0 ) {
	cert = NULL;
	/* use the pk11 call so that we pick up any certs on tokens,
	 * which may require login
	 */
	if ( proto_win != NULL ) {
	    cert = PK11_FindCertFromNickname(*nnptr,proto_win);
	}

	/* Sigh, It turns out if the cert is already in the temp db, because
	 * it's in the perm db, then the nickname lookup doesn't work.
	 * since we already have the cert here, though, than we can just call
	 * CERT_CreateSubjectCertList directly. For those cases where we didn't
	 * find the cert in pkcs #11 (because we didn't have a password arg,
	 * or because the nickname is for a peer, server, or CA cert, then we
	 * go look the cert up.
	 */
	if (cert == NULL) { 
	    cert = CERT_FindCertByNickname(handle,*nnptr);
	}

	if ( cert != NULL ) {
	   /* collect certs for this nickname, sorting them into the list */
	    certList = CERT_CreateSubjectCertList(certList, handle, 
				&cert->derSubject, time, validOnly);
	
	    /* drop the extra reference */
	    CERT_DestroyCertificate(cert);
	}
	
	nnptr++;
	nn--;
    }

    /* remove certs with incorrect usage */
    rv = CERT_FilterCertListByUsage(certList, usage, PR_FALSE);

    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* remove any extra certs for each name */
    if ( oneCertPerName ) {
	PRBool *flags;

	nn = nicknames->numnicknames;
	nnptr = nicknames->nicknames;
	
	flags = (PRBool *)PORT_ZAlloc(sizeof(PRBool) * nn);
	if ( flags == NULL ) {
	    goto loser;
	}
	
	node = CERT_LIST_HEAD(certList);
	
	/* treverse all certs in the list */
	while ( !CERT_LIST_END(node, certList) ) {

	    /* find matching nickname index */
	    for ( n = 0; n < nn; n++ ) {
		if ( PORT_Strcmp(nnptr[n], node->cert->nickname) == 0 ) {
		    /* We found a match.  If this is the first one, then
		     * set the flag and move on to the next cert.  If this
		     * is not the first one then delete it from the list.
		     */
		    if ( flags[n] ) {
			/* We have already seen a cert with this nickname,
			 * so delete this one.
			 */
			freenode = node;
			node = CERT_LIST_NEXT(node);
			CERT_RemoveCertListNode(freenode);
		    } else {
			/* keep the first cert for each nickname, but set the
			 * flag so we know to delete any others with the same
			 * nickname.
			 */
			flags[n] = PR_TRUE;
			node = CERT_LIST_NEXT(node);
		    }
		    break;
		}
	    }
	    if ( n == nn ) {
		/* if we get here it means that we didn't find a matching
		 * nickname, which should not happen.
		 */
		PORT_Assert(0);
		node = CERT_LIST_NEXT(node);
	    }
	}
	PORT_Free(flags);
    }

    goto done;
    
loser:
    if ( certList != NULL ) {
	CERT_DestroyCertList(certList);
	certList = NULL;
    }

done:
    if ( nicknames != NULL ) {
	CERT_FreeNicknames(nicknames);
    }

    return(certList);
}

/*
 * Find a user certificate that matchs the given criteria.
 * 
 *	"handle" - database to search
 *	"nickname" - nickname to match
 *	"usage" - certificate usage to match
 *	"validOnly" - only return certs that are curently valid
 *	"proto_win" - window handle passed to pkcs11
 */
CERTCertificate *
CERT_FindUserCertByUsage(CERTCertDBHandle *handle,
			 char *nickname,
			 SECCertUsage usage,
			 PRBool validOnly,
			 void *proto_win)
{
    CERTCertificate *cert = NULL;
    CERTCertList *certList = NULL;
    SECStatus rv;
    int64 time;
    
    time = PR_Now();
    
    /* use the pk11 call so that we pick up any certs on tokens,
     * which may require login
     */
    /* XXX - why is this restricted? */
    if ( proto_win != NULL ) {
	cert = PK11_FindCertFromNickname(nickname,proto_win);
    }


    /* sigh, There are still problems find smart cards from the temp
     * db. This will get smart cards working again. The real fix
     * is to make sure we can search the temp db by their token nickname.
     */
    if (cert == NULL) {
	cert = CERT_FindCertByNickname(handle,nickname);
    }

    if ( cert != NULL ) {
 	/* collect certs for this nickname, sorting them into the list */
	certList = CERT_CreateSubjectCertList(certList, handle, 
					&cert->derSubject, time, validOnly);

	/* drop the extra reference */
	CERT_DestroyCertificate(cert);
	cert = NULL;
    }
	
    if ( certList == NULL ) {
	goto loser;
    }
    
    /* remove certs with incorrect usage */
    rv = CERT_FilterCertListByUsage(certList, usage, PR_FALSE);

    if ( rv != SECSuccess ) {
	goto loser;
    }

    if ( ! CERT_LIST_END(CERT_LIST_HEAD(certList), certList) ) {
	cert = CERT_DupCertificate(CERT_LIST_HEAD(certList)->cert);
    }
    
loser:
    if ( certList != NULL ) {
	CERT_DestroyCertList(certList);
    }

    return(cert);
}

CERTCertList *
CERT_MatchUserCert(CERTCertDBHandle *handle,
		   SECCertUsage usage,
		   int nCANames, char **caNames,
		   void *proto_win)
{
    CERTCertList *certList = NULL;
    SECStatus rv;

    certList = CERT_FindUserCertsByUsage(handle, usage, PR_TRUE, PR_TRUE,
					 proto_win);
    if ( certList == NULL ) {
	goto loser;
    }
    
    rv = CERT_FilterCertListByCANames(certList, nCANames, caNames, usage);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    
    goto done;
    
loser:
    if ( certList != NULL ) {
	CERT_DestroyCertList(certList);
	certList = NULL;
    }

done:

    return(certList);
}


typedef struct stringNode {
    struct stringNode *next;
    char *string;
} stringNode;
    
static SECStatus
CollectNicknames( CERTCertificate *cert, SECItem *k, void *data)
{
    CERTCertNicknames *names;
    PRBool saveit = PR_FALSE;
    CERTCertTrust *trust;
    stringNode *node;
    int len;
    
    names = (CERTCertNicknames *)data;
    
    if ( cert->nickname ) {
	trust = cert->trust;
	
	switch(names->what) {
	  case SEC_CERT_NICKNAMES_ALL:
	    if ( ( trust->sslFlags & (CERTDB_VALID_CA|CERTDB_VALID_PEER) ) ||
	      ( trust->emailFlags & (CERTDB_VALID_CA|CERTDB_VALID_PEER) ) ||
	      ( trust->objectSigningFlags & (CERTDB_VALID_CA|CERTDB_VALID_PEER) ) ) {
		saveit = PR_TRUE;
	    }
	    
	    break;
	  case SEC_CERT_NICKNAMES_USER:
	    if ( ( trust->sslFlags & CERTDB_USER ) ||
		( trust->emailFlags & CERTDB_USER ) ||
		( trust->objectSigningFlags & CERTDB_USER ) ) {
		saveit = PR_TRUE;
	    }
	    
	    break;
	  case SEC_CERT_NICKNAMES_SERVER:
	    if ( trust->sslFlags & CERTDB_VALID_PEER ) {
		saveit = PR_TRUE;
	    }
	    
	    break;
	  case SEC_CERT_NICKNAMES_CA:
	    if ( ( ( trust->sslFlags & CERTDB_VALID_CA ) == CERTDB_VALID_CA ) ||
		( ( trust->emailFlags & CERTDB_VALID_CA ) == CERTDB_VALID_CA ) ||
		( ( trust->objectSigningFlags & CERTDB_VALID_CA ) == CERTDB_VALID_CA ) ) {
		saveit = PR_TRUE;
	    }
	    break;
	}
    }

    /* traverse the list of collected nicknames and make sure we don't make
     * a duplicate
     */
    if ( saveit ) {
	node = (stringNode *)names->head;
	while ( node != NULL ) {
	    if ( PORT_Strcmp(cert->nickname, node->string) == 0 ) { 
		/* if the string matches, then don't save this one */
		saveit = PR_FALSE;
		break;
	    }
	    node = node->next;
	}
    }

    if ( saveit ) {
	
	/* allocate the node */
	node = (stringNode*)PORT_ArenaAlloc(names->arena, sizeof(stringNode));
	if ( node == NULL ) {
	    return(SECFailure);
	}

	/* copy the string */
	len = PORT_Strlen(cert->nickname) + 1;
	node->string = (char*)PORT_ArenaAlloc(names->arena, len);
	if ( node->string == NULL ) {
	    return(SECFailure);
	}
	PORT_Memcpy(node->string, cert->nickname, len);

	/* link it into the list */
	node->next = (stringNode *)names->head;
	names->head = (void *)node;

	/* bump the count */
	names->numnicknames++;
    }
    
    return(SECSuccess);
}

CERTCertNicknames *
CERT_GetCertNicknames(CERTCertDBHandle *handle, int what, void *wincx)
{
    PRArenaPool *arena;
    CERTCertNicknames *names;
    int i;
    SECStatus rv;
    stringNode *node;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return(NULL);
    }
    
    names = (CERTCertNicknames *)PORT_ArenaAlloc(arena, sizeof(CERTCertNicknames));
    if ( names == NULL ) {
	goto loser;
    }

    names->arena = arena;
    names->head = NULL;
    names->numnicknames = 0;
    names->nicknames = NULL;
    names->what = what;
    names->totallen = 0;
    
    rv = PK11_TraverseSlotCerts(CollectNicknames, (void *)names, wincx);
    if ( rv ) {
	goto loser;
    }

    if ( names->numnicknames ) {
	names->nicknames = (char**)PORT_ArenaAlloc(arena,
					 names->numnicknames * sizeof(char *));

	if ( names->nicknames == NULL ) {
	    goto loser;
	}
    
	node = (stringNode *)names->head;
	
	for ( i = 0; i < names->numnicknames; i++ ) {
	    PORT_Assert(node != NULL);
	    
	    names->nicknames[i] = node->string;
	    names->totallen += PORT_Strlen(node->string);
	    node = node->next;
	}

	PORT_Assert(node == NULL);
    }

    return(names);
    
loser:
    PORT_FreeArena(arena, PR_FALSE);
    return(NULL);
}

void
CERT_FreeNicknames(CERTCertNicknames *nicknames)
{
    PORT_FreeArena(nicknames->arena, PR_FALSE);
    
    return;
}

/* [ FROM pcertdb.c ] */

typedef struct dnameNode {
    struct dnameNode *next;
    SECItem name;
} dnameNode;

void
CERT_FreeDistNames(CERTDistNames *names)
{
    PORT_FreeArena(names->arena, PR_FALSE);
    
    return;
}

static SECStatus
CollectDistNames( CERTCertificate *cert, SECItem *k, void *data)
{
    CERTDistNames *names;
    PRBool saveit = PR_FALSE;
    CERTCertTrust *trust;
    dnameNode *node;
    int len;
    
    names = (CERTDistNames *)data;
    
    if ( cert->trust ) {
	trust = cert->trust;
	
	/* only collect names of CAs trusted for issuing SSL clients */
	if (  trust->sslFlags &  CERTDB_TRUSTED_CLIENT_CA )  {
	    saveit = PR_TRUE;
	}
    }

    if ( saveit ) {
	/* allocate the node */
	node = (dnameNode*)PORT_ArenaAlloc(names->arena, sizeof(dnameNode));
	if ( node == NULL ) {
	    return(SECFailure);
	}

	/* copy the name */
	node->name.len = len = cert->derSubject.len;
	node->name.type = siBuffer;
	node->name.data = (unsigned char*)PORT_ArenaAlloc(names->arena, len);
	if ( node->name.data == NULL ) {
	    return(SECFailure);
	}
	PORT_Memcpy(node->name.data, cert->derSubject.data, len);

	/* link it into the list */
	node->next = (dnameNode *)names->head;
	names->head = (void *)node;

	/* bump the count */
	names->nnames++;
    }
    
    return(SECSuccess);
}

/*
 * Return all of the CAs that are "trusted" for SSL.
 */
CERTDistNames *
CERT_GetSSLCACerts(CERTCertDBHandle *handle)
{
    PRArenaPool *arena;
    CERTDistNames *names;
    int i;
    SECStatus rv;
    dnameNode *node;
    
    /* allocate an arena to use */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return(NULL);
    }
    
    /* allocate the header structure */
    names = (CERTDistNames *)PORT_ArenaAlloc(arena, sizeof(CERTDistNames));
    if ( names == NULL ) {
	goto loser;
    }

    /* initialize the header struct */
    names->arena = arena;
    names->head = NULL;
    names->nnames = 0;
    names->names = NULL;
    
    /* collect the names from the database */
    rv = PK11_TraverseSlotCerts(CollectDistNames, (void *)names, NULL);
    if ( rv ) {
	goto loser;
    }

    /* construct the array from the list */
    if ( names->nnames ) {
	names->names = (SECItem*)PORT_ArenaAlloc(arena, names->nnames * sizeof(SECItem));

	if ( names->names == NULL ) {
	    goto loser;
	}
    
	node = (dnameNode *)names->head;
	
	for ( i = 0; i < names->nnames; i++ ) {
	    PORT_Assert(node != NULL);
	    
	    names->names[i] = node->name;
	    node = node->next;
	}

	PORT_Assert(node == NULL);
    }

    return(names);
    
loser:
    PORT_FreeArena(arena, PR_FALSE);
    return(NULL);
}

CERTDistNames *
CERT_DistNamesFromNicknames(CERTCertDBHandle *handle, char **nicknames,
			   int nnames)
{
    CERTDistNames *dnames = NULL;
    PRArenaPool *arena;
    int i, rv;
    SECItem *names = NULL;
    CERTCertificate *cert = NULL;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) goto loser;
    dnames = (CERTDistNames*)PORT_Alloc(sizeof(CERTDistNames));
    if (dnames == NULL) goto loser;

    dnames->arena = arena;
    dnames->nnames = nnames;
    dnames->names = names = (SECItem*)PORT_Alloc(nnames * sizeof(SECItem));
    if (names == NULL) goto loser;
    
    for (i = 0; i < nnames; i++) {
	cert = CERT_FindCertByNicknameOrEmailAddr(handle, nicknames[i]);
	if (cert == NULL) goto loser;
	rv = SECITEM_CopyItem(arena, &names[i], &cert->derSubject);
	if (rv == SECFailure) goto loser;
	CERT_DestroyCertificate(cert);
    }
    return dnames;
    
loser:
    if (cert != NULL)
	CERT_DestroyCertificate(cert);
    if (arena != NULL)
	PORT_FreeArena(arena, PR_FALSE);
    return NULL;
}

/* [ from pcertdb.c - calls Ascii to Name ] */
/*
 * Lookup a certificate in the database by name
 */
CERTCertificate *
CERT_FindCertByNameString(CERTCertDBHandle *handle, char *nameStr)
{
    CERTName *name;
    SECItem *nameItem;
    CERTCertificate *cert = NULL;
    PRArenaPool *arena = NULL;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    
    if ( arena == NULL ) {
	goto loser;
    }
    
    name = CERT_AsciiToName(nameStr);
    
    if ( name ) {
	nameItem = SEC_ASN1EncodeItem (arena, NULL, (void *)name,
				       CERT_NameTemplate);
	if ( nameItem == NULL ) {
	    goto loser;
	}

	cert = CERT_FindCertByName(handle, nameItem);
	CERT_DestroyName(name);
    }

loser:
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(cert);
}

/* From certv3.c */

CERTCrlDistributionPoints *
CERT_FindCRLDistributionPoints (CERTCertificate *cert)
{
    SECItem encodedExtenValue;
    SECStatus rv;

    encodedExtenValue.data = NULL;
    encodedExtenValue.len = 0;

    rv = cert_FindExtension(cert->extensions, SEC_OID_X509_CRL_DIST_POINTS,
			    &encodedExtenValue);
    if ( rv != SECSuccess ) {
	return (NULL);
    }

    return (CERT_DecodeCRLDistributionPoints (cert->arena,
					      &encodedExtenValue));
}

/* From crl.c */
CERTSignedCrl * CERT_ImportCRL
   (CERTCertDBHandle *handle, SECItem *derCRL, char *url, int type, void *wincx)
{
    CERTCertificate *caCert;
    CERTSignedCrl *newCrl, *crl;
    SECStatus rv;

    newCrl = crl = NULL;

    PORT_Assert (handle != NULL);
    do {

	newCrl = CERT_DecodeDERCrl(NULL, derCRL, type);
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
    
	caCert = CERT_FindCertByName (handle, &newCrl->crl.derName);
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

#ifdef FIXME
	/* Do CRL validation and add to the dbase if this crl is more present then the one
	   in the dbase, if one exists.
	 */
	crl = cert_DBInsertCRL (handle, url, newCrl, derCRL, type);
#endif

    } while (0);

    SEC_DestroyCrl (newCrl);
    return (crl);
}

/* From certdb.c */
static SECStatus
cert_ImportCAChain(SECItem *certs, int numcerts, SECCertUsage certUsage, PRBool trusted)
{
    SECStatus rv;
    SECItem *derCert;
    PRArenaPool *arena;
    CERTCertificate *cert = NULL;
    CERTCertificate *newcert = NULL;
    CERTCertDBHandle *handle;
    CERTCertTrust trust;
    PRBool isca;
    char *nickname;
    unsigned int certtype;
    
    handle = CERT_GetDefaultCertDB();
    
    arena = NULL;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( ! arena ) {
	goto loser;
    }

    while (numcerts--) {
	derCert = certs;
	certs++;

	/* decode my certificate */
	newcert = CERT_DecodeDERCertificate(derCert, PR_FALSE, NULL);
	if ( newcert == NULL ) {
	    goto loser;
	}

	if (!trusted) {
	    /* make sure that cert is valid */
	    rv = CERT_CertTimesValid(newcert);
	    if ( rv == SECFailure ) {
		goto endloop;
	    }
	}

	/* does it have the CA extension */
	
	/*
	 * Make sure that if this is an intermediate CA in the chain that
	 * it was given permission by its signer to be a CA.
	 */
	isca = CERT_IsCACert(newcert, &certtype);

	if ( !isca ) {
	    if (!trusted) {
		goto endloop;
	    }
	    trust.sslFlags = CERTDB_VALID_CA;
	    trust.emailFlags = CERTDB_VALID_CA;
	    trust.objectSigningFlags = CERTDB_VALID_CA;
	} else {
	    /* SSL ca's must have the ssl bit set */
	    if ( ( certUsage == certUsageSSLCA ) &&
		(( certtype & NS_CERT_TYPE_SSL_CA ) != NS_CERT_TYPE_SSL_CA )) {
		goto endloop;
	    }

	    /* it passed all of the tests, so lets add it to the database */
	    /* mark it as a CA */
	    PORT_Memset((void *)&trust, 0, sizeof(trust));
	    switch ( certUsage ) {
	      case certUsageSSLCA:
		trust.sslFlags = CERTDB_VALID_CA;
		break;
	      case certUsageUserCertImport:
		if ((certtype & NS_CERT_TYPE_SSL_CA) == NS_CERT_TYPE_SSL_CA) {
		    trust.sslFlags = CERTDB_VALID_CA;
		}
		if ((certtype & NS_CERT_TYPE_EMAIL_CA) 
						== NS_CERT_TYPE_EMAIL_CA ) {
		    trust.emailFlags = CERTDB_VALID_CA;
		}
		if ( ( certtype & NS_CERT_TYPE_OBJECT_SIGNING_CA ) ==
					NS_CERT_TYPE_OBJECT_SIGNING_CA ) {
		     trust.objectSigningFlags = CERTDB_VALID_CA;
		}
		break;
	      default:
		PORT_Assert(0);
		break;
	    }
	}
	
	cert = CERT_DecodeDERCertificate(derCert, PR_FALSE, NULL);
	if ( cert == NULL ) {
	    goto loser;
	}
	
	/* get a default nickname for it */
	nickname = CERT_MakeCANickname(cert);

	cert->trust = &trust;
	rv = PK11_ImportCert(PK11_GetInternalKeySlot(), cert, 
			CK_INVALID_HANDLE, nickname, PR_TRUE);

	/* free the nickname */
	if ( nickname ) {
	    PORT_Free(nickname);
	}

	CERT_DestroyCertificate(cert);
	cert = NULL;
	
	if ( rv != SECSuccess ) {
	    goto loser;
	}

endloop:
	if ( newcert ) {
	    CERT_DestroyCertificate(newcert);
	    newcert = NULL;
	}
	
    }

    rv = SECSuccess;
    goto done;
loser:
    rv = SECFailure;
done:
    
    if ( newcert ) {
	CERT_DestroyCertificate(newcert);
	newcert = NULL;
    }
    
    if ( cert ) {
	CERT_DestroyCertificate(cert);
	cert = NULL;
    }
    
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }

    return(rv);
}

SECStatus
CERT_ImportCAChain(SECItem *certs, int numcerts, SECCertUsage certUsage)
{
    return cert_ImportCAChain(certs, numcerts, certUsage, PR_FALSE);
}

SECStatus
CERT_ImportCAChainTrusted(SECItem *certs, int numcerts, SECCertUsage certUsage) {
    return cert_ImportCAChain(certs, numcerts, certUsage, PR_TRUE);
}

/* Moved from certdb.c */
/*
** CERT_CertChainFromCert
**
** Construct a CERTCertificateList consisting of the given certificate and all
** of the issuer certs until we either get to a self-signed cert or can't find
** an issuer.  Since we don't know how many certs are in the chain we have to
** build a linked list first as we count them.
*/

typedef struct certNode {
    struct certNode *next;
    CERTCertificate *cert;
} certNode;

CERTCertificateList *
CERT_CertChainFromCert(CERTCertificate *cert, SECCertUsage usage,
		       PRBool includeRoot)
{
#ifndef NSS_SOFTOKEN_MODULE
    CERTCertificateList *chain = NULL;
    CERTCertificate *c;
    SECItem *p;
    int rv, len = 0;
    PRArenaPool *tmpArena, *arena;
    certNode *head, *tail, *node;

    /*
     * Initialize stuff so we can goto loser.
     */
    head = NULL;
    arena = NULL;

    /* arena for linked list */
    tmpArena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (tmpArena == NULL) goto no_memory;

    /* arena for SecCertificateList */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) goto no_memory;

    head = tail = (certNode*)PORT_ArenaZAlloc(tmpArena, sizeof(certNode));
    if (head == NULL) goto no_memory;

    /* put primary cert first in the linked list */
    head->cert = c = CERT_DupCertificate(cert);
    if (head->cert == NULL) goto loser;
    len++;

    /* add certs until we come to a self-signed one */
    while(SECITEM_CompareItem(&c->derIssuer, &c->derSubject) != SECEqual) {
	c = CERT_FindCertIssuer(tail->cert, PR_Now(), usage);
	if (c == NULL) {
	    /* no root is found, so make sure we don't attempt to delete one
	     * below
	     */
	    includeRoot = PR_TRUE;
	    break;
	}
	
	tail->next = (certNode*)PORT_ArenaZAlloc(tmpArena, sizeof(certNode));
	tail = tail->next;
	if (tail == NULL) goto no_memory;
	
	tail->cert = c;
	len++;
    }

    /* now build the CERTCertificateList */
    chain = (CERTCertificateList *)PORT_ArenaAlloc(arena, sizeof(CERTCertificateList));
    if (chain == NULL) goto no_memory;
    chain->certs = (SECItem*)PORT_ArenaAlloc(arena, len * sizeof(SECItem));
    if (chain->certs == NULL) goto no_memory;
    
    for(node = head, p = chain->certs; node; node = node->next, p++) {
	rv = SECITEM_CopyItem(arena, p, &node->cert->derCert);
	CERT_DestroyCertificate(node->cert);
	node->cert = NULL;
	if (rv < 0) goto loser;
    }
    if ( !includeRoot && len > 1) {
	chain->len = len - 1;
    } else {
	chain->len = len;
    }
    
    chain->arena = arena;

    PORT_FreeArena(tmpArena, PR_FALSE);
    
    return chain;

no_memory:
    PORT_SetError(SEC_ERROR_NO_MEMORY);
loser:
    if (head != NULL) {
	for (node = head; node; node = node->next) {
	    if (node->cert != NULL)
		CERT_DestroyCertificate(node->cert);
	}
    }

    if (arena != NULL) {
	PORT_FreeArena(arena, PR_FALSE);
    }

    if (tmpArena != NULL) {
	PORT_FreeArena(tmpArena, PR_FALSE);
    }

    return NULL;
#else
    CERTCertificateList *chain = NULL;
    NSSCertificate **stanChain;
    NSSCertificate *stanCert;
    PRArenaPool *arena;
    NSSUsage nssUsage;
    int i, len;

    stanCert = STAN_GetNSSCertificate(cert);
    nssUsage.anyUsage = PR_FALSE;
    nssUsage.nss3usage = usage;
    stanChain = NSSCertificate_BuildChain(stanCert, NULL, &nssUsage, NULL,
                                                    NULL, 0, NULL, NULL);
    if (!stanChain) {
	return NULL;
    }

    len = 0;
    stanCert = stanChain[0];
    while (stanCert) {
	stanCert = stanChain[++len];
    }

    arena = PORT_NewArena(4096);
    if (arena == NULL) {
	/* XXX destroy certs in chain */
	nss_ZFreeIf(stanChain);
    }

    chain = (CERTCertificateList *)PORT_ArenaAlloc(arena, 
                                                 sizeof(CERTCertificateList));
    chain->certs = (SECItem*)PORT_ArenaAlloc(arena, len * sizeof(SECItem));
    i = 0;
    stanCert = stanChain[i];
    while (stanCert) {
	SECItem derCert;
	derCert.len = (unsigned int)stanCert->encoding.size;
	derCert.data = (unsigned char *)stanCert->encoding.data;
	SECITEM_CopyItem(arena, &chain->certs[i], &derCert);
	/* XXX CERT_DestroyCertificate(node->cert); */
	stanCert = stanChain[++i];
    }
    if ( !includeRoot && len > 1) {
	chain->len = len - 1;
    } else {
	chain->len = len;
    }
    
    chain->arena = arena;
    nss_ZFreeIf(stanChain);
    return chain;
#endif
}

/* Builds a CERTCertificateList holding just one DER-encoded cert, namely
** the one for the cert passed as an argument.
*/
CERTCertificateList *
CERT_CertListFromCert(CERTCertificate *cert)
{
    CERTCertificateList *chain = NULL;
    int rv;
    PRArenaPool *arena;

    /* arena for SecCertificateList */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) goto no_memory;

    /* build the CERTCertificateList */
    chain = (CERTCertificateList *)PORT_ArenaAlloc(arena, sizeof(CERTCertificateList));
    if (chain == NULL) goto no_memory;
    chain->certs = (SECItem*)PORT_ArenaAlloc(arena, 1 * sizeof(SECItem));
    if (chain->certs == NULL) goto no_memory;
    rv = SECITEM_CopyItem(arena, chain->certs, &(cert->derCert));
    if (rv < 0) goto loser;
    chain->len = 1;
    chain->arena = arena;

    return chain;

no_memory:
    PORT_SetError(SEC_ERROR_NO_MEMORY);
loser:
    if (arena != NULL) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    return NULL;
}

CERTCertificateList *
CERT_DupCertList(CERTCertificateList * oldList)
{
    CERTCertificateList *newList = NULL;
    PRArenaPool         *arena   = NULL;
    SECItem             *newItem;
    SECItem             *oldItem;
    int                 len      = oldList->len;
    int                 rv;

    /* arena for SecCertificateList */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) 
	goto no_memory;

    /* now build the CERTCertificateList */
    newList = PORT_ArenaNew(arena, CERTCertificateList);
    if (newList == NULL) 
	goto no_memory;
    newList->arena = arena;
    newItem = (SECItem*)PORT_ArenaAlloc(arena, len * sizeof(SECItem));
    if (newItem == NULL) 
	goto no_memory;
    newList->certs = newItem;
    newList->len   = len;

    for (oldItem = oldList->certs; len > 0; --len, ++newItem, ++oldItem) {
	rv = SECITEM_CopyItem(arena, newItem, oldItem);
	if (rv < 0) 
	    goto loser;
    }
    return newList;

no_memory:
    PORT_SetError(SEC_ERROR_NO_MEMORY);
loser:
    if (arena != NULL) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    return NULL;
}

void
CERT_DestroyCertificateList(CERTCertificateList *list)
{
    PORT_FreeArena(list->arena, PR_FALSE);
}

