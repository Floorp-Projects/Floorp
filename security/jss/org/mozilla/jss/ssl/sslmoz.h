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
 * Include this file to use the stubs in sslmoz.c and you're all set.  
 *
 */

typedef PRFileDesc * SSLFD;

PR_EXTERN( PRStatus ) nsn_stuberr_PR_Connect(int* err_code,  SSLFD in0, const PRNetAddr * in1, PRIntervalTime in2);
PR_EXTERN( SSLFD ) nsn_stuberr_PR_Accept(int* err_code,  SSLFD in0, PRNetAddr * in1, PRIntervalTime in2);
PR_EXTERN( PRStatus ) nsn_stuberr_PR_Bind(int* err_code,  SSLFD in0, const PRNetAddr * in1);
PR_EXTERN( PRStatus ) nsn_stuberr_PR_Listen(int* err_code,  SSLFD in0, PRIntn in1);
PR_EXTERN( PRStatus ) nsn_stuberr_PR_Shutdown(int* err_code,  SSLFD in0, PRShutdownHow in1);
PR_EXTERN( PRStatus ) nsn_stuberr_PR_Close(int* err_code,  SSLFD in0);
PR_EXTERN( PRInt32 ) nsn_stuberr_PR_Recv(int* err_code,  SSLFD in0, void * in1, PRInt32 in2, PRIntn in3, PRIntervalTime in4);
PR_EXTERN( PRInt32 ) nsn_stuberr_PR_Send(int* err_code,  SSLFD in0, const void * in1, PRInt32 in2, PRIntn in3, PRIntervalTime in4);
PR_EXTERN( PRInt32 ) nsn_stuberr_PR_Read(int* err_code,  SSLFD in0, void * in1, PRInt32 in2);
PR_EXTERN( PRInt32 ) nsn_stuberr_PR_Write(int* err_code,  SSLFD in0, const void * in1, PRInt32 in2);
PR_EXTERN( PRStatus ) nsn_stuberr_PR_GetPeerName(int* err_code,  SSLFD in0, PRNetAddr * in1);
PR_EXTERN( PRStatus ) nsn_stuberr_PR_GetSockName(int* err_code,  SSLFD in0, PRNetAddr * in1);
PR_EXTERN( PRStatus ) nsn_stuberr_PR_GetSocketOption(int* err_code,  SSLFD in0, PRSocketOptionData * in1);
PR_EXTERN( PRStatus ) nsn_stuberr_PR_SetSocketOption(int* err_code,  SSLFD in0, const PRSocketOptionData * in1);
PR_EXTERN( void ) nsn_stuberr_SSL_ClearSessionCache(int* err_code );
PR_EXTERN( int ) nsn_stuberr_SSL_ConfigServerSessionIDCache(int* err_code,  int in0, time_t in1, time_t in2, const char * in3);
PR_EXTERN( int ) nsn_stuberr_SSL_DataPending(int* err_code,  SSLFD in0);
PR_EXTERN( int ) nsn_stuberr_SSL_Enable(int* err_code,  SSLFD in0, int in1, int in2);
PR_EXTERN( int ) nsn_stuberr_SSL_EnableCipher(int* err_code,  long in0, int in1);
PR_EXTERN( int ) nsn_stuberr_SSL_EnableDefault(int* err_code,  int in0, int in1);
PR_EXTERN( int ) nsn_stuberr_SSL_ForceHandshake(int* err_code,  SSLFD in0);
PR_EXTERN( int ) nsn_stuberr_SSL_GetClientAuthDataHook(int* err_code,  SSLFD in0, SSLGetClientAuthData in1, void * in2);
PR_EXTERN( int ) nsn_stuberr_SSL_HandshakeCallback(int* err_code,  SSLFD in0, SSLHandshakeCallback in1, void * in2);
PR_EXTERN( SSLFD ) nsn_stuberr_SSL_ImportFD(int* err_code,  SSLFD in0, SSLFD in1);
PR_EXTERN( int ) nsn_stuberr_SSL_InvalidateSession(int* err_code,  SSLFD in0);
PR_EXTERN( struct ) CERTCertificateStr * nsn_stuberr_SSL_PeerCertificate(int* err_code,  SSLFD in0);
PR_EXTERN( int ) nsn_stuberr_SSL_RedoHandshake(int* err_code,  SSLFD in0);
PR_EXTERN( int ) nsn_stuberr_SSL_ResetHandshake(int* err_code,  SSLFD in0, int in1);
PR_EXTERN( int ) nsn_stuberr_SSL_SecurityStatus(int* err_code,  SSLFD in0, int * in1, char ** in2, int * in3, int * in4, char ** in5, char ** in6);
PR_EXTERN( int ) nsn_stuberr_SSL_SetPolicy(int* err_code,  long in0, int in1);
PR_EXTERN( int ) nsn_stuberr_SSL_SetURL(int* err_code,  SSLFD in0, char * in1);
PR_EXTERN( void ) nsn_stuberr_CERT_DestroyCertificate(int* err_code,  CERTCertificate * in0);
PR_EXTERN( void ) nsn_stuberr_SECMOD_init(int* err_code,  char * in0);
#ifdef DOHCLSTUFF
PR_EXTERN( int ) nsn_stuberr_JSSL_InitDefaultCertDB(int* err_code,  const char * in0);
PR_EXTERN( int ) nsn_stuberr_JSSL_InitDefaultKeyDB(int* err_code,  const char * in0);
#endif
PR_EXTERN( int ) nsn_stuberr_JSSL_SetupSecureSocket(int* err_code,  PRFileDesc * in0, char * url, void * env, void * obj);
PR_EXTERN( int ) nsn_stuberr_JSSL_ConfigSecureServerNickname(int* err_code,  PRFileDesc * in0, const char * in1);
