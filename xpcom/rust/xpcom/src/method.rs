/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
/// And a Rust implementation that uses #[xpcom] to implement it:
///
/// ```ignore
/// #[xpcom(implement(nsIFooBarBaz), atomic)]
/// struct FooBarBaz {
///     // …
/// }
/// ```
///
/// With the appropriate extern crate and use declarations
///
/// ```ignore
/// extern crate xpcom;
/// use xpcom::xpcom_method;
/// ```
///
/// Invoking the macro with the name of the XPCOM method, the name of its
/// Rustic implementation, the set of its arguments, and its return value:
///
/// ```ignore
/// impl FooBarBaz {
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
///     let bar = match Ensure::ensure(bar) {
///         Ok(val) => val,
///         Err(result) => return result,
///     };
///     let baz = match Ensure::ensure(baz) {
///         Ok(val) => val,
///         Err(result) => return result,
///     };
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
    // This rule is provided to ensure external modules don't need to import
    // internal implementation details of xpcom_method.
    // The @ensure_param rule converts raw pointer arguments to references,
    // returning NS_ERROR_NULL_POINTER if the argument is_null().
    //
    // Notes:
    //
    // This rule can be called on a non-pointer copy parameter, but there's no
    // benefit to doing so.  The macro will just set the value of the parameter
    // to itself. (This macro does this anyway due to limitations in declarative
    // macros; it isn't currently possible to distinguish between pointer and
    // copy types when processing a set of parameters.)
    //
    // The macro currently supports only in-parameters (*const nsIFoo); It
    // doesn't (yet?) support out-parameters (*mut nsIFoo).  The xpcom_method
    // macro itself does, however, support the return value out-parameter.
    (@ensure_param $name:ident) => {
        let $name = match $crate::Ensure::ensure($name) {
            Ok(val) => val,
            Err(result) => return result,
        };
    };

    // `#[allow(non_snake_case)]` is used for each method because `$xpcom_name`
    // is almost always UpperCamelCase, and Rust gives a warning that it should
    // be snake_case. It isn't reasonable to rename the XPCOM methods, so
    // silence the warning.

    // A method whose return value is a *mut *const nsISomething type.
    // Example: foo => Foo(bar: *const nsACString, baz: bool) -> *const nsIVariant
    ($rust_name:ident => $xpcom_name:ident($($param_name:ident: $param_type:ty),*) -> *const $retval:ty) => {
        #[allow(non_snake_case)]
        unsafe fn $xpcom_name(&self, $($param_name: $param_type,)* retval: *mut *const $retval) -> nsresult {
            $(xpcom_method!(@ensure_param $param_name);)*
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
            $(xpcom_method!(@ensure_param $param_name);)*
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
            $(xpcom_method!(@ensure_param $param_name);)*
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
            $(xpcom_method!(@ensure_param $param_name);)*
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
            $(xpcom_method!(@ensure_param $param_name);)*
            match self.$rust_name($($param_name, )*) {
                Ok(_) => NS_OK,
                Err(error) => {
                    error.into()
                }
            }
        }
    };
}

/// A trait that ensures that a raw pointer isn't null and converts it to
/// a reference.  Because of limitations in declarative macros, this includes an
/// implementation for types that are Copy, which simply returns the value
/// itself.
#[doc(hidden)]
pub trait Ensure<T> {
    unsafe fn ensure(value: T) -> Self;
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
        Ok(if ptr.is_null() { None } else { Some(&*ptr) })
    }
}

impl<T: Copy> Ensure<T> for Result<T, nsresult> {
    unsafe fn ensure(copyable: T) -> Result<T, nsresult> {
        Ok(copyable)
    }
}
