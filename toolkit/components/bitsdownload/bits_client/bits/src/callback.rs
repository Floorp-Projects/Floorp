// Licensed under the Apache License, Version 2.0
// <LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your option.
// All files in the project carrying such notice may not be copied, modified, or distributed
// except according to those terms.

use std::panic::{catch_unwind, RefUnwindSafe};
use std::ptr::NonNull;
use std::sync::atomic::{AtomicUsize, Ordering};

use comedy::{com::ComRef, HResult};
use guid_win::Guid;
use winapi::ctypes::c_void;
use winapi::shared::guiddef::REFIID;
use winapi::shared::minwindef::DWORD;
use winapi::shared::ntdef::ULONG;
use winapi::shared::winerror::{E_FAIL, E_NOINTERFACE, HRESULT, NOERROR, S_OK};
use winapi::um::bits::{
    IBackgroundCopyCallback, IBackgroundCopyCallbackVtbl, IBackgroundCopyError, IBackgroundCopyJob,
};
use winapi::um::unknwnbase::{IUnknown, IUnknownVtbl};
use winapi::Interface;

use BitsJob;

/// The type of a notification callback.
///
/// The callbacks must be `Fn()` to be called arbitrarily many times, `RefUnwindSafe` to have a
/// panic unwind safely caught, `Send`, `Sync` and `'static` to run on any thread COM invokes us on
/// any time.
///
/// If the callback returns a non-success `HRESULT`, the notification may pass to other BITS
/// mechanisms such as `IBackgroundCopyJob2::SetNotifyCmdLine`.
pub type TransferredCallback =
    dyn (Fn() -> Result<(), HRESULT>) + RefUnwindSafe + Send + Sync + 'static;
pub type ErrorCallback = dyn (Fn() -> Result<(), HRESULT>) + RefUnwindSafe + Send + Sync + 'static;
pub type ModificationCallback =
    dyn (Fn() -> Result<(), HRESULT>) + RefUnwindSafe + Send + Sync + 'static;

#[repr(C)]
pub struct BackgroundCopyCallback {
    // Everything assumes that the interface vtable is the first member of this struct.
    interface: IBackgroundCopyCallback,
    rc: AtomicUsize,
    transferred_cb: Option<Box<TransferredCallback>>,
    error_cb: Option<Box<ErrorCallback>>,
    modification_cb: Option<Box<ModificationCallback>>,
}

impl BackgroundCopyCallback {
    /// Construct the callback object and register it with a job.
    ///
    /// Only one notify interface can be present on a job at once, so this will release BITS'
    /// ref to any previously registered interface.
    pub fn register(
        job: &mut BitsJob,
        transferred_cb: Option<Box<TransferredCallback>>,
        error_cb: Option<Box<ErrorCallback>>,
        modification_cb: Option<Box<ModificationCallback>>,
    ) -> Result<(), HResult> {
        let cb = Box::new(BackgroundCopyCallback {
            interface: IBackgroundCopyCallback { lpVtbl: &VTBL },
            rc: AtomicUsize::new(1),
            transferred_cb,
            error_cb,
            modification_cb,
        });

        // Leak the callback, it has no Rust owner until we need to drop it later.
        // The ComRef will Release when it goes out of scope.
        unsafe {
            let cb = ComRef::from_raw(NonNull::new_unchecked(Box::into_raw(cb) as *mut IUnknown));

            job.set_notify_interface(cb.as_raw_ptr())?;
        }

        Ok(())
    }
}

extern "system" fn query_interface(
    this: *mut IUnknown,
    riid: REFIID,
    obj: *mut *mut c_void,
) -> HRESULT {
    unsafe {
        // `IBackgroundCopyCallback` is the first (currently only) interface on the
        // `BackgroundCopyCallback` object, so we can return `this` either as
        // `IUnknown` or `IBackgroundCopyCallback`.
        if Guid(*riid) == Guid(IUnknown::uuidof())
            || Guid(*riid) == Guid(IBackgroundCopyCallback::uuidof())
        {
            addref(this);
            // Cast first to `IBackgroundCopyCallback` to be clear which `IUnknown`
            // we are pointing at.
            *obj = this as *mut IBackgroundCopyCallback as *mut c_void;
            NOERROR
        } else {
            E_NOINTERFACE
        }
    }
}

extern "system" fn addref(raw_this: *mut IUnknown) -> ULONG {
    unsafe {
        let this = raw_this as *const BackgroundCopyCallback;

        // Forge a reference for just this statement.
        let old_rc = (*this).rc.fetch_add(1, Ordering::SeqCst);
        (old_rc + 1) as ULONG
    }
}

extern "system" fn release(raw_this: *mut IUnknown) -> ULONG {
    unsafe {
        {
            let this = raw_this as *const BackgroundCopyCallback;

            // Forge a reference for just this statement.
            let old_rc = (*this).rc.fetch_sub(1, Ordering::SeqCst);

            let rc = old_rc - 1;
            if rc > 0 {
                return rc as ULONG;
            }
        }

        // rc will have been 0 for us to get here, and we're out of scope of the reference above,
        // so there should be no references or pointers left (besides `this`).
        // Re-Box and to drop immediately.
        let _ = Box::from_raw(raw_this as *mut BackgroundCopyCallback);

        0
    }
}

extern "system" fn transferred_stub(
    raw_this: *mut IBackgroundCopyCallback,
    _job: *mut IBackgroundCopyJob,
) -> HRESULT {
    unsafe {
        let this = raw_this as *const BackgroundCopyCallback;
        // Forge a reference just for this statement.
        if let Some(ref cb) = (*this).transferred_cb {
            match catch_unwind(|| cb()) {
                Ok(Ok(())) => S_OK,
                Ok(Err(hr)) => hr,
                Err(_) => E_FAIL,
            }
        } else {
            S_OK
        }
    }
}

extern "system" fn error_stub(
    raw_this: *mut IBackgroundCopyCallback,
    _job: *mut IBackgroundCopyJob,
    _error: *mut IBackgroundCopyError,
) -> HRESULT {
    unsafe {
        let this = raw_this as *const BackgroundCopyCallback;
        // Forge a reference just for this statement.
        if let Some(ref cb) = (*this).error_cb {
            match catch_unwind(|| cb()) {
                Ok(Ok(())) => S_OK,
                Ok(Err(hr)) => hr,
                Err(_) => E_FAIL,
            }
        } else {
            S_OK
        }
    }
}

extern "system" fn modification_stub(
    raw_this: *mut IBackgroundCopyCallback,
    _job: *mut IBackgroundCopyJob,
    _reserved: DWORD,
) -> HRESULT {
    unsafe {
        let this = raw_this as *const BackgroundCopyCallback;
        // Forge a reference just for this statement.
        if let Some(ref cb) = (*this).modification_cb {
            match catch_unwind(|| cb()) {
                Ok(Ok(())) => S_OK,
                Ok(Err(hr)) => hr,
                Err(_) => E_FAIL,
            }
        } else {
            S_OK
        }
    }
}

pub static VTBL: IBackgroundCopyCallbackVtbl = IBackgroundCopyCallbackVtbl {
    parent: IUnknownVtbl {
        QueryInterface: query_interface,
        AddRef: addref,
        Release: release,
    },
    JobTransferred: transferred_stub,
    JobError: error_stub,
    JobModification: modification_stub,
};
