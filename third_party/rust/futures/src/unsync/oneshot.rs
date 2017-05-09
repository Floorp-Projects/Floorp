//! A one-shot, futures-aware channel
//!
//! This channel is similar to that in `sync::oneshot` but cannot be sent across
//! threads.

use std::cell::RefCell;
use std::rc::{Rc, Weak};

use {Future, Poll, Async};
use task::{self, Task};

/// Creates a new futures-aware, one-shot channel.
///
/// This function is the same as `sync::oneshot::channel` except that the
/// returned values cannot be sent across threads.
pub fn channel<T>() -> (Sender<T>, Receiver<T>) {
    let inner = Rc::new(RefCell::new(Inner {
        value: None,
        tx_task: None,
        rx_task: None,
    }));
    let tx = Sender {
        inner: Rc::downgrade(&inner),
    };
    let rx = Receiver {
        state: State::Open(inner),
    };
    (tx, rx)
}

/// Represents the completion half of a oneshot through which the result of a
/// computation is signaled.
///
/// This is created by the `unsync::oneshot::channel` function and is equivalent
/// in functionality to `sync::oneshot::Sender` except that it cannot be sent
/// across threads.
#[derive(Debug)]
pub struct Sender<T> {
    inner: Weak<RefCell<Inner<T>>>,
}

/// A future representing the completion of a computation happening elsewhere in
/// memory.
///
/// This is created by the `unsync::oneshot::channel` function and is equivalent
/// in functionality to `sync::oneshot::Receiver` except that it cannot be sent
/// across threads.
#[derive(Debug)]
#[must_use = "futures do nothing unless polled"]
pub struct Receiver<T> {
    state: State<T>,
}

#[derive(Debug)]
enum State<T> {
    Open(Rc<RefCell<Inner<T>>>),
    Closed(Option<T>),
}

/// Represents that the `Sender` dropped before sending a message.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct Canceled;

#[derive(Debug)]
struct Inner<T> {
    value: Option<T>,
    tx_task: Option<Task>,
    rx_task: Option<Task>,
}

impl<T> Sender<T> {
    /// Completes this oneshot with a successful result.
    ///
    /// This function will consume `self` and indicate to the other end, the
    /// `Receiver`, that the error provided is the result of the computation this
    /// represents.
    ///
    /// If the value is successfully enqueued for the remote end to receive,
    /// then `Ok(())` is returned. If the receiving end was deallocated before
    /// this function was called, however, then `Err` is returned with the value
    /// provided.
    pub fn send(self, val: T) -> Result<(), T> {
        if let Some(inner) = self.inner.upgrade() {
            inner.borrow_mut().value = Some(val);
            Ok(())
        } else {
            Err(val)
        }
    }

    /// Polls this `Sender` half to detect whether the `Receiver` this has
    /// paired with has gone away.
    ///
    /// This function can be used to learn about when the `Receiver` (consumer)
    /// half has gone away and nothing will be able to receive a message sent
    /// from `complete`.
    ///
    /// Like `Future::poll`, this function will panic if it's not called from
    /// within the context of a task. In otherwords, this should only ever be
    /// called from inside another future.
    ///
    /// If `Ready` is returned then it means that the `Receiver` has disappeared
    /// and the result this `Sender` would otherwise produce should no longer
    /// be produced.
    ///
    /// If `NotReady` is returned then the `Receiver` is still alive and may be
    /// able to receive a message if sent. The current task, however, is
    /// scheduled to receive a notification if the corresponding `Receiver` goes
    /// away.
    pub fn poll_cancel(&mut self) -> Poll<(), ()> {
        match self.inner.upgrade() {
            Some(inner) => {
                inner.borrow_mut().tx_task = Some(task::park());
                Ok(Async::NotReady)
            }
            None => Ok(().into()),
        }
    }
}

impl<T> Drop for Sender<T> {
    fn drop(&mut self) {
        let inner = match self.inner.upgrade() {
            Some(inner) => inner,
            None => return,
        };
        let rx_task = {
            let mut borrow = inner.borrow_mut();
            borrow.tx_task.take();
            borrow.rx_task.take()
        };
        if let Some(task) = rx_task {
            task.unpark();
        }
    }
}

impl<T> Receiver<T> {
    /// Gracefully close this receiver, preventing sending any future messages.
    ///
    /// Any `send` operation which happens after this method returns is
    /// guaranteed to fail. Once this method is called the normal `poll` method
    /// can be used to determine whether a message was actually sent or not. If
    /// `Canceled` is returned from `poll` then no message was sent.
    pub fn close(&mut self) {
        let (item, task) = match self.state {
            State::Open(ref inner) => {
                let mut inner = inner.borrow_mut();
                drop(inner.rx_task.take());
                (inner.value.take(), inner.tx_task.take())
            }
            State::Closed(_) => return,
        };
        self.state = State::Closed(item);
        if let Some(task) = task {
            task.unpark();
        }
    }
}

impl<T> Future for Receiver<T> {
    type Item = T;
    type Error = Canceled;

    fn poll(&mut self) -> Poll<Self::Item, Self::Error> {
        let inner = match self.state {
            State::Open(ref mut inner) => inner,
            State::Closed(ref mut item) => {
                match item.take() {
                    Some(item) => return Ok(item.into()),
                    None => return Err(Canceled),
                }
            }
        };

        // If we've got a value, then skip the logic below as we're done.
        if let Some(val) = inner.borrow_mut().value.take() {
            return Ok(Async::Ready(val))
        }

        // If we can get mutable access, then the sender has gone away. We
        // didn't see a value above, so we're canceled. Otherwise we park
        // our task and wait for a value to come in.
        if Rc::get_mut(inner).is_some() {
            Err(Canceled)
        } else {
            inner.borrow_mut().rx_task = Some(task::park());
            Ok(Async::NotReady)
        }
    }
}

impl<T> Drop for Receiver<T> {
    fn drop(&mut self) {
        self.close();
    }
}
