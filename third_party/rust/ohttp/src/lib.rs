#![deny(warnings, clippy::pedantic)]
#![allow(clippy::missing_errors_doc)] // I'm too lazy
#![cfg_attr(
    not(all(feature = "client", feature = "server")),
    allow(dead_code, unused_imports)
)]

mod err;
pub mod hpke;
#[cfg(feature = "nss")]
mod nss;
#[cfg(feature = "rust-hpke")]
mod rand;
#[cfg(feature = "rust-hpke")]
mod rh;

pub use err::Error;

use crate::hpke::{Aead as AeadId, Kdf, Kem};
use byteorder::{NetworkEndian, ReadBytesExt, WriteBytesExt};
use err::Res;
use log::trace;
use std::{
    cmp::max,
    convert::TryFrom,
    io::{BufReader, Read},
    mem::size_of,
};

#[cfg(feature = "nss")]
use nss::random;
#[cfg(feature = "nss")]
use nss::{
    aead::{Aead, Mode, NONCE_LEN},
    hkdf::{Hkdf, KeyMechanism},
    hpke::{generate_key_pair, Config as HpkeConfig, Exporter, HpkeR, HpkeS},
    PrivateKey, PublicKey,
};

#[cfg(feature = "rust-hpke")]
use crate::rand::random;
#[cfg(feature = "rust-hpke")]
use rh::{
    aead::{Aead, Mode, NONCE_LEN},
    hkdf::{Hkdf, KeyMechanism},
    hpke::{
        derive_key_pair, generate_key_pair, Config as HpkeConfig, Exporter, HpkeR, HpkeS,
        PrivateKey, PublicKey,
    },
};

/// The request header is a `KeyId` and 2 each for KEM, KDF, and AEAD identifiers
const REQUEST_HEADER_LEN: usize = size_of::<KeyId>() + 6;
const INFO_REQUEST: &[u8] = b"message/bhttp request";
/// The info used for HPKE export is `INFO_REQUEST`, a zero byte, and the header.
const INFO_LEN: usize = INFO_REQUEST.len() + 1 + REQUEST_HEADER_LEN;
const LABEL_RESPONSE: &[u8] = b"message/bhttp response";
const INFO_KEY: &[u8] = b"key";
const INFO_NONCE: &[u8] = b"nonce";

/// The type of a key identifier.
pub type KeyId = u8;

pub fn init() {
    #[cfg(feature = "nss")]
    nss::init();
}

/// A tuple of KDF and AEAD identifiers.
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub struct SymmetricSuite {
    kdf: Kdf,
    aead: AeadId,
}

impl SymmetricSuite {
    #[must_use]
    pub const fn new(kdf: Kdf, aead: AeadId) -> Self {
        Self { kdf, aead }
    }

    #[must_use]
    pub fn kdf(self) -> Kdf {
        self.kdf
    }

    #[must_use]
    pub fn aead(self) -> AeadId {
        self.aead
    }
}

/// The key configuration of a server.  This can be used by both client and server.
/// An important invariant of this structure is that it does not include
/// any combination of KEM, KDF, and AEAD that is not supported.
pub struct KeyConfig {
    key_id: KeyId,
    kem: Kem,
    symmetric: Vec<SymmetricSuite>,
    sk: Option<PrivateKey>,
    pk: PublicKey,
}

impl KeyConfig {
    fn strip_unsupported(symmetric: &mut Vec<SymmetricSuite>, kem: Kem) {
        symmetric.retain(|s| HpkeConfig::new(kem, s.kdf(), s.aead()).supported());
    }

    /// Construct a configuration for the server side.
    /// # Panics
    /// If the configurations don't include a supported configuration.
    pub fn new(key_id: u8, kem: Kem, mut symmetric: Vec<SymmetricSuite>) -> Res<Self> {
        Self::strip_unsupported(&mut symmetric, kem);
        assert!(!symmetric.is_empty());
        let (sk, pk) = generate_key_pair(kem)?;
        Ok(Self {
            key_id,
            kem,
            symmetric,
            sk: Some(sk),
            pk,
        })
    }

    /// Derive a configuration for the server side from input keying material,
    /// using the `DeriveKeyPair` functionality of the HPKE KEM defined here:
    /// <https://www.ietf.org/archive/id/draft-irtf-cfrg-hpke-12.html#section-4>
    /// # Panics
    /// If the configurations don't include a supported configuration.
    #[allow(unused)]
    pub fn derive(
        key_id: u8,
        kem: Kem,
        mut symmetric: Vec<SymmetricSuite>,
        ikm: &[u8],
    ) -> Res<Self> {
        #[cfg(feature = "rust-hpke")]
        {
            Self::strip_unsupported(&mut symmetric, kem);
            assert!(!symmetric.is_empty());
            let (sk, pk) = derive_key_pair(kem, ikm)?;
            Ok(Self {
                key_id,
                kem,
                symmetric,
                sk: Some(sk),
                pk,
            })
        }
        #[cfg(not(feature = "rust-hpke"))]
        {
            Err(Error::Unsupported)
        }
    }

    /// Encode into a wire format.  This shares a format with the core of ECH:
    ///
    /// ```tls-format
    /// opaque HpkePublicKey[Npk];
    /// uint16 HpkeKemId;  // Defined in I-D.irtf-cfrg-hpke
    /// uint16 HpkeKdfId;  // Defined in I-D.irtf-cfrg-hpke
    /// uint16 HpkeAeadId; // Defined in I-D.irtf-cfrg-hpke
    ///
    /// struct {
    ///   HpkeKdfId kdf_id;
    ///   HpkeAeadId aead_id;
    /// } ECHCipherSuite;
    ///
    /// struct {
    ///   uint8 key_id;
    ///   HpkeKemId kem_id;
    ///   HpkePublicKey public_key;
    ///   ECHCipherSuite cipher_suites<4..2^16-4>;
    /// } ECHKeyConfig;
    /// ```
    /// # Panics
    /// Not as a result of this function.
    pub fn encode(&self) -> Res<Vec<u8>> {
        let mut buf = Vec::new();
        buf.write_u8(self.key_id)?;
        buf.write_u16::<NetworkEndian>(u16::from(self.kem))?;
        let pk_buf = self.pk.key_data()?;
        buf.extend_from_slice(&pk_buf);
        buf.write_u16::<NetworkEndian>((self.symmetric.len() * 4).try_into()?)?;
        for s in &self.symmetric {
            buf.write_u16::<NetworkEndian>(u16::from(s.kdf()))?;
            buf.write_u16::<NetworkEndian>(u16::from(s.aead()))?;
        }
        Ok(buf)
    }

    /// Construct a configuration from the encoded server configuration.
    /// The format of `encoded_config` is the output of `Self::encode`.
    fn parse(encoded_config: &[u8]) -> Res<Self> {
        let mut r = BufReader::new(encoded_config);
        let key_id = r.read_u8()?;
        let kem = Kem::try_from(r.read_u16::<NetworkEndian>()?)?;

        // Note that the KDF and AEAD doesn't matter here.
        let kem_config = HpkeConfig::new(kem, Kdf::HkdfSha256, AeadId::Aes128Gcm);
        if !kem_config.supported() {
            return Err(Error::Unsupported);
        }
        let mut pk_buf = vec![0; kem_config.kem().n_pk()];
        r.read_exact(&mut pk_buf)?;

        let sym_len = r.read_u16::<NetworkEndian>()?;
        let mut sym = vec![0; usize::from(sym_len)];
        r.read_exact(&mut sym)?;
        if sym.is_empty() || (sym.len() % 4 != 0) {
            return Err(Error::Format);
        }
        let sym_count = sym.len() / 4;
        let mut sym_r = BufReader::new(&sym[..]);
        let mut symmetric = Vec::with_capacity(sym_count);
        for _ in 0..sym_count {
            let kdf = Kdf::try_from(sym_r.read_u16::<NetworkEndian>()?)?;
            let aead = AeadId::try_from(sym_r.read_u16::<NetworkEndian>()?)?;
            symmetric.push(SymmetricSuite::new(kdf, aead));
        }

        // Check that there was nothing extra.
        let mut tmp = [0; 1];
        if r.read(&mut tmp)? > 0 {
            return Err(Error::Format);
        }

        Self::strip_unsupported(&mut symmetric, kem);
        let pk = HpkeR::decode_public_key(kem_config.kem(), &pk_buf)?;

        Ok(Self {
            key_id,
            kem,
            symmetric,
            sk: None,
            pk,
        })
    }

    fn select(&self, sym: SymmetricSuite) -> Res<HpkeConfig> {
        if self.symmetric.contains(&sym) {
            let config = HpkeConfig::new(self.kem, sym.kdf(), sym.aead());
            Ok(config)
        } else {
            Err(Error::Unsupported)
        }
    }
}

/// Construct the info parameter we use to initialize an `HpkeS` instance.
fn build_info(key_id: KeyId, config: HpkeConfig) -> Res<Vec<u8>> {
    let mut info = Vec::with_capacity(INFO_LEN);
    info.extend_from_slice(INFO_REQUEST);
    info.push(0);
    info.write_u8(key_id)?;
    info.write_u16::<NetworkEndian>(u16::from(config.kem()))?;
    info.write_u16::<NetworkEndian>(u16::from(config.kdf()))?;
    info.write_u16::<NetworkEndian>(u16::from(config.aead()))?;
    trace!("HPKE info: {}", hex::encode(&info));
    Ok(info)
}

/// This is the sort of information we expect to receive from the receiver.
/// This might not be necessary if we agree on a format.
#[cfg(feature = "client")]
pub struct ClientRequest {
    hpke: HpkeS,
    header: Vec<u8>,
}

#[cfg(feature = "client")]
impl ClientRequest {
    /// Reads an encoded configuration and constructs a single use client sender.
    /// See `KeyConfig::encode` for the structure details.
    #[allow(clippy::similar_names)] // for `sk_s` and `pk_s`
    pub fn new(encoded_config: &[u8]) -> Res<Self> {
        let mut config = KeyConfig::parse(encoded_config)?;
        // TODO(mt) choose the best config, not just the first.
        let selected = config.select(config.symmetric[0])?;

        // Build the info, which contains the message header.
        let info = build_info(config.key_id, selected)?;
        let hpke = HpkeS::new(selected, &mut config.pk, &info)?;

        let header = Vec::from(&info[INFO_REQUEST.len() + 1..]);
        debug_assert_eq!(header.len(), REQUEST_HEADER_LEN);
        Ok(Self { hpke, header })
    }

    /// Encapsulate a request.  This consumes this object.
    /// This produces a response handler and the bytes of an encapsulated request.
    pub fn encapsulate(mut self, request: &[u8]) -> Res<(Vec<u8>, ClientResponse)> {
        let extra =
            self.hpke.config().kem().n_enc() + self.hpke.config().aead().n_t() + request.len();
        let expected_len = self.header.len() + extra;

        let mut enc_request = self.header;
        enc_request.reserve_exact(extra);

        let enc = self.hpke.enc()?;
        enc_request.extend_from_slice(&enc);

        let mut ct = self.hpke.seal(&[], request)?;
        enc_request.append(&mut ct);

        debug_assert_eq!(expected_len, enc_request.len());
        Ok((enc_request, ClientResponse::new(self.hpke, enc)))
    }
}

/// A server can handle multiple requests.
/// It holds a single key pair and can generate a configuration.
/// (A more complex server would have multiple key pairs. This is simple.)
#[cfg(feature = "server")]
pub struct Server {
    config: KeyConfig,
}

#[cfg(feature = "server")]
impl Server {
    /// Create a new server configuration.
    /// # Panics
    /// If the configuration doesn't include a private key.
    pub fn new(config: KeyConfig) -> Res<Self> {
        assert!(config.sk.is_some());
        Ok(Self { config })
    }

    /// Get the configuration that this server uses.
    #[must_use]
    pub fn config(&self) -> &KeyConfig {
        &self.config
    }

    /// Remove encapsulation on a message.
    /// # Panics
    /// Not as a consequence of this code, but Rust won't know that for sure.
    #[allow(clippy::similar_names)] // for kem_id and key_id
    pub fn decapsulate(&mut self, enc_request: &[u8]) -> Res<(Vec<u8>, ServerResponse)> {
        if enc_request.len() < REQUEST_HEADER_LEN {
            return Err(Error::Truncated);
        }
        let mut r = BufReader::new(enc_request);
        let key_id = r.read_u8()?;
        if key_id != self.config.key_id {
            return Err(Error::KeyId);
        }
        let kem_id = Kem::try_from(r.read_u16::<NetworkEndian>()?)?;
        if kem_id != self.config.kem {
            return Err(Error::InvalidKem);
        }
        let kdf_id = Kdf::try_from(r.read_u16::<NetworkEndian>()?)?;
        let aead_id = AeadId::try_from(r.read_u16::<NetworkEndian>()?)?;
        let sym = SymmetricSuite::new(kdf_id, aead_id);

        let info = build_info(
            key_id,
            HpkeConfig::new(self.config.kem, sym.kdf(), sym.aead()),
        )?;

        let cfg = self.config.select(sym)?;
        let mut enc = vec![0; cfg.kem().n_enc()];
        r.read_exact(&mut enc)?;
        let mut hpke = HpkeR::new(
            cfg,
            &self.config.pk,
            self.config.sk.as_mut().unwrap(),
            &enc,
            &info,
        )?;

        let mut ct = Vec::new();
        r.read_to_end(&mut ct)?;

        let request = hpke.open(&[], &ct)?;
        Ok((request, ServerResponse::new(&hpke, enc)?))
    }
}

fn entropy(config: HpkeConfig) -> usize {
    max(config.aead().n_n(), config.aead().n_k())
}

fn make_aead(
    mode: Mode,
    cfg: HpkeConfig,
    exp: &impl Exporter,
    enc: Vec<u8>,
    response_nonce: &[u8],
) -> Res<Aead> {
    let secret = exp.export(LABEL_RESPONSE, entropy(cfg))?;
    let mut salt = enc;
    salt.extend_from_slice(response_nonce);

    let hkdf = Hkdf::new(cfg.kdf());
    let prk = hkdf.extract(&salt, &secret)?;

    let key = hkdf.expand_key(&prk, INFO_KEY, KeyMechanism::Aead(cfg.aead()))?;
    let iv = hkdf.expand_data(&prk, INFO_NONCE, cfg.aead().n_n())?;
    let nonce_base = <[u8; NONCE_LEN]>::try_from(iv).unwrap();

    Aead::new(mode, cfg.aead(), &key, nonce_base)
}

/// An object for encapsulating responses.
/// The only way to obtain one of these is through `Server::decapsulate()`.
#[cfg(feature = "server")]
pub struct ServerResponse {
    response_nonce: Vec<u8>,
    aead: Aead,
}

#[cfg(feature = "server")]
impl ServerResponse {
    fn new(hpke: &HpkeR, enc: Vec<u8>) -> Res<Self> {
        let response_nonce = random(entropy(hpke.config()));
        let aead = make_aead(Mode::Encrypt, hpke.config(), hpke, enc, &response_nonce)?;
        Ok(Self {
            response_nonce,
            aead,
        })
    }

    /// Consume this object by encapsulating a response.
    pub fn encapsulate(mut self, response: &[u8]) -> Res<Vec<u8>> {
        let mut enc_response = self.response_nonce;
        let mut ct = self.aead.seal(&[], response)?;
        enc_response.append(&mut ct);
        Ok(enc_response)
    }
}

#[cfg(feature = "server")]
impl std::fmt::Debug for ServerResponse {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("ServerResponse")
    }
}

/// An object for decapsulating responses.
/// The only way to obtain one of these is through `ClientRequest::encapsulate()`.
#[cfg(feature = "client")]
pub struct ClientResponse {
    hpke: HpkeS,
    enc: Vec<u8>,
}

#[cfg(feature = "client")]
impl ClientResponse {
    /// Private method for constructing one of these.
    /// Doesn't do anything because we don't have the nonce yet, so
    /// the work that can be done is limited.
    fn new(hpke: HpkeS, enc: Vec<u8>) -> Self {
        Self { hpke, enc }
    }

    /// Consume this object by decapsulating a response.
    pub fn decapsulate(self, enc_response: &[u8]) -> Res<Vec<u8>> {
        let mid = entropy(self.hpke.config());
        if mid >= enc_response.len() {
            return Err(Error::Truncated);
        }
        let (response_nonce, ct) = enc_response.split_at(mid);
        let mut aead = make_aead(
            Mode::Decrypt,
            self.hpke.config(),
            &self.hpke,
            self.enc,
            response_nonce,
        )?;
        aead.open(&[], 0, ct) // 0 is the sequence number
    }
}

#[cfg(all(test, feature = "client", feature = "server"))]
mod test {
    use crate::{
        err::Res,
        hpke::{Aead, Kdf, Kem},
        ClientRequest, Error, KeyConfig, KeyId, Server, SymmetricSuite,
    };
    use log::trace;
    use std::{fmt::Debug, io::ErrorKind};

    const KEY_ID: KeyId = 1;
    const KEM: Kem = Kem::X25519Sha256;
    const SYMMETRIC: &[SymmetricSuite] = &[
        SymmetricSuite::new(Kdf::HkdfSha256, Aead::Aes128Gcm),
        SymmetricSuite::new(Kdf::HkdfSha256, Aead::ChaCha20Poly1305),
    ];

    const REQUEST: &[u8] = &[
        0x00, 0x03, 0x47, 0x45, 0x54, 0x05, 0x68, 0x74, 0x74, 0x70, 0x73, 0x0b, 0x65, 0x78, 0x61,
        0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x01, 0x2f,
    ];
    const RESPONSE: &[u8] = &[0x01, 0x40, 0xc8];

    fn init() {
        crate::init();
        let _ = env_logger::try_init();
    }

    #[test]
    fn request_response() {
        init();

        let server_config = KeyConfig::new(KEY_ID, KEM, Vec::from(SYMMETRIC)).unwrap();
        let mut server = Server::new(server_config).unwrap();
        let encoded_config = server.config().encode().unwrap();
        trace!("Config: {}", hex::encode(&encoded_config));

        let client = ClientRequest::new(&encoded_config).unwrap();
        let (enc_request, client_response) = client.encapsulate(REQUEST).unwrap();
        trace!("Request: {}", hex::encode(REQUEST));
        trace!("Encapsulated Request: {}", hex::encode(&enc_request));

        let (request, server_response) = server.decapsulate(&enc_request).unwrap();
        assert_eq!(&request[..], REQUEST);

        let enc_response = server_response.encapsulate(RESPONSE).unwrap();
        trace!("Encapsulated Response: {}", hex::encode(&enc_response));

        let response = client_response.decapsulate(&enc_response).unwrap();
        assert_eq!(&response[..], RESPONSE);
        trace!("Response: {}", hex::encode(RESPONSE));
    }

    #[test]
    fn two_requests() {
        init();

        let server_config = KeyConfig::new(KEY_ID, KEM, Vec::from(SYMMETRIC)).unwrap();
        let mut server = Server::new(server_config).unwrap();
        let encoded_config = server.config().encode().unwrap();

        let client1 = ClientRequest::new(&encoded_config).unwrap();
        let (enc_request1, client_response1) = client1.encapsulate(REQUEST).unwrap();
        let client2 = ClientRequest::new(&encoded_config).unwrap();
        let (enc_request2, client_response2) = client2.encapsulate(REQUEST).unwrap();
        assert_ne!(enc_request1, enc_request2);

        let (request1, server_response1) = server.decapsulate(&enc_request1).unwrap();
        assert_eq!(&request1[..], REQUEST);
        let (request2, server_response2) = server.decapsulate(&enc_request2).unwrap();
        assert_eq!(&request2[..], REQUEST);

        let enc_response1 = server_response1.encapsulate(RESPONSE).unwrap();
        let enc_response2 = server_response2.encapsulate(RESPONSE).unwrap();
        assert_ne!(enc_response1, enc_response2);

        let response1 = client_response1.decapsulate(&enc_response1).unwrap();
        assert_eq!(&response1[..], RESPONSE);
        let response2 = client_response2.decapsulate(&enc_response2).unwrap();
        assert_eq!(&response2[..], RESPONSE);
    }

    fn assert_truncated<T: Debug>(res: Res<T>) {
        match res.unwrap_err() {
            Error::Truncated => {}
            #[cfg(feature = "rust-hpke")]
            Error::Aead(_) => {}
            #[cfg(feature = "nss")]
            Error::Crypto(_) => {}
            Error::Io(e) => assert_eq!(e.kind(), ErrorKind::UnexpectedEof),
            e => panic!("unexpected error type: {e:?}"),
        }
    }

    fn request_truncated(cut: usize) {
        init();

        let server_config = KeyConfig::new(KEY_ID, KEM, Vec::from(SYMMETRIC)).unwrap();
        let mut server = Server::new(server_config).unwrap();
        let encoded_config = server.config().encode().unwrap();

        let client = ClientRequest::new(&encoded_config).unwrap();
        let (enc_request, _) = client.encapsulate(REQUEST).unwrap();

        let res = server.decapsulate(&enc_request[..cut]);
        assert_truncated(res);
    }

    #[test]
    fn request_truncated_header() {
        request_truncated(4);
    }

    #[test]
    fn request_truncated_enc() {
        // header is 7, enc is 32
        request_truncated(24);
    }

    #[test]
    fn request_truncated_ct() {
        // header and enc is 39, aead needs at least 16 more
        request_truncated(42);
    }

    fn response_truncated(cut: usize) {
        init();

        let server_config = KeyConfig::new(KEY_ID, KEM, Vec::from(SYMMETRIC)).unwrap();
        let mut server = Server::new(server_config).unwrap();
        let encoded_config = server.config().encode().unwrap();

        let client = ClientRequest::new(&encoded_config).unwrap();
        let (enc_request, client_response) = client.encapsulate(REQUEST).unwrap();

        let (request, server_response) = server.decapsulate(&enc_request).unwrap();
        assert_eq!(&request[..], REQUEST);

        let enc_response = server_response.encapsulate(RESPONSE).unwrap();

        let res = client_response.decapsulate(&enc_response[..cut]);
        assert_truncated(res);
    }

    #[test]
    fn response_truncated_ct() {
        // nonce is 16, aead needs at least 16 more
        response_truncated(20);
    }

    #[test]
    fn response_truncated_nonce() {
        response_truncated(7);
    }

    #[cfg(feature = "rust-hpke")]
    #[test]
    fn derive_key_pair() {
        const IKM: &[u8] = &[
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
            0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        ];
        const EXPECTED_CONFIG: &[u8] = &[
            0x01, 0x00, 0x20, 0xfc, 0x01, 0x38, 0x93, 0x64, 0x10, 0x31, 0x1a, 0x0c, 0x64, 0x1a,
            0x5c, 0xa0, 0x86, 0x39, 0x1d, 0xe8, 0xe7, 0x03, 0x82, 0x33, 0x3f, 0x6d, 0x64, 0x49,
            0x25, 0x21, 0xad, 0x7d, 0xc7, 0x8a, 0x5d, 0x00, 0x08, 0x00, 0x01, 0x00, 0x01, 0x00,
            0x01, 0x00, 0x03,
        ];

        init();

        let config = KeyConfig::parse(EXPECTED_CONFIG).unwrap();

        let new_config = KeyConfig::derive(KEY_ID, KEM, Vec::from(SYMMETRIC), IKM).unwrap();
        assert_eq!(config.key_id, new_config.key_id);
        assert_eq!(config.kem, new_config.kem);
        assert_eq!(config.symmetric, new_config.symmetric);

        let server = Server::new(new_config).unwrap();
        let encoded_config = server.config().encode().unwrap();
        assert_eq!(EXPECTED_CONFIG, encoded_config);
    }
}
