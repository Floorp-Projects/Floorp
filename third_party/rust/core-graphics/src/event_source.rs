use core_foundation::base::{CFRelease, CFRetain, CFTypeID, CFTypeRef, TCFType};

use std::mem;
use std::ptr;

/// Possible source states of an event source.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub enum CGEventSourceStateID {
    Private = -1,
    CombinedSessionState = 0,
    HIDSystemState = 1,
}

// This is an enum due to zero-sized types warnings.
// For more details see https://github.com/rust-lang/rust/issues/27303
pub enum __CGEventSource {}

pub type CGEventSourceRef = *const __CGEventSource;

pub struct CGEventSource {
    obj: CGEventSourceRef,
}

impl Clone for CGEventSource {
    #[inline]
    fn clone(&self) -> CGEventSource {
        unsafe {
            TCFType::wrap_under_get_rule(self.obj)
        }
    }
}

impl Drop for CGEventSource {
    fn drop(&mut self) {
        unsafe {
            let ptr = self.as_CFTypeRef();
            assert!(ptr != ptr::null());
            CFRelease(ptr);
        }
    }
}

impl TCFType<CGEventSourceRef> for CGEventSource {
    #[inline]
    fn as_concrete_TypeRef(&self) -> CGEventSourceRef {
        self.obj
    }

    #[inline]
    unsafe fn wrap_under_get_rule(reference: CGEventSourceRef) -> CGEventSource {
        let reference: CGEventSourceRef = mem::transmute(CFRetain(mem::transmute(reference)));
        TCFType::wrap_under_create_rule(reference)
    }

    #[inline]
    fn as_CFTypeRef(&self) -> CFTypeRef {
        unsafe {
            mem::transmute(self.as_concrete_TypeRef())
        }
    }

    #[inline]
    unsafe fn wrap_under_create_rule(obj: CGEventSourceRef) -> CGEventSource {
        CGEventSource {
            obj: obj,
        }
    }

    #[inline]
    fn type_id() -> CFTypeID {
        unsafe {
            CGEventSourceGetTypeID()
        }
    }
}

impl CGEventSource {
    pub fn new(state_id: CGEventSourceStateID) -> Result<CGEventSource, ()> {
        unsafe {
            let event_source_ref = CGEventSourceCreate(state_id);
            if event_source_ref != ptr::null() {
                Ok(TCFType::wrap_under_create_rule(event_source_ref))
            } else {
                Err(())
            }
        }
    }
}

#[link(name = "ApplicationServices", kind = "framework")]
extern {
    /// Return the type identifier for the opaque type `CGEventSourceRef'.
    fn CGEventSourceGetTypeID() -> CFTypeID;

    /// Return a Quartz event source created with a specified source state.
    fn CGEventSourceCreate(stateID: CGEventSourceStateID) -> CGEventSourceRef;
}
