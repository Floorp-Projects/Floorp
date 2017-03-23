// Copyright 2016 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

#[cfg(feature = "nightly")]
use std::sync::atomic::{AtomicUsize, Ordering};
#[cfg(not(feature = "nightly"))]
use stable::{AtomicUsize, Ordering};
use std::time::Instant;
use std::mem;
use winapi;
use kernel32;

#[allow(non_snake_case)]
pub struct WaitAddress {
    WaitOnAddress: extern "system" fn(Address: winapi::PVOID,
                                      CompareAddress: winapi::PVOID,
                                      AddressSize: winapi::SIZE_T,
                                      dwMilliseconds: winapi::DWORD)
                                      -> winapi::BOOL,
    WakeByAddressSingle: extern "system" fn(Address: winapi::PVOID),
}

impl WaitAddress {
    #[allow(non_snake_case)]
    pub unsafe fn create() -> Option<WaitAddress> {
        // MSDN claims that that WaitOnAddress and WakeByAddressSingle are
        // located in kernel32.dll, but they are lying...
        let synch_dll = kernel32::GetModuleHandleA(b"api-ms-win-core-synch-l1-2-0.dll\0"
            .as_ptr() as winapi::LPCSTR);
        if synch_dll.is_null() {
            return None;
        }

        let WaitOnAddress = kernel32::GetProcAddress(synch_dll,
                                                     b"WaitOnAddress\0".as_ptr() as winapi::LPCSTR);
        if WaitOnAddress.is_null() {
            return None;
        }
        let WakeByAddressSingle = kernel32::GetProcAddress(synch_dll,
                                                           b"WakeByAddressSingle\0".as_ptr() as
                                                           winapi::LPCSTR);
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
            let r = (self.WaitOnAddress)(key as *const _ as winapi::PVOID,
                                         &cmp as *const _ as winapi::PVOID,
                                         mem::size_of::<usize>() as winapi::SIZE_T,
                                         winapi::INFINITE);
            debug_assert!(r == winapi::TRUE);
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
                .map(|ms| if ms > <winapi::DWORD>::max_value() as u64 {
                    winapi::INFINITE
                } else {
                    ms as winapi::DWORD
                })
                .unwrap_or(winapi::INFINITE);
            let cmp = 1usize;
            let r = (self.WaitOnAddress)(key as *const _ as winapi::PVOID,
                                         &cmp as *const _ as winapi::PVOID,
                                         mem::size_of::<usize>() as winapi::SIZE_T,
                                         timeout);
            if r == winapi::FALSE {
                debug_assert_eq!(kernel32::GetLastError(), winapi::ERROR_TIMEOUT);
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
        (self.waitaddress.WakeByAddressSingle)(self.key as winapi::PVOID);
    }
}
