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
 * The Original Code is the Netscape Security Services for Java.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2001
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

#include <jni.h>

#include <nspr.h>
#include <java_ids.h>
#include <jss_exceptions.h>
#include <secitem.h>
#include <jssutil.h>
#include <certt.h>
#include <keythi.h>
#include <keyhi.h>
#include <cert.h>
#include <pk11func.h>
#include "jssl.h"
#include <ssl.h>
#include <sslerr.h>
#include <pk11util.h>
#include <secder.h>

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

/* 
 * This callback is called when the peer has request you to send you
 * client-auth certificate. You get to pick which one you want
 * to send.
 *
 * Expected return values:
 *    0        SECSuccess
 *    -1        SECFailure - No suitable certificate found.
 *    -2         SECWouldBlock (we're waiting while we ask the user).
 */
SECStatus
JSSL_CallCertSelectionCallback(    void * arg,
            PRFileDesc *        fd,
            CERTDistNames *     caNames,
            CERTCertificate **  pRetCert,
            SECKEYPrivateKey ** pRetKey)
{
    CERTCertificate *  cert;
    PK11SlotInfo *  slot;
    SECKEYPrivateKey * privkey;
    jobject             nicknamecallback = (jobject)arg;
    SECStatus          rv         = SECFailure;
    CERTCertNicknames * names;
    int                 i;
    int count           =0;
    jclass    vectorclass;
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

    if((*JSS_javaVM)->AttachCurrentThread(JSS_javaVM, (void**)&env, NULL) != 0){
        PR_ASSERT(PR_FALSE);
        goto loser;
    }
    PR_ASSERT(env != NULL);
    

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
        cert = JSS_PK11_findCertAndSlotFromNickname(
                names->nicknames[i],
                NULL /*pinarg*/,
                &slot);
        if ( !cert )
            continue;

        /* Only check unexpired certs */
        if ( CERT_CheckCertValidTimes(cert, PR_Now(), PR_TRUE /*allowOverride*/)
                 != secCertTimeValid ) {
        if (debug_cc) { PR_fprintf(PR_STDOUT,"  not valid\n"); }
            CERT_DestroyCertificate(cert);
            PK11_FreeSlot(slot);
            continue;
        }
        rv = secCmpCertChainWCANames(cert, caNames);
        if ( rv == SECSuccess ) {
            if (debug_cc) { PR_fprintf(PR_STDOUT,"  matches ca name\n"); }

            privkey = PK11_FindPrivateKeyFromCert(slot, cert, NULL /*pinarg*/);

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
        PK11_FreeSlot(slot);
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
    cert = JSS_PK11_findCertAndSlotFromNickname(chosen_nickname_for_c,
        NULL /*pinarg*/,
        &slot);

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

        privkey = PK11_FindPrivateKeyFromCert(slot, cert, NULL /*pinarg*/);
        PK11_FreeSlot(slot);
        if ( privkey == NULL )  {
        CERT_DestroyCertificate(cert);
        rv = SECFailure;
        goto loser;
    }
    if (debug_cc) { PR_fprintf(PR_STDOUT,"  found privkey. returning\n"); }

    *pRetCert = cert;
    *pRetKey  = privkey;
    rv = SECSuccess;

loser:
    return rv;
}


void
JSSL_HandshakeCallback(PRFileDesc *fd, void *arg)
{
    JSSL_SocketData *sock = (JSSL_SocketData*) arg;
    jclass sockClass;
    jmethodID notifierID;
    JNIEnv *env;

    PR_ASSERT(sock!=NULL);

    /* get the JNI environment */
    if((*JSS_javaVM)->AttachCurrentThread(JSS_javaVM, (void**)&env, NULL) != 0){
        PR_ASSERT(PR_FALSE);
        goto finish;
    }
    PR_ASSERT(env != NULL);

    /* get the handshake notification method ID */
    PR_ASSERT(sock->socketObject!=NULL);
    sockClass = (*env)->GetObjectClass(env, sock->socketObject);
    notifierID = (*env)->GetMethodID(env, sockClass,
        SSLSOCKET_HANDSHAKE_NOTIFIER_NAME, SSLSOCKET_HANDSHAKE_NOTIFIER_SIG);
    if(notifierID == NULL) goto finish;

    /* call the handshake notification method */
    (*env)->CallVoidMethod(env, sock->socketObject, notifierID);

finish:
    return;
}

/*
 * Callback from SSL for checking certificate the peer (other end of
 * the socket) presents.
 */
SECStatus
JSSL_DefaultCertAuthCallback(void *arg, PRFileDesc *fd, PRBool checkSig,
             PRBool isServer)
{
    char *          hostname = NULL;
    SECStatus         rv    = SECFailure;
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
    hostname = SSL_RevealURL(fd);    /* really is a hostname, not a URL */
    if (hostname && hostname[0]) {
        rv = CERT_VerifyCertName(peerCert, hostname);
        PORT_Free(hostname); 
    } else
        rv = SECFailure;

    if (peerCert) CERT_DestroyCertificate(peerCert);
    return rv;
}

static void
addToVerifyLog(JNIEnv *env, CERTVerifyLog *log, CERTCertificate *cert,
    unsigned long error, unsigned int depth)
{
    CERTVerifyLogNode *node, *tnode;

    PR_ASSERT(log != NULL);

    PL_ARENA_ALLOCATE(node, log->arena, sizeof(CERTVerifyLogNode));
    if ( node == NULL ) {
        JSS_throw(env, OUT_OF_MEMORY_ERROR);
        return;
    }
    node->cert = CERT_DupCertificate(cert);
    node->error = error;
    node->depth = depth;
    node->arg = NULL;

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

/*
 * Callback from SSL for checking a (possibly) expired
 * certificate the peer presents.
 *
 * obj - a jobject -> instance of a class implementing 
 *       the SSLCertificateApprovalCallback interface
 */
SECStatus
JSSL_JavaCertAuthCallback(void *arg, PRFileDesc *fd, PRBool checkSig,
             PRBool isServer)
{
    CERTCertificate *peerCert=NULL;
    CERTVerifyLog log;
    JNIEnv *env;
    jobject validityStatus;
    jmethodID addReasonMethod;
    int certUsage;
    int checkcn_rv;
    jmethodID approveMethod;
    jboolean result;
    char *hostname=NULL;
    SECStatus retval = SECFailure;
    SECStatus verificationResult;

    PR_ASSERT(arg != NULL);
    PR_ASSERT(fd != NULL);

    /* initialize logging structures */
    log.arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if( log.arena == NULL ) return SECFailure;
    log.head = NULL;
    log.tail = NULL;
    log.count = 0;

    /* get the JNI environment */
    if((*JSS_javaVM)->AttachCurrentThread(JSS_javaVM, (void**)&env, NULL) != 0){
        PR_ASSERT(PR_FALSE);
        goto finish;
    }

    /* First, get a handle on the cert that the peer presented */
    peerCert = SSL_PeerCertificate(fd);
    
    /* if peer didn't present a cert, why am I called? */
    if (peerCert == NULL) goto finish;

    certUsage = isServer ? certUsageSSLClient : certUsageSSLServer;

    /* 
     * verify it against current time - (can't use
     * CERT_VerifyCertNow() since it doesn't allow passing of
     * logging parameter)
     */

    verificationResult = CERT_VerifyCert(   CERT_GetDefaultCertDB(),
                            peerCert,
                            checkSig,
                            certUsage,
                            PR_Now(),
                            NULL /*pinarg*/,
                            &log);

    if (verificationResult == SECSuccess && log.count > 0) {
        verificationResult = SECFailure;
    }

    /*
     * Verify the domain name of the cert.
     */
    hostname = SSL_RevealURL(fd);    /* really is a hostname, not a URL */
    if (hostname && hostname[0]) {
        checkcn_rv = CERT_VerifyCertName(peerCert, hostname);
        PORT_Free(hostname);
    } else {
        checkcn_rv = SECFailure;
    }
    if (checkcn_rv != SECSuccess) {
        addToVerifyLog(env, &log,peerCert,SSL_ERROR_BAD_CERT_DOMAIN,0);
        if((*env)->ExceptionOccurred(env) != NULL) goto finish;
        verificationResult = SECFailure;
    }

    /*
     * create a new ValidityStatus object
     */
    {
        jclass clazz;
        jmethodID cons;

        clazz = (*env)->FindClass(env, SSLCERT_APP_CB_VALIDITY_STATUS_CLASS);
        if( clazz == NULL ) goto finish;

        cons = (*env)->GetMethodID(env, clazz,
                        PLAIN_CONSTRUCTOR, PLAIN_CONSTRUCTOR_SIG);
        if( cons == NULL ) goto finish;

        validityStatus = (*env)->NewObject(env, clazz, cons);
        if( validityStatus == NULL ) {
            goto finish;
        }

        /* get the addReason methodID while we're at it */
        addReasonMethod = (*env)->GetMethodID(env, clazz,
            SSLCERT_APP_CB_VALIDITY_STATUS_ADD_REASON_NAME,
            SSLCERT_APP_CB_VALIDITY_STATUS_ADD_REASON_SIG);
        if( addReasonMethod == NULL ) {
            goto finish;
        }
    }
    
    /*
     * Load up the ValidityStatus object with all the reasons for failure
     */
    if (verificationResult == SECFailure)  {
        CERTVerifyLogNode *node;
        int error;
        CERTCertificate *errorcert=NULL;
        int depth;
        jobject ninjacert;

        node = log.head;
        while (node) {
            error = node->error;
            errorcert = node->cert;
            node->cert = NULL;
            depth = node->depth;

            ninjacert = JSS_PK11_wrapCert(env,&errorcert);
            (*env)->CallVoidMethod(env, validityStatus, addReasonMethod,
                error,
                ninjacert,
                depth
                );

            node = node->next;
        }
    }

    /*
     * Call the approval callback
     */
    {
        jobject approvalCallbackObj;
        jclass approvalCallbackClass;
        jobject peerninjacert;

        approvalCallbackObj = (jobject) arg;

        approvalCallbackClass = (*env)->GetObjectClass(env,approvalCallbackObj);
        approveMethod = (*env)->GetMethodID(
            env,
            approvalCallbackClass,
            SSLCERT_APP_CB_APPROVE_NAME,
            SSLCERT_APP_CB_APPROVE_SIG);
        if( approveMethod == NULL ) {
            PR_ASSERT(PR_FALSE);
            goto finish;
        }

        peerninjacert = JSS_PK11_wrapCert(env,&peerCert);
        if( peerninjacert == NULL) {
            PR_ASSERT(PR_FALSE);
            goto finish;
        }
        result = (*env)->CallBooleanMethod(env, approvalCallbackObj,
            approveMethod, peerninjacert, validityStatus);
        if( result == JNI_TRUE ) {
            retval = SECSuccess;
        }
    }

finish:
    if( peerCert != NULL ) {
        CERT_DestroyCertificate(peerCert);
    }
    PORT_FreeArena(log.arena, PR_FALSE);
    return retval;
}

SECStatus
JSSL_GetClientAuthData( void * arg,
                        PRFileDesc *        fd,
                        CERTDistNames *     caNames,
                        CERTCertificate **  pRetCert,
                        SECKEYPrivateKey ** pRetKey)
{
    SECKEYPrivateKey * privkey;
    JSSL_SocketData *  sock;
    SECStatus          rv         = SECFailure;

    PR_ASSERT(arg != NULL);
    sock = (JSSL_SocketData*) arg;

    if (sock->clientCert) {
        privkey = PK11_FindPrivateKeyFromCert(sock->clientCertSlot,
            sock->clientCert, NULL /*pinarg*/);
        if ( privkey ) {
            rv = SECSuccess;
            *pRetCert = CERT_DupCertificate(sock->clientCert); 
            *pRetKey  = privkey;
        }
    }
    
    return rv;
}

/*
 * Callback from SSL for checking a (possibly) expired
 * certificate the peer presents.
 */
SECStatus
JSSL_ConfirmExpiredPeerCert(void *arg, PRFileDesc *fd, PRBool checkSig,
             PRBool isServer)
{
    SECStatus rv=SECFailure;
    SECCertUsage certUsage;
    CERTCertificate* peerCert=NULL;
    int64 notAfter, notBefore;

    certUsage = isServer ? certUsageSSLClient : certUsageSSLServer;

    peerCert = SSL_PeerCertificate(fd);

    if (peerCert) {
        rv = CERT_GetCertTimes(peerCert, &notBefore, &notAfter);
        if (rv != SECSuccess) goto finish;

        /*
         * Verify the certificate based on it's expiry date. This should
         * always succeed, if the cert is trusted. It doesn't care if
         * the cert has expired.
         */
        rv = CERT_VerifyCert(CERT_GetDefaultCertDB(), peerCert,
                             checkSig, certUsage, 
                             notAfter, NULL /*pinarg*/,
                             NULL /* log */);
    }
    if ( rv != SECSuccess ) goto finish;

    if( ! isServer )  {
        /* This is the client side of an SSL connection.
        * Now check the name field in the cert against the desired hostname.
        * NB: This is our only defense against Man-In-The-Middle (MITM) attacks!
        */
        if( peerCert == NULL ) {
            rv = SECFailure;
        } else {
            char* hostname = NULL;
            hostname = SSL_RevealURL(fd); /* really is a hostname, not a URL */
            if (hostname && hostname[0]) {
                rv = CERT_VerifyCertName(peerCert, hostname);
                PORT_Free(hostname);
            } else {
                rv = SECFailure;
            }
        }
    }

finish:
    if (peerCert!=NULL) CERT_DestroyCertificate(peerCert);
    return rv;
}
