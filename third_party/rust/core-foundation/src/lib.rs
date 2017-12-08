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
macro_rules! impl_TCFType {
    ($ty:ident, $raw:ident, $ty_id:ident) => {
        impl $crate::base::TCFType<$raw> for $ty {
            #[inline]
            fn as_concrete_TypeRef(&self) -> $raw {
                self.0
            }

            #[inline]
            unsafe fn wrap_under_get_rule(reference: $raw) -> $ty {
                let reference = ::std::mem::transmute(::core_foundation_sys::base::CFRetain(::std::mem::transmute(reference)));
                $crate::base::TCFType::wrap_under_create_rule(reference)
            }

            #[inline]
            fn as_CFTypeRef(&self) -> ::core_foundation_sys::base::CFTypeRef {
                unsafe {
                    ::std::mem::transmute(self.as_concrete_TypeRef())
                }
            }

            #[inline]
            unsafe fn wrap_under_create_rule(obj: $raw) -> $ty {
                $ty(obj)
            }

            #[inline]
            fn type_id() -> ::core_foundation_sys::base::CFTypeID {
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
    ($ty:ident, $raw:ident, $ty_id:ident) => {
        impl<T> $crate::base::TCFType<$raw> for $ty<T> {
            #[inline]
            fn as_concrete_TypeRef(&self) -> $raw {
                self.0
            }

            #[inline]
            unsafe fn wrap_under_get_rule(reference: $raw) -> $ty<T> {
                let reference = ::std::mem::transmute(::core_foundation_sys::base::CFRetain(::std::mem::transmute(reference)));
                $crate::base::TCFType::wrap_under_create_rule(reference)
            }

            #[inline]
            fn as_CFTypeRef(&self) -> ::core_foundation_sys::base::CFTypeRef {
                unsafe {
                    ::std::mem::transmute(self.as_concrete_TypeRef())
                }
            }

            #[inline]
            unsafe fn wrap_under_create_rule(obj: $raw) -> $ty<T> {
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
pub mod number;
pub mod set;
pub mod string;
pub mod url;
pub mod bundle;
pub mod propertylist;
pub mod runloop;
pub mod timezone;
pub mod uuid;

#[cfg(test)]
pub mod test {
    #[test]
    fn test_stuff() {
        use base::TCFType;
        use boolean::CFBoolean;
        use number::CFNumber;
        use dictionary::CFDictionary;
        use string::CFString;

        /*let n = CFNumber::new_number(42 as i32);
        io::println(format!("%d", (&n).retain_count() as int));
        (&n).show();*/

        let bar = CFString::from_static_string("Bar");
        let baz = CFString::from_static_string("Baz");
        let boo = CFString::from_static_string("Boo");
        let foo = CFString::from_static_string("Foo");
        let tru = CFBoolean::true_value();
        let n42 = CFNumber::from(42);

        let d = CFDictionary::from_CFType_pairs(&[
            (bar.as_CFType(), boo.as_CFType()),
            (baz.as_CFType(), tru.as_CFType()),
            (foo.as_CFType(), n42.as_CFType()),
        ]);

        let (v1, v2) = d.get_keys_and_values();

        assert!(v1 == &[bar.as_CFTypeRef(), baz.as_CFTypeRef(), foo.as_CFTypeRef()]);
        assert!(v2 == &[boo.as_CFTypeRef(), tru.as_CFTypeRef(), n42.as_CFTypeRef()]);
    }
}
