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

#ifndef __jssl_h__
#define __jssl_h__
#include "seccomon.h"
#include "nspr.h"
#include "cert.h"
#include "key.h"

#include "jni.h"


PR_EXTERN( JNIEnv * ) jni_GetCurrentEnv(void);


/*
 * Callback from SSL for checking certificate of the server
 */
PR_EXTERN( int ) JSSL_ConfirmPeerCert(void *arg, PRFileDesc *fd, PRBool checkSig,
		                PRBool isServer);
	

/*
 * Callback from SSL for checking certificate of the server (but ignore expired status of cert)
 */
PR_EXTERN( int ) JSSL_ConfirmExpiredPeerCert(void *arg, PRFileDesc *fd, PRBool checkSig,
		                PRBool isServer);	         
/* 
 * Expected return values:
 *	0		SECSuccess
 *	-1		SECFailure - No suitable certificate found.
 *	-2 		SECWouldBlock (we're waiting while we ask the user).
 */
PR_EXTERN( int )
JSSL_GetClientAuthData(	void *              arg,
			PRFileDesc *        fd,
			CERTDistNames *     caNames,
			CERTCertificate **  pRetCert,
			SECKEYPrivateKey ** pRetKey);

PR_EXTERN( int ) JSSL_SetupSecureSocket(PRFileDesc *fd, char *url, JNIEnv *cx, jobject obj);

PR_EXTERN( void ) JSSL_Shutdown(void);

PR_EXTERN( SECStatus ) JSSL_ConfigSecureServerNickname(PRFileDesc *fd, 
                                                 const char *nickName);

PR_EXTERN( const ) char * JSSL_Strerror(PRErrorCode errNum);

#if defined(DEBUG_nelsonb)

PR_EXTERN( void ) JSSL_Init(void);

#endif
#endif
