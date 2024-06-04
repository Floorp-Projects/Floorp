use core::mem;
use oneshot::TryRecvError;

#[cfg(feature = "std")]
use oneshot::{RecvError, RecvTimeoutError};
#[cfg(feature = "std")]
use std::time::{Duration, Instant};

#[cfg(feature = "std")]
mod thread {
    #[cfg(loom)]
    pub use loom::thread::spawn;
    #[cfg(not(loom))]
    pub use std::thread::{sleep, spawn};

    #[cfg(loom)]
    pub fn sleep(_timeout: core::time::Duration) {
        loom::thread::yield_now()
    }
}

mod helpers;
use helpers::{maybe_loom_model, DropCounter};

#[test]
fn send_before_try_recv() {
    maybe_loom_model(|| {
        let (sender, receiver) = oneshot::channel();
        assert!(sender.send(19i128).is_ok());

        assert_eq!(receiver.try_recv(), Ok(19i128));
        assert_eq!(receiver.try_recv(), Err(TryRecvError::Disconnected));
        #[cfg(feature = "std")]
        {
            assert_eq!(receiver.recv_ref(), Err(RecvError));
            assert!(receiver.recv_timeout(Duration::from_secs(1)).is_err());
        }
    })
}

#[cfg(feature = "std")]
#[test]
fn send_before_recv() {
    maybe_loom_model(|| {
        let (sender, receiver) = oneshot::channel::<()>();
        assert!(sender.send(()).is_ok());
        assert_eq!(receiver.recv(), Ok(()));
    });
    maybe_loom_model(|| {
        let (sender, receiver) = oneshot::channel::<u8>();
        assert!(sender.send(19).is_ok());
        assert_eq!(receiver.recv(), Ok(19));
    });
    maybe_loom_model(|| {
        let (sender, receiver) = oneshot::channel::<u64>();
        assert!(sender.send(21).is_ok());
        assert_eq!(receiver.recv(), Ok(21));
    });
    // FIXME: This test does not work with loom. There is something that happens after the
    // channel object becomes larger than ~500 bytes and that makes an atomic read from the state
    // result in "signal: 10, SIGBUS: access to undefined memory"
    #[cfg(not(loom))]
    maybe_loom_model(|| {
        let (sender, receiver) = oneshot::channel::<[u8; 4096]>();
        assert!(sender.send([0b10101010; 4096]).is_ok());
        assert!(receiver.recv().unwrap()[..] == [0b10101010; 4096][..]);
    });
}

#[cfg(feature = "std")]
#[test]
fn send_before_recv_ref() {
    maybe_loom_model(|| {
        let (sender, receiver) = oneshot::channel();
        assert!(sender.send(19i128).is_ok());

        assert_eq!(receiver.recv_ref(), Ok(19i128));
        assert_eq!(receiver.recv_ref(), Err(RecvError));
        assert_eq!(receiver.try_recv(), Err(TryRecvError::Disconnected));
        assert!(receiver.recv_timeout(Duration::from_secs(1)).is_err());
    })
}

#[cfg(feature = "std")]
#[test]
fn send_before_recv_timeout() {
    maybe_loom_model(|| {
        let (sender, receiver) = oneshot::channel();
        assert!(sender.send(19i128).is_ok());

        let start = Instant::now();
        let timeout = Duration::from_secs(1);
        assert_eq!(receiver.recv_timeout(timeout), Ok(19i128));
        assert!(start.elapsed() < Duration::from_millis(100));

        assert!(receiver.recv_timeout(timeout).is_err());
        assert!(receiver.try_recv().is_err());
        assert!(receiver.recv().is_err());
    })
}

#[test]
fn send_then_drop_receiver() {
    maybe_loom_model(|| {
        let (sender, receiver) = oneshot::channel();
        assert!(sender.send(19i128).is_ok());
        mem::drop(receiver);
    })
}

#[test]
fn send_with_dropped_receiver() {
    maybe_loom_model(|| {
        let (sender, receiver) = oneshot::channel();
        mem::drop(receiver);
        let send_error = sender.send(5u128).unwrap_err();
        assert_eq!(*send_error.as_inner(), 5);
        assert_eq!(send_error.into_inner(), 5);
    })
}

#[test]
fn try_recv_with_dropped_sender() {
    maybe_loom_model(|| {
        let (sender, receiver) = oneshot::channel::<u128>();
        mem::drop(sender);
        receiver.try_recv().unwrap_err();
    })
}

#[cfg(feature = "std")]
#[test]
fn recv_with_dropped_sender() {
    maybe_loom_model(|| {
        let (sender, receiver) = oneshot::channel::<u128>();
        mem::drop(sender);
        receiver.recv().unwrap_err();
    })
}

#[cfg(feature = "std")]
#[test]
fn recv_before_send() {
    maybe_loom_model(|| {
        let (sender, receiver) = oneshot::channel();
        let t = thread::spawn(move || {
            thread::sleep(Duration::from_millis(2));
            sender.send(9u128).unwrap();
        });
        assert_eq!(receiver.recv(), Ok(9));
        t.join().unwrap();
    })
}

#[cfg(feature = "std")]
#[test]
fn recv_timeout_before_send() {
    maybe_loom_model(|| {
        let (sender, receiver) = oneshot::channel();
        let t = thread::spawn(move || {
            thread::sleep(Duration::from_millis(2));
            sender.send(9u128).unwrap();
        });
        assert_eq!(receiver.recv_timeout(Duration::from_secs(1)), Ok(9));
        t.join().unwrap();
    })
}

#[cfg(feature = "std")]
#[test]
fn recv_before_send_then_drop_sender() {
    maybe_loom_model(|| {
        let (sender, receiver) = oneshot::channel::<u128>();
        let t = thread::spawn(move || {
            thread::sleep(Duration::from_millis(10));
            mem::drop(sender);
        });
        assert!(receiver.recv().is_err());
        t.join().unwrap();
    })
}

#[cfg(feature = "std")]
#[test]
fn recv_timeout_before_send_then_drop_sender() {
    maybe_loom_model(|| {
        let (sender, receiver) = oneshot::channel::<u128>();
        let t = thread::spawn(move || {
            thread::sleep(Duration::from_millis(10));
            mem::drop(sender);
        });
        assert!(receiver.recv_timeout(Duration::from_secs(1)).is_err());
        t.join().unwrap();
    })
}

#[test]
fn try_recv() {
    maybe_loom_model(|| {
        let (sender, receiver) = oneshot::channel::<u128>();
        assert_eq!(receiver.try_recv(), Err(TryRecvError::Empty));
        mem::drop(sender)
    })
}

#[cfg(feature = "std")]
#[test]
fn try_recv_then_drop_receiver() {
    maybe_loom_model(|| {
        let (sender, receiver) = oneshot::channel::<u128>();
        let t1 = thread::spawn(move || {
            let _ = sender.send(42);
        });
        let t2 = thread::spawn(move || {
            assert!(matches!(
                receiver.try_recv(),
                Ok(42) | Err(TryRecvError::Empty)
            ));
            mem::drop(receiver);
        });
        t1.join().unwrap();
        t2.join().unwrap();
    })
}

#[cfg(feature = "std")]
#[test]
fn recv_deadline_and_timeout_no_time() {
    maybe_loom_model(|| {
        let (_sender, receiver) = oneshot::channel::<u128>();

        let start = Instant::now();
        assert_eq!(
            receiver.recv_deadline(start),
            Err(RecvTimeoutError::Timeout)
        );
        assert!(start.elapsed() < Duration::from_millis(200));

        let start = Instant::now();
        assert_eq!(
            receiver.recv_timeout(Duration::from_millis(0)),
            Err(RecvTimeoutError::Timeout)
        );
        assert!(start.elapsed() < Duration::from_millis(200));
    })
}

// This test doesn't give meaningful results when run with oneshot_test_delay and loom
#[cfg(all(feature = "std", not(all(oneshot_test_delay, loom))))]
#[test]
fn recv_deadline_time_should_elapse() {
    maybe_loom_model(|| {
        let (_sender, receiver) = oneshot::channel::<u128>();

        let start = Instant::now();
        #[cfg(not(loom))]
        let timeout = Duration::from_millis(100);
        #[cfg(loom)]
        let timeout = Duration::from_millis(1);
        assert_eq!(
            receiver.recv_deadline(start + timeout),
            Err(RecvTimeoutError::Timeout)
        );
        assert!(start.elapsed() > timeout);
        assert!(start.elapsed() < timeout * 3);
    })
}

#[cfg(all(feature = "std", not(all(oneshot_test_delay, loom))))]
#[test]
fn recv_timeout_time_should_elapse() {
    maybe_loom_model(|| {
        let (_sender, receiver) = oneshot::channel::<u128>();

        let start = Instant::now();
        #[cfg(not(loom))]
        let timeout = Duration::from_millis(100);
        #[cfg(loom)]
        let timeout = Duration::from_millis(1);

        assert_eq!(
            receiver.recv_timeout(timeout),
            Err(RecvTimeoutError::Timeout)
        );
        assert!(start.elapsed() > timeout);
        assert!(start.elapsed() < timeout * 3);
    })
}

#[cfg(not(loom))]
#[test]
fn non_send_type_can_be_used_on_same_thread() {
    use std::ptr;

    #[derive(Debug, Eq, PartialEq)]
    struct NotSend(*mut ());

    let (sender, receiver) = oneshot::channel();
    sender.send(NotSend(ptr::null_mut())).unwrap();
    let reply = receiver.try_recv().unwrap();
    assert_eq!(reply, NotSend(ptr::null_mut()));
}

#[test]
fn message_in_channel_dropped_on_receiver_drop() {
    maybe_loom_model(|| {
        let (sender, receiver) = oneshot::channel();
        let (message, counter) = DropCounter::new(());
        assert_eq!(counter.count(), 0);
        sender.send(message).unwrap();
        assert_eq!(counter.count(), 0);
        mem::drop(receiver);
        assert_eq!(counter.count(), 1);
    })
}

#[test]
fn send_error_drops_message_correctly() {
    maybe_loom_model(|| {
        let (sender, _) = oneshot::channel();
        let (message, counter) = DropCounter::new(());

        let send_error = sender.send(message).unwrap_err();
        assert_eq!(counter.count(), 0);
        mem::drop(send_error);
        assert_eq!(counter.count(), 1);
    });
}

#[test]
fn send_error_drops_message_correctly_on_into_inner() {
    maybe_loom_model(|| {
        let (sender, _) = oneshot::channel();
        let (message, counter) = DropCounter::new(());

        let send_error = sender.send(message).unwrap_err();
        assert_eq!(counter.count(), 0);
        let message = send_error.into_inner();
        assert_eq!(counter.count(), 0);
        mem::drop(message);
        assert_eq!(counter.count(), 1);
    });
}
