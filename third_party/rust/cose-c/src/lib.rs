/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate cose;

use std::slice;
use cose::decoder::decode_signature;
use cose::SignatureAlgorithm;
use std::os::raw;

unsafe fn from_raw(ptr: *const u8, len: usize) -> Vec<u8> {
    slice::from_raw_parts(ptr, len).to_vec()
}

type VerifyCallback = extern "C" fn(*const u8, /* payload */
                                    usize, /* payload len */
                                    *const *const u8, /* cert_chain */
                                    usize, /* # certs */
                                    *const usize, /* cert lengths in cert_chain */
                                    *const u8, /* signer cert */
                                    usize, /* signer cert len */
                                    *const u8, /* signature bytes */
                                    usize, /* signature len */
                                    u8, /* signature algorithm */
                                    *const raw::c_void /* some context of the app */)
                                    -> bool;

#[no_mangle]
pub extern "C" fn verify_cose_signature_ffi(
    payload: *const u8,
    payload_len: usize,
    cose_signature: *const u8,
    cose_signature_len: usize,
    ctx: *const raw::c_void,
    verify_callback: VerifyCallback,
) -> bool {
    if payload.is_null() || cose_signature.is_null() || payload_len == 0 ||
        cose_signature_len == 0
    {
        return false;
    }

    // Build Rust variables from C parameters.
    let payload = unsafe { from_raw(payload, payload_len) };
    let cose_signature = unsafe { from_raw(cose_signature, cose_signature_len) };

    // Parse the incoming signature.
    let cose_signatures = decode_signature(&cose_signature, &payload);
    let cose_signatures = match cose_signatures {
        Ok(signatures) => signatures,
        Err(_) => Vec::new(),
    };
    if cose_signatures.len() == 0 {
        return false;
    }

    return cose_signatures.into_iter().all(|cose_signature| {
        let signature_type = cose_signature.signature_type;
        // ES256 = 0, ES384 = 1, ES512 = 2, PS256 = 3
        let signature_type = match signature_type {
            SignatureAlgorithm::ES256 => 0,
            SignatureAlgorithm::ES384 => 1,
            SignatureAlgorithm::ES512 => 2,
            SignatureAlgorithm::PS256 => 3,
        };
        let signature_bytes = cose_signature.signature;
        let real_payload = cose_signature.to_verify;

        // Build cert chain params.
        let certs: Vec<_> = cose_signature.certs.iter().map(|c| c.as_ptr()).collect();
        let cert_lens: Vec<_> = cose_signature.certs.iter().map(|c| c.len()).collect();

        // Call callback to verify the parsed signatures.
        verify_callback(
            real_payload.as_ptr(),
            real_payload.len(),
            certs.as_ptr(),
            certs.len(),
            cert_lens.as_ptr(),
            cose_signature.signer_cert.as_ptr(),
            cose_signature.signer_cert.len(),
            signature_bytes.as_ptr(),
            signature_bytes.len(),
            signature_type,
            ctx,
        )
    });
}
