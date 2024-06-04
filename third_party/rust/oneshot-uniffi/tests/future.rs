#![cfg(feature = "async")]

use core::{future, mem, pin, task};

#[cfg(loom)]
pub use loom::sync::{Arc, Mutex};
#[cfg(not(loom))]
pub use std::sync::{Arc, Mutex};

mod helpers;
use helpers::maybe_loom_model;

#[test]
fn multiple_receiver_polls_keeps_only_latest_waker() {
    #[derive(Default)]
    struct MockWaker {
        cloned: usize,
        dropped: usize,
    }

    fn clone_mock_waker(waker: *const ()) -> task::RawWaker {
        let mock_waker = unsafe { Arc::from_raw(waker as *const Mutex<MockWaker>) };
        mock_waker.lock().unwrap().cloned += 1;
        let new_waker =
            task::RawWaker::new(Arc::into_raw(mock_waker.clone()) as *const (), &VTABLE);
        mem::forget(mock_waker);
        new_waker
    }

    fn drop_mock_waker(waker: *const ()) {
        let mock_waker = unsafe { Arc::from_raw(waker as *const Mutex<MockWaker>) };
        mock_waker.lock().unwrap().dropped += 1;
    }

    const VTABLE: task::RawWakerVTable =
        task::RawWakerVTable::new(clone_mock_waker, |_| (), |_| (), drop_mock_waker);

    maybe_loom_model(|| {
        let mock_waker1 = Arc::new(Mutex::new(MockWaker::default()));
        let raw_waker1 =
            task::RawWaker::new(Arc::into_raw(mock_waker1.clone()) as *const (), &VTABLE);
        let waker1 = unsafe { task::Waker::from_raw(raw_waker1) };
        let mut context1 = task::Context::from_waker(&waker1);

        let (_sender, mut receiver) = oneshot::channel::<()>();

        let poll_result = future::Future::poll(pin::Pin::new(&mut receiver), &mut context1);
        assert_eq!(poll_result, task::Poll::Pending);
        assert_eq!(mock_waker1.lock().unwrap().cloned, 1);
        assert_eq!(mock_waker1.lock().unwrap().dropped, 0);

        let mock_waker2 = Arc::new(Mutex::new(MockWaker::default()));
        let raw_waker2 =
            task::RawWaker::new(Arc::into_raw(mock_waker2.clone()) as *const (), &VTABLE);
        let waker2 = unsafe { task::Waker::from_raw(raw_waker2) };
        let mut context2 = task::Context::from_waker(&waker2);

        let poll_result = future::Future::poll(pin::Pin::new(&mut receiver), &mut context2);
        assert_eq!(poll_result, task::Poll::Pending);
        assert_eq!(mock_waker2.lock().unwrap().cloned, 1);
        assert_eq!(mock_waker2.lock().unwrap().dropped, 0);
        assert_eq!(mock_waker1.lock().unwrap().cloned, 1);
        assert_eq!(mock_waker1.lock().unwrap().dropped, 1);
    });
}
