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
/*
 * THIS FILE IS MACHINE GENERATED.  DO NOT EDIT!
 *
 * Instead, go edit sslmoz.data and re-run mozStubGen.pl
 * (or you should be able to just run nmake/gmake on NT or Unix)
 *
 * You are meant to create another ".c" file which includes all the
 * appropriate header files for your stubs and include *this* C file
 * in *that* one.
 *
 */

#include "sslmoz.h"

/*
 * If mozilla_event_queue is non-null, then we have to do the crazy
 * function calls.  This is commented out because it's defined for
 * real in one of the files we include.
 */
/* extern void *mozilla_event_queue; */

/*
 * error handling is unfortunately complex -- if error values are
 * not thread-safe, then we should only ever call the get and set
 * methods while on the Mozilla thread.
 */

#define THREADSAFE_ERROR 0
#define GET_ERROR() (nsn_GetSSLError())
#define SET_ERROR(x) (nsn_SetSSLError(x))


/*******************************************************************
 * automatically generated stubs for PR_Connect
 * gen PRStatus PR_FAILURE PR_Connect(SSLFD,  const PRNetAddr *, PRIntervalTime)
 *******************************************************************/

/*
 * stub for PR_Connect -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( PRStatus )
nsn_stuberr_PR_Connect(int* err_code,  SSLFD in0, const PRNetAddr * in1, PRIntervalTime in2) {
    /* return type defn or nothing */
    PRStatus retVal;
    
    *err_code = 0;
    retVal = PR_Connect(in0,in1,in2);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for PR_Accept
 * gen SSLFD    NULL       PR_Accept(SSLFD,         PRNetAddr *, PRIntervalTime)
 *******************************************************************/

/*
 * stub for PR_Accept -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( SSLFD )
nsn_stuberr_PR_Accept(int* err_code,  SSLFD in0, PRNetAddr * in1, PRIntervalTime in2) {
    /* return type defn or nothing */
    SSLFD retVal;
    
    *err_code = 0;
    retVal = PR_Accept(in0,in1,in2);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for PR_Bind
 * gen PRStatus PR_FAILURE PR_Bind(SSLFD,     const PRNetAddr *)
 *******************************************************************/

/*
 * stub for PR_Bind -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( PRStatus )
nsn_stuberr_PR_Bind(int* err_code,  SSLFD in0, const PRNetAddr * in1) {
    /* return type defn or nothing */
    PRStatus retVal;
    
    *err_code = 0;
    retVal = PR_Bind(in0,in1);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for PR_Listen
 * gen PRStatus PR_FAILURE PR_Listen(SSLFD,   PRIntn)
 *******************************************************************/

/*
 * stub for PR_Listen -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( PRStatus )
nsn_stuberr_PR_Listen(int* err_code,  SSLFD in0, PRIntn in1) {
    /* return type defn or nothing */
    PRStatus retVal;
    
    *err_code = 0;
    retVal = PR_Listen(in0,in1);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for PR_Shutdown
 * gen PRStatus PR_FAILURE PR_Shutdown(SSLFD, PRShutdownHow)
 *******************************************************************/

/*
 * stub for PR_Shutdown -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( PRStatus )
nsn_stuberr_PR_Shutdown(int* err_code,  SSLFD in0, PRShutdownHow in1) {
    /* return type defn or nothing */
    PRStatus retVal;
    
    *err_code = 0;
    retVal = PR_Shutdown(in0,in1);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for PR_Close
 * gen PRStatus PR_FAILURE PR_Close(SSLFD)
 *******************************************************************/

/*
 * stub for PR_Close -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( PRStatus )
nsn_stuberr_PR_Close(int* err_code,  SSLFD in0) {
    /* return type defn or nothing */
    PRStatus retVal;
    
    *err_code = 0;
    retVal = PR_Close(in0);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for PR_Recv
 * gen PRInt32 -1 PR_Recv(SSLFD,        void *, PRInt32, PRIntn, PRIntervalTime)
 *******************************************************************/

/*
 * stub for PR_Recv -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( PRInt32 )
nsn_stuberr_PR_Recv(int* err_code,  SSLFD in0, void * in1, PRInt32 in2, PRIntn in3, PRIntervalTime in4) {
    /* return type defn or nothing */
    PRInt32 retVal;
    
    *err_code = 0;
    retVal = PR_Recv(in0,in1,in2,in3,in4);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for PR_Send
 * gen PRInt32 -1 PR_Send(SSLFD,  const void *, PRInt32, PRIntn, PRIntervalTime)
 *******************************************************************/

/*
 * stub for PR_Send -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( PRInt32 )
nsn_stuberr_PR_Send(int* err_code,  SSLFD in0, const void * in1, PRInt32 in2, PRIntn in3, PRIntervalTime in4) {
    /* return type defn or nothing */
    PRInt32 retVal;
    
    *err_code = 0;
    retVal = PR_Send(in0,in1,in2,in3,in4);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for PR_Read
 * gen PRInt32 -1 PR_Read(SSLFD,        void *, PRInt32)
 *******************************************************************/

/*
 * stub for PR_Read -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( PRInt32 )
nsn_stuberr_PR_Read(int* err_code,  SSLFD in0, void * in1, PRInt32 in2) {
    /* return type defn or nothing */
    PRInt32 retVal;
    
    *err_code = 0;
    retVal = PR_Read(in0,in1,in2);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for PR_Write
 * gen PRInt32 -1 PR_Write(SSLFD, const void *, PRInt32)
 *******************************************************************/

/*
 * stub for PR_Write -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( PRInt32 )
nsn_stuberr_PR_Write(int* err_code,  SSLFD in0, const void * in1, PRInt32 in2) {
    /* return type defn or nothing */
    PRInt32 retVal;
    
    *err_code = 0;
    retVal = PR_Write(in0,in1,in2);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for PR_GetPeerName
 * gen PRStatus PR_FAILURE PR_GetPeerName(SSLFD, PRNetAddr *)
 *******************************************************************/

/*
 * stub for PR_GetPeerName -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( PRStatus )
nsn_stuberr_PR_GetPeerName(int* err_code,  SSLFD in0, PRNetAddr * in1) {
    /* return type defn or nothing */
    PRStatus retVal;
    
    *err_code = 0;
    retVal = PR_GetPeerName(in0,in1);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for PR_GetSockName
 * gen PRStatus PR_FAILURE PR_GetSockName(SSLFD, PRNetAddr *)
 *******************************************************************/

/*
 * stub for PR_GetSockName -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( PRStatus )
nsn_stuberr_PR_GetSockName(int* err_code,  SSLFD in0, PRNetAddr * in1) {
    /* return type defn or nothing */
    PRStatus retVal;
    
    *err_code = 0;
    retVal = PR_GetSockName(in0,in1);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for PR_GetSocketOption
 * gen PRStatus PR_FAILURE PR_GetSocketOption(SSLFD,       PRSocketOptionData *)
 *******************************************************************/

/*
 * stub for PR_GetSocketOption -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( PRStatus )
nsn_stuberr_PR_GetSocketOption(int* err_code,  SSLFD in0, PRSocketOptionData * in1) {
    /* return type defn or nothing */
    PRStatus retVal;
    
    *err_code = 0;
    retVal = PR_GetSocketOption(in0,in1);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for PR_SetSocketOption
 * gen PRStatus PR_FAILURE PR_SetSocketOption(SSLFD, const PRSocketOptionData *)
 *******************************************************************/

/*
 * stub for PR_SetSocketOption -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( PRStatus )
nsn_stuberr_PR_SetSocketOption(int* err_code,  SSLFD in0, const PRSocketOptionData * in1) {
    /* return type defn or nothing */
    PRStatus retVal;
    
    *err_code = 0;
    retVal = PR_SetSocketOption(in0,in1);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for SSL_ClearSessionCache
 * gen void  NULL SSL_ClearSessionCache()
 *******************************************************************/

/*
 * stub for SSL_ClearSessionCache -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( void )
nsn_stuberr_SSL_ClearSessionCache(int* err_code ) {
    /* return type defn or nothing */
    
    
    *err_code = 0;
    SSL_ClearSessionCache();
    *err_code = GET_ERROR();

    /* return result or nothing */
    
}


/*******************************************************************
 * automatically generated stubs for SSL_ConfigServerSessionIDCache
 * gen int   -1   SSL_ConfigServerSessionIDCache(int, time_t, time_t, const char *)
 *******************************************************************/

/*
 * stub for SSL_ConfigServerSessionIDCache -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( int )
nsn_stuberr_SSL_ConfigServerSessionIDCache(int* err_code,  int in0, time_t in1, time_t in2, const char * in3) {
    /* return type defn or nothing */
    int retVal;
    
    *err_code = 0;
    retVal = SSL_ConfigServerSessionIDCache(in0,in1,in2,in3);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for SSL_DataPending
 * gen int   -1   SSL_DataPending(SSLFD)
 *******************************************************************/

/*
 * stub for SSL_DataPending -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( int )
nsn_stuberr_SSL_DataPending(int* err_code,  SSLFD in0) {
    /* return type defn or nothing */
    int retVal;
    
    *err_code = 0;
    retVal = SSL_DataPending(in0);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for SSL_Enable
 * gen int   -1   SSL_Enable(SSLFD, int, int)
 *******************************************************************/

/*
 * stub for SSL_Enable -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( int )
nsn_stuberr_SSL_Enable(int* err_code,  SSLFD in0, int in1, int in2) {
    /* return type defn or nothing */
    int retVal;
    
    *err_code = 0;
    retVal = SSL_Enable(in0,in1,in2);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for SSL_EnableCipher
 * gen int   -1   SSL_EnableCipher(long, int);
 *******************************************************************/

/*
 * stub for SSL_EnableCipher -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( int )
nsn_stuberr_SSL_EnableCipher(int* err_code,  long in0, int in1) {
    /* return type defn or nothing */
    int retVal;
    
    *err_code = 0;
    retVal = SSL_EnableCipher(in0,in1);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for SSL_EnableDefault
 * gen int   -1   SSL_EnableDefault(int, int)
 *******************************************************************/

/*
 * stub for SSL_EnableDefault -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( int )
nsn_stuberr_SSL_EnableDefault(int* err_code,  int in0, int in1) {
    /* return type defn or nothing */
    int retVal;
    
    *err_code = 0;
    retVal = SSL_EnableDefault(in0,in1);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for SSL_ForceHandshake
 * gen int   -1   SSL_ForceHandshake(SSLFD)
 *******************************************************************/

/*
 * stub for SSL_ForceHandshake -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( int )
nsn_stuberr_SSL_ForceHandshake(int* err_code,  SSLFD in0) {
    /* return type defn or nothing */
    int retVal;
    
    *err_code = 0;
    retVal = SSL_ForceHandshake(in0);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for SSL_GetClientAuthDataHook
 * gen int   -1   SSL_GetClientAuthDataHook(SSLFD, SSLGetClientAuthData, void *)
 *******************************************************************/

/*
 * stub for SSL_GetClientAuthDataHook -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( int )
nsn_stuberr_SSL_GetClientAuthDataHook(int* err_code,  SSLFD in0, SSLGetClientAuthData in1, void * in2) {
    /* return type defn or nothing */
    int retVal;
    
    *err_code = 0;
    retVal = SSL_GetClientAuthDataHook(in0,in1,in2);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for SSL_HandshakeCallback
 * gen int   -1   SSL_HandshakeCallback(SSLFD, SSLHandshakeCallback, void *)
 *******************************************************************/

/*
 * stub for SSL_HandshakeCallback -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( int )
nsn_stuberr_SSL_HandshakeCallback(int* err_code,  SSLFD in0, SSLHandshakeCallback in1, void * in2) {
    /* return type defn or nothing */
    int retVal;
    
    *err_code = 0;
    retVal = SSL_HandshakeCallback(in0,in1,in2);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for SSL_ImportFD
 * gen SSLFD NULL SSL_ImportFD(SSLFD, SSLFD)
 *******************************************************************/

/*
 * stub for SSL_ImportFD -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( SSLFD )
nsn_stuberr_SSL_ImportFD(int* err_code,  SSLFD in0, SSLFD in1) {
    /* return type defn or nothing */
    SSLFD retVal;
    
    *err_code = 0;
    retVal = SSL_ImportFD(in0,in1);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for SSL_InvalidateSession
 * gen int   -1   SSL_InvalidateSession(SSLFD)
 *******************************************************************/

/*
 * stub for SSL_InvalidateSession -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( int )
nsn_stuberr_SSL_InvalidateSession(int* err_code,  SSLFD in0) {
    /* return type defn or nothing */
    int retVal;
    
    *err_code = 0;
    retVal = SSL_InvalidateSession(in0);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for SSL_PeerCertificate
 * gen (struct CERTCertificateStr *) NULL SSL_PeerCertificate(SSLFD)
 *******************************************************************/

/*
 * stub for SSL_PeerCertificate -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( struct CERTCertificateStr * )
nsn_stuberr_SSL_PeerCertificate(int* err_code,  SSLFD in0) {
    /* return type defn or nothing */
    struct CERTCertificateStr * retVal;
    
    *err_code = 0;
    retVal = SSL_PeerCertificate(in0);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for SSL_RedoHandshake
 * gen int   -1   SSL_RedoHandshake(SSLFD)
 *******************************************************************/

/*
 * stub for SSL_RedoHandshake -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( int )
nsn_stuberr_SSL_RedoHandshake(int* err_code,  SSLFD in0) {
    /* return type defn or nothing */
    int retVal;
    
    *err_code = 0;
    retVal = SSL_RedoHandshake(in0);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for SSL_ResetHandshake
 * gen int   -1   SSL_ResetHandshake(SSLFD, int)
 *******************************************************************/

/*
 * stub for SSL_ResetHandshake -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( int )
nsn_stuberr_SSL_ResetHandshake(int* err_code,  SSLFD in0, int in1) {
    /* return type defn or nothing */
    int retVal;
    
    *err_code = 0;
    retVal = SSL_ResetHandshake(in0,in1);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for SSL_SecurityStatus
 * gen int   -1   SSL_SecurityStatus(SSLFD, int *, char **, int *, int *, char **, char **)
 *******************************************************************/

/*
 * stub for SSL_SecurityStatus -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( int )
nsn_stuberr_SSL_SecurityStatus(int* err_code,  SSLFD in0, int * in1, char ** in2, int * in3, int * in4, char ** in5, char ** in6) {
    /* return type defn or nothing */
    int retVal;
    
    *err_code = 0;
    retVal = SSL_SecurityStatus(in0,in1,in2,in3,in4,in5,in6);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for SSL_SetPolicy
 * gen int   -1   SSL_SetPolicy(long, int);
 *******************************************************************/

/*
 * stub for SSL_SetPolicy -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( int )
nsn_stuberr_SSL_SetPolicy(int* err_code,  long in0, int in1) {
    /* return type defn or nothing */
    int retVal;
    
    *err_code = 0;
    retVal = SSL_SetPolicy(in0,in1);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for SSL_SetURL
 * gen int   -1   SSL_SetURL(SSLFD, char *)
 *******************************************************************/

/*
 * stub for SSL_SetURL -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( int )
nsn_stuberr_SSL_SetURL(int* err_code,  SSLFD in0, char * in1) {
    /* return type defn or nothing */
    int retVal;
    
    *err_code = 0;
    retVal = SSL_SetURL(in0,in1);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for CERT_DestroyCertificate
 * gen void NULL CERT_DestroyCertificate(CERTCertificate *)
 *******************************************************************/

/*
 * stub for CERT_DestroyCertificate -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( void )
nsn_stuberr_CERT_DestroyCertificate(int* err_code,  CERTCertificate * in0) {
    /* return type defn or nothing */
    
    
    *err_code = 0;
    CERT_DestroyCertificate(in0);
    *err_code = GET_ERROR();

    /* return result or nothing */
    
}


/*******************************************************************
 * automatically generated stubs for SECMOD_init
 * gen void NULL SECMOD_init(char *)
 *******************************************************************/

/*
 * stub for SECMOD_init -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( void )
nsn_stuberr_SECMOD_init(int* err_code,  char * in0) {
    /* return type defn or nothing */
    
    
    *err_code = 0;
    SECMOD_init(in0);
    *err_code = GET_ERROR();

    /* return result or nothing */
    
}


/*******************************************************************
 * automatically generated stubs for JSSL_SetupSecureSocket
 * gen int   -1   JSSL_SetupSecureSocket(PRFileDesc *, char *, void *)
 *******************************************************************/

/*
 * stub for JSSL_SetupSecureSocket -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( int )
nsn_stuberr_JSSL_SetupSecureSocket(int* err_code,  
		PRFileDesc * in0, 
		char * url, 		/* url */
		void * env,         /* jni env */
		void * obj) {       /* sslsocketimpl object */
    /* return type defn or nothing */
    int retVal;
    
    *err_code = 0;
    retVal = JSSL_SetupSecureSocket(in0,url,env,obj);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}


/*******************************************************************
 * automatically generated stubs for JSSL_ConfigSecureServerNickname
 * gen int   -1   JSSL_ConfigSecureServerNickname(PRFileDesc *, const char *)
 *******************************************************************/

/*
 * stub for JSSL_ConfigSecureServerNickname -- call this from your thread
 * -- this one also returns the error code of the internal method
 *    by writing to the err_code argument you pass it
 */
PR_IMPLEMENT( int )
nsn_stuberr_JSSL_ConfigSecureServerNickname(int* err_code,  PRFileDesc * in0, const char * in1) {
    /* return type defn or nothing */
    int retVal;
    
    *err_code = 0;
    retVal = JSSL_ConfigSecureServerNickname(in0,in1);
    *err_code = GET_ERROR();

    /* return result or nothing */
    return retVal;
}

