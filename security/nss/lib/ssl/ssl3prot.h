/*
 * Various and sundry protocol constants. DON'T CHANGE THESE. These
 * values are defined by the SSL 3.0 protocol specification.
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
 *   Dr Vipul Gupta <vipul.gupta@sun.com>, Sun Microsystems Laboratories
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

#ifndef __ssl3proto_h_
#define __ssl3proto_h_

typedef uint8 SSL3Opaque;

typedef uint16 SSL3ProtocolVersion;
/* version numbers are defined in sslproto.h */

typedef uint16 ssl3CipherSuite;
/* The cipher suites are defined in sslproto.h */

#define MAX_CERT_TYPES			10
#define MAX_COMPRESSION_METHODS		10
#define MAX_MAC_LENGTH			64
#define MAX_PADDING_LENGTH		64
#define MAX_KEY_LENGTH			64
#define EXPORT_KEY_LENGTH		 5
#define SSL3_RANDOM_LENGTH		32

#define SSL3_RECORD_HEADER_LENGTH	 5

#define MAX_FRAGMENT_LENGTH		16384
     
typedef enum {
    content_change_cipher_spec = 20, 
    content_alert              = 21,
    content_handshake          = 22, 
    content_application_data   = 23
} SSL3ContentType;

typedef struct {
    SSL3ContentType     type;
    SSL3ProtocolVersion version;
    uint16              length;
    SECItem             fragment;
} SSL3Plaintext;

typedef struct {
    SSL3ContentType     type;
    SSL3ProtocolVersion version;
    uint16              length;
    SECItem             fragment;
} SSL3Compressed;

typedef struct {
    SECItem    content;
    SSL3Opaque MAC[MAX_MAC_LENGTH];
} SSL3GenericStreamCipher;

typedef struct {
    SECItem    content;
    SSL3Opaque MAC[MAX_MAC_LENGTH];
    uint8      padding[MAX_PADDING_LENGTH];
    uint8      padding_length;
} SSL3GenericBlockCipher;

typedef enum { change_cipher_spec_choice = 1 } SSL3ChangeCipherSpecChoice;

typedef struct {
    SSL3ChangeCipherSpecChoice choice;
} SSL3ChangeCipherSpec;

typedef enum { alert_warning = 1, alert_fatal = 2 } SSL3AlertLevel;

typedef enum {
    close_notify            = 0,
    unexpected_message      = 10,
    bad_record_mac          = 20,
    decryption_failed       = 21,	/* TLS only */
    record_overflow         = 22,	/* TLS only */
    decompression_failure   = 30,
    handshake_failure       = 40,
    no_certificate          = 41,	/* SSL3 only, NOT TLS */
    bad_certificate         = 42,
    unsupported_certificate = 43,
    certificate_revoked     = 44,
    certificate_expired     = 45,
    certificate_unknown     = 46,
    illegal_parameter       = 47,

/* All alerts below are TLS only. */
    unknown_ca              = 48,
    access_denied           = 49,
    decode_error            = 50,
    decrypt_error           = 51,
    export_restriction      = 60,
    protocol_version        = 70,
    insufficient_security   = 71,
    internal_error          = 80,
    user_canceled           = 90,
    no_renegotiation        = 100

} SSL3AlertDescription;

typedef struct {
    SSL3AlertLevel       level;
    SSL3AlertDescription description;
} SSL3Alert;

typedef enum {
    hello_request	= 0, 
    client_hello	= 1, 
    server_hello	= 2,
    certificate 	= 11, 
    server_key_exchange = 12,
    certificate_request	= 13, 
    server_hello_done	= 14,
    certificate_verify	= 15, 
    client_key_exchange	= 16, 
    finished		= 20
} SSL3HandshakeType;

typedef struct {
    uint8 empty;
} SSL3HelloRequest;
     
typedef struct {
    SSL3Opaque rand[SSL3_RANDOM_LENGTH];
} SSL3Random;
     
typedef struct {
    SSL3Opaque id[32];
    uint8 length;
} SSL3SessionID;
     
typedef enum { compression_null = 0 } SSL3CompressionMethod;
     
typedef struct {
    SSL3ProtocolVersion   client_version;
    SSL3Random            random;
    SSL3SessionID         session_id;
    SECItem               cipher_suites;
    uint8                 cm_count;
    SSL3CompressionMethod compression_methods[MAX_COMPRESSION_METHODS];
} SSL3ClientHello;
     
typedef struct  {
    SSL3ProtocolVersion   server_version;
    SSL3Random            random;
    SSL3SessionID         session_id;
    ssl3CipherSuite       cipher_suite;
    SSL3CompressionMethod compression_method;
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
    kea_fortezza, 
    kea_rsa_fips,
    kea_ecdh_ecdsa,
    kea_ecdhe_ecdsa,
    kea_ecdh_rsa,
    kea_ecdhe_rsa
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

typedef struct {
    uint8 md5[16];
    uint8 sha[20];
} SSL3Hashes;
     
typedef struct {
    union {
	SSL3Opaque anonymous;
	SSL3Hashes certified;
    } u;
} SSL3ServerKeyExchange;
     
typedef enum {
    ct_RSA_sign 	=  1, 
    ct_DSS_sign 	=  2, 
    ct_RSA_fixed_DH 	=  3,
    ct_DSS_fixed_DH 	=  4, 
    ct_RSA_ephemeral_DH =  5, 
    ct_DSS_ephemeral_DH =  6,
    /* XXX The numbers assigned to the following EC-based 
     * certificate types might change before the ECC in TLS
     * draft becomes an IETF RFC.
     */
    ct_ECDSA_sign	=  7, 
    ct_RSA_fixed_ECDH	=  8, 
    ct_ECDSA_fixed_ECDH	=  9, 

    ct_Fortezza 	= 20
} SSL3ClientCertificateType;
     
typedef SECItem *SSL3DistinquishedName;

typedef struct {
    SSL3Opaque client_version[2];
    SSL3Opaque random[46];
} SSL3RSAPreMasterSecret;
     
typedef SECItem SSL3EncryptedPreMasterSecret;

/* Following struct is the format of a Fortezza ClientKeyExchange message. */
typedef struct {
    SECItem    y_c;
    SSL3Opaque r_c                      [128];
    SSL3Opaque y_signature              [40];
    SSL3Opaque wrapped_client_write_key [12];
    SSL3Opaque wrapped_server_write_key [12];
    SSL3Opaque client_write_iv          [24];
    SSL3Opaque server_write_iv          [24];
    SSL3Opaque master_secret_iv         [24];
    SSL3Opaque encrypted_preMasterSecret[48];
} SSL3FortezzaKeys;

typedef SSL3Opaque SSL3MasterSecret[48];

typedef enum { implicit, explicit } SSL3PublicValueEncoding;
     
typedef struct {
    union {
	SSL3Opaque implicit;
	SECItem    explicit;
    } dh_public;
} SSL3ClientDiffieHellmanPublic;
     
typedef struct {
    union {
	SSL3EncryptedPreMasterSecret  rsa;
	SSL3ClientDiffieHellmanPublic diffie_helman;
	SSL3FortezzaKeys              fortezza;
    } exchange_keys;
} SSL3ClientKeyExchange;

typedef SSL3Hashes SSL3PreSignedCertificateVerify;

typedef SECItem SSL3CertificateVerify;

typedef enum {
    sender_client = 0x434c4e54,
    sender_server = 0x53525652
} SSL3Sender;

typedef SSL3Hashes SSL3Finished;   

typedef struct {
    SSL3Opaque verify_data[12];
} TLSFinished;

#endif /* __ssl3proto_h_ */
