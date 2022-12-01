/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate libc;

use crate::errors;
use crate::platform::iokit::{CFRunLoopEntryObserver, IOHIDDeviceRef, SendableRunLoop};
use crate::platform::monitor::Monitor;
use crate::statecallback::StateCallback;
use core_foundation::runloop::*;
use std::os::raw::c_void;
use std::sync::mpsc::{channel, Receiver, Sender};
use std::thread;

// A transaction will run the given closure in a new thread, thereby using a
// separate per-thread state machine for each HID. It will either complete or
// fail through user action, timeout, or be cancelled when overridden by a new
// transaction.
pub struct Transaction {
    runloop: Option<SendableRunLoop>,
    thread: Option<thread::JoinHandle<()>>,
}

impl Transaction {
    pub fn new<F, T>(
        timeout: u64,
        callback: StateCallback<crate::Result<T>>,
        new_device_cb: F,
    ) -> crate::Result<Self>
    where
        F: Fn((IOHIDDeviceRef, Receiver<Vec<u8>>), &dyn Fn() -> bool) + Sync + Send + 'static,
        T: 'static,
    {
        let (tx, rx) = channel();
        let timeout = (timeout as f64) / 1000.0;

        let builder = thread::Builder::new();
        let thread = builder
            .spawn(move || {
                // Add a runloop observer that will be notified when we enter the
                // runloop and tx.send() the current runloop to the owning thread.
                // We need to ensure the runloop was entered before unblocking
                // Transaction::new(), so we can always properly cancel.
                let context = &tx as *const _ as *mut c_void;
                let obs = CFRunLoopEntryObserver::new(Transaction::observe, context);
                obs.add_to_current_runloop();

                // Create a new HID device monitor and start polling.
                let mut monitor = Monitor::new(new_device_cb);
                try_or!(monitor.start(), |_| callback
                    .call(Err(errors::AuthenticatorError::Platform)));

                // This will block until completion, abortion, or timeout.
                unsafe { CFRunLoopRunInMode(kCFRunLoopDefaultMode, timeout, 0) };

                // Close the monitor and its devices.
                monitor.stop();

                // Send an error, if the callback wasn't called already.
                callback.call(Err(errors::AuthenticatorError::U2FToken(
                    errors::U2FTokenError::NotAllowed,
                )));
            })
            .map_err(|_| errors::AuthenticatorError::Platform)?;

        // Block until we enter the CFRunLoop.
        let runloop = rx
            .recv()
            .map_err(|_| errors::AuthenticatorError::Platform)?;

        Ok(Self {
            runloop: Some(runloop),
            thread: Some(thread),
        })
    }

    extern "C" fn observe(_: CFRunLoopObserverRef, _: CFRunLoopActivity, context: *mut c_void) {
        let tx: &Sender<SendableRunLoop> = unsafe { &*(context as *mut _) };

        // Send the current runloop to the receiver to unblock it.
        let _ = tx.send(SendableRunLoop::new(unsafe { CFRunLoopGetCurrent() }));
    }

    pub fn cancel(&mut self) {
        // This must never be None. This won't block.
        unsafe { CFRunLoopStop(*self.runloop.take().unwrap()) };

        // This must never be None. Ignore return value.
        let _ = self.thread.take().unwrap().join();
    }
}
