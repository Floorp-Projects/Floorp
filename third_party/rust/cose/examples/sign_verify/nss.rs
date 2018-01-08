use std::marker::PhantomData;
use std::{mem, ptr};
use std::os::raw;
use std::os::raw::c_char;
use cose::SignatureAlgorithm;

type SECItemType = raw::c_uint; // TODO: actually an enum - is this the right size?
const SI_BUFFER: SECItemType = 0; // called siBuffer in NSS

#[repr(C)]
struct SECItem {
    typ: SECItemType,
    data: *const u8,
    len: raw::c_uint,
}

impl SECItem {
    fn maybe_new(data: &[u8]) -> Result<SECItem, NSSError> {
        if data.len() > u32::max_value() as usize {
            return Err(NSSError::InputTooLarge);
        }
        Ok(SECItem {
            typ: SI_BUFFER,
            data: data.as_ptr(),
            len: data.len() as u32,
        })
    }

    fn maybe_from_parts(data: *const u8, len: usize) -> Result<SECItem, NSSError> {
        if len > u32::max_value() as usize {
            return Err(NSSError::InputTooLarge);
        }
        Ok(SECItem {
            typ: SI_BUFFER,
            data: data,
            len: len as u32,
        })
    }
}

/// Many NSS APIs take constant data input as SECItems. Some, however, output data as SECItems.
/// To represent this, we define another type of mutable SECItem.
#[repr(C)]
struct SECItemMut<'a> {
    typ: SECItemType,
    data: *mut u8,
    len: raw::c_uint,
    _marker: PhantomData<&'a mut Vec<u8>>,
}

impl<'a> SECItemMut<'a> {
    /// Given a mutable reference to a Vec<u8> that has a particular allocated capacity, create a
    /// SECItemMut that points to the vec and has the same capacity.
    /// The input vec is not expected to have any actual contents, and in any case is cleared.
    fn maybe_from_empty_preallocated_vec(vec: &'a mut Vec<u8>) -> Result<SECItemMut<'a>, NSSError> {
        if vec.capacity() > u32::max_value() as usize {
            return Err(NSSError::InputTooLarge);
        }
        vec.clear();
        Ok(SECItemMut {
            typ: SI_BUFFER,
            data: vec.as_mut_ptr(),
            len: vec.capacity() as u32,
            _marker: PhantomData,
        })
    }
}

#[repr(C)]
struct CkRsaPkcsPssParams {
    // Called CK_RSA_PKCS_PSS_PARAMS in NSS
    hash_alg: CkMechanismType, // Called hashAlg in NSS
    mgf: CkRsaPkcsMgfType,
    s_len: raw::c_ulong, // Called sLen in NSS
}

impl CkRsaPkcsPssParams {
    fn new() -> CkRsaPkcsPssParams {
        CkRsaPkcsPssParams {
            hash_alg: CKM_SHA256,
            mgf: CKG_MGF1_SHA256,
            s_len: 32,
        }
    }

    fn get_params_item(&self) -> Result<SECItem, NSSError> {
        // This isn't entirely NSS' fault, but it mostly is.
        let params_ptr: *const CkRsaPkcsPssParams = self;
        let params_ptr: *const u8 = params_ptr as *const u8;
        let params_secitem =
            SECItem::maybe_from_parts(params_ptr, mem::size_of::<CkRsaPkcsPssParams>())?;
        Ok(params_secitem)
    }
}

type CkMechanismType = raw::c_ulong; // called CK_MECHANISM_TYPE in NSS
const CKM_ECDSA: CkMechanismType = 0x0000_1041;
const CKM_RSA_PKCS_PSS: CkMechanismType = 0x0000_000D;
const CKM_SHA256: CkMechanismType = 0x0000_0250;

type CkRsaPkcsMgfType = raw::c_ulong; // called CK_RSA_PKCS_MGF_TYPE in NSS
const CKG_MGF1_SHA256: CkRsaPkcsMgfType = 0x0000_0002;

type SECStatus = raw::c_int; // TODO: enum - right size?
const SEC_SUCCESS: SECStatus = 0; // Called SECSuccess in NSS
const SEC_FAILURE: SECStatus = -1; // Called SECFailure in NSS

enum SECKEYPublicKey {}
enum SECKEYPrivateKey {}
enum PK11SlotInfo {}
enum CERTCertificate {}
enum CERTCertDBHandle {}

const SHA256_LENGTH: usize = 32;
const SHA384_LENGTH: usize = 48;
const SHA512_LENGTH: usize = 64;

// TODO: ugh this will probably have a platform-specific name...
#[link(name = "nss3")]
extern "C" {
    fn PK11_HashBuf(
        hashAlg: HashAlgorithm,
        out: *mut u8,
        data_in: *const u8, // called "in" in NSS
        len: raw::c_int,
    ) -> SECStatus;
    fn PK11_VerifyWithMechanism(
        key: *const SECKEYPublicKey,
        mechanism: CkMechanismType,
        param: *const SECItem,
        sig: *const SECItem,
        hash: *const SECItem,
        wincx: *const raw::c_void,
    ) -> SECStatus;

    fn SECKEY_DestroyPublicKey(pubk: *const SECKEYPublicKey);

    fn CERT_GetDefaultCertDB() -> *const CERTCertDBHandle;
    fn CERT_DestroyCertificate(cert: *mut CERTCertificate);
    fn CERT_NewTempCertificate(
        handle: *const CERTCertDBHandle,
        derCert: *const SECItem,
        nickname: *const c_char,
        isperm: bool,
        copyDER: bool,
    ) -> *mut CERTCertificate;
    fn CERT_ExtractPublicKey(cert: *const CERTCertificate) -> *const SECKEYPublicKey;

    fn PK11_ImportDERPrivateKeyInfoAndReturnKey(
        slot: *mut PK11SlotInfo,
        derPKI: *const SECItem,
        nickname: *const SECItem,
        publicValue: *const SECItem,
        isPerm: bool,
        isPrivate: bool,
        keyUsage: u32,
        privk: *mut *mut SECKEYPrivateKey,
        wincx: *const u8,
    ) -> SECStatus;
    fn PK11_GetInternalSlot() -> *mut PK11SlotInfo;
    fn PK11_FreeSlot(slot: *mut PK11SlotInfo);
    fn PK11_SignatureLen(key: *const SECKEYPrivateKey) -> usize;
    fn PK11_SignWithMechanism(
        key: *const SECKEYPrivateKey,
        mech: CkMechanismType,
        param: *const SECItem,
        sig: *mut SECItemMut,
        hash: *const SECItem,
    ) -> SECStatus;
}

/// An error type describing errors that may be encountered during verification.
#[derive(Debug, PartialEq)]
pub enum NSSError {
    ImportCertError,
    DecodingPKCS8Failed,
    InputTooLarge,
    LibraryFailure,
    SignatureVerificationFailed,
    SigningFailed,
    ExtractPublicKeyFailed,
}

// https://searchfox.org/nss/rev/990c2e793aa731cd66238c6c4f00b9473943bc66/lib/util/secoidt.h#274
#[derive(Debug, PartialEq, Clone)]
#[repr(C)]
enum HashAlgorithm {
    SHA256 = 191,
    SHA384 = 192,
    SHA512 = 193,
}

fn hash(payload: &[u8], signature_algorithm: &SignatureAlgorithm) -> Result<Vec<u8>, NSSError> {
    if payload.len() > raw::c_int::max_value() as usize {
        return Err(NSSError::InputTooLarge);
    }
    let (hash_algorithm, digest_length) = match *signature_algorithm {
        SignatureAlgorithm::ES256 => (HashAlgorithm::SHA256, SHA256_LENGTH),
        SignatureAlgorithm::ES384 => (HashAlgorithm::SHA384, SHA384_LENGTH),
        SignatureAlgorithm::ES512 => (HashAlgorithm::SHA512, SHA512_LENGTH),
        SignatureAlgorithm::PS256 => (HashAlgorithm::SHA256, SHA256_LENGTH),
    };
    let mut hash_buf = vec![0; digest_length];
    let len: raw::c_int = payload.len() as raw::c_int;
    let hash_result =
        unsafe { PK11_HashBuf(hash_algorithm, hash_buf.as_mut_ptr(), payload.as_ptr(), len) };
    if hash_result != SEC_SUCCESS {
        return Err(NSSError::LibraryFailure);
    }
    Ok(hash_buf)
}

/// Main entrypoint for verification. Given a signature algorithm, the bytes of a subject public key
/// info, a payload, and a signature over the payload, returns a result based on the outcome of
/// decoding the subject public key info and running the signature verification algorithm on the
/// signed data.
pub fn verify_signature(
    signature_algorithm: &SignatureAlgorithm,
    cert: &[u8],
    payload: &[u8],
    signature: &[u8],
) -> Result<(), NSSError> {
    let slot = unsafe { PK11_GetInternalSlot() };
    if slot.is_null() {
        return Err(NSSError::LibraryFailure);
    }
    defer!(unsafe {
        PK11_FreeSlot(slot);
    });

    let hash_buf = hash(payload, signature_algorithm).unwrap();
    let hash_item = SECItem::maybe_new(hash_buf.as_slice())?;

    // Import DER cert into NSS.
    let der_cert = SECItem::maybe_new(cert)?;
    let db_handle = unsafe { CERT_GetDefaultCertDB() };
    if db_handle.is_null() {
        // TODO #28
        return Err(NSSError::LibraryFailure);
    }
    let nss_cert =
        unsafe { CERT_NewTempCertificate(db_handle, &der_cert, ptr::null(), false, true) };
    if nss_cert.is_null() {
        return Err(NSSError::ImportCertError);
    }
    defer!(unsafe {
        CERT_DestroyCertificate(nss_cert);
    });

    let key = unsafe { CERT_ExtractPublicKey(nss_cert) };
    if key.is_null() {
        return Err(NSSError::ExtractPublicKeyFailed);
    }
    defer!(unsafe {
        SECKEY_DestroyPublicKey(key);
    });
    let signature_item = SECItem::maybe_new(signature)?;
    let mechanism = match *signature_algorithm {
        SignatureAlgorithm::ES256 => CKM_ECDSA,
        SignatureAlgorithm::ES384 => CKM_ECDSA,
        SignatureAlgorithm::ES512 => CKM_ECDSA,
        SignatureAlgorithm::PS256 => CKM_RSA_PKCS_PSS,
    };
    let rsa_pss_params = CkRsaPkcsPssParams::new();
    let rsa_pss_params_item = rsa_pss_params.get_params_item()?;
    let params_item = match *signature_algorithm {
        SignatureAlgorithm::ES256 => ptr::null(),
        SignatureAlgorithm::ES384 => ptr::null(),
        SignatureAlgorithm::ES512 => ptr::null(),
        SignatureAlgorithm::PS256 => &rsa_pss_params_item,
    };
    let null_cx_ptr: *const raw::c_void = ptr::null();
    let result = unsafe {
        PK11_VerifyWithMechanism(
            key,
            mechanism,
            params_item,
            &signature_item,
            &hash_item,
            null_cx_ptr,
        )
    };
    match result {
        SEC_SUCCESS => Ok(()),
        SEC_FAILURE => Err(NSSError::SignatureVerificationFailed),
        _ => Err(NSSError::LibraryFailure),
    }
}

pub fn sign(
    signature_algorithm: &SignatureAlgorithm,
    pk8: &[u8],
    payload: &[u8],
) -> Result<Vec<u8>, NSSError> {
    let slot = unsafe { PK11_GetInternalSlot() };
    if slot.is_null() {
        return Err(NSSError::LibraryFailure);
    }
    defer!(unsafe {
        PK11_FreeSlot(slot);
    });
    let pkcs8item = SECItem::maybe_new(pk8)?;
    let mut key: *mut SECKEYPrivateKey = ptr::null_mut();
    let ku_all = 0xFF;
    let rv = unsafe {
        PK11_ImportDERPrivateKeyInfoAndReturnKey(
            slot,
            &pkcs8item,
            ptr::null(),
            ptr::null(),
            false,
            false,
            ku_all,
            &mut key,
            ptr::null(),
        )
    };
    if rv != SEC_SUCCESS || key.is_null() {
        return Err(NSSError::DecodingPKCS8Failed);
    }
    let mechanism = match *signature_algorithm {
        SignatureAlgorithm::ES256 => CKM_ECDSA,
        SignatureAlgorithm::ES384 => CKM_ECDSA,
        SignatureAlgorithm::ES512 => CKM_ECDSA,
        SignatureAlgorithm::PS256 => CKM_RSA_PKCS_PSS,
    };
    let rsa_pss_params = CkRsaPkcsPssParams::new();
    let rsa_pss_params_item = rsa_pss_params.get_params_item()?;
    let params_item = match *signature_algorithm {
        SignatureAlgorithm::ES256 => ptr::null(),
        SignatureAlgorithm::ES384 => ptr::null(),
        SignatureAlgorithm::ES512 => ptr::null(),
        SignatureAlgorithm::PS256 => &rsa_pss_params_item,
    };
    let signature_len = unsafe { PK11_SignatureLen(key) };
    // Allocate enough space for the signature.
    let mut signature: Vec<u8> = Vec::with_capacity(signature_len);
    let hash_buf = hash(payload, signature_algorithm).unwrap();
    let hash_item = SECItem::maybe_new(hash_buf.as_slice())?;
    {
        // Get a mutable SECItem on the preallocated signature buffer. PK11_SignWithMechanism will
        // fill the SECItem's buf with the bytes of the signature.
        let mut signature_item = SECItemMut::maybe_from_empty_preallocated_vec(&mut signature)?;
        let rv = unsafe {
            PK11_SignWithMechanism(key, mechanism, params_item, &mut signature_item, &hash_item)
        };
        if rv != SEC_SUCCESS || signature_item.len as usize != signature_len {
            return Err(NSSError::SigningFailed);
        }
    }
    unsafe {
        // Now that the bytes of the signature have been filled out, set its length.
        signature.set_len(signature_len);
    }
    Ok(signature)
}
