/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Schedule tasks using a foreign executor.

use std::panic;

use crate::{ForeignExecutorCallback, ForeignExecutorCallbackCell};

/// Opaque handle for a foreign task executor.
///
/// Foreign code can either use an actual pointer, or use an integer value casted to it.
#[repr(transparent)]
#[derive(Clone, Copy, Debug)]
pub struct ForeignExecutorHandle(pub(crate) *const ());

// Implement Send + Sync for `ForeignExecutor`.  The foreign bindings code is responsible for
// making the `ForeignExecutorCallback` thread-safe.
unsafe impl Send for ForeignExecutorHandle {}

unsafe impl Sync for ForeignExecutorHandle {}

/// Result code returned by `ForeignExecutorCallback`
#[repr(i8)]
#[derive(Debug, PartialEq, Eq)]
pub enum ForeignExecutorCallbackResult {
    /// Callback was scheduled successfully
    Success = 0,
    /// Callback couldn't be scheduled because the foreign executor is canceled/closed.
    Cancelled = 1,
    /// Callback couldn't be scheduled because of some other error
    Error = 2,
}

impl ForeignExecutorCallbackResult {
    /// Check the result code for the foreign executor callback
    ///
    /// If the result was `ForeignExecutorCallbackResult.Success`, this method returns `true`.
    ///
    /// If not, this method returns `false`, logging errors for any unexpected return values
    pub fn check_result_code(result: i8) -> bool {
        match result {
            n if n == ForeignExecutorCallbackResult::Success as i8 => true,
            n if n == ForeignExecutorCallbackResult::Cancelled as i8 => false,
            n if n == ForeignExecutorCallbackResult::Error as i8 => {
                log::error!(
                    "ForeignExecutorCallbackResult::Error returned by foreign executor callback"
                );
                false
            }
            n => {
                log::error!("Unknown code ({n}) returned by foreign executor callback");
                false
            }
        }
    }
}

// Option<RustTaskCallback> should use the null pointer optimization and be represented in C as a
// regular pointer.  Let's check that.
static_assertions::assert_eq_size!(usize, Option<RustTaskCallback>);

/// Callback for a Rust task, this is what the foreign executor invokes
///
/// The task will be passed the `task_data` passed to `ForeignExecutorCallback` in addition to one
/// of the `RustTaskCallbackCode` values.
pub type RustTaskCallback = extern "C" fn(*const (), RustTaskCallbackCode);

/// Passed to a `RustTaskCallback` function when the executor invokes them.
///
/// Every `RustTaskCallback` will be invoked eventually, this code is used to distinguish the times
/// when it's invoked successfully vs times when the callback is being called because the foreign
/// executor has been cancelled / shutdown
#[repr(i8)]
#[derive(Debug, PartialEq, Eq)]
pub enum RustTaskCallbackCode {
    /// Successful task callback invocation
    Success = 0,
    /// The `ForeignExecutor` has been cancelled.
    ///
    /// This signals that any progress using the executor should be halted.  In particular, Futures
    /// should not continue to progress.
    Cancelled = 1,
}

static FOREIGN_EXECUTOR_CALLBACK: ForeignExecutorCallbackCell = ForeignExecutorCallbackCell::new();

/// Set the global ForeignExecutorCallback.  This is called by the foreign bindings, normally
/// during initialization.
pub fn foreign_executor_callback_set(callback: ForeignExecutorCallback) {
    FOREIGN_EXECUTOR_CALLBACK.set(callback);
}

/// Schedule Rust calls using a foreign executor
#[derive(Debug)]
pub struct ForeignExecutor {
    pub(crate) handle: ForeignExecutorHandle,
}

impl ForeignExecutor {
    pub fn new(executor: ForeignExecutorHandle) -> Self {
        Self { handle: executor }
    }

    /// Schedule a closure to be run.
    ///
    /// This method can be used for "fire-and-forget" style calls, where the calling code doesn't
    /// need to await the result.
    ///
    /// Closure requirements:
    ///   - Send: since the closure will likely run on a different thread
    ///   - 'static: since it runs at an arbitrary time, so all references need to be 'static
    ///   - panic::UnwindSafe: if the closure panics, it should not corrupt any data
    pub fn schedule<F: FnOnce() + Send + 'static + panic::UnwindSafe>(&self, delay: u32, task: F) {
        let leaked_ptr: *mut F = Box::leak(Box::new(task));
        if !schedule_raw(
            self.handle,
            delay,
            schedule_callback::<F>,
            leaked_ptr as *const (),
        ) {
            // If schedule_raw() failed, drop the leaked box since `schedule_callback()` has not been
            // scheduled to run.
            unsafe {
                drop(Box::<F>::from_raw(leaked_ptr));
            };
        }
    }

    /// Schedule a closure to be run and get a Future for the result
    ///
    /// Closure requirements:
    ///   - Send: since the closure will likely run on a different thread
    ///   - 'static: since it runs at an arbitrary time, so all references need to be 'static
    ///   - panic::UnwindSafe: if the closure panics, it should not corrupt any data
    pub async fn run<F, T>(&self, delay: u32, closure: F) -> T
    where
        F: FnOnce() -> T + Send + 'static + panic::UnwindSafe,
        T: Send + 'static,
    {
        // Create a oneshot channel to handle the future
        let (sender, receiver) = oneshot::channel();
        // We can use `AssertUnwindSafe` here because:
        //   - The closure is unwind safe
        //   - `Sender` is not marked unwind safe, maybe this is just an oversight in the oneshot
        //     library.  However, calling `send()` and dropping the Sender should certainly be
        //     unwind safe.  `send()` should probably not panic at all and if it does it shouldn't
        //     do it in a way that breaks the Receiver.
        //   - Calling `expect` may result in a panic, but this should should not break either the
        //     Sender or Receiver.
        self.schedule(
            delay,
            panic::AssertUnwindSafe(move || {
                sender.send(closure()).expect("Error sending future result")
            }),
        );
        receiver.await.expect("Error receiving future result")
    }
}

/// Low-level schedule interface
///
/// When using this function, take care to ensure that the `ForeignExecutor` that holds the
/// `ForeignExecutorHandle` has not been dropped.
///
/// Returns true if the callback was successfully scheduled
pub(crate) fn schedule_raw(
    handle: ForeignExecutorHandle,
    delay: u32,
    callback: RustTaskCallback,
    data: *const (),
) -> bool {
    let result_code = (FOREIGN_EXECUTOR_CALLBACK.get())(handle, delay, Some(callback), data);
    ForeignExecutorCallbackResult::check_result_code(result_code)
}

impl Drop for ForeignExecutor {
    fn drop(&mut self) {
        (FOREIGN_EXECUTOR_CALLBACK.get())(self.handle, 0, None, std::ptr::null());
    }
}

extern "C" fn schedule_callback<F>(data: *const (), status_code: RustTaskCallbackCode)
where
    F: FnOnce() + Send + 'static + panic::UnwindSafe,
{
    // No matter what, we need to call Box::from_raw() to balance the Box::leak() call.
    let task = unsafe { Box::from_raw(data as *mut F) };
    // Skip running the task for the `RustTaskCallbackCode::Cancelled` code
    if status_code == RustTaskCallbackCode::Success {
        run_task(task);
    }
}

/// Run a scheduled task, catching any panics.
///
/// If there are panics, then we will log a warning and return None.
fn run_task<F: FnOnce() -> T + panic::UnwindSafe, T>(task: F) -> Option<T> {
    match panic::catch_unwind(task) {
        Ok(v) => Some(v),
        Err(cause) => {
            let message = if let Some(s) = cause.downcast_ref::<&'static str>() {
                (*s).to_string()
            } else if let Some(s) = cause.downcast_ref::<String>() {
                s.clone()
            } else {
                "Unknown panic!".to_string()
            };
            log::warn!("Error calling UniFFI callback function: {message}");
            None
        }
    }
}

#[cfg(test)]
pub use test::MockEventLoop;

#[cfg(test)]
mod test {
    use super::*;
    use std::{
        future::Future,
        pin::Pin,
        sync::{
            atomic::{AtomicU32, Ordering},
            Arc, Mutex, Once,
        },
        task::{Context, Poll, Wake, Waker},
    };

    /// Simulate an event loop / task queue / coroutine scope on the foreign side
    ///
    /// This simply collects scheduled calls into a Vec for testing purposes.
    ///
    /// Most of the MockEventLoop methods are `pub` since it's also used by the `rustfuture` tests.
    pub struct MockEventLoop {
        // Wrap everything in a mutex since we typically share access to MockEventLoop via an Arc
        inner: Mutex<MockEventLoopInner>,
    }

    pub struct MockEventLoopInner {
        // calls that have been scheduled
        calls: Vec<(u32, Option<RustTaskCallback>, *const ())>,
        // has the event loop been shutdown?
        is_shutdown: bool,
    }

    unsafe impl Send for MockEventLoopInner {}

    static FOREIGN_EXECUTOR_CALLBACK_INIT: Once = Once::new();

    impl MockEventLoop {
        pub fn new() -> Arc<Self> {
            // Make sure we install a foreign executor callback that can deal with mock event loops
            FOREIGN_EXECUTOR_CALLBACK_INIT
                .call_once(|| foreign_executor_callback_set(mock_executor_callback));

            Arc::new(Self {
                inner: Mutex::new(MockEventLoopInner {
                    calls: vec![],
                    is_shutdown: false,
                }),
            })
        }

        /// Create a new ForeignExecutorHandle
        pub fn new_handle(self: &Arc<Self>) -> ForeignExecutorHandle {
            // To keep the memory management simple, we simply leak an arc reference for this.  We
            // only create a handful of these in the tests so there's no need for proper cleanup.
            ForeignExecutorHandle(Arc::into_raw(Arc::clone(self)) as *const ())
        }

        pub fn new_executor(self: &Arc<Self>) -> ForeignExecutor {
            ForeignExecutor {
                handle: self.new_handle(),
            }
        }

        /// Get the current number of scheduled calls
        pub fn call_count(&self) -> usize {
            self.inner.lock().unwrap().calls.len()
        }

        /// Get the last scheduled call
        pub fn last_call(&self) -> (u32, Option<RustTaskCallback>, *const ()) {
            self.inner
                .lock()
                .unwrap()
                .calls
                .last()
                .cloned()
                .expect("no calls scheduled")
        }

        /// Run all currently scheduled calls
        pub fn run_all_calls(&self) {
            let mut inner = self.inner.lock().unwrap();
            let is_shutdown = inner.is_shutdown;
            for (_delay, callback, data) in inner.calls.drain(..) {
                if !is_shutdown {
                    callback.unwrap()(data, RustTaskCallbackCode::Success);
                } else {
                    callback.unwrap()(data, RustTaskCallbackCode::Cancelled);
                }
            }
        }

        /// Shutdown the eventloop, causing scheduled calls and future calls to be cancelled
        pub fn shutdown(&self) {
            self.inner.lock().unwrap().is_shutdown = true;
        }
    }

    // `ForeignExecutorCallback` that we install for testing
    extern "C" fn mock_executor_callback(
        handle: ForeignExecutorHandle,
        delay: u32,
        task: Option<RustTaskCallback>,
        task_data: *const (),
    ) -> i8 {
        let eventloop = handle.0 as *const MockEventLoop;
        let mut inner = unsafe { (*eventloop).inner.lock().unwrap() };
        if inner.is_shutdown {
            ForeignExecutorCallbackResult::Cancelled as i8
        } else {
            inner.calls.push((delay, task, task_data));
            ForeignExecutorCallbackResult::Success as i8
        }
    }

    #[test]
    fn test_schedule_raw() {
        extern "C" fn callback(data: *const (), _status_code: RustTaskCallbackCode) {
            unsafe {
                *(data as *mut u32) += 1;
            }
        }

        let eventloop = MockEventLoop::new();

        let value: u32 = 0;
        assert_eq!(eventloop.call_count(), 0);

        schedule_raw(
            eventloop.new_handle(),
            0,
            callback,
            &value as *const u32 as *const (),
        );
        assert_eq!(eventloop.call_count(), 1);
        assert_eq!(value, 0);

        eventloop.run_all_calls();
        assert_eq!(eventloop.call_count(), 0);
        assert_eq!(value, 1);
    }

    #[test]
    fn test_schedule() {
        let eventloop = MockEventLoop::new();
        let executor = eventloop.new_executor();
        let value = Arc::new(AtomicU32::new(0));
        assert_eq!(eventloop.call_count(), 0);

        let value2 = value.clone();
        executor.schedule(0, move || {
            value2.fetch_add(1, Ordering::Relaxed);
        });
        assert_eq!(eventloop.call_count(), 1);
        assert_eq!(value.load(Ordering::Relaxed), 0);

        eventloop.run_all_calls();
        assert_eq!(eventloop.call_count(), 0);
        assert_eq!(value.load(Ordering::Relaxed), 1);
    }

    #[derive(Default)]
    struct MockWaker {
        wake_count: AtomicU32,
    }

    impl Wake for MockWaker {
        fn wake(self: Arc<Self>) {
            self.wake_count.fetch_add(1, Ordering::Relaxed);
        }
    }

    #[test]
    fn test_run() {
        let eventloop = MockEventLoop::new();
        let executor = eventloop.new_executor();
        let mock_waker = Arc::new(MockWaker::default());
        let waker = Waker::from(mock_waker.clone());
        let mut context = Context::from_waker(&waker);
        assert_eq!(eventloop.call_count(), 0);

        let mut future = executor.run(0, move || "test-return-value");
        unsafe {
            assert_eq!(
                Pin::new_unchecked(&mut future).poll(&mut context),
                Poll::Pending
            );
        }
        assert_eq!(eventloop.call_count(), 1);
        assert_eq!(mock_waker.wake_count.load(Ordering::Relaxed), 0);

        eventloop.run_all_calls();
        assert_eq!(eventloop.call_count(), 0);
        assert_eq!(mock_waker.wake_count.load(Ordering::Relaxed), 1);
        unsafe {
            assert_eq!(
                Pin::new_unchecked(&mut future).poll(&mut context),
                Poll::Ready("test-return-value")
            );
        }
    }

    #[test]
    fn test_drop() {
        let eventloop = MockEventLoop::new();
        let executor = eventloop.new_executor();

        drop(executor);
        // Calling drop should schedule a call with null task data.
        assert_eq!(eventloop.call_count(), 1);
        assert_eq!(eventloop.last_call().1, None);
    }

    // Test that cancelled calls never run
    #[test]
    fn test_cancelled_call() {
        let eventloop = MockEventLoop::new();
        let executor = eventloop.new_executor();
        // Create a shared counter
        let counter = Arc::new(AtomicU32::new(0));
        // schedule increments using both `schedule()` and run()`
        let counter_clone = Arc::clone(&counter);
        executor.schedule(0, move || {
            counter_clone.fetch_add(1, Ordering::Relaxed);
        });
        let counter_clone = Arc::clone(&counter);
        let future = executor.run(0, move || {
            counter_clone.fetch_add(1, Ordering::Relaxed);
        });
        // shutdown the eventloop before the scheduled call gets a chance to run.
        eventloop.shutdown();
        // `run_all_calls()` will cause the scheduled task callbacks to run, but will pass
        // `RustTaskCallbackCode::Cancelled` to it.  This drop the scheduled closure without executing
        // it.
        eventloop.run_all_calls();

        assert_eq!(counter.load(Ordering::Relaxed), 0);
        drop(future);
    }

    // Test that when scheduled calls are cancelled, the closures are dropped properly
    #[test]
    fn test_cancellation_drops_closures() {
        let eventloop = MockEventLoop::new();
        let executor = eventloop.new_executor();

        // Create an Arc<> that we will move into the closures to test if they are dropped or not
        let arc = Arc::new(0);
        let arc_clone = Arc::clone(&arc);
        executor.schedule(0, move || assert_eq!(*arc_clone, 0));
        let arc_clone = Arc::clone(&arc);
        let future = executor.run(0, move || assert_eq!(*arc_clone, 0));

        // shutdown the eventloop and run the (cancelled) scheduled calls.
        eventloop.shutdown();
        eventloop.run_all_calls();
        // try to schedule some more calls now that the loop has been shutdown
        let arc_clone = Arc::clone(&arc);
        executor.schedule(0, move || assert_eq!(*arc_clone, 0));
        let arc_clone = Arc::clone(&arc);
        let future2 = executor.run(0, move || assert_eq!(*arc_clone, 0));

        // Drop the futures so they don't hold on to any references
        drop(future);
        drop(future2);

        // All of these closures should have been dropped by now, there only remaining arc
        // reference should be the original
        assert_eq!(Arc::strong_count(&arc), 1);
    }
}
