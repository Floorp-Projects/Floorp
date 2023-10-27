// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::{
    constants::{
        Cipher, Version, TLS_AES_128_GCM_SHA256, TLS_AES_256_GCM_SHA384,
        TLS_CHACHA20_POLY1305_SHA256,
    },
    err::{secstatus_to_res, Error, Res},
    p11::{
        Context, Item, PK11SymKey, PK11_CipherOp, PK11_CreateContextBySymKey, PK11_Encrypt,
        PK11_GetBlockSize, SymKey, CKA_ENCRYPT, CKM_AES_ECB, CKM_CHACHA20, CK_ATTRIBUTE_TYPE,
        CK_CHACHA20_PARAMS, CK_MECHANISM_TYPE,
    },
};
use std::{
    cell::RefCell,
    convert::TryFrom,
    fmt::{self, Debug},
    os::raw::{c_char, c_int, c_uint},
    ptr::{addr_of_mut, null, null_mut},
    rc::Rc,
};

experimental_api!(SSL_HkdfExpandLabelWithMech(
    version: Version,
    cipher: Cipher,
    prk: *mut PK11SymKey,
    handshake_hash: *const u8,
    handshake_hash_len: c_uint,
    label: *const c_char,
    label_len: c_uint,
    mech: CK_MECHANISM_TYPE,
    key_size: c_uint,
    secret: *mut *mut PK11SymKey,
));

#[derive(Clone)]
pub enum HpKey {
    /// An AES encryption context.
    /// Note: as we need to clone this object, we clone the pointer and
    /// track references using `Rc`.  `PK11Context` can't be used with `PK11_CloneContext`
    /// as that is not supported for these contexts.
    Aes(Rc<RefCell<Context>>),
    /// The ChaCha20 mask has to invoke a new PK11_Encrypt every time as it needs to
    /// change the counter and nonce on each invocation.
    Chacha(SymKey),
}

impl Debug for HpKey {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "HpKey")
    }
}

impl HpKey {
    const SAMPLE_SIZE: usize = 16;

    /// QUIC-specific API for extracting a header-protection key.
    ///
    /// # Errors
    /// Errors if HKDF fails or if the label is too long to fit in a `c_uint`.
    /// # Panics
    /// When `cipher` is not known to this code.
    #[allow(clippy::cast_sign_loss)] // Cast for PK11_GetBlockSize is safe.
    pub fn extract(version: Version, cipher: Cipher, prk: &SymKey, label: &str) -> Res<Self> {
        const ZERO: &[u8] = &[0; 12];

        let l = label.as_bytes();
        let mut secret: *mut PK11SymKey = null_mut();

        let (mech, key_size) = match cipher {
            TLS_AES_128_GCM_SHA256 => (CK_MECHANISM_TYPE::from(CKM_AES_ECB), 16),
            TLS_AES_256_GCM_SHA384 => (CK_MECHANISM_TYPE::from(CKM_AES_ECB), 32),
            TLS_CHACHA20_POLY1305_SHA256 => (CK_MECHANISM_TYPE::from(CKM_CHACHA20), 32),
            _ => unreachable!(),
        };

        // Note that this doesn't allow for passing null() for the handshake hash.
        // A zero-length slice produces an identical result.
        unsafe {
            SSL_HkdfExpandLabelWithMech(
                version,
                cipher,
                **prk,
                null(),
                0,
                l.as_ptr().cast(),
                c_uint::try_from(l.len())?,
                mech,
                key_size,
                &mut secret,
            )
        }?;
        let key = SymKey::from_ptr(secret).or(Err(Error::HkdfError))?;

        let res = match cipher {
            TLS_AES_128_GCM_SHA256 | TLS_AES_256_GCM_SHA384 => {
                let context_ptr = unsafe {
                    PK11_CreateContextBySymKey(
                        mech,
                        CK_ATTRIBUTE_TYPE::from(CKA_ENCRYPT),
                        *key,
                        &Item::wrap(&ZERO[..0]), // Borrow a zero-length slice of ZERO.
                    )
                };
                let context = Context::from_ptr(context_ptr).or(Err(Error::CipherInitFailure))?;
                Self::Aes(Rc::new(RefCell::new(context)))
            }
            TLS_CHACHA20_POLY1305_SHA256 => Self::Chacha(key),
            _ => unreachable!(),
        };

        debug_assert_eq!(
            res.block_size(),
            usize::try_from(unsafe { PK11_GetBlockSize(mech, null_mut()) }).unwrap()
        );
        Ok(res)
    }

    /// Get the sample size, which is also the output size.
    #[must_use]
    #[allow(clippy::unused_self)] // To maintain an API contract.
    pub fn sample_size(&self) -> usize {
        Self::SAMPLE_SIZE
    }

    fn block_size(&self) -> usize {
        match self {
            Self::Aes(_) => 16,
            Self::Chacha(_) => 64,
        }
    }

    /// Generate a header protection mask for QUIC.
    ///
    /// # Errors
    /// An error is returned if the NSS functions fail; a sample of the
    /// wrong size is the obvious cause.
    /// # Panics
    /// When the mechanism for our key is not supported.
    pub fn mask(&self, sample: &[u8]) -> Res<Vec<u8>> {
        let mut output = vec![0_u8; self.block_size()];

        match self {
            Self::Aes(context) => {
                let mut output_len: c_int = 0;
                secstatus_to_res(unsafe {
                    PK11_CipherOp(
                        **context.borrow_mut(),
                        output.as_mut_ptr(),
                        &mut output_len,
                        c_int::try_from(output.len())?,
                        sample[..Self::SAMPLE_SIZE].as_ptr().cast(),
                        c_int::try_from(Self::SAMPLE_SIZE).unwrap(),
                    )
                })?;
                assert_eq!(usize::try_from(output_len).unwrap(), output.len());
                Ok(output)
            }

            Self::Chacha(key) => {
                let params: CK_CHACHA20_PARAMS = CK_CHACHA20_PARAMS {
                    pBlockCounter: sample.as_ptr().cast_mut(),
                    blockCounterBits: 32,
                    pNonce: sample[4..Self::SAMPLE_SIZE].as_ptr().cast_mut(),
                    ulNonceBits: 96,
                };
                let mut output_len: c_uint = 0;
                let mut param_item = Item::wrap_struct(&params);
                secstatus_to_res(unsafe {
                    PK11_Encrypt(
                        **key,
                        CK_MECHANISM_TYPE::from(CKM_CHACHA20),
                        addr_of_mut!(param_item),
                        output[..].as_mut_ptr(),
                        &mut output_len,
                        c_uint::try_from(output.len())?,
                        output[..].as_ptr(),
                        c_uint::try_from(output.len())?,
                    )
                })?;
                assert_eq!(usize::try_from(output_len).unwrap(), output.len());
                Ok(output)
            }
        }
    }
}
