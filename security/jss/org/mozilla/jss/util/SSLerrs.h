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

ER3(SSL_ERROR_WRONG_CERTIFICATE,			SSL_ERROR_BASE + 11,
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


ER3(SSL_ERROR_UNKNOWN_CIPHER_SUITE          , (SSL_ERROR_BASE + 22),
"An unknown SSL cipher suite has been requested.")

ER3(SSL_ERROR_NO_CIPHERS_SUPPORTED          , (SSL_ERROR_BASE + 23),
"No cipher suites are present and enabled in this program.")

ER3(SSL_ERROR_BAD_BLOCK_PADDING             , (SSL_ERROR_BASE + 24),
"SSL received a record with bad block padding.")

ER3(SSL_ERROR_RX_RECORD_TOO_LONG            , (SSL_ERROR_BASE + 25),
"SSL received a record that exceeded the maximum permissible length.")

ER3(SSL_ERROR_TX_RECORD_TOO_LONG            , (SSL_ERROR_BASE + 26),
"SSL attempted to send a record that exceeded the maximum permissible length.")

/*
 * Received a malformed (too long or short or invalid content) SSL handshake.
 */
ER3(SSL_ERROR_RX_MALFORMED_HELLO_REQUEST    , (SSL_ERROR_BASE + 27),
"SSL received a malformed Hello Request handshake message.")

ER3(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO     , (SSL_ERROR_BASE + 28),
"SSL received a malformed Client Hello handshake message.")

ER3(SSL_ERROR_RX_MALFORMED_SERVER_HELLO     , (SSL_ERROR_BASE + 29),
"SSL received a malformed Server Hello handshake message.")

ER3(SSL_ERROR_RX_MALFORMED_CERTIFICATE      , (SSL_ERROR_BASE + 30),
"SSL received a malformed Certificate handshake message.")

ER3(SSL_ERROR_RX_MALFORMED_SERVER_KEY_EXCH  , (SSL_ERROR_BASE + 31),
"SSL received a malformed Server Key Exchange handshake message.")

ER3(SSL_ERROR_RX_MALFORMED_CERT_REQUEST     , (SSL_ERROR_BASE + 32),
"SSL received a malformed Certificate Request handshake message.")

ER3(SSL_ERROR_RX_MALFORMED_HELLO_DONE       , (SSL_ERROR_BASE + 33),
"SSL received a malformed Server Hello Done handshake message.")

ER3(SSL_ERROR_RX_MALFORMED_CERT_VERIFY      , (SSL_ERROR_BASE + 34),
"SSL received a malformed Certificate Verify handshake message.")

ER3(SSL_ERROR_RX_MALFORMED_CLIENT_KEY_EXCH  , (SSL_ERROR_BASE + 35),
"SSL received a malformed Client Key Exchange handshake message.")

ER3(SSL_ERROR_RX_MALFORMED_FINISHED         , (SSL_ERROR_BASE + 36),
"SSL received a malformed Finished handshake message.")

/*
 * Received a malformed (too long or short) SSL record.
 */
ER3(SSL_ERROR_RX_MALFORMED_CHANGE_CIPHER    , (SSL_ERROR_BASE + 37),
"SSL received a malformed Change Cipher Spec record.")

ER3(SSL_ERROR_RX_MALFORMED_ALERT            , (SSL_ERROR_BASE + 38),
"SSL received a malformed Alert record.")

ER3(SSL_ERROR_RX_MALFORMED_HANDSHAKE        , (SSL_ERROR_BASE + 39),
"SSL received a malformed Handshake record.")

ER3(SSL_ERROR_RX_MALFORMED_APPLICATION_DATA , (SSL_ERROR_BASE + 40),
"SSL received a malformed Application Data record.")

/*
 * Received an SSL handshake that was inappropriate for the state we're in.
 * E.g. Server received message from server, or wrong state in state machine.
 */
ER3(SSL_ERROR_RX_UNEXPECTED_HELLO_REQUEST   , (SSL_ERROR_BASE + 41),
"SSL received an unexpected Hello Request handshake message.")

ER3(SSL_ERROR_RX_UNEXPECTED_CLIENT_HELLO    , (SSL_ERROR_BASE + 42),
"SSL received an unexpected Client Hello handshake message.")

ER3(SSL_ERROR_RX_UNEXPECTED_SERVER_HELLO    , (SSL_ERROR_BASE + 43),
"SSL received an unexpected Server Hello handshake message.")

ER3(SSL_ERROR_RX_UNEXPECTED_CERTIFICATE     , (SSL_ERROR_BASE + 44),
"SSL received an unexpected Certificate handshake message.")

ER3(SSL_ERROR_RX_UNEXPECTED_SERVER_KEY_EXCH , (SSL_ERROR_BASE + 45),
"SSL received an unexpected Server Key Exchange handshake message.")

ER3(SSL_ERROR_RX_UNEXPECTED_CERT_REQUEST    , (SSL_ERROR_BASE + 46),
"SSL received an unexpected Certificate Request handshake message.")

ER3(SSL_ERROR_RX_UNEXPECTED_HELLO_DONE      , (SSL_ERROR_BASE + 47),
"SSL received an unexpected Server Hello Done handshake message.")

ER3(SSL_ERROR_RX_UNEXPECTED_CERT_VERIFY     , (SSL_ERROR_BASE + 48),
"SSL received an unexpected Certificate Verify handshake message.")

ER3(SSL_ERROR_RX_UNEXPECTED_CLIENT_KEY_EXCH , (SSL_ERROR_BASE + 49),
"SSL received an unexpected Cllient Key Exchange handshake message.")

ER3(SSL_ERROR_RX_UNEXPECTED_FINISHED        , (SSL_ERROR_BASE + 50),
"SSL received an unexpected Finished handshake message.")

/*
 * Received an SSL record that was inappropriate for the state we're in.
 */
ER3(SSL_ERROR_RX_UNEXPECTED_CHANGE_CIPHER   , (SSL_ERROR_BASE + 51),
"SSL received an unexpected Change Cipher Spec record.")

ER3(SSL_ERROR_RX_UNEXPECTED_ALERT           , (SSL_ERROR_BASE + 52),
"SSL received an unexpected Alert record.")

ER3(SSL_ERROR_RX_UNEXPECTED_HANDSHAKE       , (SSL_ERROR_BASE + 53),
"SSL received an unexpected Handshake record.")

ER3(SSL_ERROR_RX_UNEXPECTED_APPLICATION_DATA, (SSL_ERROR_BASE + 54),
"SSL received an unexpected Application Data record.")

/*
 * Received record/message with unknown discriminant.
 */
ER3(SSL_ERROR_RX_UNKNOWN_RECORD_TYPE        , (SSL_ERROR_BASE + 55),
"SSL received a record with an unknown content type.")

ER3(SSL_ERROR_RX_UNKNOWN_HANDSHAKE          , (SSL_ERROR_BASE + 56),
"SSL received a handshake message with an unknown message type.")

ER3(SSL_ERROR_RX_UNKNOWN_ALERT              , (SSL_ERROR_BASE + 57),
"SSL received an alert record with an unknown alert description.")

/*
 * Received an alert reporting what we did wrong.  (more alerts above)
 */
ER3(SSL_ERROR_CLOSE_NOTIFY_ALERT            , (SSL_ERROR_BASE + 58),
"SSL peer has closed this connection.")

ER3(SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT    , (SSL_ERROR_BASE + 59),
"SSL peer was not expecting a handshake message it received.")

ER3(SSL_ERROR_DECOMPRESSION_FAILURE_ALERT   , (SSL_ERROR_BASE + 60),
"SSL peer was unable to succesfully decompress an SSL record it received.")

ER3(SSL_ERROR_HANDSHAKE_FAILURE_ALERT       , (SSL_ERROR_BASE + 61),
"SSL peer was unable to negotiate an acceptable set of security parameters.")

ER3(SSL_ERROR_ILLEGAL_PARAMETER_ALERT       , (SSL_ERROR_BASE + 62),
"SSL peer rejected a handshake message for unacceptable content.")

ER3(SSL_ERROR_UNSUPPORTED_CERT_ALERT        , (SSL_ERROR_BASE + 63),
"SSL peer does not support certificates of the type it received.")

ER3(SSL_ERROR_CERTIFICATE_UNKNOWN_ALERT     , (SSL_ERROR_BASE + 64),
"SSL peer had some unspecified issue with the certificate it received.")


ER3(SSL_ERROR_GENERATE_RANDOM_FAILURE       , (SSL_ERROR_BASE + 65),
"SSL experienced a failure of its random number generator.")

ER3(SSL_ERROR_SIGN_HASHES_FAILURE           , (SSL_ERROR_BASE + 66),
"Unable to digitally sign data required to verify your certificate.")

ER3(SSL_ERROR_EXTRACT_PUBLIC_KEY_FAILURE    , (SSL_ERROR_BASE + 67),
"SSL was unable to extract the public key from the peer's certificate.")

ER3(SSL_ERROR_SERVER_KEY_EXCHANGE_FAILURE   , (SSL_ERROR_BASE + 68),
"Unspecified failure while processing SSL Server Key Exchange handshake.")

ER3(SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE   , (SSL_ERROR_BASE + 69),
"Unspecified failure while processing SSL Client Key Exchange handshake.")

ER3(SSL_ERROR_ENCRYPTION_FAILURE            , (SSL_ERROR_BASE + 70),
"Bulk data encryption algorithm failed in selected cipher suite.")

ER3(SSL_ERROR_DECRYPTION_FAILURE            , (SSL_ERROR_BASE + 71),
"Bulk data decryption algorithm failed in selected cipher suite.")

ER3(SSL_ERROR_SOCKET_WRITE_FAILURE          , (SSL_ERROR_BASE + 72),
"Attempt to write encrypted data to underlying socket failed.")

ER3(SSL_ERROR_MD5_DIGEST_FAILURE            , (SSL_ERROR_BASE + 73),
"MD5 digest function failed.")

ER3(SSL_ERROR_SHA_DIGEST_FAILURE            , (SSL_ERROR_BASE + 74),
"SHA-1 digest function failed.")

ER3(SSL_ERROR_MAC_COMPUTATION_FAILURE       , (SSL_ERROR_BASE + 75),
"MAC computation failed.")

ER3(SSL_ERROR_SYM_KEY_CONTEXT_FAILURE       , (SSL_ERROR_BASE + 76),
"Failure to create Symmetric Key context.")

ER3(SSL_ERROR_SYM_KEY_UNWRAP_FAILURE        , (SSL_ERROR_BASE + 77),
"Failure to unwrap the Symmetric key in Client Key Exchange message.")

ER3(SSL_ERROR_PUB_KEY_SIZE_LIMIT_EXCEEDED   , (SSL_ERROR_BASE + 78),
"SSL Server attempted to use domestic-grade public key with export cipher suite.")

ER3(SSL_ERROR_IV_PARAM_FAILURE              , (SSL_ERROR_BASE + 79),
"PKCS11 code failed to translate an IV into a param.")

ER3(SSL_ERROR_INIT_CIPHER_SUITE_FAILURE     , (SSL_ERROR_BASE + 80),
"Failed to initialize the selected cipher suite.")

ER3(SSL_ERROR_SESSION_KEY_GEN_FAILURE       , (SSL_ERROR_BASE + 81),
"Client failed to generate session keys for SSL session.")

ER3(SSL_ERROR_NO_SERVER_KEY_FOR_ALG         , (SSL_ERROR_BASE + 82),
"Server has no key for the attempted key exchange algorithm.")

ER3(SSL_ERROR_TOKEN_INSERTION_REMOVAL       , (SSL_ERROR_BASE + 83),
"PKCS#11 token was inserted or removed while operation was in progress.")

ER3(SSL_ERROR_TOKEN_SLOT_NOT_FOUND          , (SSL_ERROR_BASE + 84),
"No PKCS#11 token could be found to do a required operation.")

ER3(SSL_ERROR_NO_COMPRESSION_OVERLAP        , (SSL_ERROR_BASE + 85),
"Cannot communicate securely with peer: no common compression algorithm(s).")

ER3(SSL_ERROR_HANDSHAKE_NOT_COMPLETED       , (SSL_ERROR_BASE + 86),
"Cannot initiate another SSL handshake until current handshake is complete.")

ER3(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE      , (SSL_ERROR_BASE + 87),
"Received incorrect handshakes hash values from peer.")

ER3(SSL_ERROR_CERT_KEA_MISMATCH             , (SSL_ERROR_BASE + 88),
"The certificate provided cannot be used with the selected key exchange algorithm.")

