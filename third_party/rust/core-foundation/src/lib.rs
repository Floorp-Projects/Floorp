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

use base::TCFType;

pub unsafe trait ConcreteCFType: TCFType {}

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
        impl_TCFType!($ty<>, $ty_ref, $ty_id);
        unsafe impl $crate::ConcreteCFType for $ty { }
    };

    ($ty:ident<$($p:ident $(: $bound:path)*),*>, $ty_ref:ident, $ty_id:ident) => {
        impl<$($p $(: $bound)*),*> $crate::base::TCFType for $ty<$($p),*> {
            type Ref = $ty_ref;

            #[inline]
            fn as_concrete_TypeRef(&self) -> $ty_ref {
                self.0
            }

            #[inline]
            unsafe fn wrap_under_get_rule(reference: $ty_ref) -> Self {
                let reference = $crate::base::CFRetain(reference as *const ::std::os::raw::c_void) as $ty_ref;
                $crate::base::TCFType::wrap_under_create_rule(reference)
            }

            #[inline]
            fn as_CFTypeRef(&self) -> $crate::base::CFTypeRef {
                self.as_concrete_TypeRef() as $crate::base::CFTypeRef
            }

            #[inline]
            unsafe fn wrap_under_create_rule(reference: $ty_ref) -> Self {
                // we need one PhantomData for each type parameter so call ourselves
                // again with @Phantom $p to produce that
                $ty(reference $(, impl_TCFType!(@Phantom $p))*)
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

        unsafe impl<'a> $crate::base::ToVoid<$ty> for &'a $ty {
            fn to_void(&self) -> *const ::std::os::raw::c_void {
                use $crate::base::TCFTypeRef;
                self.as_concrete_TypeRef().as_void_ptr()
            }
        }

        unsafe impl $crate::base::ToVoid<$ty> for $ty {
            fn to_void(&self) -> *const ::std::os::raw::c_void {
                use $crate::base::TCFTypeRef;
                self.as_concrete_TypeRef().as_void_ptr()
            }
        }

        unsafe impl $crate::base::ToVoid<$ty> for $ty_ref {
            fn to_void(&self) -> *const ::std::os::raw::c_void {
                use $crate::base::TCFTypeRef;
                self.as_void_ptr()
            }
        }

    };

    (@Phantom $x:ident) => { ::std::marker::PhantomData };
}


#[macro_export]
macro_rules! impl_CFTypeDescription {
    ($ty:ident) => {
        // it's fine to use an empty <> list
        impl_CFTypeDescription!($ty<>);
    };
    ($ty:ident<$($p:ident $(: $bound:path)*),*>) => {
        impl<$($p $(: $bound)*),*> ::std::fmt::Debug for $ty<$($p),*> {
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
pub mod attributed_string;
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
