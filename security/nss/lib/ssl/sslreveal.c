/* 
 * Accessor functions for SSLSocket private members.
 *
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
 *
 * $Id: sslreveal.c,v 1.2 2002/02/27 04:40:17 nelsonb%netscape.com Exp $
 */

#include "cert.h"
#include "ssl.h"
#include "certt.h"
#include "sslimpl.h"

/* given PRFileDesc, returns a copy of certificate associated with the socket
 * the caller should delete the cert when done with SSL_DestroyCertificate
 */
CERTCertificate * 
SSL_RevealCert(PRFileDesc * fd)
{
  CERTCertificate * cert = NULL;
  sslSocket * sslsocket = NULL;

  sslsocket = ssl_FindSocket(fd);
  
  /* CERT_DupCertificate increases reference count and returns pointer to 
   * the same cert
   */
  if (sslsocket && sslsocket->sec.peerCert)
    cert = CERT_DupCertificate(sslsocket->sec.peerCert);
  
  return cert;
}

/* given PRFileDesc, returns a pointer to PinArg associated with the socket
 */
void * 
SSL_RevealPinArg(PRFileDesc * fd)
{
  sslSocket * sslsocket = NULL;
  void * PinArg = NULL;
  
  sslsocket = ssl_FindSocket(fd);
  
  /* is pkcs11PinArg part of the sslSocket or sslSecurityInfo ? */
  if (sslsocket)
    PinArg = sslsocket->pkcs11PinArg;
  
  return PinArg;
}


/* given PRFileDesc, returns a pointer to the URL associated with the socket
 * the caller should free url when done  
 */
char * 
SSL_RevealURL(PRFileDesc * fd)
{
  sslSocket * sslsocket = NULL;
  char * url = NULL;

  sslsocket = ssl_FindSocket(fd);
  
  if (sslsocket && sslsocket->url)
    url = PL_strdup(sslsocket->url);
  
  return url;
}

