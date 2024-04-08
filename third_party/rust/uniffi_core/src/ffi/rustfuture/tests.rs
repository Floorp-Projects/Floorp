use once_cell::sync::OnceCell;
use std::{
    future::Future,
    panic,
    pin::Pin,
    sync::{Arc, Mutex},
    task::{Context, Poll, Waker},
};

use super::*;
use crate::{test_util::TestError, Lift, RustBuffer, RustCallStatusCode};

// Sender/Receiver pair that we use for testing
struct Channel {
    result: Option<Result<String, TestError>>,
    waker: Option<Waker>,
}

struct Sender(Arc<Mutex<Channel>>);

impl Sender {
    fn wake(&self) {
        let inner = self.0.lock().unwrap();
        if let Some(waker) = &inner.waker {
            waker.wake_by_ref();
        }
    }

    fn send(&self, value: Result<String, TestError>) {
        let mut inner = self.0.lock().unwrap();
        if inner.result.replace(value).is_some() {
            panic!("value already sent");
        }
        if let Some(waker) = &inner.waker {
            waker.wake_by_ref();
        }
    }
}

struct Receiver(Arc<Mutex<Channel>>);

impl Future for Receiver {
    type Output = Result<String, TestError>;

    fn poll(self: Pin<&mut Self>, context: &mut Context<'_>) -> Poll<Result<String, TestError>> {
        let mut inner = self.0.lock().unwrap();
        match &inner.result {
            Some(v) => Poll::Ready(v.clone()),
            None => {
                inner.waker = Some(context.waker().clone());
                Poll::Pending
            }
        }
    }
}

// Create a sender and rust future that we can use for testing
fn channel() -> (Sender, Arc<dyn RustFutureFfi<RustBuffer>>) {
    let channel = Arc::new(Mutex::new(Channel {
        result: None,
        waker: None,
    }));
    let rust_future = RustFuture::new(Receiver(channel.clone()), crate::UniFfiTag);
    (Sender(channel), rust_future)
}

/// Poll a Rust future and get an OnceCell that's set when the continuation is called
fn poll(rust_future: &Arc<dyn RustFutureFfi<RustBuffer>>) -> Arc<OnceCell<RustFuturePoll>> {
    let cell = Arc::new(OnceCell::new());
    let handle = Arc::into_raw(cell.clone()) as u64;
    rust_future.clone().ffi_poll(poll_continuation, handle);
    cell
}

extern "C" fn poll_continuation(data: u64, code: RustFuturePoll) {
    let cell = unsafe { Arc::from_raw(data as *const OnceCell<RustFuturePoll>) };
    cell.set(code).expect("Error setting OnceCell");
}

fn complete(rust_future: Arc<dyn RustFutureFfi<RustBuffer>>) -> (RustBuffer, RustCallStatus) {
    let mut out_status_code = RustCallStatus::default();
    let return_value = rust_future.ffi_complete(&mut out_status_code);
    (return_value, out_status_code)
}

#[test]
fn test_success() {
    let (sender, rust_future) = channel();

    // Test polling the rust future before it's ready
    let continuation_result = poll(&rust_future);
    assert_eq!(continuation_result.get(), None);
    sender.wake();
    assert_eq!(continuation_result.get(), Some(&RustFuturePoll::MaybeReady));

    // Test polling the rust future when it's ready
    let continuation_result = poll(&rust_future);
    assert_eq!(continuation_result.get(), None);
    sender.send(Ok("All done".into()));
    assert_eq!(continuation_result.get(), Some(&RustFuturePoll::MaybeReady));

    // Future polls should immediately return ready
    let continuation_result = poll(&rust_future);
    assert_eq!(continuation_result.get(), Some(&RustFuturePoll::Ready));

    // Complete the future
    let (return_buf, call_status) = complete(rust_future);
    assert_eq!(call_status.code, RustCallStatusCode::Success);
    assert_eq!(
        <String as Lift<crate::UniFfiTag>>::try_lift(return_buf).unwrap(),
        "All done"
    );
}

#[test]
fn test_error() {
    let (sender, rust_future) = channel();

    let continuation_result = poll(&rust_future);
    assert_eq!(continuation_result.get(), None);
    sender.send(Err("Something went wrong".into()));
    assert_eq!(continuation_result.get(), Some(&RustFuturePoll::MaybeReady));

    let continuation_result = poll(&rust_future);
    assert_eq!(continuation_result.get(), Some(&RustFuturePoll::Ready));

    let (_, call_status) = complete(rust_future);
    assert_eq!(call_status.code, RustCallStatusCode::Error);
    unsafe {
        assert_eq!(
            <TestError as Lift<crate::UniFfiTag>>::try_lift_from_rust_buffer(
                call_status.error_buf.assume_init()
            )
            .unwrap(),
            TestError::from("Something went wrong"),
        )
    }
}

// Once `complete` is called, the inner future should be released, even if wakers still hold a
// reference to the RustFuture
#[test]
fn test_cancel() {
    let (_sender, rust_future) = channel();

    let continuation_result = poll(&rust_future);
    assert_eq!(continuation_result.get(), None);
    rust_future.ffi_cancel();
    // Cancellation should immediately invoke the callback with RustFuturePoll::Ready
    assert_eq!(continuation_result.get(), Some(&RustFuturePoll::Ready));

    // Future polls should immediately invoke the callback with RustFuturePoll::Ready
    let continuation_result = poll(&rust_future);
    assert_eq!(continuation_result.get(), Some(&RustFuturePoll::Ready));

    let (_, call_status) = complete(rust_future);
    assert_eq!(call_status.code, RustCallStatusCode::Cancelled);
}

// Once `free` is called, the inner future should be released, even if wakers still hold a
// reference to the RustFuture
#[test]
fn test_release_future() {
    let (sender, rust_future) = channel();
    // Create a weak reference to the channel to use to check if rust_future has dropped its
    // future.
    let channel_weak = Arc::downgrade(&sender.0);
    drop(sender);
    // Create an extra ref to rust_future, simulating a waker that still holds a reference to
    // it
    let rust_future2 = rust_future.clone();

    // Complete the rust future
    rust_future.ffi_free();
    // Even though rust_future is still alive, the channel shouldn't be
    assert!(Arc::strong_count(&rust_future2) > 0);
    assert_eq!(channel_weak.strong_count(), 0);
    assert!(channel_weak.upgrade().is_none());
}

// If `free` is called with a continuation still stored, we should call it them then.
//
// This shouldn't happen in practice, but it seems like good defensive programming
#[test]
fn test_complete_with_stored_continuation() {
    let (_sender, rust_future) = channel();

    let continuation_result = poll(&rust_future);
    rust_future.ffi_free();
    assert_eq!(continuation_result.get(), Some(&RustFuturePoll::Ready));
}

// Test what happens if we see a `wake()` call while we're polling the future.  This can
// happen, for example, with futures that are handled by a tokio thread pool.  We should
// schedule another poll of the future in this case.
#[test]
fn test_wake_during_poll() {
    let mut first_time = true;
    let future = std::future::poll_fn(move |ctx| {
        if first_time {
            first_time = false;
            // Wake the future while we are in the middle of polling it
            ctx.waker().clone().wake();
            Poll::Pending
        } else {
            // The second time we're polled, we're ready
            Poll::Ready("All done".to_owned())
        }
    });
    let rust_future: Arc<dyn RustFutureFfi<RustBuffer>> = RustFuture::new(future, crate::UniFfiTag);
    let continuation_result = poll(&rust_future);
    // The continuation function should called immediately
    assert_eq!(continuation_result.get(), Some(&RustFuturePoll::MaybeReady));
    // A second poll should finish the future
    let continuation_result = poll(&rust_future);
    assert_eq!(continuation_result.get(), Some(&RustFuturePoll::Ready));
    let (return_buf, call_status) = complete(rust_future);
    assert_eq!(call_status.code, RustCallStatusCode::Success);
    assert_eq!(
        <String as Lift<crate::UniFfiTag>>::try_lift(return_buf).unwrap(),
        "All done"
    );
}
