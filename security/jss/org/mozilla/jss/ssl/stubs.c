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
  !!! WARNING: If you add files here, be sure to:
	1. add them to the Metrowerks project
	2. edit Makefile and makefile.win to add a dependency on the .c file
  !!! HEED THE WARNING.
*/

#include "jni.h"

#define IMPLEMENT_org_mozilla_jss_ssl_SSLSocketImpl
#define IMPLEMENT_org_mozilla_jss_ssl_SSLInputStream
#define IMPLEMENT_org_mozilla_jss_ssl_SSLOutputStream
#define IMPLEMENT_org_mozilla_jss_ssl_SSLSecurityStatus

#ifndef FAR
#define FAR
#endif

#define jnInAd jobject	/* was struct java_net_InetAddress *       in JRI */
#define jnSoIm jobject	/* was struct java_net_SocketImpl *        in JRI */
#define nnSoIm jobject	/* was struct netscape_net_SSLSocketImpl * in JRI */
#define nnSeSt jobject	/* was struct netscape_net_SSLSecurityStatus * in JRI */
#define nnInSt jobject	/* was struct netscape_net_SSLInputStream * in JRI */
#define nnOuSt jobject	/* was struct netscape_net_SSLOutputStream * in JRI */

/* #include "netscape_net_SSLSocketImpl.c"	*/
/* #include "netscape_net_SSLInputStream.c"	*/
/* #include "netscape_net_SSLOutputStream.c"	*/

#include "nn_SSLInputStream.h"
#include "nn_SSLOutputStream.h"
#include "nn_SSLSecurityStatus.h"
#include "nn_SSLSocketImpl.h"

#include "nn_SSLInputStream.c"
#include "nn_SSLOutputStream.c"
#include "nn_SSLSecurityStatus.c"
#include "nn_SSLSocketImpl.c"




/* this function exists for the sole purpose of making sure that this
 * obj gets linked into the client.  
 * This function is "called" from LJ_FakeInit() in modules/applet/src/lj_init.c
 */


void _java_jsl_init(void) { }
