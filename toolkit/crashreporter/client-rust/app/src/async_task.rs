/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Manage work across multiple threads.
//!
//! Each thread has thread-bound data which can be accessed in queued task functions.

pub type TaskFn<T> = Box<dyn FnOnce(&T) + Send + 'static>;

pub struct AsyncTask<T> {
    send: Box<dyn Fn(TaskFn<T>) + Send + Sync>,
}

impl<T> AsyncTask<T> {
    pub fn new<F: Fn(TaskFn<T>) + Send + Sync + 'static>(send: F) -> Self {
        AsyncTask {
            send: Box::new(send),
        }
    }

    pub fn push<F: FnOnce(&T) + Send + 'static>(&self, f: F) {
        (self.send)(Box::new(f));
    }

    pub fn wait<R: Send + 'static, F: FnOnce(&T) -> R + Send + 'static>(&self, f: F) -> R {
        let (tx, rx) = std::sync::mpsc::sync_channel(0);
        self.push(move |v| tx.send(f(v)).unwrap());
        rx.recv().unwrap()
    }
}
