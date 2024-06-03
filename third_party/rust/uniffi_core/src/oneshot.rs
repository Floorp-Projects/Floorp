/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Implements a simple oneshot channel.
//!
//! We used to use the `oneshot` crate for this, but the dependency was hard to manage
//! (https://github.com/mozilla/uniffi-rs/issues/1736)
//!
//! This implementation is less efficient, but the difference is probably negligible for most
//! use-cases involving UniFFI.

use std::{
    future::Future,
    pin::Pin,
    sync::{Arc, Mutex},
    task::{Context, Poll, Waker},
};

pub struct Sender<T>(Arc<Mutex<OneshotInner<T>>>);
pub struct Receiver<T>(Arc<Mutex<OneshotInner<T>>>);

struct OneshotInner<T> {
    value: Option<T>,
    waker: Option<Waker>,
}

pub fn channel<T>() -> (Sender<T>, Receiver<T>) {
    let arc = Arc::new(Mutex::new(OneshotInner {
        value: None,
        waker: None,
    }));
    (Sender(arc.clone()), Receiver(arc))
}

impl<T> Sender<T> {
    /// Send a value to the receiver
    pub fn send(self, value: T) {
        let mut inner = self.0.lock().unwrap();
        inner.value = Some(value);
        if let Some(waker) = inner.waker.take() {
            waker.wake();
        }
    }

    /// Convert a Sender into a raw pointer
    ///
    /// from_raw must be called with this pointer or else the sender will leak
    pub fn into_raw(self) -> *const () {
        Arc::into_raw(self.0) as *const ()
    }

    /// Convert a raw pointer back to a Sender
    ///
    /// # Safety
    ///
    /// `raw_ptr` must have come from into_raw().  Once a pointer is passed into `from_raw` it must
    /// not be used again.
    pub unsafe fn from_raw(raw_ptr: *const ()) -> Self {
        Self(Arc::from_raw(raw_ptr as *const Mutex<OneshotInner<T>>))
    }
}

impl<T> Future for Receiver<T> {
    type Output = T;

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        let mut inner = self.0.lock().unwrap();
        match inner.value.take() {
            Some(v) => Poll::Ready(v),
            None => {
                inner.waker = Some(cx.waker().clone());
                Poll::Pending
            }
        }
    }
}
