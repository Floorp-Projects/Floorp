/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate nserror;

use nserror::{nsresult, NS_ERROR_NULL_POINTER};

/// The xpcom_method macro generates a Rust XPCOM method stub that converts
/// raw pointer arguments to references, calls a Rustic implementation
/// of the method, writes its return value into the XPCOM method's outparameter,
/// and returns an nsresult.
///
/// In other words, given an XPCOM method like:
///
/// ```ignore
/// interface nsIFooBarBaz : nsISupports {
///   nsIVariant foo(in AUTF8String bar, [optional] in bool baz);
/// }
/// ```
///
/// And a Rust implementation that uses #[derive(xpcom)] to implement it:
///
/// ```ignore
/// #[derive(xpcom)]
/// #[xpimplements(nsIFooBarBaz)]
/// #[refcnt = "atomic"]
/// struct InitFooBarBaz {
///     // …
/// }
/// ```
///
/// With the appropriate extern crate and use declarations (which include
/// using the Ensure trait from this module):
///
/// ```ignore
/// #[macro_use]
/// extern crate xpcom;
/// use xpcom::Ensure;
/// ```
///
/// Invoking the macro with the name of the XPCOM method, the name of its
/// Rustic implementation, the set of its arguments, and its return value:
///
/// ```ignore
/// impl FooBarBaz {
///   xpcom_method(Foo, foo, { bar: *const nsACString, baz: bool }, *mut *const nsIVariant);
///   xpcom_method!(
///       foo => Foo(bar: *const nsACString, baz: bool) -> *const nsIVariant
///   );
/// }
/// ```
///
/// Results in the macro generating an XPCOM stub like the following:
///
/// ```ignore
/// unsafe fn Foo(&self, bar: *const nsACString, baz: bool, retval: *mut *const nsIVariant) -> nsresult {
///     ensure_param!(bar);
///     ensure_param!(baz);
///
///     match self.foo(bar, baz) {
///         Ok(val) => {
///             val.forget(&mut *retval);
///             NS_OK
///         }
///         Err(error) => {
///             error!("{}", error);
///             error.into()
///         }
///     }
/// }
/// ```
///
/// Which calls a Rustic implementation (that you implement) like the following:
///
/// ```ignore
/// impl FooBarBaz {
///     fn foo(&self, bar: &nsACString, baz: bool) -> Result<RefPtr<nsIVariant>, nsresult> {
///         // …
///     }
/// }
/// ```
///
/// Notes:
///
/// On error, the Rustic implementation can return an Err(nsresult) or any
/// other type that implements Into<nsresult>.  So you can define and return
/// a custom error type, which the XPCOM stub will convert to nsresult.
///
/// This macro assumes that all non-null pointer arguments are valid!
/// It does ensure that they aren't null, using the `ensure_param` macro.
/// But it doesn't otherwise check their validity. That makes the function
/// unsafe, so callers must ensure that they only call it with valid pointer
/// arguments.
#[macro_export]
macro_rules! xpcom_method {
    // `#[allow(non_snake_case)]` is used for each method because `$xpcom_name`
    // is almost always UpperCamelCase, and Rust gives a warning that it should
    // be snake_case. It isn't reasonable to rename the XPCOM methods, so
    // silence the warning.

    // A method whose return value is a *mut *const nsISomething type.
    // Example: foo => Foo(bar: *const nsACString, baz: bool) -> *const nsIVariant
    ($rust_name:ident => $xpcom_name:ident($($param_name:ident: $param_type:ty),*) -> *const $retval:ty) => {
        #[allow(non_snake_case)]
        unsafe fn $xpcom_name(&self, $($param_name: $param_type,)* retval: *mut *const $retval) -> nsresult {
            $(ensure_param!($param_name);)*
            match self.$rust_name($($param_name, )*) {
                Ok(val) => {
                    val.forget(&mut *retval);
                    NS_OK
                }
                Err(error) => {
                    error.into()
                }
            }
        }
    };

    // A method whose return value is a *mut nsAString type.
    // Example: foo => Foo(bar: *const nsACString, baz: bool) -> nsAString
    ($rust_name:ident => $xpcom_name:ident($($param_name:ident: $param_type:ty),*) -> nsAString) => {
        #[allow(non_snake_case)]
        unsafe fn $xpcom_name(&self, $($param_name: $param_type,)* retval: *mut nsAString) -> nsresult {
            $(ensure_param!($param_name);)*
            match self.$rust_name($($param_name, )*) {
                Ok(val) => {
                    (*retval).assign(&val);
                    NS_OK
                }
                Err(error) => {
                    error.into()
                }
            }
        }
    };

    // A method whose return value is a *mut nsACString type.
    // Example: foo => Foo(bar: *const nsACString, baz: bool) -> nsACString
    ($rust_name:ident => $xpcom_name:ident($($param_name:ident: $param_type:ty),*) -> nsACString) => {
        #[allow(non_snake_case)]
        unsafe fn $xpcom_name(&self, $($param_name: $param_type,)* retval: *mut nsACString) -> nsresult {
            $(ensure_param!($param_name);)*
            match self.$rust_name($($param_name, )*) {
                Ok(val) => {
                    (*retval).assign(&val);
                    NS_OK
                }
                Err(error) => {
                    error.into()
                }
            }
        }
    };

    // A method whose return value is a non-nsA[C]String *mut type.
    // Example: foo => Foo(bar: *const nsACString, baz: bool) -> bool
    ($rust_name:ident => $xpcom_name:ident($($param_name:ident: $param_type:ty),*) -> $retval:ty) => {
        #[allow(non_snake_case)]
        unsafe fn $xpcom_name(&self, $($param_name: $param_type,)* retval: *mut $retval) -> nsresult {
            $(ensure_param!($param_name);)*
            match self.$rust_name($($param_name, )*) {
                Ok(val) => {
                    *retval = val;
                    NS_OK
                }
                Err(error) => {
                    error.into()
                }
            }
        }
    };

    // A method that doesn't have a return value.
    // Example: foo => Foo(bar: *const nsACString, baz: bool)
    ($rust_name:ident => $xpcom_name:ident($($param_name:ident: $param_type:ty),*)) => {
        #[allow(non_snake_case)]
        unsafe fn $xpcom_name(&self, $($param_name: $param_type,)*) -> nsresult {
            $(ensure_param!($param_name);)*
            match self.$rust_name($($param_name, )*) {
                Ok(_) => NS_OK,
                Err(error) => {
                    error.into()
                }
            }
        }
    };
}

/// The ensure_param macro converts raw pointer arguments to references, and
/// passes them to a Rustic implementation of an XPCOM method. This macro isn't
/// intended to be used directly but rather via the xpcom_method macro.
///
/// If the argument `is_null()`, and the corresponding Rustic method parameter
/// is `&T`, the macro returns `NS_ERROR_NULL_POINTER`. However, if the
/// parameter is `Option<&T>`, the macro passes `None` instead. This makes it
/// easy to use optional arguments safely.
///
/// Given the appropriate extern crate and use declarations (which include
/// using the Ensure trait from this module):
///
/// ```ignore
/// #[macro_use]
/// extern crate xpcom;
/// use xpcom::Ensure;
/// ```
///
/// Invoking the macro like this:
///
/// ```ignore
/// fn do_something_with_foo(foo: &nsIBar) {}
/// fn DoSomethingWithFoo(foo: *const nsIBar) -> nsresult {
///     let foo = ensure_param!(foo);
///     do_something_with_foo(foo);
/// }
/// ```
///
/// Expands to code like this:
///
/// ```ignore
/// fn do_something_with_foo(foo: &nsIBar) {}
/// fn DoSomethingWithFoo(foo: *const nsIBar) -> nsresult {
///     let foo = match Ensure::ensure(foo) {
///         Ok(val) => val,
///         Err(result) => return result,
///     };
///     do_something_with_foo(foo);
/// }
/// ```
///
/// Which converts `foo` from a `*const nsIBar` to a `&nsIBar`, or returns
/// `NS_ERROR_NULL_POINTER` if `foo` is null.
///
/// To pass an optional argument, we only need to change
/// `do_something_with_foo` to take an `Option<&nsIBar> instead. The macro
/// generates the same code, but `do_something_with_foo` receives `None` if
/// `foo` is null.
///
/// Notes:
///
/// You can call the macro on a non-pointer copy parameter, but there's no
/// benefit to doing so.  The macro will just set the value of the parameter
/// to itself.  (The xpcom_method macro does this anyway due to limitations
/// in declarative macros; it isn't currently possible to distinguish between
/// pointer and copy types when processing a set of parameters.)
///
/// The macro currently supports only inparameters (*const nsIFoo); It doesn't
/// (yet?) support outparameters (*mut nsIFoo).  The xpcom_method macro itself
/// does, however, support the return value outparameter.
///
#[doc(hidden)]
#[macro_export]
macro_rules! ensure_param {
    ($name:ident) => {
        let $name = match $crate::Ensure::ensure($name) {
            Ok(val) => val,
            Err(result) => return result,
        };
    };
}

/// A trait that ensures that a raw pointer isn't null and converts it to
/// a reference.  Because of limitations in declarative macros (see the docs
/// for the ensure_param macro), this includes an implementation for types
/// that are Copy, which simply returns the value itself.
#[doc(hidden)]
pub trait Ensure<T> {
    unsafe fn ensure(T) -> Self;
}

impl<'a, T: 'a> Ensure<*const T> for Result<&'a T, nsresult> {
    unsafe fn ensure(ptr: *const T) -> Result<&'a T, nsresult> {
        if ptr.is_null() {
            Err(NS_ERROR_NULL_POINTER)
        } else {
            Ok(&*ptr)
        }
    }
}

impl<'a, T: 'a> Ensure<*const T> for Result<Option<&'a T>, nsresult> {
    unsafe fn ensure(ptr: *const T) -> Result<Option<&'a T>, nsresult> {
        Ok(if ptr.is_null() {
            None
        } else {
            Some(&*ptr)
        })
    }
}

impl<T: Copy> Ensure<T> for Result<T, nsresult> {
    unsafe fn ensure(copyable: T) -> Result<T, nsresult> {
        Ok(copyable)
    }
}
