// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use std::cell::UnsafeCell;
pub struct Opaque(UnsafeCell<()>);

/// Generate a newtype wrapper `$owned` and reference wrapper
/// `$borrowed` around a POD FFI type that lives on the heap.
macro_rules! ffi_type_heap {
    (
        $(#[$impl_attr:meta])*
        type CType = $ctype:ty;
        $(fn drop = $drop:expr;)*
        $(fn clone = $clone:expr;)*
        $(#[$owned_attr:meta])*
        pub struct $owned:ident;
        $(#[$borrowed_attr:meta])*
        pub struct $borrowed:ident;
    ) => {
        $(#[$owned_attr])*
        pub struct $owned(*mut $ctype);

        impl $owned {
            /// # Safety
            ///
            /// This function is unsafe because it dereferences the given `ptr` pointer.
            /// The caller should ensure that pointer is valid.
            #[inline]
            pub unsafe fn from_ptr(ptr: *mut $ctype) -> $owned {
                $owned(ptr)
            }

            #[inline]
            pub fn as_ptr(&self) -> *mut $ctype {
                self.0
            }
        }

        $(
            impl Drop for $owned {
                #[inline]
                fn drop(&mut self) {
                    unsafe { $drop(self.0) }
                }
            }
        )*

        $(
            impl Clone for $owned {
                #[inline]
                fn clone(&self) -> $owned {
                    unsafe {
                        let handle: *mut $ctype = $clone(self.0);
                        $owned::from_ptr(handle)
                    }
                }
            }

            impl ::std::borrow::ToOwned for $borrowed {
                type Owned = $owned;
                #[inline]
                fn to_owned(&self) -> $owned {
                    unsafe {
                        let handle: *mut $ctype = $clone(self.as_ptr());
                        $owned::from_ptr(handle)
                    }
                }
            }
        )*

        impl ::std::ops::Deref for $owned {
            type Target = $borrowed;

            #[inline]
            fn deref(&self) -> &$borrowed {
                unsafe { $borrowed::from_ptr(self.0) }
            }
        }

        impl ::std::ops::DerefMut for $owned {
            #[inline]
            fn deref_mut(&mut self) -> &mut $borrowed {
                unsafe { $borrowed::from_ptr_mut(self.0) }
            }
        }

        impl ::std::borrow::Borrow<$borrowed> for $owned {
            #[inline]
            fn borrow(&self) -> &$borrowed {
                &**self
            }
        }

        impl ::std::convert::AsRef<$borrowed> for $owned {
            #[inline]
            fn as_ref(&self) -> &$borrowed {
                &**self
            }
        }

        $(#[$borrowed_attr])*
        pub struct $borrowed($crate::ffi_types::Opaque);

        impl $borrowed {
            /// # Safety
            ///
            /// This function is unsafe because it dereferences the given `ptr` pointer.
            /// The caller should ensure that pointer is valid.
            #[inline]
            pub unsafe fn from_ptr<'a>(ptr: *mut $ctype) -> &'a Self {
                &*(ptr as *mut _)
            }

            /// # Safety
            ///
            /// This function is unsafe because it dereferences the given `ptr` pointer.
            /// The caller should ensure that pointer is valid.
            #[inline]
            pub unsafe fn from_ptr_mut<'a>(ptr: *mut $ctype) -> &'a mut Self {
                &mut *(ptr as *mut _)
            }

            #[inline]
            pub fn as_ptr(&self) -> *mut $ctype {
                self as *const _ as *mut _
            }
        }

        impl ::std::fmt::Debug for $borrowed {
            fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
                let ptr = self as *const $borrowed as usize;
                f.debug_tuple(stringify!($borrowed))
                    .field(&ptr)
                    .finish()
            }
        }
    }
}

/// Generate a newtype wrapper `$owned` and reference wrapper
/// `$borrowed` around a POD FFI type that lives on the stack.
macro_rules! ffi_type_stack {
    ($(#[$impl_attr:meta])*
     type CType = $ctype:ty;
     $(#[$owned_attr:meta])*
     pub struct $owned:ident;
     $(#[$borrowed_attr:meta])*
     pub struct $borrowed:ident;
    ) => {
        $(#[$owned_attr])*
        pub struct $owned($ctype);

        impl $owned {
            pub fn as_ptr(&self) -> *mut $ctype {
                &self.0 as *const $ctype as *mut $ctype
            }
        }

        impl Default for $owned {
            fn default() -> $owned {
                $owned(Default::default())
            }
        }

        impl From<$ctype> for $owned {
            fn from(x: $ctype) -> $owned {
                $owned(x)
            }
        }

        impl ::std::ops::Deref for $owned {
            type Target = $borrowed;

            #[inline]
            fn deref(&self) -> &$borrowed {
                let ptr = &self.0 as *const $ctype as *mut $ctype;
                unsafe { $borrowed::from_ptr(ptr) }
            }
        }

        impl ::std::ops::DerefMut for $owned {
            #[inline]
            fn deref_mut(&mut self) -> &mut $borrowed {
                let ptr = &self.0 as *const $ctype as *mut $ctype;
                unsafe { $borrowed::from_ptr_mut(ptr) }
            }
        }

        impl ::std::borrow::Borrow<$borrowed> for $owned {
            #[inline]
            fn borrow(&self) -> &$borrowed {
                &**self
            }
        }

        impl ::std::convert::AsRef<$borrowed> for $owned {
            #[inline]
            fn as_ref(&self) -> &$borrowed {
                &**self
            }
        }

        $(#[$borrowed_attr])*
        pub struct $borrowed($crate::ffi_types::Opaque);

        impl $borrowed {
            /// # Safety
            ///
            /// This function is unsafe because it dereferences the given `ptr` pointer.
            /// The caller should ensure that pointer is valid.
            #[inline]
            pub unsafe fn from_ptr<'a>(ptr: *mut $ctype) -> &'a Self {
                &*(ptr as *mut _)
            }

            /// # Safety
            ///
            /// This function is unsafe because it dereferences the given `ptr` pointer.
            /// The caller should ensure that pointer is valid.
            #[inline]
            pub unsafe fn from_ptr_mut<'a>(ptr: *mut $ctype) -> &'a mut Self {
                &mut *(ptr as *mut _)
            }

            #[inline]
            pub fn as_ptr(&self) -> *mut $ctype {
                self as *const _ as *mut _
            }
        }

        impl ::std::fmt::Debug for $borrowed {
            fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
                let ptr = self as *const $borrowed as usize;
                f.debug_tuple(stringify!($borrowed))
                    .field(&ptr)
                    .finish()
            }
        }
    }
}
