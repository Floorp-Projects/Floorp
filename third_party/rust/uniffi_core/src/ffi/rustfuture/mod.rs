/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{future::Future, sync::Arc};

mod future;
mod scheduler;
use future::*;
use scheduler::*;

#[cfg(test)]
mod tests;

use crate::{derive_ffi_traits, Handle, HandleAlloc, LowerReturn, RustCallStatus};

/// Result code for [rust_future_poll].  This is passed to the continuation function.
#[repr(i8)]
#[derive(Debug, PartialEq, Eq)]
pub enum RustFuturePoll {
    /// The future is ready and is waiting for [rust_future_complete] to be called
    Ready = 0,
    /// The future might be ready and [rust_future_poll] should be called again
    MaybeReady = 1,
}

/// Foreign callback that's passed to [rust_future_poll]
///
/// The Rust side of things calls this when the foreign side should call [rust_future_poll] again
/// to continue progress on the future.
pub type RustFutureContinuationCallback = extern "C" fn(callback_data: u64, RustFuturePoll);

// === Public FFI API ===

/// Create a new [Handle] for a Rust future
///
/// For each exported async function, UniFFI will create a scaffolding function that uses this to
/// create the [Handle] to pass to the foreign code.
pub fn rust_future_new<F, T, UT>(future: F, tag: UT) -> Handle
where
    // F is the future type returned by the exported async function.  It needs to be Send + `static
    // since it will move between threads for an indeterminate amount of time as the foreign
    // executor calls polls it and the Rust executor wakes it.  It does not need to by `Sync`,
    // since we synchronize all access to the values.
    F: Future<Output = T> + Send + 'static,
    // T is the output of the Future.  It needs to implement [LowerReturn].  Also it must be Send +
    // 'static for the same reason as F.
    T: LowerReturn<UT> + Send + 'static,
    // The UniFfiTag ZST. The Send + 'static bound is to keep rustc happy.
    UT: Send + 'static,
    // Needed to allocate a handle
    dyn RustFutureFfi<T::ReturnType>: HandleAlloc<UT>,
{
    <dyn RustFutureFfi<T::ReturnType> as HandleAlloc<UT>>::new_handle(
        RustFuture::new(future, tag) as Arc<dyn RustFutureFfi<T::ReturnType>>
    )
}

/// Poll a Rust future
///
/// When the future is ready to progress the continuation will be called with the `data` value and
/// a [RustFuturePoll] value. For each [rust_future_poll] call the continuation will be called
/// exactly once.
///
/// # Safety
///
/// The [Handle] must not previously have been passed to [rust_future_free]
pub unsafe fn rust_future_poll<ReturnType, UT>(
    handle: Handle,
    callback: RustFutureContinuationCallback,
    data: u64,
) where
    dyn RustFutureFfi<ReturnType>: HandleAlloc<UT>,
{
    <dyn RustFutureFfi<ReturnType> as HandleAlloc<UT>>::get_arc(handle).ffi_poll(callback, data)
}

/// Cancel a Rust future
///
/// Any current and future continuations will be immediately called with RustFuturePoll::Ready.
///
/// This is needed for languages like Swift, which continuation to wait for the continuation to be
/// called when tasks are cancelled.
///
/// # Safety
///
/// The [Handle] must not previously have been passed to [rust_future_free]
pub unsafe fn rust_future_cancel<ReturnType, UT>(handle: Handle)
where
    dyn RustFutureFfi<ReturnType>: HandleAlloc<UT>,
{
    <dyn RustFutureFfi<ReturnType> as HandleAlloc<UT>>::get_arc(handle).ffi_cancel()
}

/// Complete a Rust future
///
/// Note: the actually extern "C" scaffolding functions can't be generic, so we generate one for
/// each supported FFI type.
///
/// # Safety
///
/// - The [Handle] must not previously have been passed to [rust_future_free]
/// - The `T` param must correctly correspond to the [rust_future_new] call.  It must
///   be `<Output as LowerReturn<UT>>::ReturnType`
pub unsafe fn rust_future_complete<ReturnType, UT>(
    handle: Handle,
    out_status: &mut RustCallStatus,
) -> ReturnType
where
    dyn RustFutureFfi<ReturnType>: HandleAlloc<UT>,
{
    <dyn RustFutureFfi<ReturnType> as HandleAlloc<UT>>::get_arc(handle).ffi_complete(out_status)
}

/// Free a Rust future, dropping the strong reference and releasing all references held by the
/// future.
///
/// # Safety
///
/// The [Handle] must not previously have been passed to [rust_future_free]
pub unsafe fn rust_future_free<ReturnType, UT>(handle: Handle)
where
    dyn RustFutureFfi<ReturnType>: HandleAlloc<UT>,
{
    <dyn RustFutureFfi<ReturnType> as HandleAlloc<UT>>::consume_handle(handle).ffi_free()
}

// Derive HandleAlloc for dyn RustFutureFfi<T> for all FFI return types
derive_ffi_traits!(impl<UT> HandleAlloc<UT> for dyn RustFutureFfi<u8>);
derive_ffi_traits!(impl<UT> HandleAlloc<UT> for dyn RustFutureFfi<i8>);
derive_ffi_traits!(impl<UT> HandleAlloc<UT> for dyn RustFutureFfi<u16>);
derive_ffi_traits!(impl<UT> HandleAlloc<UT> for dyn RustFutureFfi<i16>);
derive_ffi_traits!(impl<UT> HandleAlloc<UT> for dyn RustFutureFfi<u32>);
derive_ffi_traits!(impl<UT> HandleAlloc<UT> for dyn RustFutureFfi<i32>);
derive_ffi_traits!(impl<UT> HandleAlloc<UT> for dyn RustFutureFfi<u64>);
derive_ffi_traits!(impl<UT> HandleAlloc<UT> for dyn RustFutureFfi<i64>);
derive_ffi_traits!(impl<UT> HandleAlloc<UT> for dyn RustFutureFfi<f32>);
derive_ffi_traits!(impl<UT> HandleAlloc<UT> for dyn RustFutureFfi<f64>);
derive_ffi_traits!(impl<UT> HandleAlloc<UT> for dyn RustFutureFfi<*const std::ffi::c_void>);
derive_ffi_traits!(impl<UT> HandleAlloc<UT> for dyn RustFutureFfi<crate::RustBuffer>);
derive_ffi_traits!(impl<UT> HandleAlloc<UT> for dyn RustFutureFfi<()>);
