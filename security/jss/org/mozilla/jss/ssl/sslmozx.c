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
 * This is just a wrapper to pull in the automatically-generated
 * sslmoz.c with the appropriate header files
 *
 */

#undef MOZILLA_CLIENT

/* #include "javaString.h"	*/
/* #include "oobj.h"		*/
/* #include "interpreter.h"	*/

#include "ssl.h"
#include "cert.h"	/* for CERT_DestroyCertificate */
#include "nspr.h"
#include "jssl.h"
#include "secmod.h"

/* #include "java.h"	*/
/* #include "xp.h"	*/
/* #include "nsn.h"	*/

/* #include "socket_md.h"	*/

/* #include "prnetdb.h"		*/
/* #include "xpgetstr.h"	*/
/* #include "xp_error.h"	*/

/* strange compile-time problems lead to this unfortunate hack */
#if 0
#include "libevent.h"  /* for ET_moz_CallFunction() */
#else
/* from libevent.h */
typedef void (*ETVoidPtrFunc) (void * data);
extern void ET_moz_CallFunction (ETVoidPtrFunc fn, void *data);
#endif


#ifdef MOZILLA_CLIENT
#include "secnav.h"
#endif

#define nsn_GetSSLError PORT_GetError
#define nsn_SetSSLError PORT_SetError


#include "sslmoz.c"
