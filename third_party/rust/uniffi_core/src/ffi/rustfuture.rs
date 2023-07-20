/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! [`RustFuture`] represents a [`Future`] that can be sent to the foreign code over FFI.
//!
//! This type is not instantiated directly, but via the procedural macros, such as `#[uniffi::export]`.
//!
//! # The big picture
//!
//! What happens when you call an async function exported from the Rust API?
//!
//! 1. You make a call to a generated async function in the foreign bindings.
//! 2. That function suspends itself, then makes a scaffolding call.  In addition to the normal
//!    arguments, it passes a `ForeignExecutor` and callback.
//! 3. Rust uses the `ForeignExecutor` to schedules polls of the Future until it's ready.  Then
//!    invokes the callback.
//! 4. The callback resumes the suspended async function from (2), closing the loop.
//!
//! # Anatomy of an async call
//!
//! Let's consider the following Rust function:
//!
//! ```rust,ignore
//! #[uniffi::export]
//! async fn hello() -> bool {
//!     true
//! }
//! ```
//!
//! In Rust, this `async fn` syntax is strictly equivalent to a normal function that returns a
//! `Future`:
//!
//! ```rust,ignore
//! #[uniffi::export]
//! fn hello() -> impl Future<Output = bool> { /* … */ }
//! ```
//!
//! `uniffi-bindgen` will generate a scaffolding function for each exported async function:
//!
//! ```rust,ignore
//! // The `hello` function, as seen from the outside. It inputs 3 extra arguments:
//! //  - executor: used to schedule polls of the future
//! //  - callback: invoked when the future is ready
//! //  - callback_data: opaque pointer that's passed to the callback.  It points to any state needed to
//! //    resume the async function.
//! #[no_mangle]
//! pub extern "C" fn _uniffi_hello(
//!     // ...If the function inputted arguments, the lowered versions would go here
//!     uniffi_executor: ForeignExecutor,
//!     uniffi_callback: <bool as FfiConverter<crate::UniFFITag>>::FutureCallback,
//!     uniffi_callback_data: *const (),
//!     uniffi_call_status: &mut ::uniffi::RustCallStatus
//! ) {
//!     ::uniffi::call_with_output(uniffi_call_status, || {
//!         let uniffi_rust_future = RustFuture::<_, bool, crate::UniFFITag,>::new(
//!             future: hello(),
//!             executor,
//!             callback,
//!             callback_data,
//!         );
//!         uniffi_rust_future.wake();
//!     })
//! }
//! ```
//!
//! Rust will continue to poll the future until it's ready, after that.  The callback will
//! eventually be invoked with these arguments:
//!   - callback_data
//!   - FfiConverter::ReturnType (the type that would be returned by a sync function)
//!   - RustCallStatus (used to signal errors/panics when executing the future)
//! - Rust will stop polling the future, even if it's waker is invoked again.
//!
//! ## How does `Future` work exactly?
//!
//! A [`Future`] in Rust does nothing. When calling an async function, it just
//! returns a `Future` but nothing has happened yet. To start the computation,
//! the future must be polled. It returns [`Poll::Ready(r)`][`Poll::Ready`] if
//! the result is ready, [`Poll::Pending`] otherwise. `Poll::Pending` basically
//! means:
//!
//! > Please, try to poll me later, maybe the result will be ready!
//!
//! This model is very different than what other languages do, but it can actually
//! be translated quite easily, fortunately for us!
//!
//! But… wait a minute… who is responsible to poll the `Future` if a `Future` does
//! nothing? Well, it's _the executor_. The executor is responsible _to drive_ the
//! `Future`: that's where they are polled.
//!
//! But… wait another minute… how does the executor know when to poll a [`Future`]?
//! Does it poll them randomly in an endless loop? Well, no, actually it depends
//! on the executor! A well-designed `Future` and executor work as follows.
//! Normally, when [`Future::poll`] is called, a [`Context`] argument is
//! passed to it. It contains a [`Waker`]. The [`Waker`] is built on top of a
//! [`RawWaker`] which implements whatever is necessary. Usually, a waker will
//! signal the executor to poll a particular `Future`. A `Future` will clone
//! or pass-by-ref the waker to somewhere, as a callback, a completion, a
//! function, or anything, to the system that is responsible to notify when a
//! task is completed. So, to recap, the waker is _not_ responsible for waking the
//! `Future`, it _is_ responsible for _signaling_ the executor that a particular
//! `Future` should be polled again. That's why the documentation of
//! [`Poll::Pending`] specifies:
//!
//! > When a function returns `Pending`, the function must also ensure that the
//! > current task is scheduled to be awoken when progress can be made.
//!
//! “awakening” is done by using the `Waker`.
//!
//! [`Future`]: https://doc.rust-lang.org/std/future/trait.Future.html
//! [`Future::poll`]: https://doc.rust-lang.org/std/future/trait.Future.html#tymethod.poll
//! [`Pol::Ready`]: https://doc.rust-lang.org/std/task/enum.Poll.html#variant.Ready
//! [`Poll::Pending`]: https://doc.rust-lang.org/std/task/enum.Poll.html#variant.Pending
//! [`Context`]: https://doc.rust-lang.org/std/task/struct.Context.html
//! [`Waker`]: https://doc.rust-lang.org/std/task/struct.Waker.html
//! [`RawWaker`]: https://doc.rust-lang.org/std/task/struct.RawWaker.html

use crate::{
    rust_call_with_out_status, schedule_raw, FfiConverter, FfiDefault, ForeignExecutor,
    ForeignExecutorHandle, RustCallStatus,
};
use std::{
    cell::UnsafeCell,
    future::Future,
    panic,
    pin::Pin,
    sync::{
        atomic::{AtomicU32, Ordering},
        Arc,
    },
    task::{Context, Poll, RawWaker, RawWakerVTable, Waker},
};

/// Callback that we invoke when a `RustFuture` is ready.
///
/// The foreign code passes a pointer to one of these callbacks along with an opaque data pointer.
/// When the future is ready, we invoke the callback.
pub type FutureCallback<T> =
    extern "C" fn(callback_data: *const (), result: T, status: RustCallStatus);

/// Future that the foreign code is awaiting
///
/// RustFuture is always stored inside a Pin<Arc<>>.  The `Arc<>` allows it to be shared between
/// wakers and Pin<> signals that it must not move, since this would break any self-references in
/// the future.
pub struct RustFuture<F, T, UT>
where
    // The future needs to be `Send`, since it will move to whatever thread the foreign executor
    // chooses.  However, it doesn't need to be `Sync', since we don't share references between
    // threads (see do_wake()).
    F: Future<Output = T> + Send,
    T: FfiConverter<UT>,
{
    future: UnsafeCell<F>,
    executor: ForeignExecutor,
    wake_counter: AtomicU32,
    callback: T::FutureCallback,
    callback_data: *const (),
}

// Mark RustFuture as Send + Sync, since we will be sharing it between threads.  This means we need
// to serialize access to any fields that aren't Send + Sync (`future`, `callback`, and
// `callback_data`).  See `do_wake()` for details on this.

unsafe impl<F, T, UT> Send for RustFuture<F, T, UT>
where
    F: Future<Output = T> + Send,
    T: FfiConverter<UT>,
{
}

unsafe impl<F, T, UT> Sync for RustFuture<F, T, UT>
where
    F: Future<Output = T> + Send,
    T: FfiConverter<UT>,
{
}

impl<F, T, UT> RustFuture<F, T, UT>
where
    F: Future<Output = T> + Send,
    T: FfiConverter<UT>,
{
    pub fn new(
        future: F,
        executor_handle: ForeignExecutorHandle,
        callback: T::FutureCallback,
        callback_data: *const (),
    ) -> Pin<Arc<Self>> {
        let executor =
            <ForeignExecutor as FfiConverter<crate::UniFfiTag>>::try_lift(executor_handle)
                .expect("Error lifting ForeignExecutorHandle");
        Arc::pin(Self {
            future: UnsafeCell::new(future),
            wake_counter: AtomicU32::new(0),
            executor,
            callback,
            callback_data,
        })
    }

    /// Wake up soon and poll our future.
    ///
    /// This method ensures that a call to `do_wake()` is scheduled.  Only one call will be scheduled
    /// at any time, even if `wake_soon` called multiple times from multiple threads.
    pub fn wake(self: Pin<Arc<Self>>) {
        if self.wake_counter.fetch_add(1, Ordering::Relaxed) == 0 {
            self.schedule_do_wake();
        }
    }

    fn schedule_do_wake(self: Pin<Arc<Self>>) {
        unsafe {
            let handle = self.executor.handle;
            let raw_ptr = Arc::into_raw(Pin::into_inner_unchecked(self)) as *const ();
            // SAFETY: The `into_raw()` / `from_raw()` contract guarantees that our executor cannot
            // be dropped before we call `from_raw()` on the raw pointer. This means we can safely
            // use its handle to schedule a callback.
            schedule_raw(handle, 0, Self::wake_callback, raw_ptr);
        }
    }

    extern "C" fn wake_callback(self_ptr: *const ()) {
        unsafe {
            Pin::new_unchecked(Arc::from_raw(self_ptr as *const Self)).do_wake();
        };
    }

    // Does the work for wake, we take care to ensure this always runs in a serialized fashion.
    fn do_wake(self: Pin<Arc<Self>>) {
        // Store 1 in `waker_counter`, which we'll use at the end of this call.
        self.wake_counter.store(1, Ordering::Relaxed);

        // Pin<&mut> from our UnsafeCell.  &mut is is safe, since this is the only reference we
        // ever take to `self.future` and calls to this function are serialized.  Pin<> is safe
        // since we never move the future out of `self.future`.
        let future = unsafe { Pin::new_unchecked(&mut *self.future.get()) };
        let waker = self.make_waker();

        // Run the poll and lift the result if it's ready
        let mut out_status = RustCallStatus::default();
        let result: Option<Poll<T::ReturnType>> = rust_call_with_out_status(
            &mut out_status,
            // This closure uses a `&mut F` value, which means it's not UnwindSafe by default.  If
            // the closure panics, the future may be in an invalid state.
            //
            // However, we can safely use `AssertUnwindSafe` since a panic will lead the `Err()`
            // case below.  In that case, we will never run `do_wake()` again and will no longer
            // access the future.
            panic::AssertUnwindSafe(|| match future.poll(&mut Context::from_waker(&waker)) {
                Poll::Pending => Ok(Poll::Pending),
                Poll::Ready(v) => T::lower_return(v).map(Poll::Ready),
            }),
        );

        // All the main work is done, time to finish up
        match result {
            Some(Poll::Pending) => {
                // Since we set `wake_counter` to 1 at the start of this function...
                //   - If it's > 1 now, then some other code tried to wake us while we were polling
                //     the future.  Schedule another poll in this case.
                //   - If it's still 1, then exit after decrementing it.  The next call to `wake()`
                //     will schedule a poll.
                if self.wake_counter.fetch_sub(1, Ordering::Relaxed) > 1 {
                    self.schedule_do_wake();
                }
            }
            // Success!  Call our callback.
            //
            // Don't decrement `wake_counter'.  This way, if wake() is called in the future, we
            // will just ignore it
            Some(Poll::Ready(v)) => {
                T::invoke_future_callback(self.callback, self.callback_data, v, out_status);
            }
            // Error/panic polling the future.  Call the callback with a default value.
            // `out_status` contains the error code and serialized error.  Again, don't decrement
            // `wake_counter'.
            None => {
                T::invoke_future_callback(
                    self.callback,
                    self.callback_data,
                    T::ReturnType::ffi_default(),
                    out_status,
                );
            }
        };
    }

    fn make_waker(self: &Pin<Arc<Self>>) -> Waker {
        // This is safe as long as we implement the waker interface correctly.
        unsafe {
            Waker::from_raw(RawWaker::new(
                self.clone().into_raw(),
                &Self::RAW_WAKER_VTABLE,
            ))
        }
    }

    /// SAFETY: Ensure that all calls to `into_raw()` are balanced with a call to `from_raw()`
    fn into_raw(self: Pin<Arc<Self>>) -> *const () {
        unsafe { Arc::into_raw(Pin::into_inner_unchecked(self)) as *const () }
    }

    /// Consume a pointer to get an arc
    ///
    /// SAFETY: The pointer must have come from `into_raw()` or was cloned with `raw_clone()`.
    /// Once a pointer is passed into this function, it should no longer be used.
    fn from_raw(self_ptr: *const ()) -> Pin<Arc<Self>> {
        unsafe { Pin::new_unchecked(Arc::from_raw(self_ptr as *const Self)) }
    }

    // Implement the waker interface by defining a RawWakerVTable
    //
    // We could also handle this by implementing the `Wake` interface, but that uses an `Arc<T>`
    // instead of a `Pin<Arc<T>>` and in theory it could try to move the `T` value out of the arc
    // with something like `Arc::try_unwrap()` which would break the pinning contract with
    // `Future`.  Implementing this using `RawWakerVTable` allows us verify that this doesn't
    // happen.
    const RAW_WAKER_VTABLE: RawWakerVTable = RawWakerVTable::new(
        Self::raw_clone,
        Self::raw_wake,
        Self::raw_wake_by_ref,
        Self::raw_drop,
    );

    /// This function will be called when the `RawWaker` gets cloned, e.g. when
    /// the `Waker` in which the `RawWaker` is stored gets cloned.
    unsafe fn raw_clone(self_ptr: *const ()) -> RawWaker {
        Arc::increment_strong_count(self_ptr as *const Self);
        RawWaker::new(self_ptr, &Self::RAW_WAKER_VTABLE)
    }

    /// This function will be called when `wake` is called on the `Waker`. It
    /// must wake up the task associated with this `RawWaker`.
    unsafe fn raw_wake(self_ptr: *const ()) {
        Self::from_raw(self_ptr).wake()
    }

    /// This function will be called when `wake_by_ref` is called on the
    /// `Waker`. It must wake up the task associated with this `RawWaker`.
    unsafe fn raw_wake_by_ref(self_ptr: *const ()) {
        // This could be optimized by only incrementing the strong count if we end up calling
        // `schedule_do_wake()`, but it's not clear that's worth the extra complexity
        Arc::increment_strong_count(self_ptr as *const Self);
        Self::from_raw(self_ptr).wake()
    }

    /// This function gets called when a `RawWaker` gets dropped.
    /// This function gets called when a `RawWaker` gets dropped.
    unsafe fn raw_drop(self_ptr: *const ()) {
        drop(Self::from_raw(self_ptr))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{try_lift_from_rust_buffer, MockExecutor};
    use std::sync::Weak;

    // Mock future that we can manually control using an Option<>
    struct MockFuture(Option<Result<bool, String>>);

    impl Future for MockFuture {
        type Output = Result<bool, String>;

        fn poll(self: Pin<&mut Self>, _context: &mut Context<'_>) -> Poll<Self::Output> {
            match &self.0 {
                Some(v) => Poll::Ready(v.clone()),
                None => Poll::Pending,
            }
        }
    }

    // Type alias for the RustFuture we'll use in our tests
    type TestRustFuture = RustFuture<MockFuture, Result<bool, String>, crate::UniFfiTag>;

    // Stores the result that we send to the foreign code
    #[derive(Default)]
    struct MockForeignResult {
        value: i8,
        status: RustCallStatus,
    }

    extern "C" fn mock_foreign_callback(data_ptr: *const (), value: i8, status: RustCallStatus) {
        println!("mock_foreign_callback: {value} {data_ptr:?}");
        let result: &mut Option<MockForeignResult> =
            unsafe { &mut *(data_ptr as *mut Option<MockForeignResult>) };
        *result = Some(MockForeignResult { value, status });
    }

    // Bundles everything together so that we can run some tests
    struct TestFutureEnvironment {
        rust_future: Pin<Arc<TestRustFuture>>,
        executor: MockExecutor,
        foreign_result: Pin<Box<Option<MockForeignResult>>>,
    }

    impl TestFutureEnvironment {
        fn new() -> Self {
            let foreign_result = Box::pin(None);
            let foreign_result_ptr = &*foreign_result as *const Option<_> as *const ();
            let executor = MockExecutor::new();
            let rust_future = TestRustFuture::new(
                MockFuture(None),
                executor.handle().unwrap(),
                mock_foreign_callback,
                foreign_result_ptr,
            );
            Self {
                executor,
                rust_future,
                foreign_result,
            }
        }

        fn scheduled_call_count(&self) -> usize {
            self.executor.call_count()
        }

        fn run_scheduled_calls(&self) {
            self.executor.run_all_calls();
        }

        fn wake(&self) {
            self.rust_future.clone().wake();
        }

        fn rust_future_weak(&self) -> Weak<TestRustFuture> {
            // It seems like there's not a great way to get an &Arc from a Pin<Arc>, so we need to
            // do a little dance here
            Arc::downgrade(&Pin::into_inner(Clone::clone(&self.rust_future)))
        }

        fn complete_future(&self, value: Result<bool, String>) {
            unsafe {
                (*self.rust_future.future.get()).0 = Some(value);
            }
        }
    }

    #[test]
    fn test_wake() {
        let mut test_env = TestFutureEnvironment::new();
        // Initially, we shouldn't have a result and nothing should be scheduled
        assert!(test_env.foreign_result.is_none());
        assert_eq!(test_env.scheduled_call_count(), 0);

        // wake() should schedule a call
        test_env.wake();
        assert_eq!(test_env.scheduled_call_count(), 1);

        // When that call runs, we should still not have a result yet
        test_env.run_scheduled_calls();
        assert!(test_env.foreign_result.is_none());
        assert_eq!(test_env.scheduled_call_count(), 0);

        // Multiple wakes should only result in 1 scheduled call
        test_env.wake();
        test_env.wake();
        assert_eq!(test_env.scheduled_call_count(), 1);

        // Make the future ready, which should call mock_foreign_callback and set the result
        test_env.complete_future(Ok(true));
        test_env.run_scheduled_calls();
        let result = test_env
            .foreign_result
            .take()
            .expect("Expected result to be set");
        assert_eq!(result.value, 1);
        assert_eq!(result.status.code, 0);
        assert_eq!(test_env.scheduled_call_count(), 0);

        // Future wakes shouldn't schedule any calls
        test_env.wake();
        assert_eq!(test_env.scheduled_call_count(), 0);
    }

    #[test]
    fn test_error() {
        let mut test_env = TestFutureEnvironment::new();
        test_env.complete_future(Err("Something went wrong".into()));
        test_env.wake();
        test_env.run_scheduled_calls();
        let result = test_env
            .foreign_result
            .take()
            .expect("Expected result to be set");
        assert_eq!(result.status.code, 1);
        unsafe {
            assert_eq!(
                try_lift_from_rust_buffer::<String, crate::UniFfiTag>(
                    result.status.error_buf.assume_init()
                )
                .unwrap(),
                String::from("Something went wrong"),
            )
        }
        assert_eq!(test_env.scheduled_call_count(), 0);
    }

    #[test]
    fn test_raw_clone_and_drop() {
        let test_env = TestFutureEnvironment::new();
        let waker = test_env.rust_future.make_waker();
        let weak_ref = test_env.rust_future_weak();
        assert_eq!(weak_ref.strong_count(), 2);
        let waker2 = waker.clone();
        assert_eq!(weak_ref.strong_count(), 3);
        drop(waker);
        assert_eq!(weak_ref.strong_count(), 2);
        drop(waker2);
        assert_eq!(weak_ref.strong_count(), 1);
        drop(test_env);
        assert_eq!(weak_ref.strong_count(), 0);
        assert!(weak_ref.upgrade().is_none());
    }

    #[test]
    fn test_raw_wake() {
        let test_env = TestFutureEnvironment::new();
        let waker = test_env.rust_future.make_waker();
        let weak_ref = test_env.rust_future_weak();
        // `test_env` and `waker` both hold a strong reference to the `RustFuture`
        assert_eq!(weak_ref.strong_count(), 2);

        // wake_by_ref() should schedule a wake
        waker.wake_by_ref();
        assert_eq!(test_env.scheduled_call_count(), 1);

        // Once the wake runs, the strong could should not have changed
        test_env.run_scheduled_calls();
        assert_eq!(weak_ref.strong_count(), 2);

        // wake() should schedule a wake
        waker.wake();
        assert_eq!(test_env.scheduled_call_count(), 1);

        // Once the wake runs, the strong have decremented, since wake() consumes the waker
        test_env.run_scheduled_calls();
        assert_eq!(weak_ref.strong_count(), 1);
    }
}
