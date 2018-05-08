use base::{CFAllocatorRef, CFIndex, CFOptionFlags, CFTypeRef};
use data::CFDataRef;
use error::CFErrorRef;

pub type CFPropertyListRef = CFTypeRef;

pub type CFPropertyListFormat = CFIndex;
pub const kCFPropertyListOpenStepFormat: CFPropertyListFormat = 1;
pub const kCFPropertyListXMLFormat_v1_0: CFPropertyListFormat = 100;
pub const kCFPropertyListBinaryFormat_v1_0: CFPropertyListFormat = 200;

pub type CFPropertyListMutabilityOptions = CFOptionFlags;
pub const kCFPropertyListImmutable: CFPropertyListMutabilityOptions = 0;
pub const kCFPropertyListMutableContainers: CFPropertyListMutabilityOptions = 1;
pub const kCFPropertyListMutableContainersAndLeaves: CFPropertyListMutabilityOptions = 2;

extern "C" {
    // CFPropertyList.h
    //

    // fn CFPropertyListCreateDeepCopy
    // fn CFPropertyListIsValid
    pub fn CFPropertyListCreateWithData(allocator: CFAllocatorRef,
                                        data: CFDataRef,
                                        options: CFPropertyListMutabilityOptions,
                                        format: *mut CFPropertyListFormat,
                                        error: *mut CFErrorRef)
                                        -> CFPropertyListRef;
    // fn CFPropertyListCreateWithStream
    // fn CFPropertyListWrite
    pub fn CFPropertyListCreateData(allocator: CFAllocatorRef,
                                    propertyList: CFPropertyListRef,
                                    format: CFPropertyListFormat,
                                    options: CFOptionFlags,
                                    error: *mut CFErrorRef)
                                    -> CFDataRef;
}
