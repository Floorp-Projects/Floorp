// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(dead_code)]

use crate::ssl;

// Ideally all of these would be enums, but size matters and we need to allow
// for values outside of those that are defined here.

pub type Alert = u8;

pub type Epoch = u16;
// TLS doesn't really have an "initial" concept that maps to QUIC so directly,
// but this should be clear enough.
pub const TLS_EPOCH_INITIAL: Epoch = 0_u16;
pub const TLS_EPOCH_ZERO_RTT: Epoch = 1_u16;
pub const TLS_EPOCH_HANDSHAKE: Epoch = 2_u16;
// Also, we don't use TLS epochs > 3.
pub const TLS_EPOCH_APPLICATION_DATA: Epoch = 3_u16;

/// Rather than defining a type alias and a bunch of constants, which leads to a ton of repetition,
/// use this macro.
macro_rules! remap_enum {
    { $t:ident: $s:ty { $( $n:ident = $v:path ),+ $(,)? } } => {
        pub type $t = $s;
        $(#[allow(clippy::cast_possible_truncation)] pub const $n: $t = $v as $t; )+
    };
    { $t:ident: $s:ty => $e:ident { $( $n:ident = $v:ident ),+ $(,)? } } => {
        remap_enum!{ $t: $s { $( $n = $e::$v ),+ } }
    };
    { $t:ident: $s:ty => $p:ident::$e:ident { $( $n:ident = $v:ident ),+ $(,)? } } => {
        remap_enum!{ $t: $s { $( $n = $p::$e::$v ),+ } }
    };
}

remap_enum! {
    Version: u16 => ssl {
        TLS_VERSION_1_2 = SSL_LIBRARY_VERSION_TLS_1_2,
        TLS_VERSION_1_3 = SSL_LIBRARY_VERSION_TLS_1_3,
    }
}

mod ciphers {
    include!(concat!(env!("OUT_DIR"), "/nss_ciphers.rs"));
}

remap_enum! {
    Cipher: u16 => ciphers {
        TLS_AES_128_GCM_SHA256 = TLS_AES_128_GCM_SHA256,
        TLS_AES_256_GCM_SHA384 = TLS_AES_256_GCM_SHA384,
        TLS_CHACHA20_POLY1305_SHA256 = TLS_CHACHA20_POLY1305_SHA256,
    }
}

remap_enum! {
    Group: u16 => ssl::SSLNamedGroup {
        TLS_GRP_EC_SECP256R1 = ssl_grp_ec_secp256r1,
        TLS_GRP_EC_SECP384R1 = ssl_grp_ec_secp384r1,
        TLS_GRP_EC_SECP521R1 = ssl_grp_ec_secp521r1,
        TLS_GRP_EC_X25519 = ssl_grp_ec_curve25519,
        TLS_GRP_KEM_XYBER768D00 = ssl_grp_kem_xyber768d00,
    }
}

remap_enum! {
    HandshakeMessage: u8 => ssl::SSLHandshakeType {
        TLS_HS_HELLO_REQUEST = ssl_hs_hello_request,
        TLS_HS_CLIENT_HELLO = ssl_hs_client_hello,
        TLS_HS_SERVER_HELLO = ssl_hs_server_hello,
        TLS_HS_HELLO_VERIFY_REQUEST = ssl_hs_hello_verify_request,
        TLS_HS_NEW_SESSION_TICKET = ssl_hs_new_session_ticket,
        TLS_HS_END_OF_EARLY_DATA = ssl_hs_end_of_early_data,
        TLS_HS_HELLO_RETRY_REQUEST = ssl_hs_hello_retry_request,
        TLS_HS_ENCRYPTED_EXTENSIONS = ssl_hs_encrypted_extensions,
        TLS_HS_CERTIFICATE = ssl_hs_certificate,
        TLS_HS_SERVER_KEY_EXCHANGE = ssl_hs_server_key_exchange,
        TLS_HS_CERTIFICATE_REQUEST = ssl_hs_certificate_request,
        TLS_HS_SERVER_HELLO_DONE = ssl_hs_server_hello_done,
        TLS_HS_CERTIFICATE_VERIFY = ssl_hs_certificate_verify,
        TLS_HS_CLIENT_KEY_EXCHANGE = ssl_hs_client_key_exchange,
        TLS_HS_FINISHED = ssl_hs_finished,
        TLS_HS_CERT_STATUS = ssl_hs_certificate_status,
        TLS_HS_KEY_UDPATE = ssl_hs_key_update,
    }
}

remap_enum! {
    ContentType: u8 => ssl::SSLContentType {
        TLS_CT_CHANGE_CIPHER_SPEC = ssl_ct_change_cipher_spec,
        TLS_CT_ALERT = ssl_ct_alert,
        TLS_CT_HANDSHAKE = ssl_ct_handshake,
        TLS_CT_APPLICATION_DATA = ssl_ct_application_data,
        TLS_CT_ACK = ssl_ct_ack,
    }
}

remap_enum! {
    Extension: u16 => ssl::SSLExtensionType {
        TLS_EXT_SERVER_NAME = ssl_server_name_xtn,
        TLS_EXT_CERT_STATUS = ssl_cert_status_xtn,
        TLS_EXT_GROUPS = ssl_supported_groups_xtn,
        TLS_EXT_EC_POINT_FORMATS = ssl_ec_point_formats_xtn,
        TLS_EXT_SIG_SCHEMES = ssl_signature_algorithms_xtn,
        TLS_EXT_USE_SRTP = ssl_use_srtp_xtn,
        TLS_EXT_ALPN = ssl_app_layer_protocol_xtn,
        TLS_EXT_SCT = ssl_signed_cert_timestamp_xtn,
        TLS_EXT_PADDING = ssl_padding_xtn,
        TLS_EXT_EMS = ssl_extended_master_secret_xtn,
        TLS_EXT_RECORD_SIZE = ssl_record_size_limit_xtn,
        TLS_EXT_SESSION_TICKET = ssl_session_ticket_xtn,
        TLS_EXT_PSK = ssl_tls13_pre_shared_key_xtn,
        TLS_EXT_EARLY_DATA = ssl_tls13_early_data_xtn,
        TLS_EXT_VERSIONS = ssl_tls13_supported_versions_xtn,
        TLS_EXT_COOKIE = ssl_tls13_cookie_xtn,
        TLS_EXT_PSK_MODES = ssl_tls13_psk_key_exchange_modes_xtn,
        TLS_EXT_CA = ssl_tls13_certificate_authorities_xtn,
        TLS_EXT_POST_HS_AUTH = ssl_tls13_post_handshake_auth_xtn,
        TLS_EXT_CERT_SIG_SCHEMES = ssl_signature_algorithms_cert_xtn,
        TLS_EXT_KEY_SHARE = ssl_tls13_key_share_xtn,
        TLS_EXT_RENEGOTIATION_INFO = ssl_renegotiation_info_xtn,
    }
}

remap_enum! {
    SignatureScheme: u16 => ssl::SSLSignatureScheme {
        TLS_SIG_NONE = ssl_sig_none,
        TLS_SIG_RSA_PKCS1_SHA256 = ssl_sig_rsa_pkcs1_sha256,
        TLS_SIG_RSA_PKCS1_SHA384 = ssl_sig_rsa_pkcs1_sha384,
        TLS_SIG_RSA_PKCS1_SHA512 = ssl_sig_rsa_pkcs1_sha512,
        TLS_SIG_ECDSA_SECP256R1_SHA256 = ssl_sig_ecdsa_secp256r1_sha256,
        TLS_SIG_ECDSA_SECP384R1_SHA384 = ssl_sig_ecdsa_secp384r1_sha384,
        TLS_SIG_ECDSA_SECP512R1_SHA512 = ssl_sig_ecdsa_secp521r1_sha512,
        TLS_SIG_RSA_PSS_RSAE_SHA256 = ssl_sig_rsa_pss_rsae_sha256,
        TLS_SIG_RSA_PSS_RSAE_SHA384 = ssl_sig_rsa_pss_rsae_sha384,
        TLS_SIG_RSA_PSS_RSAE_SHA512 = ssl_sig_rsa_pss_rsae_sha512,
        TLS_SIG_ED25519 = ssl_sig_ed25519,
        TLS_SIG_ED448 = ssl_sig_ed448,
        TLS_SIG_RSA_PSS_PSS_SHA256 = ssl_sig_rsa_pss_pss_sha256,
        TLS_SIG_RSA_PSS_PSS_SHA384 = ssl_sig_rsa_pss_pss_sha384,
        TLS_SIG_RSA_PSS_PSS_SHA512 = ssl_sig_rsa_pss_pss_sha512,
    }
}
