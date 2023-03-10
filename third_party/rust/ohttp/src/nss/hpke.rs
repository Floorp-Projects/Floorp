use super::{
    super::hpke::{Aead, Kdf, Kem},
    err::{sec::SEC_ERROR_INVALID_ARGS, secstatus_to_res, Error},
    p11::{sys, Item, PrivateKey, PublicKey, Slot, SymKey},
};
use crate::err::Res;
use log::{log_enabled, trace};
use std::{
    convert::TryFrom,
    ops::Deref,
    os::raw::c_uint,
    ptr::{addr_of_mut, null, null_mut},
};

pub use sys::{HpkeAeadId as AeadId, HpkeKdfId as KdfId, HpkeKemId as KemId};

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
        secstatus_to_res(unsafe {
            sys::PK11_HPKE_ValidateParameters(
                KemId::Type::from(u16::from(self.kem)),
                KdfId::Type::from(u16::from(self.kdf)),
                AeadId::Type::from(u16::from(self.aead)),
            )
        })
        .is_ok()
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

pub trait Exporter {
    fn export(&self, info: &[u8], len: usize) -> Res<SymKey>;
}

unsafe fn destroy_hpke_context(cx: *mut sys::HpkeContext) {
    sys::PK11_HPKE_DestroyContext(cx, sys::PRBool::from(true));
}

scoped_ptr!(HpkeContext, sys::HpkeContext, destroy_hpke_context);

impl HpkeContext {
    fn new(config: Config) -> Res<Self> {
        let ptr = unsafe {
            sys::PK11_HPKE_NewContext(
                KemId::Type::from(u16::from(config.kem)),
                KdfId::Type::from(u16::from(config.kdf)),
                AeadId::Type::from(u16::from(config.aead)),
                null_mut(),
                null(),
            )
        };
        Self::from_ptr(ptr)
    }
}

impl Exporter for HpkeContext {
    fn export(&self, info: &[u8], len: usize) -> Res<SymKey> {
        let mut out: *mut sys::PK11SymKey = null_mut();
        secstatus_to_res(unsafe {
            sys::PK11_HPKE_ExportSecret(
                self.ptr,
                &Item::wrap(info),
                c_uint::try_from(len).unwrap(),
                &mut out,
            )
        })?;
        SymKey::from_ptr(out)
    }
}

#[allow(clippy::module_name_repetitions)]
pub struct HpkeS {
    context: HpkeContext,
    config: Config,
}

impl HpkeS {
    /// Create a new context that uses the KEM mode for sending.
    #[allow(clippy::similar_names)]
    pub fn new(config: Config, pk_r: &mut PublicKey, info: &[u8]) -> Res<Self> {
        let (sk_e, pk_e) = generate_key_pair(config.kem)?;
        let context = HpkeContext::new(config)?;
        secstatus_to_res(unsafe {
            sys::PK11_HPKE_SetupS(*context, *pk_e, *sk_e, **pk_r, &Item::wrap(info))
        })?;
        Ok(Self { context, config })
    }

    pub fn config(&self) -> Config {
        self.config
    }

    /// Get the encapsulated KEM secret.
    pub fn enc(&self) -> Res<Vec<u8>> {
        let v = unsafe { sys::PK11_HPKE_GetEncapPubKey(*self.context) };
        let r = unsafe { v.as_ref() }.ok_or_else(|| Error::from(SEC_ERROR_INVALID_ARGS))?;
        // This is just an alias, so we can't use `Item`.
        let len = usize::try_from(r.len).unwrap();
        let slc = unsafe { std::slice::from_raw_parts(r.data, len) };
        Ok(Vec::from(slc))
    }

    pub fn seal(&mut self, aad: &[u8], pt: &[u8]) -> Res<Vec<u8>> {
        let mut out: *mut sys::SECItem = null_mut();
        secstatus_to_res(unsafe {
            sys::PK11_HPKE_Seal(*self.context, &Item::wrap(aad), &Item::wrap(pt), &mut out)
        })?;
        let v = Item::from_ptr(out)?;
        Ok(unsafe { v.into_vec() })
    }
}

impl Exporter for HpkeS {
    fn export(&self, info: &[u8], len: usize) -> Res<SymKey> {
        self.context.export(info, len)
    }
}

impl Deref for HpkeS {
    type Target = Config;
    fn deref(&self) -> &Self::Target {
        &self.config
    }
}

#[allow(clippy::module_name_repetitions)]
pub struct HpkeR {
    context: HpkeContext,
    config: Config,
}

impl HpkeR {
    /// Create a new context that uses the KEM mode for sending.
    #[allow(clippy::similar_names)]
    pub fn new(
        config: Config,
        pk_r: &PublicKey,
        sk_r: &mut PrivateKey,
        enc: &[u8],
        info: &[u8],
    ) -> Res<Self> {
        let context = HpkeContext::new(config)?;
        secstatus_to_res(unsafe {
            sys::PK11_HPKE_SetupR(
                *context,
                **pk_r,
                **sk_r,
                &Item::wrap(enc),
                &Item::wrap(info),
            )
        })?;
        Ok(Self { context, config })
    }

    pub fn config(&self) -> Config {
        self.config
    }

    pub fn decode_public_key(kem: Kem, k: &[u8]) -> Res<PublicKey> {
        // NSS uses a context for this, but we don't want that, but a dummy one works fine.
        let context = HpkeContext::new(Config {
            kem,
            ..Config::default()
        })?;
        let mut ptr: *mut sys::SECKEYPublicKey = null_mut();
        secstatus_to_res(unsafe {
            sys::PK11_HPKE_Deserialize(
                *context,
                k.as_ptr(),
                c_uint::try_from(k.len()).unwrap(),
                &mut ptr,
            )
        })?;
        PublicKey::from_ptr(ptr)
    }

    pub fn open(&mut self, aad: &[u8], ct: &[u8]) -> Res<Vec<u8>> {
        let mut out: *mut sys::SECItem = null_mut();
        secstatus_to_res(unsafe {
            sys::PK11_HPKE_Open(*self.context, &Item::wrap(aad), &Item::wrap(ct), &mut out)
        })?;
        let v = Item::from_ptr(out)?;
        Ok(unsafe { v.into_vec() })
    }
}

impl Exporter for HpkeR {
    fn export(&self, info: &[u8], len: usize) -> Res<SymKey> {
        self.context.export(info, len)
    }
}

impl Deref for HpkeR {
    type Target = Config;
    fn deref(&self) -> &Self::Target {
        &self.config
    }
}

/// Generate a key pair for the identified KEM.
pub fn generate_key_pair(kem: Kem) -> Res<(PrivateKey, PublicKey)> {
    assert_eq!(kem, Kem::X25519Sha256);
    let slot = Slot::internal()?;

    let oid_data = unsafe { sys::SECOID_FindOIDByTag(sys::SECOidTag::SEC_OID_CURVE25519) };
    let oid = unsafe { oid_data.as_ref() }.ok_or_else(Error::internal)?;
    let oid_slc =
        unsafe { std::slice::from_raw_parts(oid.oid.data, usize::try_from(oid.oid.len).unwrap()) };
    let mut params: Vec<u8> = Vec::with_capacity(oid_slc.len() + 2);
    params.push(u8::try_from(sys::SEC_ASN1_OBJECT_ID).unwrap());
    params.push(u8::try_from(oid.oid.len).unwrap());
    params.extend_from_slice(oid_slc);

    let mut public_ptr: *mut sys::SECKEYPublicKey = null_mut();
    let mut wrapped = Item::wrap(&params);

    // Try to make an insensitive key so that we can read the key data for tracing.
    let insensitive_secret_ptr = if log_enabled!(log::Level::Trace) {
        unsafe {
            sys::PK11_GenerateKeyPairWithOpFlags(
                *slot,
                sys::CK_MECHANISM_TYPE::from(sys::CKM_EC_KEY_PAIR_GEN),
                addr_of_mut!(wrapped).cast(),
                &mut public_ptr,
                sys::PK11_ATTR_SESSION | sys::PK11_ATTR_INSENSITIVE | sys::PK11_ATTR_PUBLIC,
                sys::CK_FLAGS::from(sys::CKF_DERIVE),
                sys::CK_FLAGS::from(sys::CKF_DERIVE),
                null_mut(),
            )
        }
    } else {
        null_mut()
    };
    assert_eq!(insensitive_secret_ptr.is_null(), public_ptr.is_null());
    let secret_ptr = if insensitive_secret_ptr.is_null() {
        unsafe {
            sys::PK11_GenerateKeyPairWithOpFlags(
                *slot,
                sys::CK_MECHANISM_TYPE::from(sys::CKM_EC_KEY_PAIR_GEN),
                addr_of_mut!(wrapped).cast(),
                &mut public_ptr,
                sys::PK11_ATTR_SESSION | sys::PK11_ATTR_SENSITIVE | sys::PK11_ATTR_PRIVATE,
                sys::CK_FLAGS::from(sys::CKF_DERIVE),
                sys::CK_FLAGS::from(sys::CKF_DERIVE),
                null_mut(),
            )
        }
    } else {
        insensitive_secret_ptr
    };
    assert_eq!(secret_ptr.is_null(), public_ptr.is_null());
    let sk = PrivateKey::from_ptr(secret_ptr)?;
    let pk = PublicKey::from_ptr(public_ptr)?;
    trace!("Generated key pair: sk={:?} pk={:?}", sk, pk);
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
