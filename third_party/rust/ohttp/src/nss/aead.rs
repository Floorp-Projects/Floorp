use super::{
    err::secstatus_to_res,
    p11::{
        sys::{
            self, PK11Context, PK11_AEADOp, PK11_CreateContextBySymKey, PRBool, CKA_DECRYPT,
            CKA_ENCRYPT, CKA_NSS_MESSAGE, CKG_GENERATE_COUNTER_XOR, CKG_NO_GENERATE, CKM_AES_GCM,
            CKM_CHACHA20_POLY1305, CK_ATTRIBUTE_TYPE, CK_GENERATOR_FUNCTION, CK_MECHANISM_TYPE,
        },
        Item, SymKey,
    },
};
use crate::{
    err::{Error, Res},
    hpke::Aead as AeadId,
};
use log::trace;
use std::{
    convert::{TryFrom, TryInto},
    mem,
    os::raw::c_int,
};

/// All the nonces are the same length.  Exploit that.
pub const NONCE_LEN: usize = 12;
/// The portion of the nonce that is a counter.
const COUNTER_LEN: usize = mem::size_of::<SequenceNumber>();
/// The NSS API insists on us identifying the tag separately, which is awful.
/// All of the AEAD functions here have a tag of this length, so use a fixed offset.
const TAG_LEN: usize = 16;

pub type SequenceNumber = u64;

/// All the lengths used by `PK11_AEADOp` are signed.  This converts to that.
fn c_int_len<T>(l: T) -> c_int
where
    T: TryInto<c_int>,
    T::Error: std::error::Error,
{
    l.try_into().unwrap()
}

unsafe fn destroy_aead_context(ctx: *mut PK11Context) {
    sys::PK11_DestroyContext(ctx, PRBool::from(true));
}
scoped_ptr!(Context, PK11Context, destroy_aead_context);

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Mode {
    Encrypt,
    Decrypt,
}

impl Mode {
    fn p11mode(self) -> CK_ATTRIBUTE_TYPE {
        CK_ATTRIBUTE_TYPE::from(
            CKA_NSS_MESSAGE
                | match self {
                    Self::Encrypt => CKA_ENCRYPT,
                    Self::Decrypt => CKA_DECRYPT,
                },
        )
    }
}

/// This is an AEAD instance that uses the
pub struct Aead {
    mode: Mode,
    ctx: Context,
    nonce_base: [u8; NONCE_LEN],
}

impl Aead {
    fn mech(algorithm: AeadId) -> CK_MECHANISM_TYPE {
        CK_MECHANISM_TYPE::from(match algorithm {
            // The key size determines which AES variant is used.
            AeadId::Aes128Gcm | AeadId::Aes256Gcm => CKM_AES_GCM,
            AeadId::ChaCha20Poly1305 => CKM_CHACHA20_POLY1305,
        })
    }

    #[cfg(test)]
    pub fn import_key(algorithm: AeadId, key: &[u8]) -> Res<SymKey> {
        let slot = super::p11::Slot::internal()?;
        let ptr = unsafe {
            sys::PK11_ImportSymKey(
                *slot,
                Self::mech(algorithm),
                sys::PK11Origin::PK11_OriginUnwrap,
                sys::CK_ATTRIBUTE_TYPE::from(sys::CKA_ENCRYPT | sys::CKA_DECRYPT),
                &mut super::p11::Item::wrap(key),
                std::ptr::null_mut(),
            )
        };
        SymKey::from_ptr(ptr)
    }

    pub fn new(
        mode: Mode,
        algorithm: AeadId,
        key: &SymKey,
        nonce_base: [u8; NONCE_LEN],
    ) -> Res<Self> {
        trace!(
            "New AEAD: key={} nonce_base={}",
            hex::encode(key.key_data()?),
            hex::encode(nonce_base)
        );

        let ptr = unsafe {
            PK11_CreateContextBySymKey(
                Self::mech(algorithm),
                mode.p11mode(),
                **key,
                &Item::wrap(&nonce_base[..]),
            )
        };
        Ok(Self {
            mode,
            ctx: Context::from_ptr(ptr)?,
            nonce_base,
        })
    }

    pub fn seal(&mut self, aad: &[u8], pt: &[u8]) -> Res<Vec<u8>> {
        assert_eq!(self.mode, Mode::Encrypt);
        // A copy for the nonce generator to write into.  But we don't use the value.
        let mut nonce = self.nonce_base;
        // Ciphertext with enough space for the tag.
        // Even though we give the operation a separate buffer for the tag,
        // reserve the capacity on allocation.
        let mut ct = vec![0; pt.len() + TAG_LEN];
        let mut ct_len: c_int = 0;
        let mut tag = vec![0; TAG_LEN];
        secstatus_to_res(unsafe {
            PK11_AEADOp(
                *self.ctx,
                CK_GENERATOR_FUNCTION::from(CKG_GENERATE_COUNTER_XOR),
                c_int_len(NONCE_LEN - COUNTER_LEN), // Fixed portion of the nonce.
                nonce.as_mut_ptr(),
                c_int_len(nonce.len()),
                aad.as_ptr(),
                c_int_len(aad.len()),
                ct.as_mut_ptr(),
                &mut ct_len,
                c_int_len(ct.len()), // signed :(
                tag.as_mut_ptr(),
                c_int_len(tag.len()),
                pt.as_ptr(),
                c_int_len(pt.len()),
            )
        })?;
        ct.truncate(usize::try_from(ct_len).unwrap());
        debug_assert_eq!(ct.len(), pt.len());
        ct.append(&mut tag);
        Ok(ct)
    }

    pub fn open(&mut self, aad: &[u8], seq: SequenceNumber, ct: &[u8]) -> Res<Vec<u8>> {
        assert_eq!(self.mode, Mode::Decrypt);
        let mut nonce = self.nonce_base;
        for (i, n) in nonce.iter_mut().rev().take(COUNTER_LEN).enumerate() {
            *n ^= u8::try_from((seq >> (8 * i)) & 0xff).unwrap();
        }
        let mut pt = vec![0; ct.len()]; // NSS needs more space than it uses for plaintext.
        let mut pt_len: c_int = 0;
        let pt_expected = ct.len().checked_sub(TAG_LEN).ok_or(Error::Truncated)?;
        secstatus_to_res(unsafe {
            PK11_AEADOp(
                *self.ctx,
                CK_GENERATOR_FUNCTION::from(CKG_NO_GENERATE),
                c_int_len(NONCE_LEN - COUNTER_LEN), // Fixed portion of the nonce.
                nonce.as_mut_ptr(),
                c_int_len(nonce.len()),
                aad.as_ptr(),
                c_int_len(aad.len()),
                pt.as_mut_ptr(),
                &mut pt_len,
                c_int_len(pt.len()),                    // signed :(
                ct.as_ptr().add(pt_expected) as *mut _, // const cast :(
                c_int_len(TAG_LEN),
                ct.as_ptr(),
                c_int_len(pt_expected),
            )
        })?;
        let len = usize::try_from(pt_len).unwrap();
        debug_assert_eq!(len, pt_expected);
        pt.truncate(len);
        Ok(pt)
    }
}

#[cfg(test)]
mod test {
    use super::{
        super::{super::hpke::Aead as AeadId, init},
        Aead, Mode, SequenceNumber, NONCE_LEN,
    };

    /// Check that the first invocation of encryption matches expected values.
    /// Also check decryption of the same.
    fn check0(
        algorithm: AeadId,
        key: &[u8],
        nonce: &[u8; NONCE_LEN],
        aad: &[u8],
        pt: &[u8],
        ct: &[u8],
    ) {
        init();
        let k = Aead::import_key(algorithm, key).unwrap();

        let mut enc = Aead::new(Mode::Encrypt, algorithm, &k, *nonce).unwrap();
        let ciphertext = enc.seal(aad, pt).unwrap();
        assert_eq!(&ciphertext[..], ct);

        let mut dec = Aead::new(Mode::Decrypt, algorithm, &k, *nonce).unwrap();
        let plaintext = dec.open(aad, 0, ct).unwrap();
        assert_eq!(&plaintext[..], pt);
    }

    fn decrypt(
        algorithm: AeadId,
        key: &[u8],
        nonce: &[u8; NONCE_LEN],
        seq: SequenceNumber,
        aad: &[u8],
        pt: &[u8],
        ct: &[u8],
    ) {
        let k = Aead::import_key(algorithm, key).unwrap();
        let mut dec = Aead::new(Mode::Decrypt, algorithm, &k, *nonce).unwrap();
        let plaintext = dec.open(aad, seq, ct).unwrap();
        assert_eq!(&plaintext[..], pt);
    }

    /// This tests the AEAD in QUIC in combination with the HKDF code.
    /// This is an AEAD-only example.
    #[test]
    fn quic_retry() {
        const KEY: &[u8] = &[
            0xbe, 0x0c, 0x69, 0x0b, 0x9f, 0x66, 0x57, 0x5a, 0x1d, 0x76, 0x6b, 0x54, 0xe3, 0x68,
            0xc8, 0x4e,
        ];
        const NONCE: &[u8; NONCE_LEN] = &[
            0x46, 0x15, 0x99, 0xd3, 0x5d, 0x63, 0x2b, 0xf2, 0x23, 0x98, 0x25, 0xbb,
        ];
        const AAD: &[u8] = &[
            0x08, 0x83, 0x94, 0xc8, 0xf0, 0x3e, 0x51, 0x57, 0x08, 0xff, 0x00, 0x00, 0x00, 0x01,
            0x00, 0x08, 0xf0, 0x67, 0xa5, 0x50, 0x2a, 0x42, 0x62, 0xb5, 0x74, 0x6f, 0x6b, 0x65,
            0x6e,
        ];
        const CT: &[u8] = &[
            0x04, 0xa2, 0x65, 0xba, 0x2e, 0xff, 0x4d, 0x82, 0x90, 0x58, 0xfb, 0x3f, 0x0f, 0x24,
            0x96, 0xba,
        ];
        check0(AeadId::Aes128Gcm, KEY, NONCE, AAD, &[], CT);
    }

    #[test]
    fn quic_server_initial() {
        const ALG: AeadId = AeadId::Aes128Gcm;
        const KEY: &[u8] = &[
            0xcf, 0x3a, 0x53, 0x31, 0x65, 0x3c, 0x36, 0x4c, 0x88, 0xf0, 0xf3, 0x79, 0xb6, 0x06,
            0x7e, 0x37,
        ];
        const NONCE_BASE: &[u8; NONCE_LEN] = &[
            0x0a, 0xc1, 0x49, 0x3c, 0xa1, 0x90, 0x58, 0x53, 0xb0, 0xbb, 0xa0, 0x3e,
        ];
        // Note that this integrates the sequence number of 1 from the example,
        // otherwise we can't use a sequence number of 0 to encrypt.
        const NONCE: &[u8; NONCE_LEN] = &[
            0x0a, 0xc1, 0x49, 0x3c, 0xa1, 0x90, 0x58, 0x53, 0xb0, 0xbb, 0xa0, 0x3f,
        ];
        const AAD: &[u8] = &[
            0xc1, 0x00, 0x00, 0x00, 0x01, 0x00, 0x08, 0xf0, 0x67, 0xa5, 0x50, 0x2a, 0x42, 0x62,
            0xb5, 0x00, 0x40, 0x75, 0x00, 0x01,
        ];
        const PT: &[u8] = &[
            0x02, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x40, 0x5a, 0x02, 0x00, 0x00, 0x56, 0x03,
            0x03, 0xee, 0xfc, 0xe7, 0xf7, 0xb3, 0x7b, 0xa1, 0xd1, 0x63, 0x2e, 0x96, 0x67, 0x78,
            0x25, 0xdd, 0xf7, 0x39, 0x88, 0xcf, 0xc7, 0x98, 0x25, 0xdf, 0x56, 0x6d, 0xc5, 0x43,
            0x0b, 0x9a, 0x04, 0x5a, 0x12, 0x00, 0x13, 0x01, 0x00, 0x00, 0x2e, 0x00, 0x33, 0x00,
            0x24, 0x00, 0x1d, 0x00, 0x20, 0x9d, 0x3c, 0x94, 0x0d, 0x89, 0x69, 0x0b, 0x84, 0xd0,
            0x8a, 0x60, 0x99, 0x3c, 0x14, 0x4e, 0xca, 0x68, 0x4d, 0x10, 0x81, 0x28, 0x7c, 0x83,
            0x4d, 0x53, 0x11, 0xbc, 0xf3, 0x2b, 0xb9, 0xda, 0x1a, 0x00, 0x2b, 0x00, 0x02, 0x03,
            0x04,
        ];
        const CT: &[u8] = &[
            0x5a, 0x48, 0x2c, 0xd0, 0x99, 0x1c, 0xd2, 0x5b, 0x0a, 0xac, 0x40, 0x6a, 0x58, 0x16,
            0xb6, 0x39, 0x41, 0x00, 0xf3, 0x7a, 0x1c, 0x69, 0x79, 0x75, 0x54, 0x78, 0x0b, 0xb3,
            0x8c, 0xc5, 0xa9, 0x9f, 0x5e, 0xde, 0x4c, 0xf7, 0x3c, 0x3e, 0xc2, 0x49, 0x3a, 0x18,
            0x39, 0xb3, 0xdb, 0xcb, 0xa3, 0xf6, 0xea, 0x46, 0xc5, 0xb7, 0x68, 0x4d, 0xf3, 0x54,
            0x8e, 0x7d, 0xde, 0xb9, 0xc3, 0xbf, 0x9c, 0x73, 0xcc, 0x3f, 0x3b, 0xde, 0xd7, 0x4b,
            0x56, 0x2b, 0xfb, 0x19, 0xfb, 0x84, 0x02, 0x2f, 0x8e, 0xf4, 0xcd, 0xd9, 0x37, 0x95,
            0xd7, 0x7d, 0x06, 0xed, 0xbb, 0x7a, 0xaf, 0x2f, 0x58, 0x89, 0x18, 0x50, 0xab, 0xbd,
            0xca, 0x3d, 0x20, 0x39, 0x8c, 0x27, 0x64, 0x56, 0xcb, 0xc4, 0x21, 0x58, 0x40, 0x7d,
            0xd0, 0x74, 0xee,
        ];
        check0(ALG, KEY, NONCE, AAD, PT, CT);
        decrypt(ALG, KEY, NONCE_BASE, 1, AAD, PT, CT);
    }

    #[test]
    fn quic_chacha() {
        const ALG: AeadId = AeadId::ChaCha20Poly1305;
        const KEY: &[u8] = &[
            0xc6, 0xd9, 0x8f, 0xf3, 0x44, 0x1c, 0x3f, 0xe1, 0xb2, 0x18, 0x20, 0x94, 0xf6, 0x9c,
            0xaa, 0x2e, 0xd4, 0xb7, 0x16, 0xb6, 0x54, 0x88, 0x96, 0x0a, 0x7a, 0x98, 0x49, 0x79,
            0xfb, 0x23, 0xe1, 0xc8,
        ];
        const NONCE_BASE: &[u8; NONCE_LEN] = &[
            0xe0, 0x45, 0x9b, 0x34, 0x74, 0xbd, 0xd0, 0xe4, 0x4a, 0x41, 0xc1, 0x44,
        ];
        // Note that this integrates the sequence number of 654360564 from the example,
        // otherwise we can't use a sequence number of 0 to encrypt.
        const NONCE: &[u8; NONCE_LEN] = &[
            0xe0, 0x45, 0x9b, 0x34, 0x74, 0xbd, 0xd0, 0xe4, 0x6d, 0x41, 0x7e, 0xb0,
        ];
        const AAD: &[u8] = &[0x42, 0x00, 0xbf, 0xf4];
        const PT: &[u8] = &[0x01];
        const CT: &[u8] = &[
            0x65, 0x5e, 0x5c, 0xd5, 0x5c, 0x41, 0xf6, 0x90, 0x80, 0x57, 0x5d, 0x79, 0x99, 0xc2,
            0x5a, 0x5b, 0xfb,
        ];
        check0(ALG, KEY, NONCE, AAD, PT, CT);
        // Now use the real nonce and sequence number from the example.
        decrypt(ALG, KEY, NONCE_BASE, 654_360_564, AAD, PT, CT);
    }
}
