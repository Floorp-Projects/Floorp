/* 
 * Accessor functions for SSLSocket private members.
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
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
/* $Id: sslreveal.c,v 1.8 2010/08/03 18:48:45 wtc%google.com Exp $ */

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


/* given PRFileDesc, returns status information related to extensions 
 * negotiated with peer during the handshake.
 */

SECStatus
SSL_HandshakeNegotiatedExtension(PRFileDesc * socket, 
                                 SSLExtensionType extId,
                                 PRBool *pYes)
{
  /* some decisions derived from SSL_GetChannelInfo */
  sslSocket * sslsocket = NULL;
  SECStatus rv = SECFailure;
  PRBool enoughFirstHsDone = PR_FALSE;

  if (!pYes)
    return rv;

  sslsocket = ssl_FindSocket(socket);
  if (!sslsocket) {
    SSL_DBG(("%d: SSL[%d]: bad socket in HandshakeNegotiatedExtension",
             SSL_GETPID(), socket));
    return rv;
  }

  if (sslsocket->firstHsDone) {
    enoughFirstHsDone = PR_TRUE;
  } else if (sslsocket->ssl3.initialized && ssl3_CanFalseStart(sslsocket)) {
    enoughFirstHsDone = PR_TRUE;
  }

  /* according to public API SSL_GetChannelInfo, this doesn't need a lock */
  if (sslsocket->opt.useSecurity && enoughFirstHsDone) {
    if (sslsocket->ssl3.initialized) { /* SSL3 and TLS */
      /* now we know this socket went through ssl3_InitState() and
       * ss->xtnData got initialized, which is the only member accessed by
       * ssl3_ExtensionNegotiated();
       * Member xtnData appears to get accessed in functions that handle
       * the handshake (hello messages and extension sending),
       * therefore the handshake lock should be sufficient.
       */
      ssl_GetSSL3HandshakeLock(sslsocket);
      *pYes = ssl3_ExtensionNegotiated(sslsocket, extId);
      ssl_ReleaseSSL3HandshakeLock(sslsocket);
      rv = SECSuccess;
    }
  }

  return rv;
}
