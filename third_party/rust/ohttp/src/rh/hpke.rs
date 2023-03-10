use super::SymKey;
use crate::{
    hpke::{Aead, Kdf, Kem},
    Error, Res,
};
use ::hpke::{
    aead::{AeadTag, AesGcm128, ChaCha20Poly1305},
    kdf::HkdfSha256,
    kem::{Kem as HpkeKem, X25519HkdfSha256},
    kex::{KeyExchange, X25519},
    op_mode::{OpModeR, OpModeS},
    setup::{setup_receiver, setup_sender},
    AeadCtxR, AeadCtxS, Deserializable, EncappedKey, Serializable,
};
use ::rand::thread_rng;
use log::trace;
use std::ops::Deref;

/// Configuration for `Hpke`.
#[derive(Clone, Copy)]
pub struct Config {
    kem: Kem,
    kdf: Kdf,
    aead: Aead,
}

impl Config {
    pub fn new(kem: Kem, kdf: Kdf, aead: Aead) -> Self {
        Self { kem, kdf, aead }
    }

    pub fn kem(self) -> Kem {
        self.kem
    }

    pub fn kdf(self) -> Kdf {
        self.kdf
    }

    pub fn aead(self) -> Aead {
        self.aead
    }

    pub fn supported(self) -> bool {
        // TODO support more options
        self.kdf == Kdf::HkdfSha256 && matches!(self.aead, Aead::Aes128Gcm | Aead::ChaCha20Poly1305)
    }
}

impl Default for Config {
    fn default() -> Self {
        Self {
            kem: Kem::X25519Sha256,
            kdf: Kdf::HkdfSha256,
            aead: Aead::Aes128Gcm,
        }
    }
}

pub enum PublicKey {
    X25519(<<X25519HkdfSha256 as HpkeKem>::Kex as KeyExchange>::PublicKey),
}

impl PublicKey {
    #[allow(clippy::unnecessary_wraps)]
    pub fn key_data(&self) -> Res<Vec<u8>> {
        Ok(match self {
            Self::X25519(k) => Vec::from(k.to_bytes().as_slice()),
        })
    }
}

impl std::fmt::Debug for PublicKey {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        if let Ok(b) = self.key_data() {
            write!(f, "PublicKey {}", hex::encode(b))
        } else {
            write!(f, "Opaque PublicKey")
        }
    }
}

pub enum PrivateKey {
    X25519(<<X25519HkdfSha256 as HpkeKem>::Kex as KeyExchange>::PrivateKey),
}

impl PrivateKey {
    #[allow(clippy::unnecessary_wraps)]
    pub fn key_data(&self) -> Res<Vec<u8>> {
        Ok(match self {
            Self::X25519(k) => Vec::from(k.to_bytes().as_slice()),
        })
    }
}

impl std::fmt::Debug for PrivateKey {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        if let Ok(b) = self.key_data() {
            write!(f, "PrivateKey {}", hex::encode(b))
        } else {
            write!(f, "Opaque PrivateKey")
        }
    }
}

// TODO: Use macros here.  To do that, we needs concat_ident!(), but it's not ready.
// This is what a macro that uses concat_ident!() might produce, written out in full.
enum SenderContextX25519HkdfSha256HkdfSha256 {
    AesGcm128(Box<AeadCtxS<AesGcm128, HkdfSha256, X25519HkdfSha256>>),
    ChaCha20Poly1305(Box<AeadCtxS<ChaCha20Poly1305, HkdfSha256, X25519HkdfSha256>>),
}

enum SenderContextX25519HkdfSha256 {
    HkdfSha256(SenderContextX25519HkdfSha256HkdfSha256),
}

enum SenderContext {
    X25519HkdfSha256(SenderContextX25519HkdfSha256),
}

impl SenderContext {
    fn seal(&mut self, plaintext: &mut [u8], aad: &[u8]) -> Res<Vec<u8>> {
        Ok(match self {
            Self::X25519HkdfSha256(SenderContextX25519HkdfSha256::HkdfSha256(
                SenderContextX25519HkdfSha256HkdfSha256::AesGcm128(context),
            )) => {
                let tag = context.seal(plaintext, aad)?;
                Vec::from(tag.to_bytes().as_slice())
            }
            Self::X25519HkdfSha256(SenderContextX25519HkdfSha256::HkdfSha256(
                SenderContextX25519HkdfSha256HkdfSha256::ChaCha20Poly1305(context),
            )) => {
                let tag = context.seal(plaintext, aad)?;
                Vec::from(tag.to_bytes().as_slice())
            }
        })
    }

    fn export(&self, info: &[u8], out_buf: &mut [u8]) -> Res<()> {
        match self {
            Self::X25519HkdfSha256(SenderContextX25519HkdfSha256::HkdfSha256(
                SenderContextX25519HkdfSha256HkdfSha256::AesGcm128(context),
            )) => {
                context.export(info, out_buf)?;
            }
            Self::X25519HkdfSha256(SenderContextX25519HkdfSha256::HkdfSha256(
                SenderContextX25519HkdfSha256HkdfSha256::ChaCha20Poly1305(context),
            )) => {
                context.export(info, out_buf)?;
            }
        }
        Ok(())
    }
}

pub trait Exporter {
    fn export(&self, info: &[u8], len: usize) -> Res<SymKey>;
}

#[allow(clippy::module_name_repetitions)]
pub struct HpkeS {
    context: SenderContext,
    enc: Vec<u8>,
    config: Config,
}

impl HpkeS {
    /// Create a new context that uses the KEM mode for sending.
    pub fn new(config: Config, pk_r: &mut PublicKey, info: &[u8]) -> Res<Self> {
        let mut csprng = thread_rng();

        macro_rules! dispatch_hpkes_new {
            {
                ($c:expr, $pk:expr, $csprng:expr): [$({
                    $kemid:path => $kem:path,
                    $kdfid:path => $kdf:path,
                    $aeadid:path => $aead:path,
                    $pke:path, $ctxt1:path, $ctxt2:path, $ctxt3:path $(,)?
                }),* $(,)?]
            } => {
                match ($c, $pk) {
                    $(
                        (
                            Config {
                                kem: $kemid,
                                kdf: $kdfid,
                                aead: $aeadid,
                            },
                            $pke(pk_r),
                        ) => {
                            let (enc, context) = setup_sender::<$aead, $kdf, $kem, _>(
                                &OpModeS::Base,
                                pk_r,
                                info,
                                $csprng,
                            )?;
                            ($ctxt1($ctxt2($ctxt3(Box::new(context)))), enc)
                        }
                    )*
                    _ => return Err(Error::InvalidKeyType),
                }
            };
        }
        let (context, enc) = dispatch_hpkes_new! { (config, pk_r, &mut csprng): [
            {
                Kem::X25519Sha256 => X25519HkdfSha256,
                Kdf::HkdfSha256 => HkdfSha256,
                Aead::Aes128Gcm => AesGcm128,
                PublicKey::X25519,
                SenderContext::X25519HkdfSha256,
                SenderContextX25519HkdfSha256::HkdfSha256,
                SenderContextX25519HkdfSha256HkdfSha256::AesGcm128,
            },
            {
                Kem::X25519Sha256 => X25519HkdfSha256,
                Kdf::HkdfSha256 => HkdfSha256,
                Aead::ChaCha20Poly1305 => ChaCha20Poly1305,
                PublicKey::X25519,
                SenderContext::X25519HkdfSha256,
                SenderContextX25519HkdfSha256::HkdfSha256,
                SenderContextX25519HkdfSha256HkdfSha256::ChaCha20Poly1305,
            },
        ]};
        let enc = Vec::from(enc.to_bytes().as_slice());
        Ok(Self {
            context,
            enc,
            config,
        })
    }

    pub fn config(&self) -> Config {
        self.config
    }

    /// Get the encapsulated KEM secret.
    #[allow(clippy::unnecessary_wraps)]
    pub fn enc(&self) -> Res<Vec<u8>> {
        Ok(self.enc.clone())
    }

    pub fn seal(&mut self, aad: &[u8], pt: &[u8]) -> Res<Vec<u8>> {
        let mut buf = pt.to_owned();
        let mut tag = self.context.seal(&mut buf, aad)?;
        buf.append(&mut tag);
        Ok(buf)
    }
}

impl Exporter for HpkeS {
    fn export(&self, info: &[u8], len: usize) -> Res<SymKey> {
        let mut buf = vec![0; len];
        self.context.export(info, &mut buf)?;
        Ok(SymKey::from(buf))
    }
}

impl Deref for HpkeS {
    type Target = Config;
    fn deref(&self) -> &Self::Target {
        &self.config
    }
}

enum ReceiverContextX25519HkdfSha256HkdfSha256 {
    AesGcm128(Box<AeadCtxR<AesGcm128, HkdfSha256, X25519HkdfSha256>>),
    ChaCha20Poly1305(Box<AeadCtxR<ChaCha20Poly1305, HkdfSha256, X25519HkdfSha256>>),
}

enum ReceiverContextX25519HkdfSha256 {
    HkdfSha256(ReceiverContextX25519HkdfSha256HkdfSha256),
}

enum ReceiverContext {
    X25519HkdfSha256(ReceiverContextX25519HkdfSha256),
}

impl ReceiverContext {
    fn open<'a>(&mut self, ciphertext: &'a mut [u8], aad: &[u8]) -> Res<&'a [u8]> {
        Ok(match self {
            Self::X25519HkdfSha256(ReceiverContextX25519HkdfSha256::HkdfSha256(
                ReceiverContextX25519HkdfSha256HkdfSha256::AesGcm128(context),
            )) => {
                if ciphertext.len() < AeadTag::<AesGcm128>::size() {
                    return Err(Error::Truncated);
                }
                let (ct, tag) =
                    ciphertext.split_at_mut(ciphertext.len() - AeadTag::<AesGcm128>::size());
                let tag = AeadTag::<AesGcm128>::from_bytes(tag)?;
                context.open(ct, aad, &tag)?;
                ct
            }
            Self::X25519HkdfSha256(ReceiverContextX25519HkdfSha256::HkdfSha256(
                ReceiverContextX25519HkdfSha256HkdfSha256::ChaCha20Poly1305(context),
            )) => {
                if ciphertext.len() < AeadTag::<ChaCha20Poly1305>::size() {
                    return Err(Error::Truncated);
                }
                let (ct, tag) =
                    ciphertext.split_at_mut(ciphertext.len() - AeadTag::<ChaCha20Poly1305>::size());
                let tag = AeadTag::<ChaCha20Poly1305>::from_bytes(tag)?;
                context.open(ct, aad, &tag)?;
                ct
            }
        })
    }

    fn export(&self, info: &[u8], out_buf: &mut [u8]) -> Res<()> {
        match self {
            Self::X25519HkdfSha256(ReceiverContextX25519HkdfSha256::HkdfSha256(
                ReceiverContextX25519HkdfSha256HkdfSha256::AesGcm128(context),
            )) => {
                context.export(info, out_buf)?;
            }
            Self::X25519HkdfSha256(ReceiverContextX25519HkdfSha256::HkdfSha256(
                ReceiverContextX25519HkdfSha256HkdfSha256::ChaCha20Poly1305(context),
            )) => {
                context.export(info, out_buf)?;
            }
        }
        Ok(())
    }
}

#[allow(clippy::module_name_repetitions)]
pub struct HpkeR {
    context: ReceiverContext,
    config: Config,
}

impl HpkeR {
    /// Create a new context that uses the KEM mode for sending.
    #[allow(clippy::similar_names)]
    pub fn new(
        config: Config,
        _pk_r: &PublicKey,
        sk_r: &mut PrivateKey,
        enc: &[u8],
        info: &[u8],
    ) -> Res<Self> {
        macro_rules! dispatch_hpker_new {
            {
                ($c:ident, $sk:ident): [$({
                    $kemid:path => $kem:path,
                    $kdfid:path => $kdf:path,
                    $aeadid:path => $aead:path,
                    $ske:path, $ctxt1:path, $ctxt2:path, $ctxt3:path $(,)?
            }),* $(,)?]
            } => {
                match ($c, $sk) {
                    $(
                        (
                            Config {
                                kem: $kemid,
                                kdf: $kdfid,
                                aead: $aeadid,
                            },
                            $ske(sk_r),
                        ) => {
                            let enc = EncappedKey::from_bytes(enc)?;
                            let context = setup_receiver::<$aead, $kdf, $kem>(
                                &OpModeR::Base,
                                sk_r,
                                &enc,
                                info,
                            )?;
                            $ctxt1($ctxt2($ctxt3(Box::new(context))))
                        }
                    )*
                    _ => return Err(Error::InvalidKeyType),
                }
            };
        }
        let context = dispatch_hpker_new! {(config, sk_r): [
            {
                Kem::X25519Sha256 => X25519HkdfSha256,
                Kdf::HkdfSha256 => HkdfSha256,
                Aead::Aes128Gcm => AesGcm128,
                PrivateKey::X25519,
                ReceiverContext::X25519HkdfSha256,
                ReceiverContextX25519HkdfSha256::HkdfSha256,
                ReceiverContextX25519HkdfSha256HkdfSha256::AesGcm128,
            },
            {
                Kem::X25519Sha256 => X25519HkdfSha256,
                Kdf::HkdfSha256 => HkdfSha256,
                Aead::ChaCha20Poly1305 => ChaCha20Poly1305,
                PrivateKey::X25519,
                ReceiverContext::X25519HkdfSha256,
                ReceiverContextX25519HkdfSha256::HkdfSha256,
                ReceiverContextX25519HkdfSha256HkdfSha256::ChaCha20Poly1305,
            },
        ]};
        Ok(Self { context, config })
    }

    pub fn config(&self) -> Config {
        self.config
    }

    pub fn decode_public_key(kem: Kem, k: &[u8]) -> Res<PublicKey> {
        Ok(match kem {
            Kem::X25519Sha256 => {
                PublicKey::X25519(<X25519 as KeyExchange>::PublicKey::from_bytes(k)?)
            }
        })
    }

    pub fn open(&mut self, aad: &[u8], ct: &[u8]) -> Res<Vec<u8>> {
        let mut buf = ct.to_owned();
        let pt_len = self.context.open(&mut buf, aad)?.len();
        buf.truncate(pt_len);
        Ok(buf)
    }
}

impl Exporter for HpkeR {
    fn export(&self, info: &[u8], len: usize) -> Res<SymKey> {
        let mut buf = vec![0; len];
        self.context.export(info, &mut buf)?;
        Ok(SymKey::from(buf))
    }
}

impl Deref for HpkeR {
    type Target = Config;
    fn deref(&self) -> &Self::Target {
        &self.config
    }
}

/// Generate a key pair for the identified KEM.
#[allow(clippy::unnecessary_wraps)]
pub fn generate_key_pair(kem: Kem) -> Res<(PrivateKey, PublicKey)> {
    let mut csprng = thread_rng();
    let (sk, pk) = match kem {
        Kem::X25519Sha256 => {
            let (sk, pk) = X25519HkdfSha256::gen_keypair(&mut csprng);
            (PrivateKey::X25519(sk), PublicKey::X25519(pk))
        }
    };
    trace!("Generated key pair: sk={:?} pk={:?}", sk, pk);
    Ok((sk, pk))
}

#[allow(clippy::unnecessary_wraps)]
pub fn derive_key_pair(kem: Kem, ikm: &[u8]) -> Res<(PrivateKey, PublicKey)> {
    let (sk, pk) = match kem {
        Kem::X25519Sha256 => {
            let (sk, pk) = X25519HkdfSha256::derive_keypair(ikm);
            (PrivateKey::X25519(sk), PublicKey::X25519(pk))
        }
    };
    trace!("Derived key pair: sk={:?} pk={:?}", sk, pk);
    Ok((sk, pk))
}

#[cfg(test)]
mod test {
    use super::{generate_key_pair, Config, HpkeR, HpkeS};
    use crate::{hpke::Aead, init};

    const INFO: &[u8] = b"info";
    const AAD: &[u8] = b"aad";
    const PT: &[u8] = b"message";

    #[allow(clippy::similar_names)] // for sk_x and pk_x
    #[test]
    fn make() {
        init();
        let cfg = Config::default();
        let (mut sk_r, mut pk_r) = generate_key_pair(cfg.kem()).unwrap();
        let hpke_s = HpkeS::new(cfg, &mut pk_r, INFO).unwrap();
        let _hpke_r = HpkeR::new(cfg, &pk_r, &mut sk_r, &hpke_s.enc().unwrap(), INFO).unwrap();
    }

    #[allow(clippy::similar_names)] // for sk_x and pk_x
    fn seal_open(aead: Aead) {
        // Setup
        init();
        let cfg = Config {
            aead,
            ..Config::default()
        };
        assert!(cfg.supported());
        let (mut sk_r, mut pk_r) = generate_key_pair(cfg.kem()).unwrap();

        // Send
        let mut hpke_s = HpkeS::new(cfg, &mut pk_r, INFO).unwrap();
        let enc = hpke_s.enc().unwrap();
        let ct = hpke_s.seal(AAD, PT).unwrap();

        // Receive
        let mut hpke_r = HpkeR::new(cfg, &pk_r, &mut sk_r, &enc, INFO).unwrap();
        let pt = hpke_r.open(AAD, &ct).unwrap();
        assert_eq!(&pt[..], PT);
    }

    #[test]
    fn seal_open_gcm() {
        seal_open(Aead::Aes128Gcm);
    }

    #[test]
    fn seal_open_chacha() {
        seal_open(Aead::ChaCha20Poly1305);
    }
}
