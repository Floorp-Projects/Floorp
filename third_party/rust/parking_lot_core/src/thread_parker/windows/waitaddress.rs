// Copyright 2016 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use std::sync::atomic::{AtomicUsize, Ordering};
use std::time::Instant;
use std::mem;

use winapi::shared::basetsd::SIZE_T;
use winapi::shared::minwindef::{BOOL, DWORD, FALSE, TRUE};
use winapi::shared::winerror::ERROR_TIMEOUT;
use winapi::um::errhandlingapi::GetLastError;
use winapi::um::libloaderapi::{GetModuleHandleA, GetProcAddress};
use winapi::um::winbase::INFINITE;
use winapi::um::winnt::{LPCSTR, PVOID};

#[allow(non_snake_case)]
pub struct WaitAddress {
    WaitOnAddress: extern "system" fn(
        Address: PVOID,
        CompareAddress: PVOID,
        AddressSize: SIZE_T,
        dwMilliseconds: DWORD,
    ) -> BOOL,
    WakeByAddressSingle: extern "system" fn(Address: PVOID),
}

impl WaitAddress {
    #[allow(non_snake_case)]
    pub unsafe fn create() -> Option<WaitAddress> {
        // MSDN claims that that WaitOnAddress and WakeByAddressSingle are
        // located in kernel32.dll, but they are lying...
        let synch_dll = GetModuleHandleA(b"api-ms-win-core-synch-l1-2-0.dll\0".as_ptr() as LPCSTR);
        if synch_dll.is_null() {
            return None;
        }

        let WaitOnAddress = GetProcAddress(synch_dll, b"WaitOnAddress\0".as_ptr() as LPCSTR);
        if WaitOnAddress.is_null() {
            return None;
        }
        let WakeByAddressSingle =
            GetProcAddress(synch_dll, b"WakeByAddressSingle\0".as_ptr() as LPCSTR);
        if WakeByAddressSingle.is_null() {
            return None;
        }
        Some(WaitAddress {
            WaitOnAddress: mem::transmute(WaitOnAddress),
            WakeByAddressSingle: mem::transmute(WakeByAddressSingle),
        })
    }

    pub unsafe fn prepare_park(&'static self, key: &AtomicUsize) {
        key.store(1, Ordering::Relaxed);
    }

    pub unsafe fn timed_out(&'static self, key: &AtomicUsize) -> bool {
        key.load(Ordering::Relaxed) != 0
    }

    pub unsafe fn park(&'static self, key: &AtomicUsize) {
        while key.load(Ordering::Acquire) != 0 {
            let cmp = 1usize;
            let r = (self.WaitOnAddress)(
                key as *const _ as PVOID,
                &cmp as *const _ as PVOID,
                mem::size_of::<usize>() as SIZE_T,
                INFINITE,
            );
            debug_assert!(r == TRUE);
        }
    }

    pub unsafe fn park_until(&'static self, key: &AtomicUsize, timeout: Instant) -> bool {
        while key.load(Ordering::Acquire) != 0 {
            let now = Instant::now();
            if timeout <= now {
                return false;
            }
            let diff = timeout - now;
            let timeout = diff.as_secs()
                .checked_mul(1000)
                .and_then(|x| x.checked_add((diff.subsec_nanos() as u64 + 999999) / 1000000))
                .map(|ms| {
                    if ms > <DWORD>::max_value() as u64 {
                        INFINITE
                    } else {
                        ms as DWORD
                    }
                })
                .unwrap_or(INFINITE);
            let cmp = 1usize;
            let r = (self.WaitOnAddress)(
                key as *const _ as PVOID,
                &cmp as *const _ as PVOID,
                mem::size_of::<usize>() as SIZE_T,
                timeout,
            );
            if r == FALSE {
                debug_assert_eq!(GetLastError(), ERROR_TIMEOUT);
            }
        }
        true
    }

    pub unsafe fn unpark_lock(&'static self, key: &AtomicUsize) -> UnparkHandle {
        // We don't need to lock anything, just clear the state
        key.store(0, Ordering::Release);

        UnparkHandle {
            key: key,
            waitaddress: self,
        }
    }
}

// Handle for a thread that is about to be unparked. We need to mark the thread
// as unparked while holding the queue lock, but we delay the actual unparking
// until after the queue lock is released.
pub struct UnparkHandle {
    key: *const AtomicUsize,
    waitaddress: &'static WaitAddress,
}

impl UnparkHandle {
    // Wakes up the parked thread. This should be called after the queue lock is
    // released to avoid blocking the queue for too long.
    pub unsafe fn unpark(self) {
        (self.waitaddress.WakeByAddressSingle)(self.key as PVOID);
    }
}
