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

/* SSL-specific security error codes  */
/* caller must include "sslerr.h" */

ER3(SSL_ERROR_EXPORT_ONLY_SERVER,			SSL_ERROR_BASE + 0,
"Unable to communicate securely.  Peer does not support high-grade encryption.")

ER3(SSL_ERROR_US_ONLY_SERVER,				SSL_ERROR_BASE + 1,
"Unable to communicate securely.  Peer requires high-grade encryption which is not supported.")

ER3(SSL_ERROR_NO_CYPHER_OVERLAP,			SSL_ERROR_BASE + 2,
"Cannot communicate securely with peer: no common encryption algorithm(s).")

ER3(SSL_ERROR_NO_CERTIFICATE,				SSL_ERROR_BASE + 3,
"Unable to find the certificate or key necessary for authentication.")

ER3(SSL_ERROR_BAD_CERTIFICATE,				SSL_ERROR_BASE + 4,
"Unable to communicate securely with peer: peers's certificate was rejected.")

/* unused						(SSL_ERROR_BASE + 5),*/

ER3(SSL_ERROR_BAD_CLIENT,				SSL_ERROR_BASE + 6,
"The server has encountered bad data from the client.")

ER3(SSL_ERROR_BAD_SERVER,				SSL_ERROR_BASE + 7,
"The client has encountered bad data from the server.")

ER3(SSL_ERROR_UNSUPPORTED_CERTIFICATE_TYPE,		SSL_ERROR_BASE + 8,
"Unsupported certificate type.")

ER3(SSL_ERROR_UNSUPPORTED_VERSION,			SSL_ERROR_BASE + 9,
"Peer using unsupported version of security protocol.")

/* unused						(SSL_ERROR_BASE + 10),*/

ER3(SSL_ERROR_WRONG_CERTIFICATE,	SSL_ERROR_BASE + 11,
"Client authentication failed: private key in key database does not match public key in certificate database.")

ER3(SSL_ERROR_BAD_CERT_DOMAIN,				SSL_ERROR_BASE + 12,
"Unable to communicate securely with peer: requested domain name does not match the server's certificate.")

/* SSL_ERROR_POST_WARNING				(SSL_ERROR_BASE + 13),
   defined in sslerr.h
*/

ER3(SSL_ERROR_SSL2_DISABLED,				(SSL_ERROR_BASE + 14),
"Peer only supports SSL version 2, which is locally disabled.")


ER3(SSL_ERROR_BAD_MAC_READ,				(SSL_ERROR_BASE + 15),
"SSL received a record with an incorrect Message Authentication Code.")

ER3(SSL_ERROR_BAD_MAC_ALERT,				(SSL_ERROR_BASE + 16),
"SSL peer reports incorrect Message Authentication Code.")

ER3(SSL_ERROR_BAD_CERT_ALERT,				(SSL_ERROR_BASE + 17),
"SSL peer cannot verify your certificate.")

ER3(SSL_ERROR_REVOKED_CERT_ALERT,			(SSL_ERROR_BASE + 18),
"SSL peer rejected your certificate as revoked.")

ER3(SSL_ERROR_EXPIRED_CERT_ALERT,			(SSL_ERROR_BASE + 19),
"SSL peer rejected your certificate as expired.")

ER3(SSL_ERROR_SSL_DISABLED,				(SSL_ERROR_BASE + 20),
"Cannot connect: SSL is disabled.")

ER3(SSL_ERROR_FORTEZZA_PQG,				(SSL_ERROR_BASE + 21),
"Cannot connect: SSL peer is in another FORTEZZA domain.")

