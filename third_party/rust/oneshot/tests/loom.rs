#![cfg(loom)]

use oneshot::TryRecvError;

use loom::hint;
use loom::thread;
#[cfg(feature = "async")]
use std::future::Future;
#[cfg(feature = "async")]
use std::pin::Pin;
#[cfg(feature = "async")]
use std::task::{self, Poll};
#[cfg(feature = "std")]
use std::time::Duration;

mod helpers;

#[test]
fn try_recv() {
    loom::model(|| {
        let (sender, receiver) = oneshot::channel::<u128>();

        let t = thread::spawn(move || loop {
            match receiver.try_recv() {
                Ok(msg) => break msg,
                Err(TryRecvError::Empty) => hint::spin_loop(),
                Err(TryRecvError::Disconnected) => panic!("Should not be disconnected"),
            }
        });

        assert!(sender.send(19).is_ok());
        assert_eq!(t.join().unwrap(), 19);
    })
}

#[cfg(feature = "std")]
#[test]
fn send_recv_different_threads() {
    loom::model(|| {
        let (sender, receiver) = oneshot::channel();
        let t2 = thread::spawn(move || {
            assert_eq!(receiver.recv_timeout(Duration::from_millis(1)), Ok(9));
        });
        let t1 = thread::spawn(move || {
            sender.send(9u128).unwrap();
        });
        t1.join().unwrap();
        t2.join().unwrap();
    })
}

#[cfg(feature = "std")]
#[test]
fn recv_drop_sender_different_threads() {
    loom::model(|| {
        let (sender, receiver) = oneshot::channel::<u128>();
        let t2 = thread::spawn(move || {
            assert!(receiver.recv_timeout(Duration::from_millis(0)).is_err());
        });
        let t1 = thread::spawn(move || {
            drop(sender);
        });
        t1.join().unwrap();
        t2.join().unwrap();
    })
}

#[cfg(feature = "async")]
#[test]
fn async_recv() {
    loom::model(|| {
        let (sender, receiver) = oneshot::channel::<u128>();
        let t1 = thread::spawn(move || {
            sender.send(987).unwrap();
        });
        assert_eq!(loom::future::block_on(receiver), Ok(987));
        t1.join().unwrap();
    })
}

#[cfg(feature = "async")]
#[test]
fn send_then_poll() {
    loom::model(|| {
        let (sender, mut receiver) = oneshot::channel::<u128>();
        sender.send(1234).unwrap();

        let (waker, waker_handle) = helpers::waker::waker();
        let mut context = task::Context::from_waker(&waker);

        assert_eq!(
            Pin::new(&mut receiver).poll(&mut context),
            Poll::Ready(Ok(1234))
        );
        assert_eq!(waker_handle.clone_count(), 0);
        assert_eq!(waker_handle.drop_count(), 0);
        assert_eq!(waker_handle.wake_count(), 0);
    })
}

#[cfg(feature = "async")]
#[test]
fn poll_then_send() {
    loom::model(|| {
        let (sender, mut receiver) = oneshot::channel::<u128>();

        let (waker, waker_handle) = helpers::waker::waker();
        let mut context = task::Context::from_waker(&waker);

        assert_eq!(Pin::new(&mut receiver).poll(&mut context), Poll::Pending);
        assert_eq!(waker_handle.clone_count(), 1);
        assert_eq!(waker_handle.drop_count(), 0);
        assert_eq!(waker_handle.wake_count(), 0);

        sender.send(1234).unwrap();
        assert_eq!(waker_handle.clone_count(), 1);
        assert_eq!(waker_handle.drop_count(), 1);
        assert_eq!(waker_handle.wake_count(), 1);

        assert_eq!(
            Pin::new(&mut receiver).poll(&mut context),
            Poll::Ready(Ok(1234))
        );
        assert_eq!(waker_handle.clone_count(), 1);
        assert_eq!(waker_handle.drop_count(), 1);
        assert_eq!(waker_handle.wake_count(), 1);
    })
}

#[cfg(feature = "async")]
#[test]
fn poll_with_different_wakers() {
    loom::model(|| {
        let (sender, mut receiver) = oneshot::channel::<u128>();

        let (waker1, waker_handle1) = helpers::waker::waker();
        let mut context1 = task::Context::from_waker(&waker1);

        assert_eq!(Pin::new(&mut receiver).poll(&mut context1), Poll::Pending);
        assert_eq!(waker_handle1.clone_count(), 1);
        assert_eq!(waker_handle1.drop_count(), 0);
        assert_eq!(waker_handle1.wake_count(), 0);

        let (waker2, waker_handle2) = helpers::waker::waker();
        let mut context2 = task::Context::from_waker(&waker2);

        assert_eq!(Pin::new(&mut receiver).poll(&mut context2), Poll::Pending);
        assert_eq!(waker_handle1.clone_count(), 1);
        assert_eq!(waker_handle1.drop_count(), 1);
        assert_eq!(waker_handle1.wake_count(), 0);

        assert_eq!(waker_handle2.clone_count(), 1);
        assert_eq!(waker_handle2.drop_count(), 0);
        assert_eq!(waker_handle2.wake_count(), 0);

        // Sending should cause the waker from the latest poll to be woken up
        sender.send(1234).unwrap();
        assert_eq!(waker_handle1.clone_count(), 1);
        assert_eq!(waker_handle1.drop_count(), 1);
        assert_eq!(waker_handle1.wake_count(), 0);

        assert_eq!(waker_handle2.clone_count(), 1);
        assert_eq!(waker_handle2.drop_count(), 1);
        assert_eq!(waker_handle2.wake_count(), 1);
    })
}

#[cfg(feature = "async")]
#[test]
fn poll_then_try_recv() {
    loom::model(|| {
        let (_sender, mut receiver) = oneshot::channel::<u128>();

        let (waker, waker_handle) = helpers::waker::waker();
        let mut context = task::Context::from_waker(&waker);

        assert_eq!(Pin::new(&mut receiver).poll(&mut context), Poll::Pending);
        assert_eq!(waker_handle.clone_count(), 1);
        assert_eq!(waker_handle.drop_count(), 0);
        assert_eq!(waker_handle.wake_count(), 0);

        assert_eq!(receiver.try_recv(), Err(TryRecvError::Empty));

        assert_eq!(Pin::new(&mut receiver).poll(&mut context), Poll::Pending);
        assert_eq!(waker_handle.clone_count(), 2);
        assert_eq!(waker_handle.drop_count(), 1);
        assert_eq!(waker_handle.wake_count(), 0);
    })
}

#[cfg(feature = "async")]
#[test]
fn poll_then_try_recv_while_sending() {
    loom::model(|| {
        let (sender, mut receiver) = oneshot::channel::<u128>();

        let (waker, waker_handle) = helpers::waker::waker();
        let mut context = task::Context::from_waker(&waker);

        assert_eq!(Pin::new(&mut receiver).poll(&mut context), Poll::Pending);
        assert_eq!(waker_handle.clone_count(), 1);
        assert_eq!(waker_handle.drop_count(), 0);
        assert_eq!(waker_handle.wake_count(), 0);

        let t = thread::spawn(move || {
            sender.send(1234).unwrap();
        });

        let msg = loop {
            match receiver.try_recv() {
                Ok(msg) => break msg,
                Err(TryRecvError::Empty) => hint::spin_loop(),
                Err(TryRecvError::Disconnected) => panic!("Should not be disconnected"),
            }
        };
        assert_eq!(msg, 1234);
        assert_eq!(waker_handle.clone_count(), 1);
        assert_eq!(waker_handle.drop_count(), 1);
        assert_eq!(waker_handle.wake_count(), 1);

        t.join().unwrap();
    })
}
