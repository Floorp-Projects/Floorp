// Copyright 2016 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

#[cfg(feature = "nightly")]
use std::sync::atomic::{AtomicUsize, ATOMIC_USIZE_INIT, Ordering};
#[cfg(not(feature = "nightly"))]
use stable::{AtomicUsize, ATOMIC_USIZE_INIT, Ordering};
use std::time::Instant;
use std::ptr;
use std::mem;
use winapi;
use kernel32;

#[allow(non_snake_case)]
struct KeyedEvent {
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

    unsafe fn get() -> &'static KeyedEvent {
        static KEYED_EVENT: AtomicUsize = ATOMIC_USIZE_INIT;

        // Fast path: use the existing object
        let keyed_event = KEYED_EVENT.load(Ordering::Acquire);
        if keyed_event != 0 {
            return &*(keyed_event as *const KeyedEvent);
        };

        // Try to create a new object
        let keyed_event = Box::into_raw(KeyedEvent::create());
        match KEYED_EVENT.compare_exchange(0,
                                           keyed_event as usize,
                                           Ordering::Release,
                                           Ordering::Relaxed) {
            Ok(_) => &*(keyed_event as *const KeyedEvent),
            Err(x) => {
                // We lost the race, free our object and return the global one
                Box::from_raw(keyed_event);
                &*(x as *const KeyedEvent)
            }
        }
    }

    #[allow(non_snake_case)]
    unsafe fn create() -> Box<KeyedEvent> {
        let ntdll = kernel32::GetModuleHandleA(b"ntdll.dll\0".as_ptr() as winapi::LPCSTR);
        if ntdll.is_null() {
            panic!("Could not get module handle for ntdll.dll");
        }

        let NtCreateKeyedEvent =
            kernel32::GetProcAddress(ntdll, b"NtCreateKeyedEvent\0".as_ptr() as winapi::LPCSTR);
        if NtCreateKeyedEvent.is_null() {
            panic!("Entry point NtCreateKeyedEvent not found in ntdll.dll");
        }
        let NtReleaseKeyedEvent =
            kernel32::GetProcAddress(ntdll, b"NtReleaseKeyedEvent\0".as_ptr() as winapi::LPCSTR);
        if NtReleaseKeyedEvent.is_null() {
            panic!("Entry point NtReleaseKeyedEvent not found in ntdll.dll");
        }
        let NtWaitForKeyedEvent =
            kernel32::GetProcAddress(ntdll, b"NtWaitForKeyedEvent\0".as_ptr() as winapi::LPCSTR);
        if NtWaitForKeyedEvent.is_null() {
            panic!("Entry point NtWaitForKeyedEvent not found in ntdll.dll");
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
            panic!("NtCreateKeyedEvent failed: {:x}", status);
        }

        Box::new(KeyedEvent {
            handle: handle,
            NtReleaseKeyedEvent: mem::transmute(NtReleaseKeyedEvent),
            NtWaitForKeyedEvent: mem::transmute(NtWaitForKeyedEvent),
        })
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

const STATE_UNPARKED: usize = 0;
const STATE_PARKED: usize = 1;
const STATE_TIMED_OUT: usize = 2;

// Helper type for putting a thread to sleep until some other thread wakes it up
pub struct ThreadParker {
    key: AtomicUsize,
    keyed_event: &'static KeyedEvent,
}

impl ThreadParker {
    pub fn new() -> ThreadParker {
        // Initialize the keyed event here to ensure we don't get any panics
        // later on, which could leave synchronization primitives in a broken
        // state.
        ThreadParker {
            key: AtomicUsize::new(STATE_UNPARKED),
            keyed_event: unsafe { KeyedEvent::get() },
        }
    }

    // Prepares the parker. This should be called before adding it to the queue.
    pub unsafe fn prepare_park(&self) {
        self.key.store(STATE_PARKED, Ordering::Relaxed);
    }

    // Checks if the park timed out. This should be called while holding the
    // queue lock after park_until has returned false.
    pub unsafe fn timed_out(&self) -> bool {
        self.key.load(Ordering::Relaxed) == STATE_TIMED_OUT
    }

    // Parks the thread until it is unparked. This should be called after it has
    // been added to the queue, after unlocking the queue.
    pub unsafe fn park(&self) {
        let status = self.keyed_event.wait_for(self as *const _ as winapi::PVOID, ptr::null_mut());
        debug_assert_eq!(status, winapi::STATUS_SUCCESS);
    }

    // Parks the thread until it is unparked or the timeout is reached. This
    // should be called after it has been added to the queue, after unlocking
    // the queue. Returns true if we were unparked and false if we timed out.
    pub unsafe fn park_until(&self, timeout: Instant) -> bool {
        let now = Instant::now();
        if timeout <= now {
            // If another thread unparked us, we need to call
            // NtWaitForKeyedEvent otherwise that thread will stay stuck at
            // NtReleaseKeyedEvent.
            if self.key.swap(STATE_TIMED_OUT, Ordering::Relaxed) == STATE_UNPARKED {
                self.park();
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
                self.park();
                return true;
            }
        };

        let status = self.keyed_event.wait_for(self as *const _ as winapi::PVOID, &mut nt_timeout);
        if status == winapi::STATUS_SUCCESS {
            return true;
        }
        debug_assert_eq!(status, winapi::STATUS_TIMEOUT);

        // If another thread unparked us, we need to call NtWaitForKeyedEvent
        // otherwise that thread will stay stuck at NtReleaseKeyedEvent.
        if self.key.swap(STATE_TIMED_OUT, Ordering::Relaxed) == STATE_UNPARKED {
            self.park();
            return true;
        }
        false
    }

    // Locks the parker to prevent the target thread from exiting. This is
    // necessary to ensure that thread-local ThreadData objects remain valid.
    // This should be called while holding the queue lock.
    pub unsafe fn unpark_lock(&self) -> UnparkHandle {
        // If the state was STATE_PARKED then we need to wake up the thread
        if self.key.swap(STATE_UNPARKED, Ordering::Relaxed) == STATE_PARKED {
            UnparkHandle { thread_parker: self }
        } else {
            UnparkHandle { thread_parker: ptr::null() }
        }
    }
}

// Handle for a thread that is about to be unparked. We need to mark the thread
// as unparked while holding the queue lock, but we delay the actual unparking
// until after the queue lock is released.
pub struct UnparkHandle {
    thread_parker: *const ThreadParker,
}

impl UnparkHandle {
    // Wakes up the parked thread. This should be called after the queue lock is
    // released to avoid blocking the queue for too long.
    pub unsafe fn unpark(self) {
        if !self.thread_parker.is_null() {
            let status = (*self.thread_parker)
                .keyed_event
                .release(self.thread_parker as winapi::PVOID);
            debug_assert_eq!(status, winapi::STATUS_SUCCESS);
        }
    }
}
