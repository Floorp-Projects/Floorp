use core_foundation::base::{CFRelease, CFRetain, CFTypeID};
use foreign_types::ForeignType;

/// Possible source states of an event source.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub enum CGEventSourceStateID {
    Private = -1,
    CombinedSessionState = 0,
    HIDSystemState = 1,
}

foreign_type! {
    #[doc(hidden)]
    type CType = ::sys::CGEventSource;
    fn drop = |p| CFRelease(p as *mut _);
    fn clone = |p| CFRetain(p as *const _) as *mut _;
    pub struct CGEventSource;
    pub struct CGEventSourceRef;
}

impl CGEventSource {
    pub fn type_id() -> CFTypeID {
        unsafe {
            CGEventSourceGetTypeID()
        }
    }

    pub fn new(state_id: CGEventSourceStateID) -> Result<Self, ()> {
        unsafe {
            let event_source_ref = CGEventSourceCreate(state_id);
            if !event_source_ref.is_null() {
                Ok(Self::from_ptr(event_source_ref))
            } else {
                Err(())
            }
        }
    }
}

#[link(name = "CoreGraphics", kind = "framework")]
extern {
    /// Return the type identifier for the opaque type `CGEventSourceRef'.
    fn CGEventSourceGetTypeID() -> CFTypeID;

    /// Return a Quartz event source created with a specified source state.
    fn CGEventSourceCreate(stateID: CGEventSourceStateID) -> ::sys::CGEventSourceRef;
}
