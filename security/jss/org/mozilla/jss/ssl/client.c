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
 * The Original Code is the Netscape Security Services for Java.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
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

#include "cert.h"
#include "certdb.h"
#include "key.h"
#include "ssl.h"
#include "sslerr.h"
#include "secder.h"
#include <nspr.h>
#include <plarena.h>

#include "jni.h"
#include "jss_ssl.h"
#include "jssl.h"

#include "pk11util.h"

/*
 * Callback from SSL for checking certificate the peer (other end of
 * the socket) presents.
 */
PR_IMPLEMENT( int )
JSSL_ConfirmPeerCert(void *arg, PRFileDesc *fd, PRBool checkSig,
		     PRBool isServer)
{
    char *	      hostname;
    SECStatus         rv	= SECFailure;
    SECCertUsage      certUsage;
    CERTCertificate   *peerCert=NULL;

    certUsage = isServer ? certUsageSSLClient : certUsageSSLServer;
 

    /* SSL_PeerCertificate() returns a shallow copy of the cert, so we
       must destroy it before we exit this function */

    peerCert   = SSL_PeerCertificate(fd);

    if (peerCert) {
        rv = CERT_VerifyCertNow(CERT_GetDefaultCertDB(), peerCert,
				checkSig, certUsage, NULL /*pinarg*/);
        }

    /* if we're a server, then we don't need to check the CN of the
       certificate, so we can just return whatever returncode we
       have now
     */

    if ( rv != SECSuccess || isServer )  {
        if (peerCert) CERT_DestroyCertificate(peerCert);
	return (int)rv;
        }

    /* cert is OK.  This is the client side of an SSL connection.
     * Now check the name field in the cert against the desired hostname.
     * NB: This is our only defense against Man-In-The-Middle (MITM) attacks!
     */
    hostname = SSL_RevealURL(fd);	/* really is a hostname, not a URL */
    if (hostname && hostname[0])
	rv = CERT_VerifyCertName(peerCert, hostname);
    else
    	rv = SECFailure;

    if (peerCert) CERT_DestroyCertificate(peerCert);
    return (int)rv;
}

/*
 * Callback from SSL for checking a (possibly) expired
 * certificate the peer presents.
 */
PR_IMPLEMENT( int )
JSSL_ConfirmExpiredPeerCert(void *arg, PRFileDesc *fd, PRBool checkSig,
		     PRBool isServer)
{
    char *	      hostname;
    SECStatus         rv	= SECFailure;
    SECCertUsage      certUsage;
    CERTCertificate   *peerCert=NULL;
    int64             notAfter, notBefore;

    certUsage = isServer ? certUsageSSLClient : certUsageSSLServer;
 

    /* SSL_PeerCertificate() returns a shallow copy of the cert, so we
       must destroy it before we exit this function */

    peerCert   = SSL_PeerCertificate(fd);

    if (peerCert) {
        rv = CERT_GetCertTimes(peerCert, &notBefore, &notAfter);
        if (rv != SECSuccess) {
            CERT_DestroyCertificate(peerCert);
            return rv;
            }

     /* Verify the certificate based on it's expiry date. This should
        always succeed, if the cert is trusted. It doesn't care if
        the cert has expired. */

        rv = CERT_VerifyCert(CERT_GetDefaultCertDB(), peerCert,
				checkSig, certUsage, 
                                notAfter, NULL /*pinarg*/,
                                NULL /* log */);
        }

    /* if we're a server, then we don't need to check the CN of the
       certificate, so we can just return whatever returncode we
       have now
     */

    if ( rv != SECSuccess || isServer )  {
        if (peerCert) CERT_DestroyCertificate(peerCert);
	return (int)rv;
        }

    /* cert is OK.  This is the client side of an SSL connection.
     * Now check the name field in the cert against the desired hostname.
     * NB: This is our only defense against Man-In-The-Middle (MITM) attacks!
     */
    hostname = SSL_RevealURL(fd);	/* really is a hostname, not a URL */
    if (hostname && hostname[0])
	rv = CERT_VerifyCertName(peerCert, hostname);
    else
    	rv = SECFailure;

    if (peerCert) CERT_DestroyCertificate(peerCert);
    return (int)rv;
}


#define USE_NINJA_PK11CERT 1

static jobject newValidityObject(JNIEnv *env,jmethodID *addreasonmethod)
{

	jclass validityclass;
	jmethodID cons;
	jobject newobject;


	validityclass = (*env)->FindClass(
			env,
      		"org/mozilla/jss/ssl/SSLCertificateApprovalCallback$ValidityStatus"
		);

	cons = (*env)->GetMethodID(
			env,
			validityclass,"<init>","()V");

#ifdef USE_NINJA_PK11CERT
	*addreasonmethod = (*env)->GetMethodID(
			env,
			validityclass,
			"addReason",
			"(ILorg/mozilla/jss/pkcs11/PK11Cert;I)V");   /* void (int, PK11Cert, int) */
	
#else
	*addreasonmethod = (*env)->GetMethodID(
			env,
			validityclass,
			"addReason",
			"(I[BI)V");   /* void (int, byte[], int) */
#endif

	newobject = (*env)->NewObject(
			env,
			validityclass,
			cons);

	return newobject;

}


/* AddToVerifyLog - copied from static method declared in certdb.c
   ugh
*/

static void
AddToVerifyLog(CERTVerifyLog *log, CERTCertificate *cert, unsigned long error,
           unsigned int depth, void *arg)
{
    CERTVerifyLogNode *node, *tnode;

    PORT_Assert(log != NULL);

    node = (CERTVerifyLogNode *)PORT_ArenaAlloc(log->arena,
                        sizeof(CERTVerifyLogNode));
    if ( node != NULL ) {
    node->cert = CERT_DupCertificate(cert);
    node->error = error;
    node->depth = depth;
    node->arg = arg;

    if ( log->tail == NULL ) {
        /* empty list */
        log->head = log->tail = node;
        node->prev = NULL;
        node->next = NULL;
    } else if ( depth >= log->tail->depth ) {
        /* add to tail */
        node->prev = log->tail;
        log->tail->next = node;
        log->tail = node;
        node->next = NULL;
    } else if ( depth < log->head->depth ) {
        /* add at head */
        node->prev = NULL;
        node->next = log->head;
        log->head->prev = node;
        log->head = node;
    } else {
        /* add in middle */
        tnode = log->tail;
        while ( tnode != NULL ) {
        if ( depth >= tnode->depth ) {
            /* insert after tnode */
            node->prev = tnode;
            node->next = tnode->next;
            tnode->next->prev = node;
            tnode->next = node;
            break;
        }

        tnode = tnode->prev;
        }
    }

    log->count++;
    }
	return;
}



/*
 * Callback from SSL for checking a (possibly) expired
 * certificate the peer presents.

 * obj - a jobject -> instance of a class implementing 
         the SSLCertificateApprovalCallback interface
 */
PR_IMPLEMENT( int )
JSSL_CallCertApprovalCallback(void *obj, PRFileDesc *fd, PRBool checkSig,
		     PRBool isServer)
{
	CERTCertificate *peerCert=NULL;
	CERTVerifyLog log;
	int error;
	int depth;
	CERTVerifyLogNode *node;
	CERTCertificate *errorcert=NULL;
	unsigned char *dercert;
	unsigned int dercert_length;
	jobject ninjacert;
	jobject peerninjacert;
	jbyteArray bytearray;
	JNIEnv *env;
	jobject validity;
	jmethodID addReasonMethod;
	int rv;
	int certUsage;
	int checkcn_rv;
	jclass approvalCallbackClass;
	jmethodID approveMethod;
	jboolean result;
	char *hostname=NULL;

	int debug_db=0;

	/* initialize logging structures */
	log.arena = PR_Calloc(1,8192);
	PL_InitArenaPool(log.arena,"jss",DER_DEFAULT_CHUNKSIZE,sizeof(double));
	log.head = NULL;
	log.tail = NULL;
	log.count = 0;

	if (debug_db) {
	PR_fprintf(PR_STDOUT,"JSSL_CallCertApprovalCallback()\n");
	PR_fprintf(PR_STDOUT,"   - arg = callback object = %x\n",obj);
	}

	env = (JNIEnv*)jni_GetCurrentEnv();

	/* First, get a handle on the cert that the peer presented */
    peerCert   = SSL_PeerCertificate(fd);

	
	/* if peer didn't present a cert, why am I called? */
	if (peerCert == NULL) return SECFailure;

    certUsage = isServer ? certUsageSSLClient : certUsageSSLServer;

	/* 
		verify it against current time - (can't use
		CERT_VerifyCertNow() since it doesn't allow passing of
		logging parameter)
	 */
	if (debug_db) { PR_fprintf(PR_STDOUT,"JSSL_Call_cac(): about to certVerifyCert()\n"); }

	rv = CERT_VerifyCert(
		CERT_GetDefaultCertDB(),
		peerCert,
		checkSig,
		certUsage,
		PR_Now(),
		NULL /*pinarg*/,
		 &log);

	if (rv == SECSuccess) {
		if (log.count > 0) {
			rv = SECFailure;
		}
	}

	if (debug_db) { PR_fprintf(PR_STDOUT,"JSSL_Call_cac(): CERT_VerifyCert() returned: %d\n",rv); }

	/* need to also verify CN . There is a call in CERT_* to do this */

    hostname = SSL_RevealURL(fd);	/* really is a hostname, not a URL */

    if (hostname && hostname[0])
		checkcn_rv = CERT_VerifyCertName(peerCert, hostname);
    else
    	checkcn_rv = SECFailure;

	if (checkcn_rv != SECSuccess) {
		AddToVerifyLog(&log,peerCert,SSL_ERROR_BAD_CERT_DOMAIN,0,0);
		rv = SECFailure;
	}

	validity = newValidityObject(env,&addReasonMethod);
	
	if (rv == SECSuccess)  {
	if (debug_db) { PR_fprintf(PR_STDOUT,"JSSL_Call_cac(): success \n"); }
	}
	else {
	if (debug_db) { PR_fprintf(PR_STDOUT,"JSSL_Call_cac(): failure\n"); }
		node = log.head;
		while (node) {
			error = node->error;
			errorcert= node->cert;
			depth = node->depth;

	if (debug_db) {
	PR_fprintf(PR_STDOUT,"JSSL_Call_cac(): error=%d, depth=%d\n",error,depth);
	}

#ifdef USE_NINJA_PK11CERT
			ninjacert = JSS_PK11_wrapCert(env,&errorcert);
			(*env)->CallVoidMethod(env,validity,addReasonMethod,
				error,
				ninjacert,
				depth
				);
#else
			dercert        = node->cert->derCert.data;
			dercert_length = node->cert->derCert.len;
	if (debug_db) {
	PR_fprintf(PR_STDOUT,"JSSL_Call_cac(): dercert_length=%d\n",dercert_length);
	}

			bytearray = (*env)->NewByteArray(env,dercert_length);
	if (debug_db) {
	PR_fprintf(PR_STDOUT,"JSSL_Call_cac(): bytearray=%lx\n",bytearray);
	}
			(*env)->SetByteArrayRegion(env,
						bytearray,
						0,
						dercert_length,
						(jbyte*) dercert);
	if (debug_db) {
	PR_fprintf(PR_STDOUT,"JSSL_Call_cac(): after setByteArrayRegion()\n");
	}
				
			(*env)->CallVoidMethod(env,validity,addReasonMethod,
				error,
				bytearray,
				depth
				);
#endif

	if (debug_db) {
	PR_fprintf(PR_STDOUT,"JSSL_Call_cac(): after calling addReason()\n");
	}
			node = node->next;
		}
	}
				
	/* 
	approvalCallbackClass = (*env)->FindClass(
			env,
      		"org/mozilla/jss/ssl/SSLCertificateApprovalCallback"
		);
	*/
	approvalCallbackClass = (*env)->GetObjectClass(env,obj);
	if (debug_db) {
	PR_fprintf(PR_STDOUT,"JSSL_Call_cac(): approvalCallbackClass = %x\n",
		approvalCallbackClass);
	}

	approveMethod = (*env)->GetMethodID(
			env,
			approvalCallbackClass,
			"approve",
			"(Lorg/mozilla/jss/crypto/X509Certificate;Lorg/mozilla/jss/ssl/SSLCertificateApprovalCallback$ValidityStatus;)Z");

	if (debug_db) {
	PR_fprintf(PR_STDOUT,"JSSL_Call_cac(): approveMethod = %x\n",
		approveMethod);
	}

	peerninjacert = JSS_PK11_wrapCert(env,&peerCert);
	result = (*env)->CallBooleanMethod(env,obj,approveMethod,
		peerninjacert,
		validity
		);
	if (debug_db) {
	PR_fprintf(PR_STDOUT,"JSSL_Call_cac(): approve returned %d\n",result);
	PR_fprintf(PR_STDOUT,"JSSL_Call_cac(): about to return\n");
	}
	
	return (result==0) ? SECFailure: SECSuccess;
}

static SECStatus
secCmpCertChainWCANames(CERTCertificate *cert, CERTDistNames *caNames) 
{
    SECItem *         caname;
    CERTCertificate * curcert;
    CERTCertificate * oldcert;
    PRUint32           contentlen;
    int               j;
    int               headerlen;
    int               depth;
    SECStatus         rv;
    SECItem           issuerName;
    SECItem           compatIssuerName;
    
    depth=0;
    curcert = CERT_DupCertificate(cert);
    
    while( curcert ) {
	issuerName = curcert->derIssuer;

	/* compute an alternate issuer name for compatibility with 2.0
	 * enterprise server, which send the CA names without
	 * the outer layer of DER hearder
	 */
	rv = DER_Lengths(&issuerName, &headerlen, &contentlen);
	if ( rv == SECSuccess ) {
	    compatIssuerName.data = &issuerName.data[headerlen];
	    compatIssuerName.len = issuerName.len - headerlen;
	} else {
	    compatIssuerName.data = NULL;
	    compatIssuerName.len = 0;
	}
	    
	for (j = 0; j < caNames->nnames; j++) {
	    caname = &caNames->names[j];
	    if (SECITEM_CompareItem(&issuerName, caname) == SECEqual) {
		rv = SECSuccess;
		CERT_DestroyCertificate(curcert);
		goto done;
	    } else if (SECITEM_CompareItem(&compatIssuerName, caname) == SECEqual) {
		rv = SECSuccess;
		CERT_DestroyCertificate(curcert);
		goto done;
	    }
	}
	if ( ( depth <= 20 ) &&
	    ( SECITEM_CompareItem(&curcert->derIssuer, &curcert->derSubject)
	     != SECEqual ) ) {
	    oldcert = curcert;
	    curcert = CERT_FindCertByName(curcert->dbhandle,
					  &curcert->derIssuer);
	    CERT_DestroyCertificate(oldcert);
	    depth++;
	} else {
	    CERT_DestroyCertificate(curcert);
	    curcert = NULL;
	}
    }
    rv = SECFailure;

done:
    return rv;
}


/* this callback is set when you 'setClientCertNickname'
  - i.e. you already know ahead of time which nickname you're going to use
*/

PR_IMPLEMENT( int )
JSSL_GetClientAuthData(	void * arg,
			PRFileDesc *        fd,
			CERTDistNames *     caNames,
			CERTCertificate **  pRetCert,
			SECKEYPrivateKey ** pRetKey)
{
    CERTCertificate *  cert;
    SECKEYPrivateKey * privkey;
    char *             chosenNickName = (char *)arg;	/* CONST */
    jobject  	       nicknamecallback = (jobject)arg;
    SECStatus          rv         = SECFailure;
    PRBool             isFortezza = PR_FALSE;
    CERTCertNicknames * names;
	int                 i;
	int debug_cc=0;

    if (chosenNickName) {
        cert = PK11_FindCertFromNickname(chosenNickName, NULL /*pinarg*/);
	if ( cert ) {
	    privkey = PK11_FindKeyByAnyCert(cert, NULL /*pinarg*/);	
	    if ( privkey ) {
	    	rv = SECSuccess;
	    } else {
		CERT_DestroyCertificate(cert);
	    }
	}
    }

    if (rv == SECSuccess) {
	*pRetCert = cert;
	*pRetKey  = privkey;
    }

    return rv;
}

/* 
 * This callback is called when the peer has request you to send you
 * client-auth certificate. You get to pick which one you want
 * to send.
 *
 * Expected return values:
 *	0		SECSuccess
 *	-1		SECFailure - No suitable certificate found.
 *	-2 		SECWouldBlock (we're waiting while we ask the user).
 */
PR_IMPLEMENT( int )
JSSL_CallCertSelectionCallback(	void * arg,
			PRFileDesc *        fd,
			CERTDistNames *     caNames,
			CERTCertificate **  pRetCert,
			SECKEYPrivateKey ** pRetKey)
{
    CERTCertificate *  cert;
    SECKEYPrivateKey * privkey;
    char *             chosenNickName = (char *)arg;	/* CONST */
    jobject  	       nicknamecallback = (jobject)arg;
    SECStatus          rv         = SECFailure;
    PRBool             isFortezza = PR_FALSE;
    CERTCertNicknames * names;
	int                 i;
	int count           =0;
	jclass	vectorclass;
	jmethodID vectorcons;
	jobject vector;
	jmethodID vector_add;
	jstring nickname_string;
	jstring chosen_nickname;
	char *chosen_nickname_for_c;
	jboolean chosen_nickname_cleanup;
	jclass clientcertselectionclass;
	jmethodID clientcertselectionclass_select;
	JNIEnv *env;
	int debug_cc=0;

	env = (JNIEnv*)jni_GetCurrentEnv();

	clientcertselectionclass = (*env)->GetObjectClass(env,nicknamecallback);

	clientcertselectionclass_select = (*env)->GetMethodID(
			env,
			clientcertselectionclass,
			"select",
			"(Ljava/util/Vector;)Ljava/lang/String;" );

	/* get java bits and piece ready to create a new vector */

	vectorclass = (*env)->FindClass(
			env, "java/util/Vector");

	if (debug_cc) { PR_fprintf(PR_STDOUT,"  got vectorclass: %lx\n",vectorclass); }

	vectorcons = (*env)->GetMethodID(
			env,
			vectorclass,"<init>","()V");

	if (debug_cc) { PR_fprintf(PR_STDOUT,"  got vectorcons: %lx\n",vectorcons); }

	vector_add = (*env)->GetMethodID(
			env,
			vectorclass,
			"addElement",
			"(Ljava/lang/Object;)V");

	if (debug_cc) { PR_fprintf(PR_STDOUT,"  got vectoradd: %lx\n",vector_add); }

	/* create new vector */
	vector = (*env)->NewObject( env, vectorclass, vectorcons); 

	if (debug_cc) { PR_fprintf(PR_STDOUT,"  got new vector: %lx\n",vector); }

/* next, get a list of all the valid nicknames */
	names = CERT_GetCertNicknames(CERT_GetDefaultCertDB(),
				      SEC_CERT_NICKNAMES_USER, NULL /*pinarg*/);
	if (names != NULL) {
	    for (i = 0; i < names->numnicknames; i++) {
		if (debug_cc) { PR_fprintf(PR_STDOUT,"checking nn: %s\n",names->nicknames[i]); }
		cert = PK11_FindCertFromNickname(names->nicknames[i],NULL /*pinarg*/);
		if ( !cert )
		    continue;

		/* Only check unexpired certs */
		if ( CERT_CheckCertValidTimes(cert, PR_Now(), PR_TRUE /*allowOverride*/)
				 != secCertTimeValid ) {
		if (debug_cc) { PR_fprintf(PR_STDOUT,"  not valid\n"); }
		    CERT_DestroyCertificate(cert);
		    continue;
		}
		rv = secCmpCertChainWCANames(cert, caNames);
		if ( rv == SECSuccess ) {
			if (debug_cc) { PR_fprintf(PR_STDOUT,"  matches ca name\n"); }

		    privkey = PK11_FindKeyByAnyCert(cert, NULL /*pinarg*/);

			/* just test if we have the private key */
		    if ( privkey )  {

			count++;
			if (debug_cc) { PR_fprintf(PR_STDOUT,"  found privkey\n"); }
			SECKEY_DestroyPrivateKey(privkey);

			/* if we have, then this nickname has passed all the
				 tests necessary to put it in the list */

			nickname_string = (*env)->NewStringUTF(env,
						names->nicknames[i]);

			if (debug_cc) { PR_fprintf(PR_STDOUT,"  calling vector_add\n"); }
			(*env)->CallVoidMethod(env,vector,vector_add,
				nickname_string
				);
						
			if (debug_cc) { PR_fprintf(PR_STDOUT,"  back from vector_add\n"); }
	            }
			
		}
		CERT_DestroyCertificate(cert);
	    }
	    CERT_FreeNicknames(names);
	}

	/* okay - so we made a vector of the certs - now call the java 
	   class to figure out which one to send */

	chosen_nickname = (*env)->CallObjectMethod(env,nicknamecallback,
			clientcertselectionclass_select,
			vector
			);

	if (chosen_nickname == NULL) {
		rv = SECFailure;
		goto loser;
	}

	chosen_nickname_for_c = (char*)(*env)->GetStringUTFChars(env,
		chosen_nickname,
		&chosen_nickname_cleanup);

	if (debug_cc) { PR_fprintf(PR_STDOUT,"  chosen nickname: %s\n",chosen_nickname_for_c); }
	cert = PK11_FindCertFromNickname(chosen_nickname_for_c,NULL /*pinarg*/);

	if (debug_cc) { PR_fprintf(PR_STDOUT,"  found certificate\n"); }

	if (chosen_nickname_cleanup == JNI_TRUE) {
		(*env)->ReleaseStringUTFChars(env,
			chosen_nickname,
			chosen_nickname_for_c);
	}
			

	if (cert == NULL) {
		rv = SECFailure;
		goto loser;
	}

        privkey = PK11_FindKeyByAnyCert(cert, NULL /*pinarg*/);
        if ( privkey == NULL )  {
		rv = SECFailure;
		goto loser;
	}
	if (debug_cc) { PR_fprintf(PR_STDOUT,"  found privkey. returning\n"); }

	*pRetCert = cert;
	*pRetKey  = privkey;
	rv = SECSuccess;

loser:
    return (int)rv;
}


PR_IMPLEMENT( int )
JSSL_SetupSecureSocket(PRFileDesc *fd, char *url, JNIEnv *env, jobject this) {
    int status;
	jclass  sslsocketimpl_class = NULL;
	jobject certApprovalCallback = NULL;
	jobject certSelectionCallback = NULL;
	jobject global_certApprovalCallback = NULL;
	jobject global_certSelectionCallback = NULL;
	jfieldID certApprovalCallback_fieldid = 0;
	jfieldID certSelectionCallback_fieldid = 0;
	jthrowable exception = NULL;

    /*
     * The window context is the global reference to the SSLImpl object
     */

	PR_ASSERT(env != NULL);
	PR_ASSERT(this != NULL);

    /* pass URL to ssl code */
    status = SSL_SetURL(fd, (char *)url);
    if (status) 
	goto loser;

	/* SWP: if user has set auth callback method, use that */

	sslsocketimpl_class = (*env)->GetObjectClass(env,this);
	/* sslsocketimpl_class = (*env)->FindClass(env,"org/mozilla/jss/ssl/SSLSocketImpl"); */
	if ( (exception = (*env)->ExceptionOccurred(env)) != NULL) {
		PR_fprintf(PR_STDOUT,"Exception occurred\n");
		(*env)->ExceptionDescribe(env);
	}
	PR_ASSERT(sslsocketimpl_class!=NULL); 

	certApprovalCallback_fieldid = (*env)->GetFieldID(env,
					sslsocketimpl_class,
					SSLSOCKETIMPL_CERT_APPROVAL_CALLBACK_FIELD_NAME,
					SSLSOCKETIMPL_CERT_APPROVAL_CALLBACK_FIELD_SIG);

	PR_ASSERT(sslsocketimpl_class); 

	certSelectionCallback_fieldid = (*env)->GetFieldID(env,
					sslsocketimpl_class,
					SSLSOCKETIMPL_CERT_SELECTION_CALLBACK_FIELD_NAME,
					SSLSOCKETIMPL_CERT_SELECTION_CALLBACK_FIELD_SIG);

	certApprovalCallback  = (*env)->GetObjectField(env, this, certApprovalCallback_fieldid);
	certSelectionCallback = (*env)->GetObjectField(env, this, certSelectionCallback_fieldid);


	/* if user specified a cert approval callback implementation, call it,
       otherwise, use the default implementation */

	if (certApprovalCallback != NULL) {
	/* make global references to this object, to make sure we have a handle on
      them when the callback is called later, efter we've exited from this method */
		global_certApprovalCallback  = (*env)->NewGlobalRef(env,certApprovalCallback);
		status = SSL_AuthCertificateHook(fd, 
										JSSL_CallCertApprovalCallback,
										global_certApprovalCallback);
		/*
		PR_fprintf(PR_STDOUT,"Setting cert approval callback. Arg = %x\n",
				global_certApprovalCallback);
		*/
		if (status) goto loser;
	}
	else {
    	/* set certificate validation hook */
    	status = SSL_AuthCertificateHook(fd, JSSL_ConfirmPeerCert, this/* cx*/);
	}

	if (certSelectionCallback != NULL) {
		global_certSelectionCallback = (*env)->NewGlobalRef(env,certSelectionCallback);
    	SSL_GetClientAuthDataHook(fd, 
			JSSL_CallCertSelectionCallback, 
			global_certSelectionCallback );
	}

    if (status) 
	goto loser;

#if 0 /* this gets done if/when user tells us the nickname. */
    status = SSL_GetClientAuthDataHook(fd, JSSL_GetClientAuthData, this/*cx*/);
    if (status) 
	goto loser;
#endif

#if 0 /* servers don't give second chances. */
    status = SSL_BadCertHook(fd, SECNAV_HandleBadCert, clazz /*cx*/);
    if (status) 
	goto loser;
#endif

    return 0;
    
loser:
    return -1;
}


/*
 * Shutdown the security library.
 *
 * Currently closes the certificate and key databases.
 */
PR_IMPLEMENT( void )
JSSL_Shutdown(void)
{
    CERTCertDBHandle *cdb_handle;
    SECKEYKeyDBHandle *kdb_handle;

    /* should never call this in Ninja */
    PR_ASSERT(PR_FALSE);

    cdb_handle = CERT_GetDefaultCertDB();
    if (cdb_handle != NULL) {
	CERT_ClosePermCertDB(cdb_handle);
    }

    kdb_handle = SECKEY_GetDefaultKeyDB();
    if (kdb_handle != NULL) {
	SECKEY_CloseKeyDB(kdb_handle);
    }
}

/***********************************************************************/


PR_IMPLEMENT( SECStatus )
JSSL_ConfigSecureServerNickname(PRFileDesc *fd, const char *nickName)
{
    CERTCertificate *  cert;
    SECKEYPrivateKey * privKey;
    SECStatus          rv 	= SECFailure;

    cert = PK11_FindCertFromNickname((char *)nickName, NULL);	/* CONST */
    if (cert != NULL) {
	privKey = PK11_FindKeyByAnyCert(cert, NULL);
    	if (privKey != NULL) {
	    rv = SSL_ConfigSecureServer(fd, cert, privKey, kt_rsa);
	    /* SSL_ConfigSecureServer makes copies of the cert and key,
	     * so don't leak these.
	     */
	    SECKEY_DestroyPrivateKey(privKey);
	}
	CERT_DestroyCertificate(cert);
    }
    return rv;
}


