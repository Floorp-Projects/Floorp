/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate base64;
extern crate digest;
extern crate libc;
extern crate md5;
extern crate nsstring;
extern crate sha1;
extern crate sha2;
#[macro_use]
extern crate xpcom;

use base64::Engine;
use digest::{Digest, DynDigest};
use nserror::{
    nsresult, NS_ERROR_FAILURE, NS_ERROR_INVALID_ARG, NS_ERROR_NOT_AVAILABLE,
    NS_ERROR_NOT_INITIALIZED, NS_OK,
};
use nsstring::{nsACString, nsCString};
use xpcom::interfaces::{nsICryptoHash, nsIInputStream};
use xpcom::xpcom_method;

use std::borrow::Borrow;
use std::sync::Mutex;

enum Algorithm {
    Md5,
    Sha1,
    Sha256,
    Sha384,
    Sha512,
}

impl TryFrom<u32> for Algorithm {
    type Error = nsresult;

    fn try_from(value: u32) -> Result<Self, Self::Error> {
        match value {
            nsICryptoHash::MD5 => Ok(Algorithm::Md5),
            nsICryptoHash::SHA1 => Ok(Algorithm::Sha1),
            nsICryptoHash::SHA256 => Ok(Algorithm::Sha256),
            nsICryptoHash::SHA384 => Ok(Algorithm::Sha384),
            nsICryptoHash::SHA512 => Ok(Algorithm::Sha512),
            _ => Err(NS_ERROR_INVALID_ARG),
        }
    }
}

impl TryFrom<&nsACString> for Algorithm {
    type Error = nsresult;

    fn try_from(value: &nsACString) -> Result<Self, Self::Error> {
        match value.to_utf8().borrow() {
            "md5" => Ok(Algorithm::Md5),
            "sha1" => Ok(Algorithm::Sha1),
            "sha256" => Ok(Algorithm::Sha256),
            "sha384" => Ok(Algorithm::Sha384),
            "sha512" => Ok(Algorithm::Sha512),
            _ => Err(NS_ERROR_INVALID_ARG),
        }
    }
}

#[xpcom(implement(nsICryptoHash), atomic)]
struct CryptoHash {
    digest: Mutex<Option<Box<dyn DynDigest>>>,
}

impl CryptoHash {
    xpcom_method!(init => Init(algorithm: u32));
    fn init(&self, algorithm: u32) -> Result<(), nsresult> {
        let algorithm = algorithm.try_into()?;
        self.init_with_algorithm(algorithm)
    }

    xpcom_method!(init_with_string => InitWithString(algorithm: *const nsACString));
    fn init_with_string(&self, algorithm: &nsACString) -> Result<(), nsresult> {
        let algorithm = algorithm.try_into()?;
        self.init_with_algorithm(algorithm)
    }

    fn init_with_algorithm(&self, algorithm: Algorithm) -> Result<(), nsresult> {
        let digest = match algorithm {
            Algorithm::Md5 => Box::new(md5::Md5::new()) as Box<dyn DynDigest>,
            Algorithm::Sha1 => Box::new(sha1::Sha1::new()) as Box<dyn DynDigest>,
            Algorithm::Sha256 => Box::new(sha2::Sha256::new()) as Box<dyn DynDigest>,
            Algorithm::Sha384 => Box::new(sha2::Sha384::new()) as Box<dyn DynDigest>,
            Algorithm::Sha512 => Box::new(sha2::Sha512::new()) as Box<dyn DynDigest>,
        };
        let mut guard = self.digest.lock().map_err(|_| NS_ERROR_FAILURE)?;
        if let Some(_expected_none_digest) = (*guard).replace(digest) {
            return Err(NS_ERROR_FAILURE);
        }
        Ok(())
    }

    xpcom_method!(update => Update(data: *const u8, len: u32));
    fn update(&self, data: *const u8, len: u32) -> Result<(), nsresult> {
        let mut guard = self.digest.lock().map_err(|_| NS_ERROR_FAILURE)?;
        let digest = match (*guard).as_mut() {
            Some(digest) => digest,
            None => return Err(NS_ERROR_NOT_INITIALIZED),
        };
        // Safety: this is safe as long as xpcom gave us valid arguments.
        let data = unsafe {
            std::slice::from_raw_parts(data, len.try_into().map_err(|_| NS_ERROR_INVALID_ARG)?)
        };
        digest.update(data);
        Ok(())
    }

    xpcom_method!(update_from_stream => UpdateFromStream(stream: *const nsIInputStream, len: u32));
    fn update_from_stream(&self, stream: &nsIInputStream, len: u32) -> Result<(), nsresult> {
        let mut guard = self.digest.lock().map_err(|_| NS_ERROR_FAILURE)?;
        let digest = match (*guard).as_mut() {
            Some(digest) => digest,
            None => return Err(NS_ERROR_NOT_INITIALIZED),
        };
        let mut available = 0u64;
        unsafe { stream.Available(&mut available as *mut u64).to_result()? };
        let mut to_read = if len == u32::MAX { available } else { len as u64 };
        if available == 0 || available < to_read {
            return Err(NS_ERROR_NOT_AVAILABLE);
        }
        let mut buf = vec![0u8; 4096];
        let buf_len = buf.len() as u64;
        while to_read > 0 {
            let chunk_len = if to_read >= buf_len { buf_len as u32 } else { to_read as u32 };
            let mut read = 0u32;
            unsafe {
                stream
                    .Read(
                        buf.as_mut_ptr() as *mut libc::c_char,
                        chunk_len,
                        &mut read as *mut u32,
                    )
                    .to_result()?
            };
            if read > chunk_len {
                return Err(NS_ERROR_FAILURE);
            }
            digest.update(&buf[0..read.try_into().map_err(|_| NS_ERROR_FAILURE)?]);
            to_read -= read as u64;
        }
        Ok(())
    }

    xpcom_method!(finish => Finish(ascii: bool) -> nsACString);
    fn finish(&self, ascii: bool) -> Result<nsCString, nsresult> {
        let mut guard = self.digest.lock().map_err(|_| NS_ERROR_FAILURE)?;
        let digest = match (*guard).take() {
            Some(digest) => digest,
            None => return Err(NS_ERROR_NOT_INITIALIZED),
        };
        let result = digest.finalize();
        if ascii {
            Ok(nsCString::from(
                base64::engine::general_purpose::STANDARD.encode(result),
            ))
        } else {
            Ok(nsCString::from(result))
        }
    }
}

#[no_mangle]
pub extern "C" fn crypto_hash_constructor(
    iid: *const xpcom::nsIID,
    result: *mut *mut xpcom::reexports::libc::c_void,
) -> nserror::nsresult {
    let crypto_hash = CryptoHash::allocate(InitCryptoHash {
        digest: Mutex::new(None),
    });
    unsafe { crypto_hash.QueryInterface(iid, result) }
}
