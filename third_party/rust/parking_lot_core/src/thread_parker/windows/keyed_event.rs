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
use std::ptr;
use std::mem;
use winapi;
use kernel32;

const STATE_UNPARKED: usize = 0;
const STATE_PARKED: usize = 1;
const STATE_TIMED_OUT: usize = 2;

#[allow(non_snake_case)]
pub struct KeyedEvent {
    handle: winapi::HANDLE,
    NtReleaseKeyedEvent: extern "system" fn(EventHandle: winapi::HANDLE,
                                            Key: winapi::PVOID,
                                            Alertable: winapi::BOOLEAN,
                                            Timeout: winapi::PLARGE_INTEGER)
                                            -> winapi::NTSTATUS,
    NtWaitForKeyedEvent: extern "system" fn(EventHandle: winapi::HANDLE,
                                            Key: winapi::PVOID,
                                            Alertable: winapi::BOOLEAN,
                                            Timeout: winapi::PLARGE_INTEGER)
                                            -> winapi::NTSTATUS,
}

impl KeyedEvent {
    unsafe fn wait_for(&self,
                       key: winapi::PVOID,
                       timeout: winapi::PLARGE_INTEGER)
                       -> winapi::NTSTATUS {
        (self.NtWaitForKeyedEvent)(self.handle, key, 0, timeout)
    }

    unsafe fn release(&self, key: winapi::PVOID) -> winapi::NTSTATUS {
        (self.NtReleaseKeyedEvent)(self.handle, key, 0, ptr::null_mut())
    }

    #[allow(non_snake_case)]
    pub unsafe fn create() -> Option<KeyedEvent> {
        let ntdll = kernel32::GetModuleHandleA(b"ntdll.dll\0".as_ptr() as winapi::LPCSTR);
        if ntdll.is_null() {
            return None;
        }

        let NtCreateKeyedEvent =
            kernel32::GetProcAddress(ntdll, b"NtCreateKeyedEvent\0".as_ptr() as winapi::LPCSTR);
        if NtCreateKeyedEvent.is_null() {
            return None;
        }
        let NtReleaseKeyedEvent =
            kernel32::GetProcAddress(ntdll, b"NtReleaseKeyedEvent\0".as_ptr() as winapi::LPCSTR);
        if NtReleaseKeyedEvent.is_null() {
            return None;
        }
        let NtWaitForKeyedEvent =
            kernel32::GetProcAddress(ntdll, b"NtWaitForKeyedEvent\0".as_ptr() as winapi::LPCSTR);
        if NtWaitForKeyedEvent.is_null() {
            return None;
        }

        let NtCreateKeyedEvent: extern "system" fn(KeyedEventHandle: winapi::PHANDLE,
                                                   DesiredAccess: winapi::ACCESS_MASK,
                                                   ObjectAttributes: winapi::PVOID,
                                                   Flags: winapi::ULONG)
                                                   -> winapi::NTSTATUS =
            mem::transmute(NtCreateKeyedEvent);
        let mut handle = mem::uninitialized();
        let status = NtCreateKeyedEvent(&mut handle,
                                        winapi::GENERIC_READ | winapi::GENERIC_WRITE,
                                        ptr::null_mut(),
                                        0);
        if status != winapi::STATUS_SUCCESS {
            return None;
        }

        Some(KeyedEvent {
            handle: handle,
            NtReleaseKeyedEvent: mem::transmute(NtReleaseKeyedEvent),
            NtWaitForKeyedEvent: mem::transmute(NtWaitForKeyedEvent),
        })
    }

    pub unsafe fn prepare_park(&'static self, key: &AtomicUsize) {
        key.store(STATE_PARKED, Ordering::Relaxed);
    }

    pub unsafe fn timed_out(&'static self, key: &AtomicUsize) -> bool {
        key.load(Ordering::Relaxed) == STATE_TIMED_OUT
    }

    pub unsafe fn park(&'static self, key: &AtomicUsize) {
        let status = self.wait_for(key as *const _ as winapi::PVOID, ptr::null_mut());
        debug_assert_eq!(status, winapi::STATUS_SUCCESS);
    }

    pub unsafe fn park_until(&'static self, key: &AtomicUsize, timeout: Instant) -> bool {
        let now = Instant::now();
        if timeout <= now {
            // If another thread unparked us, we need to call
            // NtWaitForKeyedEvent otherwise that thread will stay stuck at
            // NtReleaseKeyedEvent.
            if key.swap(STATE_TIMED_OUT, Ordering::Relaxed) == STATE_UNPARKED {
                self.park(key);
                return true;
            }
            return false;
        }

        // NT uses a timeout in units of 100ns. We use a negative value to
        // indicate a relative timeout based on a monotonic clock.
        let diff = timeout - now;
        let nt_timeout = (diff.as_secs() as winapi::LARGE_INTEGER)
            .checked_mul(-10000000)
            .and_then(|x| x.checked_sub((diff.subsec_nanos() as winapi::LARGE_INTEGER + 99) / 100));
        let mut nt_timeout = match nt_timeout {
            Some(x) => x,
            None => {
                // Timeout overflowed, just sleep indefinitely
                self.park(key);
                return true;
            }
        };

        let status = self.wait_for(key as *const _ as winapi::PVOID, &mut nt_timeout);
        if status == winapi::STATUS_SUCCESS {
            return true;
        }
        debug_assert_eq!(status, winapi::STATUS_TIMEOUT);

        // If another thread unparked us, we need to call NtWaitForKeyedEvent
        // otherwise that thread will stay stuck at NtReleaseKeyedEvent.
        if key.swap(STATE_TIMED_OUT, Ordering::Relaxed) == STATE_UNPARKED {
            self.park(key);
            return true;
        }
        false
    }

    pub unsafe fn unpark_lock(&'static self, key: &AtomicUsize) -> UnparkHandle {
        // If the state was STATE_PARKED then we need to wake up the thread
        if key.swap(STATE_UNPARKED, Ordering::Relaxed) == STATE_PARKED {
            UnparkHandle {
                key: key,
                keyed_event: self,
            }
        } else {
            UnparkHandle {
                key: ptr::null(),
                keyed_event: self,
            }
        }
    }
}

impl Drop for KeyedEvent {
    fn drop(&mut self) {
        unsafe {
            let ok = kernel32::CloseHandle(self.handle);
            debug_assert_eq!(ok, winapi::TRUE);
        }
    }
}

// Handle for a thread that is about to be unparked. We need to mark the thread
// as unparked while holding the queue lock, but we delay the actual unparking
// until after the queue lock is released.
pub struct UnparkHandle {
    key: *const AtomicUsize,
    keyed_event: &'static KeyedEvent,
}

impl UnparkHandle {
    // Wakes up the parked thread. This should be called after the queue lock is
    // released to avoid blocking the queue for too long.
    pub unsafe fn unpark(self) {
        if !self.key.is_null() {
            let status = self.keyed_event.release(self.key as winapi::PVOID);
            debug_assert_eq!(status, winapi::STATUS_SUCCESS);
        }
    }
}
