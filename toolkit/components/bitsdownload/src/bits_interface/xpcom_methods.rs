/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// This macro is very similar to xpcom_macro, but works a bit differently:
///
/// When possible, it returns errors via the callback rather than via the return
/// value.
///
/// It implicitly adds the callback argument of type: nsIBitsNewRequestCallback
///
/// It needs an action type, to be specified before the rust name, in square
/// brackets.
///
/// The rustic implementation that the xpcom method calls is expected to return
/// the type: Result<_, BitsTaskError>. If this value is Ok, it will be ignored.
/// If the value is Err, it will be returned via the callback passed.
///
/// Usage like this:
///
/// ```ignore
///     nsIBits_method!(
///         [ActionType]
///         rust_method => XpcomMethod(
///             foo: *const nsACString,
///             bar: *const nsIBar,
///             baz: bool,
///             [optional] qux: *const nsIQux,
///         )
///     );
/// ```
///
/// Results in the macro generating a method like:
///
/// ```ignore
///     unsafe fn XpcomMethod(
///         &self,
///         foo: *const nsACString,
///         bar: *const nsIBar,
///         baz: bool,
///         qux: *const nsIQux,
///         callback: *const nsIBitsNewRequestCallback,
///     ) -> nsresult {
///         let callback: &nsIBitsNewRequestCallback = match xpcom::Ensure::ensure(callback) {
///             Ok(val) => val,
///             Err(result) => return result,
///         };
///         let foo = match xpcom::Ensure::ensure(foo) {
///             Ok(val) => val,
///             Err(_) => {
///                 dispatch_pretask_interface_error(BitsTaskError::new(ErrorType::NullArgument, ActionType.into(), ErrorStage::Pretask), callback);
///                 return NS_OK;
///             }
///         };
///         let bar = match xpcom::Ensure::ensure(bar) {
///             Ok(val) => val,
///             Err(_) => {
///                 dispatch_pretask_interface_error(BitsTaskError::new(ErrorType::NullArgument, ActionType.into(), ErrorStage::Pretask), callback);
///                 return NS_OK;
///             }
///         };
///         let baz = match xpcom::Ensure::ensure(baz) {
///             Ok(val) => val,
///             Err(_) => {
///                 dispatch_pretask_interface_error(BitsTaskError::new(ErrorType::NullArgument, ActionType.into(), ErrorStage::Pretask), callback);
///                 return NS_OK;
///             }
///         };
///         let qux = match xpcom::Ensure::ensure(qux) {
///             Ok(val) => Some(val),
///             Err(_) => None,
///         };
///
///         if let Err(error) = self.rust_method(foo, bar, baz, qux, callback) {
///             dispatch_pretask_interface_error(error, callback);
///         }
///
///         NS_OK
///     }
/// ```
///
/// Which expects a Rustic implementation method like:
///
/// ```ignore
///     fn rust_method(
///         &self,
///         foo: &nsACString,
///         bar: &nsIBar,
///         baz: bool,
///         qux: Option<&nsIQux>,
///         callback: &nsIBitsNewRequestCallback,
///     ) -> Result<(), BitsTaskError> {
///         do_something()
///     }
/// ```
#[macro_export]
macro_rules! nsIBits_method {
    // The internal rule @ensure_param converts raw pointer arguments to
    // references, calling dispatch_pretask_interface_error and returning if the
    // argument is null.
    // If, however, the type is optional, the reference will also be wrapped
    // in an option and null pointers will be converted to None.
    (@ensure_param [optional] $name:ident, $action:expr, $callback:ident) => {
        let $name = match Ensure::ensure($name) {
            Ok(val) => Some(val),
            Err(_) => None,
        };
    };
    (@ensure_param $name:ident, $action:expr, $callback:ident) => {
        let $name = match Ensure::ensure($name) {
            Ok(val) => val,
            Err(_) => {
                dispatch_pretask_interface_error(BitsTaskError::new(NullArgument, $action.into(), Pretask), $callback);
                return NS_OK;
            }
        };
    };

    ([$action:expr] $rust_name:ident => $xpcom_name:ident($($([$param_required:ident])* $param_name:ident: $param_type:ty $(,)*)*)) => {
        #[allow(non_snake_case)]
        unsafe fn $xpcom_name(&self, $($param_name: $param_type, )* callback: *const nsIBitsNewRequestCallback) -> nsresult {
            use xpcom::Ensure;
            use nserror::NS_OK;
            // When no params are passed, the imports below will not be used, so silence the
            // warning
            #[allow(unused_imports)]
            use bits_interface::{
                dispatch_callback::dispatch_pretask_interface_error,
                error::{BitsTaskError, ErrorStage::Pretask, ErrorType::NullArgument},
            };

            let callback: &nsIBitsNewRequestCallback = match Ensure::ensure(callback) {
                Ok(val) => val,
                Err(result) => return result,
            };
            $(nsIBits_method!(@ensure_param $([$param_required])* $param_name, $action, callback);)*
            if let Err(error) = self.$rust_name($($param_name, )* callback) {
                dispatch_pretask_interface_error(error, callback);
            }
            NS_OK
        }
    };
}

/*
 * Same as above, but expects a nsIBitsCallback as its callback.
 */
#[macro_export]
macro_rules! nsIBitsRequest_method {
    // The internal rule @ensure_param converts raw pointer arguments to
    // references, calling dispatch_pretask_interface_error and returning if the
    // argument is null.
    // If, however, the type is optional, the reference will also be wrapped
    // in an option and null pointers will be converted to None.
    (@ensure_param [optional] $name:ident, $action:expr, $callback:ident) => {
        let $name = match Ensure::ensure($name) {
            Ok(val) => Some(val),
            Err(_) => None,
        };
    };
    (@ensure_param $name:ident, $action:expr, $callback:ident) => {
        let $name = match Ensure::ensure($name) {
            Ok(val) => val,
            Err(_) => {
                dispatch_pretask_request_error(BitsTaskError::new(NullArgument, $action.into(), Pretask), $callback);
                return NS_OK;
            }
        };
    };

    ([$action:expr] $rust_name:ident => $xpcom_name:ident($($([$param_required:ident])* $param_name:ident: $param_type:ty $(,)*)*)) => {
        #[allow(non_snake_case)]
        unsafe fn $xpcom_name(&self, $($param_name: $param_type, )* callback: *const nsIBitsCallback) -> nsresult {
            use xpcom::Ensure;
            use nserror::NS_OK;
            // When no params are passed, the imports below will not be used, so silence the
            // warning
            #[allow(unused_imports)]
            use bits_interface::{
                dispatch_callback::dispatch_pretask_request_error,
                error::{BitsTaskError, ErrorStage::Pretask, ErrorType::NullArgument},
            };

            let callback: &nsIBitsCallback = match Ensure::ensure(callback) {
                Ok(val) => val,
                Err(result) => return result,
            };
            $(nsIBitsRequest_method!(@ensure_param $([$param_required])* $param_name, $action, callback);)*
            if let Err(error) = self.$rust_name($($param_name, )* callback) {
                dispatch_pretask_request_error(error, callback);
            }
            NS_OK
        }
    };
}
