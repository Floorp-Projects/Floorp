/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module defines a Rust Future that wraps an async foreign function call.
//!
//! The general idea is to create a [oneshot::Channel], hand the sender to the foreign side, and
//! await the receiver side on the Rust side.
//!
//! The foreign side should:
//!   * Input a [ForeignFutureCallback] and a `u64` handle in their scaffolding function.
//!     This is the sender, converted to a raw pointer, and an extern "C" function that sends the result.
//!   * Return a [ForeignFuture], which represents the foreign task object corresponding to the async function.
//!   * Call the [ForeignFutureCallback] when the async function completes with:
//!     * The `u64` handle initially passed in
//!     * The `ForeignFutureResult` for the call
//!   * Wait for the [ForeignFutureHandle::free] function to be called to free the task object.
//!     If this is called before the task completes, then the task will be cancelled.

use crate::{LiftReturn, RustCallStatus, UnexpectedUniFFICallbackError};

/// Handle for a foreign future
pub type ForeignFutureHandle = u64;

/// Handle for a callback data associated with a foreign future.
pub type ForeignFutureCallbackData = *mut ();

/// Callback that's passed to a foreign async functions.
///
/// See `LiftReturn` trait for how this is implemented.
pub type ForeignFutureCallback<FfiType> =
    extern "C" fn(oneshot_handle: u64, ForeignFutureResult<FfiType>);

/// C struct that represents the result of a foreign future
#[repr(C)]
pub struct ForeignFutureResult<T> {
    // Note: for void returns, T is `()`, which isn't directly representable with C since it's a ZST.
    // Foreign code should treat that case as if there was no `return_value` field.
    return_value: T,
    call_status: RustCallStatus,
}

/// Perform a call to a foreign async method

/// C struct that represents the foreign future.
///
/// This is what's returned by the async scaffolding functions.
#[repr(C)]
pub struct ForeignFuture {
    pub handle: ForeignFutureHandle,
    pub free: extern "C" fn(handle: ForeignFutureHandle),
}

impl Drop for ForeignFuture {
    fn drop(&mut self) {
        (self.free)(self.handle)
    }
}

unsafe impl Send for ForeignFuture {}

pub async fn foreign_async_call<F, T, UT>(call_scaffolding_function: F) -> T
where
    F: FnOnce(ForeignFutureCallback<T::ReturnType>, u64) -> ForeignFuture,
    T: LiftReturn<UT>,
{
    let (sender, receiver) = oneshot::channel::<ForeignFutureResult<T::ReturnType>>();
    // Keep the ForeignFuture around, even though we don't ever use it.
    // The important thing is that the ForeignFuture will be dropped when this Future is.
    let _foreign_future =
        call_scaffolding_function(foreign_future_complete::<T, UT>, sender.into_raw() as u64);
    match receiver.await {
        Ok(result) => T::lift_foreign_return(result.return_value, result.call_status),
        Err(e) => {
            // This shouldn't happen in practice, but we can do our best to recover
            T::handle_callback_unexpected_error(UnexpectedUniFFICallbackError::new(format!(
                "Error awaiting foreign future: {e}"
            )))
        }
    }
}

pub extern "C" fn foreign_future_complete<T: LiftReturn<UT>, UT>(
    oneshot_handle: u64,
    result: ForeignFutureResult<T::ReturnType>,
) {
    let channel = unsafe { oneshot::Sender::from_raw(oneshot_handle as *mut ()) };
    // Ignore errors in send.
    //
    // Error means the receiver was already dropped which will happen when the future is cancelled.
    let _ = channel.send(result);
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::{Lower, RustBuffer};
    use once_cell::sync::OnceCell;
    use std::{
        future::Future,
        pin::Pin,
        sync::{
            atomic::{AtomicU32, Ordering},
            Arc,
        },
        task::{Context, Poll, Wake},
    };

    struct MockForeignFuture {
        freed: Arc<AtomicU32>,
        callback_info: Arc<OnceCell<(ForeignFutureCallback<RustBuffer>, u64)>>,
        rust_future: Option<Pin<Box<dyn Future<Output = String>>>>,
    }

    impl MockForeignFuture {
        fn new() -> Self {
            let callback_info = Arc::new(OnceCell::new());
            let freed = Arc::new(AtomicU32::new(0));

            let rust_future: Pin<Box<dyn Future<Output = String>>> = {
                let callback_info = callback_info.clone();
                let freed = freed.clone();
                Box::pin(foreign_async_call::<_, String, crate::UniFfiTag>(
                    move |callback, data| {
                        callback_info.set((callback, data)).unwrap();
                        ForeignFuture {
                            handle: Arc::into_raw(freed) as *mut () as u64,
                            free: Self::free,
                        }
                    },
                ))
            };
            let rust_future = Some(rust_future);
            let mut mock_foreign_future = Self {
                freed,
                callback_info,
                rust_future,
            };
            // Poll the future once, to start it up.   This ensures that `callback_info` is set.
            let _ = mock_foreign_future.poll();
            mock_foreign_future
        }

        fn poll(&mut self) -> Poll<String> {
            let waker = Arc::new(NoopWaker).into();
            let mut context = Context::from_waker(&waker);
            self.rust_future
                .as_mut()
                .unwrap()
                .as_mut()
                .poll(&mut context)
        }

        fn complete_success(&self, value: String) {
            let (callback, data) = self.callback_info.get().unwrap();
            callback(
                *data,
                ForeignFutureResult {
                    return_value: <String as Lower<crate::UniFfiTag>>::lower(value),
                    call_status: RustCallStatus::new(),
                },
            );
        }

        fn complete_error(&self, error_message: String) {
            let (callback, data) = self.callback_info.get().unwrap();
            callback(
                *data,
                ForeignFutureResult {
                    return_value: RustBuffer::default(),
                    call_status: RustCallStatus::error(error_message),
                },
            );
        }

        fn drop_future(&mut self) {
            self.rust_future = None
        }

        fn free_count(&self) -> u32 {
            self.freed.load(Ordering::Relaxed)
        }

        extern "C" fn free(handle: u64) {
            let flag = unsafe { Arc::from_raw(handle as *mut AtomicU32) };
            flag.fetch_add(1, Ordering::Relaxed);
        }
    }

    struct NoopWaker;

    impl Wake for NoopWaker {
        fn wake(self: Arc<Self>) {}
    }

    #[test]
    fn test_foreign_future() {
        let mut mock_foreign_future = MockForeignFuture::new();
        assert_eq!(mock_foreign_future.poll(), Poll::Pending);
        mock_foreign_future.complete_success("It worked!".to_owned());
        assert_eq!(
            mock_foreign_future.poll(),
            Poll::Ready("It worked!".to_owned())
        );
        // Since the future is complete, it should free the foreign future
        assert_eq!(mock_foreign_future.free_count(), 1);
    }

    #[test]
    #[should_panic]
    fn test_foreign_future_error() {
        let mut mock_foreign_future = MockForeignFuture::new();
        assert_eq!(mock_foreign_future.poll(), Poll::Pending);
        mock_foreign_future.complete_error("It Failed!".to_owned());
        let _ = mock_foreign_future.poll();
    }

    #[test]
    fn test_drop_after_complete() {
        let mut mock_foreign_future = MockForeignFuture::new();
        mock_foreign_future.complete_success("It worked!".to_owned());
        assert_eq!(mock_foreign_future.free_count(), 0);
        assert_eq!(
            mock_foreign_future.poll(),
            Poll::Ready("It worked!".to_owned())
        );
        // Dropping the future after it's complete should not panic, and not cause a double-free
        mock_foreign_future.drop_future();
        assert_eq!(mock_foreign_future.free_count(), 1);
    }

    #[test]
    fn test_drop_before_complete() {
        let mut mock_foreign_future = MockForeignFuture::new();
        mock_foreign_future.complete_success("It worked!".to_owned());
        // Dropping the future before it's complete should cancel the future
        assert_eq!(mock_foreign_future.free_count(), 0);
        mock_foreign_future.drop_future();
        assert_eq!(mock_foreign_future.free_count(), 1);
    }
}
