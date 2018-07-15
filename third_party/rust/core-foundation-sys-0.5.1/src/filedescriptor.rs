use libc::{c_void, c_int};
use base::{Boolean, CFIndex, CFTypeID, CFOptionFlags, CFAllocatorRef};
use string::CFStringRef;
use runloop::CFRunLoopSourceRef;

pub type CFFileDescriptorNativeDescriptor = c_int;

#[repr(C)]
pub struct __CFFileDescriptor(c_void);

pub type CFFileDescriptorRef = *mut __CFFileDescriptor;

/* Callback Reason Types */
pub const kCFFileDescriptorReadCallBack: CFOptionFlags  = 1 << 0;
pub const kCFFileDescriptorWriteCallBack: CFOptionFlags = 1 << 1;

pub type CFFileDescriptorCallBack = extern "C" fn (f: CFFileDescriptorRef, callBackTypes: CFOptionFlags, info: *mut c_void);

#[repr(C)]
#[derive(Clone, Copy)]
pub struct CFFileDescriptorContext {
    pub version: CFIndex,
    pub info: *mut c_void,
    pub retain: Option<extern "C" fn (info: *const c_void) -> *const c_void>,
    pub release: Option<extern "C" fn (info: *const c_void)>,
    pub copyDescription: Option<extern "C" fn (info: *const c_void) -> CFStringRef>,
}

extern {
    /*
     * CFFileDescriptor.h
     */
    pub fn CFFileDescriptorGetTypeID() -> CFTypeID;

    pub fn CFFileDescriptorCreate(allocator: CFAllocatorRef, fd: CFFileDescriptorNativeDescriptor, closeOnInvalidate: Boolean, callout: CFFileDescriptorCallBack, context: *const CFFileDescriptorContext) -> CFFileDescriptorRef;

    pub fn CFFileDescriptorGetNativeDescriptor(f: CFFileDescriptorRef) -> CFFileDescriptorNativeDescriptor;

    pub fn CFFileDescriptorGetContext(f: CFFileDescriptorRef, context: *mut CFFileDescriptorContext);

    pub fn CFFileDescriptorEnableCallBacks(f: CFFileDescriptorRef, callBackTypes: CFOptionFlags);
    pub fn CFFileDescriptorDisableCallBacks(f: CFFileDescriptorRef, callBackTypes: CFOptionFlags);

    pub fn CFFileDescriptorInvalidate(f: CFFileDescriptorRef);
    pub fn CFFileDescriptorIsValid(f: CFFileDescriptorRef) -> Boolean;

    pub fn CFFileDescriptorCreateRunLoopSource(allocator: CFAllocatorRef, f: CFFileDescriptorRef, order: CFIndex) -> CFRunLoopSourceRef;
}
