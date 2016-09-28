/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* Private header file of libSSL.
 * Various and sundry protocol constants. DON'T CHANGE THESE. These
 * values are defined by the SSL 3.0 protocol specification.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ssl3proto_h_
#define __ssl3proto_h_

typedef PRUint8 SSL3Opaque;

typedef PRUint16 SSL3ProtocolVersion;
/* version numbers are defined in sslproto.h */

/* The TLS 1.3 draft version. Used to avoid negotiating
 * between incompatible pre-standard TLS 1.3 drafts.
 * TODO(ekr@rtfm.com): Remove when TLS 1.3 is published. */
#define TLS_1_3_DRAFT_VERSION 14

typedef PRUint16 ssl3CipherSuite;
/* The cipher suites are defined in sslproto.h */

#define MAX_CERT_TYPES 10
#define MAX_COMPRESSION_METHODS 10
#define MAX_MAC_LENGTH 64
#define MAX_PADDING_LENGTH 64
#define MAX_KEY_LENGTH 64
#define EXPORT_KEY_LENGTH 5
#define SSL3_RANDOM_LENGTH 32

#define SSL3_RECORD_HEADER_LENGTH 5

/* SSL3_RECORD_HEADER_LENGTH + epoch/sequence_number */
#define DTLS_RECORD_HEADER_LENGTH 13

#define MAX_FRAGMENT_LENGTH 16384

typedef enum {
    content_change_cipher_spec = 20,
    content_alert = 21,
    content_handshake = 22,
    content_application_data = 23
} SSL3ContentType;

typedef struct {
    SSL3ContentType type;
    SSL3ProtocolVersion version;
    PRUint16 length;
    SECItem fragment;
} SSL3Plaintext;

typedef struct {
    SSL3ContentType type;
    SSL3ProtocolVersion version;
    PRUint16 length;
    SECItem fragment;
} SSL3Compressed;

typedef struct {
    SECItem content;
    SSL3Opaque MAC[MAX_MAC_LENGTH];
} SSL3GenericStreamCipher;

typedef struct {
    SECItem content;
    SSL3Opaque MAC[MAX_MAC_LENGTH];
    PRUint8 padding[MAX_PADDING_LENGTH];
    PRUint8 padding_length;
} SSL3GenericBlockCipher;

typedef enum { change_cipher_spec_choice = 1 } SSL3ChangeCipherSpecChoice;

typedef struct {
    SSL3ChangeCipherSpecChoice choice;
} SSL3ChangeCipherSpec;

typedef enum { alert_warning = 1,
               alert_fatal = 2 } SSL3AlertLevel;

typedef enum {
    close_notify = 0,
    end_of_early_data = 1, /* TLS 1.3 */
    unexpected_message = 10,
    bad_record_mac = 20,
    decryption_failed_RESERVED = 21, /* do not send; see RFC 5246 */
    record_overflow = 22,            /* TLS only */
    decompression_failure = 30,
    handshake_failure = 40,
    no_certificate = 41, /* SSL3 only, NOT TLS */
    bad_certificate = 42,
    unsupported_certificate = 43,
    certificate_revoked = 44,
    certificate_expired = 45,
    certificate_unknown = 46,
    illegal_parameter = 47,

    /* All alerts below are TLS only. */
    unknown_ca = 48,
    access_denied = 49,
    decode_error = 50,
    decrypt_error = 51,
    export_restriction = 60,
    protocol_version = 70,
    insufficient_security = 71,
    internal_error = 80,
    inappropriate_fallback = 86, /* could also be sent for SSLv3 */
    user_canceled = 90,
    no_renegotiation = 100,

    /* Alerts for client hello extensions */
    missing_extension = 109,
    unsupported_extension = 110,
    certificate_unobtainable = 111,
    unrecognized_name = 112,
    bad_certificate_status_response = 113,
    bad_certificate_hash_value = 114,
    no_application_protocol = 120

} SSL3AlertDescription;

typedef struct {
    SSL3AlertLevel level;
    SSL3AlertDescription description;
} SSL3Alert;

typedef enum {
    hello_request = 0,
    client_hello = 1,
    server_hello = 2,
    hello_verify_request = 3,
    new_session_ticket = 4,
    hello_retry_request = 6,
    encrypted_extensions = 8,
    certificate = 11,
    server_key_exchange = 12,
    certificate_request = 13,
    server_hello_done = 14,
    certificate_verify = 15,
    client_key_exchange = 16,
    finished = 20,
    certificate_status = 22,
    next_proto = 67
} SSL3HandshakeType;

typedef struct {
    PRUint8 empty;
} SSL3HelloRequest;

typedef struct {
    SSL3Opaque rand[SSL3_RANDOM_LENGTH];
} SSL3Random;

typedef struct {
    SSL3Opaque id[32];
    PRUint8 length;
} SSL3SessionID;

typedef struct {
    SSL3ProtocolVersion client_version;
    SSL3Random random;
    SSL3SessionID session_id;
    SECItem cipher_suites;
    PRUint8 cm_count;
    SSLCompressionMethod compression_methods[MAX_COMPRESSION_METHODS];
} SSL3ClientHello;

typedef struct {
    SSL3ProtocolVersion server_version;
    SSL3Random random;
    SSL3SessionID session_id;
    ssl3CipherSuite cipher_suite;
    SSLCompressionMethod compression_method;
} SSL3ServerHello;

typedef struct {
    SECItem list;
} SSL3Certificate;

/* SSL3SignType moved to ssl.h */

/* The SSL key exchange method used */
typedef enum {
    kea_null,
    kea_rsa,
    kea_rsa_export,
    kea_rsa_export_1024,
    kea_dh_dss,
    kea_dh_dss_export,
    kea_dh_rsa,
    kea_dh_rsa_export,
    kea_dhe_dss,
    kea_dhe_dss_export,
    kea_dhe_rsa,
    kea_dhe_rsa_export,
    kea_dh_anon,
    kea_dh_anon_export,
    kea_rsa_fips,
    kea_ecdh_ecdsa,
    kea_ecdhe_ecdsa,
    kea_ecdh_rsa,
    kea_ecdhe_rsa,
    kea_ecdh_anon,
    kea_ecdhe_psk,
    kea_dhe_psk,
} SSL3KeyExchangeAlgorithm;

typedef struct {
    SECItem modulus;
    SECItem exponent;
} SSL3ServerRSAParams;

typedef struct {
    SECItem p;
    SECItem g;
    SECItem Ys;
} SSL3ServerDHParams;

typedef struct {
    union {
        SSL3ServerDHParams dh;
        SSL3ServerRSAParams rsa;
    } u;
} SSL3ServerParams;

/* SSL3HashesIndividually contains a combination MD5/SHA1 hash, as used in TLS
 * prior to 1.2. */
typedef struct {
    PRUint8 md5[16];
    PRUint8 sha[20];
} SSL3HashesIndividually;

/* SSL3Hashes contains an SSL hash value. The digest is contained in |u.raw|
 * which, if |hashAlg==ssl_hash_none| is also a SSL3HashesIndividually
 * struct. */
typedef struct {
    unsigned int len;
    SSLHashType hashAlg;
    union {
        PRUint8 raw[64];
        SSL3HashesIndividually s;
        SECItem pointer_to_hash_input;
    } u;
} SSL3Hashes;

typedef struct {
    union {
        SSL3Opaque anonymous;
        SSL3Hashes certified;
    } u;
} SSL3ServerKeyExchange;

typedef enum {
    ct_RSA_sign = 1,
    ct_DSS_sign = 2,
    ct_RSA_fixed_DH = 3,
    ct_DSS_fixed_DH = 4,
    ct_RSA_ephemeral_DH = 5,
    ct_DSS_ephemeral_DH = 6,
    ct_ECDSA_sign = 64,
    ct_RSA_fixed_ECDH = 65,
    ct_ECDSA_fixed_ECDH = 66

} SSL3ClientCertificateType;

typedef struct {
    SSL3Opaque client_version[2];
    SSL3Opaque random[46];
} SSL3RSAPreMasterSecret;

typedef SSL3Opaque SSL3MasterSecret[48];

typedef enum {
    sender_client = 0x434c4e54,
    sender_server = 0x53525652
} SSL3Sender;

typedef SSL3HashesIndividually SSL3Finished;

typedef struct {
    SSL3Opaque verify_data[12];
} TLSFinished;

/*
 * TLS extension related data structures and constants.
 */

/* SessionTicket extension related data structures. */

/* NewSessionTicket handshake message. */
typedef struct {
    PRUint32 received_timestamp;
    PRUint32 ticket_lifetime_hint;
    PRUint32 flags;
    PRUint32 ticket_age_add;
    SECItem ticket;
} NewSessionTicket;

typedef enum {
    ticket_allow_early_data = 1,
    ticket_allow_dhe_resumption = 2,
    ticket_allow_psk_resumption = 4
} TLS13SessionTicketFlags;

typedef enum {
    CLIENT_AUTH_ANONYMOUS = 0,
    CLIENT_AUTH_CERTIFICATE = 1
} ClientAuthenticationType;

typedef struct {
    ClientAuthenticationType client_auth_type;
    union {
        SSL3Opaque *certificate_list;
    } identity;
} ClientIdentity;

#define SESS_TICKET_KEY_NAME_LEN 16
#define SESS_TICKET_KEY_NAME_PREFIX "NSS!"
#define SESS_TICKET_KEY_NAME_PREFIX_LEN 4
#define SESS_TICKET_KEY_VAR_NAME_LEN 12

typedef struct {
    unsigned char *key_name;
    unsigned char *iv;
    SECItem encrypted_state;
    unsigned char *mac;
} EncryptedSessionTicket;

#define TLS_EX_SESS_TICKET_MAC_LENGTH 32

#define TLS_STE_NO_SERVER_NAME -1

typedef enum {
    ssl_sig_none = 0,
    ssl_sig_rsa_pkcs1_sha1 = 0x0201,
    ssl_sig_rsa_pkcs1_sha256 = 0x0401,
    ssl_sig_rsa_pkcs1_sha384 = 0x0501,
    ssl_sig_rsa_pkcs1_sha512 = 0x0601,
    /* For ECDSA, the pairing of the hash with a specific curve is only enforced
     * in TLS 1.3; in TLS 1.2 any curve can be used with each of these. */
    ssl_sig_ecdsa_secp256r1_sha256 = 0x0403,
    ssl_sig_ecdsa_secp384r1_sha384 = 0x0503,
    ssl_sig_ecdsa_secp521r1_sha512 = 0x0603,
    ssl_sig_rsa_pss_sha256 = 0x0804,
    ssl_sig_rsa_pss_sha384 = 0x0805,
    ssl_sig_rsa_pss_sha512 = 0x0806,
    ssl_sig_ed25519 = 0x0807,
    ssl_sig_ed448 = 0x0808,

    ssl_sig_dsa_sha1 = 0x0202,
    ssl_sig_dsa_sha256 = 0x0402,
    ssl_sig_dsa_sha384 = 0x0502,
    ssl_sig_dsa_sha512 = 0x0602,
    ssl_sig_ecdsa_sha1 = 0x0203
} SignatureScheme;

#endif /* __ssl3proto_h_ */
