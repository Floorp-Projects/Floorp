/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::io;
use std::sync::{Arc, Mutex, Weak};
use std::sync::atomic::{AtomicBool, Ordering};
use std::thread::{Builder, JoinHandle};
use std::time::{Duration, Instant};

struct Canary {
    alive: AtomicBool,
    thread: Mutex<Option<JoinHandle<()>>>,
}

impl Canary {
    fn new() -> Self {
        Self {
            alive: AtomicBool::new(true),
            thread: Mutex::new(None),
        }
    }
}

pub struct RunLoop {
    flag: Weak<Canary>,
}

impl RunLoop {
    pub fn new<F, T>(fun: F) -> io::Result<Self>
    where
        F: FnOnce(&Fn() -> bool) -> T,
        F: Send + 'static,
    {
        Self::new_with_timeout(fun, 0 /* no timeout */)
    }

    pub fn new_with_timeout<F, T>(fun: F, timeout_ms: u64) -> io::Result<Self>
    where
        F: FnOnce(&Fn() -> bool) -> T,
        F: Send + 'static,
    {
        let flag = Arc::new(Canary::new());
        let flag_ = flag.clone();

        // Spawn the run loop thread.
        let thread = Builder::new().spawn(move || {
            let timeout = Duration::from_millis(timeout_ms);
            let start = Instant::now();

            // A callback to determine whether the thread should terminate.
            let still_alive = || {
                // `flag.alive` will be false after cancel() was called.
                flag.alive.load(Ordering::Relaxed) &&
                // If a timeout was provided, we'll check that too.
                (timeout_ms == 0 || start.elapsed() < timeout)
            };

            // Ignore return values.
            let _ = fun(&still_alive);
        })?;

        // We really should never fail to lock here.
        let mut guard = (*flag_).thread.lock().map_err(|_| {
            io::Error::new(io::ErrorKind::Other, "failed to lock")
        })?;

        // Store the thread handle so we can join later.
        *guard = Some(thread);

        Ok(Self { flag: Arc::downgrade(&flag_) })
    }

    // Cancels the run loop and waits for the thread to terminate.
    // This is a potentially BLOCKING operation.
    pub fn cancel(&self) {
        // If the thread still exists...
        if let Some(flag) = self.flag.upgrade() {
            // ...let the run loop terminate.
            flag.alive.store(false, Ordering::Relaxed);

            // Locking should never fail here either.
            if let Ok(mut guard) = flag.thread.lock() {
                // This really can't fail.
                if let Some(handle) = (*guard).take() {
                    // This might fail, ignore.
                    let _ = handle.join();
                }
            }
        }
    }

    // Tells whether the runloop is alive.
    pub fn alive(&self) -> bool {
        // If the thread still exists...
        if let Some(flag) = self.flag.upgrade() {
            flag.alive.load(Ordering::Relaxed)
        } else {
            false
        }
    }
}

#[cfg(test)]
mod tests {
    use std::sync::{Arc, Barrier};
    use std::sync::mpsc::channel;

    use super::RunLoop;

    #[test]
    fn test_empty() {
        // Create a runloop that exits right away.
        let rloop = RunLoop::new(|_| {}).unwrap();
        while rloop.alive() { /* wait */ }
        rloop.cancel(); // noop
    }

    #[test]
    fn test_cancel_early() {
        // Create a runloop and cancel it before the thread spawns.
        RunLoop::new(|alive| assert!(!alive())).unwrap().cancel();
    }

    #[test]
    fn test_cancel_endless_loop() {
        let barrier = Arc::new(Barrier::new(2));
        let b = barrier.clone();

        // Create a runloop that never exits.
        let rloop = RunLoop::new(move |alive| {
            b.wait();
            while alive() { /* loop */ }
        }).unwrap();

        barrier.wait();
        assert!(rloop.alive());
        rloop.cancel();
        assert!(!rloop.alive());
    }

    #[test]
    fn test_timeout() {
        // Create a runloop that never exits, but times out after 1ms.
        let rloop = RunLoop::new_with_timeout(|alive| while alive() {}, 1).unwrap();

        while rloop.alive() { /* wait */ }
        assert!(!rloop.alive());
        rloop.cancel(); // noop
    }

    #[test]
    fn test_channel() {
        let (tx, rx) = channel();

        // A runloop that sends data via a channel.
        let rloop = RunLoop::new(move |alive| while alive() {
            tx.send(0u8).unwrap();
        }).unwrap();

        // Wait until the data arrives.
        assert_eq!(rx.recv().unwrap(), 0u8);

        assert!(rloop.alive());
        rloop.cancel();
        assert!(!rloop.alive());
    }
}
