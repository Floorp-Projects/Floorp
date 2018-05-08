// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
#![allow(non_snake_case)]

extern crate core_foundation_sys;
extern crate libc;

#[cfg(feature = "with-chrono")]
extern crate chrono;

#[macro_export]
macro_rules! declare_TCFType {
    (
        $(#[$doc:meta])*
        $ty:ident, $raw:ident
    ) => {
        $(#[$doc])*
        pub struct $ty($raw);

        impl Drop for $ty {
            fn drop(&mut self) {
                unsafe { $crate::base::CFRelease(self.as_CFTypeRef()) }
            }
        }
    }
}

#[macro_export]
macro_rules! impl_TCFType {
    ($ty:ident, $ty_ref:ident, $ty_id:ident) => {
        impl $crate::base::TCFType for $ty {
            type Ref = $ty_ref;

            #[inline]
            fn as_concrete_TypeRef(&self) -> $ty_ref {
                self.0
            }

            #[inline]
            unsafe fn wrap_under_get_rule(reference: $ty_ref) -> $ty {
                use std::mem;
                let reference = mem::transmute($crate::base::CFRetain(mem::transmute(reference)));
                $crate::base::TCFType::wrap_under_create_rule(reference)
            }

            #[inline]
            fn as_CFTypeRef(&self) -> $crate::base::CFTypeRef {
                unsafe {
                    ::std::mem::transmute(self.as_concrete_TypeRef())
                }
            }

            #[inline]
            unsafe fn wrap_under_create_rule(reference: $ty_ref) -> $ty {
                $ty(reference)
            }

            #[inline]
            fn type_id() -> $crate::base::CFTypeID {
                unsafe {
                    $ty_id()
                }
            }
        }

        impl Clone for $ty {
            #[inline]
            fn clone(&self) -> $ty {
                unsafe {
                    $ty::wrap_under_get_rule(self.0)
                }
            }
        }

        impl PartialEq for $ty {
            #[inline]
            fn eq(&self, other: &$ty) -> bool {
                self.as_CFType().eq(&other.as_CFType())
            }
        }

        impl Eq for $ty { }
    }
}

// This is basically identical to the implementation above. I can't
// think of a clean way to have them share code
#[macro_export]
macro_rules! impl_TCFTypeGeneric {
    ($ty:ident, $ty_ref:ident, $ty_id:ident) => {
        impl<T> $crate::base::TCFType for $ty<T> {
            type Ref = $ty_ref;

            #[inline]
            fn as_concrete_TypeRef(&self) -> $ty_ref {
                self.0
            }

            #[inline]
            unsafe fn wrap_under_get_rule(reference: $ty_ref) -> $ty<T> {
                use std::mem;
                let reference = mem::transmute($crate::base::CFRetain(mem::transmute(reference)));
                $crate::base::TCFType::wrap_under_create_rule(reference)
            }

            #[inline]
            fn as_CFTypeRef(&self) -> ::core_foundation_sys::base::CFTypeRef {
                unsafe {
                    ::std::mem::transmute(self.as_concrete_TypeRef())
                }
            }

            #[inline]
            unsafe fn wrap_under_create_rule(obj: $ty_ref) -> $ty<T> {
                $ty(obj, PhantomData)
            }

            #[inline]
            fn type_id() -> ::core_foundation_sys::base::CFTypeID {
                unsafe {
                    $ty_id()
                }
            }
        }

        impl<T> Clone for $ty<T> {
            #[inline]
            fn clone(&self) -> $ty<T> {
                unsafe {
                    $ty::wrap_under_get_rule(self.0)
                }
            }
        }

        impl<T> PartialEq for $ty<T> {
            #[inline]
            fn eq(&self, other: &$ty<T>) -> bool {
                self.as_CFType().eq(&other.as_CFType())
            }
        }

        impl<T> Eq for $ty<T> { }
    }
}

#[macro_export]
macro_rules! impl_CFTypeDescription {
    ($ty:ident) => {
        impl ::std::fmt::Debug for $ty {
            fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
                self.as_CFType().fmt(f)
            }
        }
    }
}

// The same as impl_CFTypeDescription but with a type parameter
#[macro_export]
macro_rules! impl_CFTypeDescriptionGeneric {
    ($ty:ident) => {
        impl<T> ::std::fmt::Debug for $ty<T> {
            fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
                self.as_CFType().fmt(f)
            }
        }
    }
}

#[macro_export]
macro_rules! impl_CFComparison {
    ($ty:ident, $compare:ident) => {
        impl PartialOrd for $ty {
            #[inline]
            fn partial_cmp(&self, other: &$ty) -> Option<::std::cmp::Ordering> {
                unsafe {
                    Some($compare(self.as_concrete_TypeRef(), other.as_concrete_TypeRef(), ::std::ptr::null_mut()).into())
                }
            }
        }

        impl Ord for $ty {
            #[inline]
            fn cmp(&self, other: &$ty) -> ::std::cmp::Ordering {
                self.partial_cmp(other).unwrap()
            }
        }
    }
}

pub mod array;
pub mod base;
pub mod boolean;
pub mod data;
pub mod date;
pub mod dictionary;
pub mod error;
pub mod filedescriptor;
pub mod number;
pub mod set;
pub mod string;
pub mod url;
pub mod bundle;
pub mod propertylist;
pub mod runloop;
pub mod timezone;
pub mod uuid;
