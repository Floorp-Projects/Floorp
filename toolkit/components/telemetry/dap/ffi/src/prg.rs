/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use std::ffi::c_void;

use prio::vdaf::prg::{Prg, SeedStream};

extern "C" {
    pub fn dapStartCmac(aSeed: *mut u8) -> *mut c_void;
    pub fn dapUpdateCmac(aContext: *mut c_void, aData: *const u8, aDataLen: u32);
    pub fn dapFinalizeCmac(aContext: *mut c_void, aMacOutput: *mut u8);
    pub fn dapReleaseCmac(aContext: *mut c_void);

    pub fn dapStartAesCtr(aKey: *const u8) -> *mut c_void;
    pub fn dapCtrFillBuffer(aContext: *mut c_void, aBuffer: *mut u8, aBufferSize: i32);
    pub fn dapReleaseCtrCtx(aContext: *mut c_void);
}

#[derive(Clone, Debug)]
pub struct PrgAes128Alt {
    nss_context: *mut c_void,
}

impl Prg<16> for PrgAes128Alt {
    type SeedStream = SeedStreamAes128Alt;

    fn init(seed_bytes: &[u8; 16]) -> Self {
        let mut my_seed_bytes = *seed_bytes;
        let ctx = unsafe { dapStartCmac(my_seed_bytes.as_mut_ptr()) };
        assert!(!ctx.is_null());

        Self { nss_context: ctx }
    }

    fn update(&mut self, data: &[u8]) {
        unsafe {
            dapUpdateCmac(
                self.nss_context,
                data.as_ptr(),
                u32::try_from(data.len()).unwrap(),
            );
        }
    }

    fn into_seed_stream(self) -> Self::SeedStream {
        // finish the MAC and create a new random data stream using the result as key and 0 as IV for AES-CTR
        let mut key = [0u8; 16];
        unsafe {
            dapFinalizeCmac(self.nss_context, key.as_mut_ptr());
        }

        SeedStreamAes128Alt::new(&mut key, &[0; 16])
    }
}

impl Drop for PrgAes128Alt {
    fn drop(&mut self) {
        unsafe {
            dapReleaseCmac(self.nss_context);
        }
    }
}

pub struct SeedStreamAes128Alt {
    nss_context: *mut c_void,
}

impl SeedStreamAes128Alt {
    pub(crate) fn new(key: &mut [u8; 16], iv: &[u8; 16]) -> Self {
        debug_assert_eq!(iv, &[0; 16]);
        let ctx = unsafe { dapStartAesCtr(key.as_ptr()) };
        Self { nss_context: ctx }
    }
}

impl SeedStream for SeedStreamAes128Alt {
    fn fill(&mut self, buf: &mut [u8]) {
        unsafe {
            dapCtrFillBuffer(
                self.nss_context,
                buf.as_mut_ptr(),
                i32::try_from(buf.len()).unwrap(),
            );
        }
    }
}

impl Drop for SeedStreamAes128Alt {
    fn drop(&mut self) {
        unsafe { dapReleaseCtrCtx(self.nss_context) };
    }
}
