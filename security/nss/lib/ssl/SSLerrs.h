/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define UNUSED_ERROR(x) ER3(SSL_ERROR_UNUSED_##x, (SSL_ERROR_BASE + x), \
                            "Unrecognized SSL error_code.")

/* SSL-specific security error codes  */
/* caller must include "sslerr.h" */

ER3(SSL_ERROR_EXPORT_ONLY_SERVER, SSL_ERROR_BASE + 0,
    "Unable to communicate securely.  Peer does not support high-grade encryption.")

ER3(SSL_ERROR_US_ONLY_SERVER, SSL_ERROR_BASE + 1,
    "Unable to communicate securely.  Peer requires high-grade encryption which is not supported.")

ER3(SSL_ERROR_NO_CYPHER_OVERLAP, SSL_ERROR_BASE + 2,
    "Cannot communicate securely with peer: no common encryption algorithm(s).")

ER3(SSL_ERROR_NO_CERTIFICATE, SSL_ERROR_BASE + 3,
    "Unable to find the certificate or key necessary for authentication.")

ER3(SSL_ERROR_BAD_CERTIFICATE, SSL_ERROR_BASE + 4,
    "Unable to communicate securely with peer: peers's certificate was rejected.")

UNUSED_ERROR(5)

ER3(SSL_ERROR_BAD_CLIENT, SSL_ERROR_BASE + 6,
    "The server has encountered bad data from the client.")

ER3(SSL_ERROR_BAD_SERVER, SSL_ERROR_BASE + 7,
    "The client has encountered bad data from the server.")

ER3(SSL_ERROR_UNSUPPORTED_CERTIFICATE_TYPE, SSL_ERROR_BASE + 8,
    "Unsupported certificate type.")

ER3(SSL_ERROR_UNSUPPORTED_VERSION, SSL_ERROR_BASE + 9,
    "Peer using unsupported version of security protocol.")

UNUSED_ERROR(10)

ER3(SSL_ERROR_WRONG_CERTIFICATE, SSL_ERROR_BASE + 11,
    "Client authentication failed: private key in key database does not match public key in certificate database.")

ER3(SSL_ERROR_BAD_CERT_DOMAIN, SSL_ERROR_BASE + 12,
    "Unable to communicate securely with peer: requested domain name does not match the server's certificate.")

ER3(SSL_ERROR_POST_WARNING, SSL_ERROR_BASE + 13,
    "Unrecognized SSL error code.")

ER3(SSL_ERROR_SSL2_DISABLED, (SSL_ERROR_BASE + 14),
    "Peer only supports SSL version 2, which is locally disabled.")

ER3(SSL_ERROR_BAD_MAC_READ, (SSL_ERROR_BASE + 15),
    "SSL received a record with an incorrect Message Authentication Code.")

ER3(SSL_ERROR_BAD_MAC_ALERT, (SSL_ERROR_BASE + 16),
    "SSL peer reports incorrect Message Authentication Code.")

ER3(SSL_ERROR_BAD_CERT_ALERT, (SSL_ERROR_BASE + 17),
    "SSL peer cannot verify your certificate.")

ER3(SSL_ERROR_REVOKED_CERT_ALERT, (SSL_ERROR_BASE + 18),
    "SSL peer rejected your certificate as revoked.")

ER3(SSL_ERROR_EXPIRED_CERT_ALERT, (SSL_ERROR_BASE + 19),
    "SSL peer rejected your certificate as expired.")

ER3(SSL_ERROR_SSL_DISABLED, (SSL_ERROR_BASE + 20),
    "Cannot connect: SSL is disabled.")

ER3(SSL_ERROR_FORTEZZA_PQG, (SSL_ERROR_BASE + 21),
    "Cannot connect: SSL peer is in another FORTEZZA domain.")

ER3(SSL_ERROR_UNKNOWN_CIPHER_SUITE, (SSL_ERROR_BASE + 22),
    "An unknown SSL cipher suite has been requested.")

ER3(SSL_ERROR_NO_CIPHERS_SUPPORTED, (SSL_ERROR_BASE + 23),
    "No cipher suites are present and enabled in this program.")

ER3(SSL_ERROR_BAD_BLOCK_PADDING, (SSL_ERROR_BASE + 24),
    "SSL received a record with bad block padding.")

ER3(SSL_ERROR_RX_RECORD_TOO_LONG, (SSL_ERROR_BASE + 25),
    "SSL received a record that exceeded the maximum permissible length.")

ER3(SSL_ERROR_TX_RECORD_TOO_LONG, (SSL_ERROR_BASE + 26),
    "SSL attempted to send a record that exceeded the maximum permissible length.")

/*
 * Received a malformed (too long or short or invalid content) SSL handshake.
 */
ER3(SSL_ERROR_RX_MALFORMED_HELLO_REQUEST, (SSL_ERROR_BASE + 27),
    "SSL received a malformed Hello Request handshake message.")

ER3(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO, (SSL_ERROR_BASE + 28),
    "SSL received a malformed Client Hello handshake message.")

ER3(SSL_ERROR_RX_MALFORMED_SERVER_HELLO, (SSL_ERROR_BASE + 29),
    "SSL received a malformed Server Hello handshake message.")

ER3(SSL_ERROR_RX_MALFORMED_CERTIFICATE, (SSL_ERROR_BASE + 30),
    "SSL received a malformed Certificate handshake message.")

ER3(SSL_ERROR_RX_MALFORMED_SERVER_KEY_EXCH, (SSL_ERROR_BASE + 31),
    "SSL received a malformed Server Key Exchange handshake message.")

ER3(SSL_ERROR_RX_MALFORMED_CERT_REQUEST, (SSL_ERROR_BASE + 32),
    "SSL received a malformed Certificate Request handshake message.")

ER3(SSL_ERROR_RX_MALFORMED_HELLO_DONE, (SSL_ERROR_BASE + 33),
    "SSL received a malformed Server Hello Done handshake message.")

ER3(SSL_ERROR_RX_MALFORMED_CERT_VERIFY, (SSL_ERROR_BASE + 34),
    "SSL received a malformed Certificate Verify handshake message.")

ER3(SSL_ERROR_RX_MALFORMED_CLIENT_KEY_EXCH, (SSL_ERROR_BASE + 35),
    "SSL received a malformed Client Key Exchange handshake message.")

ER3(SSL_ERROR_RX_MALFORMED_FINISHED, (SSL_ERROR_BASE + 36),
    "SSL received a malformed Finished handshake message.")

/*
 * Received a malformed (too long or short) SSL record.
 */
ER3(SSL_ERROR_RX_MALFORMED_CHANGE_CIPHER, (SSL_ERROR_BASE + 37),
    "SSL received a malformed Change Cipher Spec record.")

ER3(SSL_ERROR_RX_MALFORMED_ALERT, (SSL_ERROR_BASE + 38),
    "SSL received a malformed Alert record.")

ER3(SSL_ERROR_RX_MALFORMED_HANDSHAKE, (SSL_ERROR_BASE + 39),
    "SSL received a malformed Handshake record.")

ER3(SSL_ERROR_RX_MALFORMED_APPLICATION_DATA, (SSL_ERROR_BASE + 40),
    "SSL received a malformed Application Data record.")

/*
 * Received an SSL handshake that was inappropriate for the state we're in.
 * E.g. Server received message from server, or wrong state in state machine.
 */
ER3(SSL_ERROR_RX_UNEXPECTED_HELLO_REQUEST, (SSL_ERROR_BASE + 41),
    "SSL received an unexpected Hello Request handshake message.")

ER3(SSL_ERROR_RX_UNEXPECTED_CLIENT_HELLO, (SSL_ERROR_BASE + 42),
    "SSL received an unexpected Client Hello handshake message.")

ER3(SSL_ERROR_RX_UNEXPECTED_SERVER_HELLO, (SSL_ERROR_BASE + 43),
    "SSL received an unexpected Server Hello handshake message.")

ER3(SSL_ERROR_RX_UNEXPECTED_CERTIFICATE, (SSL_ERROR_BASE + 44),
    "SSL received an unexpected Certificate handshake message.")

ER3(SSL_ERROR_RX_UNEXPECTED_SERVER_KEY_EXCH, (SSL_ERROR_BASE + 45),
    "SSL received an unexpected Server Key Exchange handshake message.")

ER3(SSL_ERROR_RX_UNEXPECTED_CERT_REQUEST, (SSL_ERROR_BASE + 46),
    "SSL received an unexpected Certificate Request handshake message.")

ER3(SSL_ERROR_RX_UNEXPECTED_HELLO_DONE, (SSL_ERROR_BASE + 47),
    "SSL received an unexpected Server Hello Done handshake message.")

ER3(SSL_ERROR_RX_UNEXPECTED_CERT_VERIFY, (SSL_ERROR_BASE + 48),
    "SSL received an unexpected Certificate Verify handshake message.")

ER3(SSL_ERROR_RX_UNEXPECTED_CLIENT_KEY_EXCH, (SSL_ERROR_BASE + 49),
    "SSL received an unexpected Client Key Exchange handshake message.")

ER3(SSL_ERROR_RX_UNEXPECTED_FINISHED, (SSL_ERROR_BASE + 50),
    "SSL received an unexpected Finished handshake message.")

/*
 * Received an SSL record that was inappropriate for the state we're in.
 */
ER3(SSL_ERROR_RX_UNEXPECTED_CHANGE_CIPHER, (SSL_ERROR_BASE + 51),
    "SSL received an unexpected Change Cipher Spec record.")

ER3(SSL_ERROR_RX_UNEXPECTED_ALERT, (SSL_ERROR_BASE + 52),
    "SSL received an unexpected Alert record.")

ER3(SSL_ERROR_RX_UNEXPECTED_HANDSHAKE, (SSL_ERROR_BASE + 53),
    "SSL received an unexpected Handshake record.")

ER3(SSL_ERROR_RX_UNEXPECTED_APPLICATION_DATA, (SSL_ERROR_BASE + 54),
    "SSL received an unexpected Application Data record.")

/*
 * Received record/message with unknown discriminant.
 */
ER3(SSL_ERROR_RX_UNKNOWN_RECORD_TYPE, (SSL_ERROR_BASE + 55),
    "SSL received a record with an unknown content type.")

ER3(SSL_ERROR_RX_UNKNOWN_HANDSHAKE, (SSL_ERROR_BASE + 56),
    "SSL received a handshake message with an unknown message type.")

ER3(SSL_ERROR_RX_UNKNOWN_ALERT, (SSL_ERROR_BASE + 57),
    "SSL received an alert record with an unknown alert description.")

/*
 * Received an alert reporting what we did wrong.  (more alerts above)
 */
ER3(SSL_ERROR_CLOSE_NOTIFY_ALERT, (SSL_ERROR_BASE + 58),
    "SSL peer has closed this connection.")

ER3(SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT, (SSL_ERROR_BASE + 59),
    "SSL peer was not expecting a handshake message it received.")

ER3(SSL_ERROR_DECOMPRESSION_FAILURE_ALERT, (SSL_ERROR_BASE + 60),
    "SSL peer was unable to successfully decompress an SSL record it received.")

ER3(SSL_ERROR_HANDSHAKE_FAILURE_ALERT, (SSL_ERROR_BASE + 61),
    "SSL peer was unable to negotiate an acceptable set of security parameters.")

ER3(SSL_ERROR_ILLEGAL_PARAMETER_ALERT, (SSL_ERROR_BASE + 62),
    "SSL peer rejected a handshake message for unacceptable content.")

ER3(SSL_ERROR_UNSUPPORTED_CERT_ALERT, (SSL_ERROR_BASE + 63),
    "SSL peer does not support certificates of the type it received.")

ER3(SSL_ERROR_CERTIFICATE_UNKNOWN_ALERT, (SSL_ERROR_BASE + 64),
    "SSL peer had some unspecified issue with the certificate it received.")

ER3(SSL_ERROR_GENERATE_RANDOM_FAILURE, (SSL_ERROR_BASE + 65),
    "SSL experienced a failure of its random number generator.")

ER3(SSL_ERROR_SIGN_HASHES_FAILURE, (SSL_ERROR_BASE + 66),
    "Unable to digitally sign data required to verify your certificate.")

ER3(SSL_ERROR_EXTRACT_PUBLIC_KEY_FAILURE, (SSL_ERROR_BASE + 67),
    "SSL was unable to extract the public key from the peer's certificate.")

ER3(SSL_ERROR_SERVER_KEY_EXCHANGE_FAILURE, (SSL_ERROR_BASE + 68),
    "Unspecified failure while processing SSL Server Key Exchange handshake.")

ER3(SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE, (SSL_ERROR_BASE + 69),
    "Unspecified failure while processing SSL Client Key Exchange handshake.")

ER3(SSL_ERROR_ENCRYPTION_FAILURE, (SSL_ERROR_BASE + 70),
    "Bulk data encryption algorithm failed in selected cipher suite.")

ER3(SSL_ERROR_DECRYPTION_FAILURE, (SSL_ERROR_BASE + 71),
    "Bulk data decryption algorithm failed in selected cipher suite.")

ER3(SSL_ERROR_SOCKET_WRITE_FAILURE, (SSL_ERROR_BASE + 72),
    "Attempt to write encrypted data to underlying socket failed.")

ER3(SSL_ERROR_MD5_DIGEST_FAILURE, (SSL_ERROR_BASE + 73),
    "MD5 digest function failed.")

ER3(SSL_ERROR_SHA_DIGEST_FAILURE, (SSL_ERROR_BASE + 74),
    "SHA-1 digest function failed.")

ER3(SSL_ERROR_MAC_COMPUTATION_FAILURE, (SSL_ERROR_BASE + 75),
    "MAC computation failed.")

ER3(SSL_ERROR_SYM_KEY_CONTEXT_FAILURE, (SSL_ERROR_BASE + 76),
    "Failure to create Symmetric Key context.")

ER3(SSL_ERROR_SYM_KEY_UNWRAP_FAILURE, (SSL_ERROR_BASE + 77),
    "Failure to unwrap the Symmetric key in Client Key Exchange message.")

ER3(SSL_ERROR_PUB_KEY_SIZE_LIMIT_EXCEEDED, (SSL_ERROR_BASE + 78),
    "SSL Server attempted to use domestic-grade public key with export cipher suite.")

ER3(SSL_ERROR_IV_PARAM_FAILURE, (SSL_ERROR_BASE + 79),
    "PKCS11 code failed to translate an IV into a param.")

ER3(SSL_ERROR_INIT_CIPHER_SUITE_FAILURE, (SSL_ERROR_BASE + 80),
    "Failed to initialize the selected cipher suite.")

ER3(SSL_ERROR_SESSION_KEY_GEN_FAILURE, (SSL_ERROR_BASE + 81),
    "Client failed to generate session keys for SSL session.")

ER3(SSL_ERROR_NO_SERVER_KEY_FOR_ALG, (SSL_ERROR_BASE + 82),
    "Server has no key for the attempted key exchange algorithm.")

ER3(SSL_ERROR_TOKEN_INSERTION_REMOVAL, (SSL_ERROR_BASE + 83),
    "PKCS#11 token was inserted or removed while operation was in progress.")

ER3(SSL_ERROR_TOKEN_SLOT_NOT_FOUND, (SSL_ERROR_BASE + 84),
    "No PKCS#11 token could be found to do a required operation.")

ER3(SSL_ERROR_NO_COMPRESSION_OVERLAP, (SSL_ERROR_BASE + 85),
    "Cannot communicate securely with peer: no common compression algorithm(s).")

ER3(SSL_ERROR_HANDSHAKE_NOT_COMPLETED, (SSL_ERROR_BASE + 86),
    "Cannot perform the operation until the handshake is complete.")

ER3(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE, (SSL_ERROR_BASE + 87),
    "Received incorrect handshakes hash values from peer.")

ER3(SSL_ERROR_CERT_KEA_MISMATCH, (SSL_ERROR_BASE + 88),
    "The certificate provided cannot be used with the selected authentication type.")

ER3(SSL_ERROR_NO_TRUSTED_SSL_CLIENT_CA, (SSL_ERROR_BASE + 89),
    "No certificate authority is trusted for SSL client authentication.")

ER3(SSL_ERROR_SESSION_NOT_FOUND, (SSL_ERROR_BASE + 90),
    "Client's SSL session ID not found in server's session cache.")

ER3(SSL_ERROR_DECRYPTION_FAILED_ALERT, (SSL_ERROR_BASE + 91),
    "Peer was unable to decrypt an SSL record it received.")

ER3(SSL_ERROR_RECORD_OVERFLOW_ALERT, (SSL_ERROR_BASE + 92),
    "Peer received an SSL record that was longer than is permitted.")

ER3(SSL_ERROR_UNKNOWN_CA_ALERT, (SSL_ERROR_BASE + 93),
    "Peer does not recognize and trust the CA that issued your certificate.")

ER3(SSL_ERROR_ACCESS_DENIED_ALERT, (SSL_ERROR_BASE + 94),
    "Peer received a valid certificate, but access was denied.")

ER3(SSL_ERROR_DECODE_ERROR_ALERT, (SSL_ERROR_BASE + 95),
    "Peer could not decode an SSL handshake message.")

ER3(SSL_ERROR_DECRYPT_ERROR_ALERT, (SSL_ERROR_BASE + 96),
    "Peer reports failure of signature verification or key exchange.")

ER3(SSL_ERROR_EXPORT_RESTRICTION_ALERT, (SSL_ERROR_BASE + 97),
    "Peer reports negotiation not in compliance with export regulations.")

ER3(SSL_ERROR_PROTOCOL_VERSION_ALERT, (SSL_ERROR_BASE + 98),
    "Peer reports incompatible or unsupported protocol version.")

ER3(SSL_ERROR_INSUFFICIENT_SECURITY_ALERT, (SSL_ERROR_BASE + 99),
    "Server requires ciphers more secure than those supported by client.")

ER3(SSL_ERROR_INTERNAL_ERROR_ALERT, (SSL_ERROR_BASE + 100),
    "Peer reports it experienced an internal error.")

ER3(SSL_ERROR_USER_CANCELED_ALERT, (SSL_ERROR_BASE + 101),
    "Peer user canceled handshake.")

ER3(SSL_ERROR_NO_RENEGOTIATION_ALERT, (SSL_ERROR_BASE + 102),
    "Peer does not permit renegotiation of SSL security parameters.")

ER3(SSL_ERROR_SERVER_CACHE_NOT_CONFIGURED, (SSL_ERROR_BASE + 103),
    "SSL server cache not configured and not disabled for this socket.")

ER3(SSL_ERROR_UNSUPPORTED_EXTENSION_ALERT, (SSL_ERROR_BASE + 104),
    "SSL peer does not support requested TLS hello extension.")

ER3(SSL_ERROR_CERTIFICATE_UNOBTAINABLE_ALERT, (SSL_ERROR_BASE + 105),
    "SSL peer could not obtain your certificate from the supplied URL.")

ER3(SSL_ERROR_UNRECOGNIZED_NAME_ALERT, (SSL_ERROR_BASE + 106),
    "SSL peer has no certificate for the requested DNS name.")

ER3(SSL_ERROR_BAD_CERT_STATUS_RESPONSE_ALERT, (SSL_ERROR_BASE + 107),
    "SSL peer was unable to get an OCSP response for its certificate.")

ER3(SSL_ERROR_BAD_CERT_HASH_VALUE_ALERT, (SSL_ERROR_BASE + 108),
    "SSL peer reported bad certificate hash value.")

ER3(SSL_ERROR_RX_UNEXPECTED_NEW_SESSION_TICKET, (SSL_ERROR_BASE + 109),
    "SSL received an unexpected New Session Ticket handshake message.")

ER3(SSL_ERROR_RX_MALFORMED_NEW_SESSION_TICKET, (SSL_ERROR_BASE + 110),
    "SSL received a malformed New Session Ticket handshake message.")

ER3(SSL_ERROR_DECOMPRESSION_FAILURE, (SSL_ERROR_BASE + 111),
    "SSL received a compressed record that could not be decompressed.")

ER3(SSL_ERROR_RENEGOTIATION_NOT_ALLOWED, (SSL_ERROR_BASE + 112),
    "Renegotiation is not allowed on this SSL socket.")

ER3(SSL_ERROR_UNSAFE_NEGOTIATION, (SSL_ERROR_BASE + 113),
    "Peer attempted old style (potentially vulnerable) handshake.")

ER3(SSL_ERROR_RX_UNEXPECTED_UNCOMPRESSED_RECORD, (SSL_ERROR_BASE + 114),
    "SSL received an unexpected uncompressed record.")

ER3(SSL_ERROR_WEAK_SERVER_EPHEMERAL_DH_KEY, (SSL_ERROR_BASE + 115),
    "SSL received a weak ephemeral Diffie-Hellman key in Server Key Exchange handshake message.")

ER3(SSL_ERROR_NEXT_PROTOCOL_DATA_INVALID, (SSL_ERROR_BASE + 116),
    "SSL received invalid NPN extension data.")

ER3(SSL_ERROR_FEATURE_NOT_SUPPORTED_FOR_SSL2, (SSL_ERROR_BASE + 117),
    "SSL feature not supported for SSL 2.0 connections.")

ER3(SSL_ERROR_FEATURE_NOT_SUPPORTED_FOR_SERVERS, (SSL_ERROR_BASE + 118),
    "SSL feature not supported for servers.")

ER3(SSL_ERROR_FEATURE_NOT_SUPPORTED_FOR_CLIENTS, (SSL_ERROR_BASE + 119),
    "SSL feature not supported for clients.")

ER3(SSL_ERROR_INVALID_VERSION_RANGE, (SSL_ERROR_BASE + 120),
    "SSL version range is not valid.")

ER3(SSL_ERROR_CIPHER_DISALLOWED_FOR_VERSION, (SSL_ERROR_BASE + 121),
    "SSL peer selected a cipher suite disallowed for the selected protocol version.")

ER3(SSL_ERROR_RX_MALFORMED_HELLO_VERIFY_REQUEST, (SSL_ERROR_BASE + 122),
    "SSL received a malformed Hello Verify Request handshake message.")

ER3(SSL_ERROR_RX_UNEXPECTED_HELLO_VERIFY_REQUEST, (SSL_ERROR_BASE + 123),
    "SSL received an unexpected Hello Verify Request handshake message.")

ER3(SSL_ERROR_FEATURE_NOT_SUPPORTED_FOR_VERSION, (SSL_ERROR_BASE + 124),
    "SSL feature not supported for the protocol version.")

ER3(SSL_ERROR_RX_UNEXPECTED_CERT_STATUS, (SSL_ERROR_BASE + 125),
    "SSL received an unexpected Certificate Status handshake message.")

ER3(SSL_ERROR_UNSUPPORTED_HASH_ALGORITHM, (SSL_ERROR_BASE + 126),
    "Unsupported hash algorithm used by TLS peer.")

ER3(SSL_ERROR_DIGEST_FAILURE, (SSL_ERROR_BASE + 127),
    "Digest function failed.")

ER3(SSL_ERROR_INCORRECT_SIGNATURE_ALGORITHM, (SSL_ERROR_BASE + 128),
    "Incorrect signature algorithm specified in a digitally-signed element.")

ER3(SSL_ERROR_NEXT_PROTOCOL_NO_CALLBACK, (SSL_ERROR_BASE + 129),
    "The next protocol negotiation extension was enabled, but the callback was cleared prior to being needed.")

ER3(SSL_ERROR_NEXT_PROTOCOL_NO_PROTOCOL, (SSL_ERROR_BASE + 130),
    "The server supports no protocols that the client advertises in the ALPN extension.")

ER3(SSL_ERROR_INAPPROPRIATE_FALLBACK_ALERT, (SSL_ERROR_BASE + 131),
    "The server rejected the handshake because the client downgraded to a lower "
    "TLS version than the server supports.")

ER3(SSL_ERROR_WEAK_SERVER_CERT_KEY, (SSL_ERROR_BASE + 132),
    "The server certificate included a public key that was too weak.")

ER3(SSL_ERROR_RX_SHORT_DTLS_READ, (SSL_ERROR_BASE + 133),
    "Not enough room in buffer for DTLS record.")

ER3(SSL_ERROR_NO_SUPPORTED_SIGNATURE_ALGORITHM, (SSL_ERROR_BASE + 134),
    "No supported TLS signature algorithm was configured.")

ER3(SSL_ERROR_UNSUPPORTED_SIGNATURE_ALGORITHM, (SSL_ERROR_BASE + 135),
    "The peer used an unsupported combination of signature and hash algorithm.")

ER3(SSL_ERROR_MISSING_EXTENDED_MASTER_SECRET, (SSL_ERROR_BASE + 136),
    "The peer tried to resume without a correct extended_master_secret extension")

ER3(SSL_ERROR_UNEXPECTED_EXTENDED_MASTER_SECRET, (SSL_ERROR_BASE + 137),
    "The peer tried to resume with an unexpected extended_master_secret extension")

ER3(SSL_ERROR_RX_MALFORMED_KEY_SHARE, (SSL_ERROR_BASE + 138),
    "SSL received a malformed Key Share extension.")

ER3(SSL_ERROR_MISSING_KEY_SHARE, (SSL_ERROR_BASE + 139),
    "SSL expected a Key Share extension.")

ER3(SSL_ERROR_RX_MALFORMED_ECDHE_KEY_SHARE, (SSL_ERROR_BASE + 140),
    "SSL received a malformed ECDHE key share handshake extension.")

ER3(SSL_ERROR_RX_MALFORMED_DHE_KEY_SHARE, (SSL_ERROR_BASE + 141),
    "SSL received a malformed DHE key share handshake extension.")

ER3(SSL_ERROR_RX_UNEXPECTED_ENCRYPTED_EXTENSIONS, (SSL_ERROR_BASE + 142),
    "SSL received an unexpected Encrypted Extensions handshake message.")

ER3(SSL_ERROR_MISSING_EXTENSION_ALERT, (SSL_ERROR_BASE + 143),
    "SSL received a missing_extension alert.")

ER3(SSL_ERROR_KEY_EXCHANGE_FAILURE, (SSL_ERROR_BASE + 144),
    "SSL had an error performing key exchange.")

ER3(SSL_ERROR_EXTENSION_DISALLOWED_FOR_VERSION, (SSL_ERROR_BASE + 145),
    "SSL received an extension that is not permitted for this version.")

ER3(SSL_ERROR_RX_MALFORMED_ENCRYPTED_EXTENSIONS, (SSL_ERROR_BASE + 146),
    "SSL received a malformed Encrypted Extensions handshake message.")

ER3(SSL_ERROR_RX_MALFORMED_PRE_SHARED_KEY, (SSL_ERROR_BASE + 147),
    "SSL received an invalid PreSharedKey extension.")
