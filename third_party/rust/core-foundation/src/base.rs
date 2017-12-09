// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::fmt;

pub use core_foundation_sys::base::*;

use string::CFString;

pub trait CFIndexConvertible {
    /// Always use this method to construct a `CFIndex` value. It performs bounds checking to
    /// ensure the value is in range.
    fn to_CFIndex(self) -> CFIndex;
}

impl CFIndexConvertible for usize {
    #[inline]
    fn to_CFIndex(self) -> CFIndex {
        let max_CFIndex = CFIndex::max_value();
        if self > (max_CFIndex as usize) {
            panic!("value out of range")
        }
        self as CFIndex
    }
}

/// Superclass of all Core Foundation objects.
pub struct CFType(CFTypeRef);

impl fmt::Debug for CFType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let desc = unsafe {
            CFString::wrap_under_create_rule(CFCopyDescription(self.0))
        };
        desc.fmt(f)
    }
}

impl Clone for CFType {
    #[inline]
    fn clone(&self) -> CFType {
        unsafe {
            TCFType::wrap_under_get_rule(self.0)
        }
    }
}

impl PartialEq for CFType {
    #[inline]
    fn eq(&self, other: &CFType) -> bool {
        unsafe {
            CFEqual(self.as_CFTypeRef(), other.as_CFTypeRef()) != 0
        }
    }
}

impl Drop for CFType {
    fn drop(&mut self) {
        unsafe {
            CFRelease(self.0)
        }
    }
}

/// An allocator for Core Foundation objects.
pub struct CFAllocator(CFAllocatorRef);

impl Drop for CFAllocator {
    fn drop(&mut self) {
        unsafe {
            CFRelease(self.as_CFTypeRef())
        }
    }
}

impl_TCFType!(CFAllocator, CFAllocatorRef, CFAllocatorGetTypeID);

impl CFAllocator {
    #[inline]
    pub fn new(mut context: CFAllocatorContext) -> CFAllocator {
        unsafe {
            let allocator_ref = CFAllocatorCreate(kCFAllocatorDefault, &mut context);
            TCFType::wrap_under_create_rule(allocator_ref)
        }
    }
}

/// All Core Foundation types implement this trait. The type parameter `TypeRef` specifies the
/// associated Core Foundation type: e.g. for `CFType` this is `CFTypeRef`; for `CFArray` this is
/// `CFArrayRef`.
pub trait TCFType<ConcreteTypeRef> {
    /// Returns the object as its concrete TypeRef.
    fn as_concrete_TypeRef(&self) -> ConcreteTypeRef;

    /// Returns an instance of the object, wrapping the underlying `CFTypeRef` subclass. Use this
    /// when following Core Foundation's "Create Rule". The reference count is *not* bumped.
    unsafe fn wrap_under_create_rule(obj: ConcreteTypeRef) -> Self;

    /// Returns the type ID for this class.
    fn type_id() -> CFTypeID;

    /// Returns the object as a wrapped `CFType`. The reference count is incremented by one.
    #[inline]
    fn as_CFType(&self) -> CFType {
        unsafe {
            TCFType::wrap_under_get_rule(self.as_CFTypeRef())
        }
    }

    /// Returns the object as a raw `CFTypeRef`. The reference count is not adjusted.
    fn as_CFTypeRef(&self) -> CFTypeRef;

    /// Returns an instance of the object, wrapping the underlying `CFTypeRef` subclass. Use this
    /// when following Core Foundation's "Get Rule". The reference count *is* bumped.
    unsafe fn wrap_under_get_rule(reference: ConcreteTypeRef) -> Self;

    /// Returns the reference count of the object. It is unwise to do anything other than test
    /// whether the return value of this method is greater than zero.
    #[inline]
    fn retain_count(&self) -> CFIndex {
        unsafe {
            CFGetRetainCount(self.as_CFTypeRef())
        }
    }

    /// Returns the type ID of this object.
    #[inline]
    fn type_of(&self) -> CFTypeID {
        unsafe {
            CFGetTypeID(self.as_CFTypeRef())
        }
    }

    /// Writes a debugging version of this object on standard error.
    fn show(&self) {
        unsafe {
            CFShow(self.as_CFTypeRef())
        }
    }

    /// Returns true if this value is an instance of another type.
    #[inline]
    fn instance_of<OtherConcreteTypeRef,OtherCFType:TCFType<OtherConcreteTypeRef>>(&self) -> bool {
        self.type_of() == <OtherCFType as TCFType<_>>::type_id()
    }
}

impl TCFType<CFTypeRef> for CFType {
    #[inline]
    fn as_concrete_TypeRef(&self) -> CFTypeRef {
        self.0
    }

    #[inline]
    unsafe fn wrap_under_get_rule(reference: CFTypeRef) -> CFType {
        let reference: CFTypeRef = CFRetain(reference);
        TCFType::wrap_under_create_rule(reference)
    }

    #[inline]
    fn as_CFTypeRef(&self) -> CFTypeRef {
        self.as_concrete_TypeRef()
    }

    #[inline]
    unsafe fn wrap_under_create_rule(obj: CFTypeRef) -> CFType {
        CFType(obj)
    }

    #[inline]
    fn type_id() -> CFTypeID {
        // FIXME(pcwalton): Is this right?
        0
    }
}


#[cfg(test)]
mod tests {
    use super::*;
    use boolean::CFBoolean;

    #[test]
    fn cftype_instance_of() {
        let string = CFString::from_static_string("foo");
        let cftype = string.as_CFType();

        assert!(cftype.instance_of::<_, CFString>());
        assert!(!cftype.instance_of::<_, CFBoolean>());
    }
}
