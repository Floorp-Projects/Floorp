/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::errors;
use crate::platform::monitor::Monitor;
use crate::statecallback::StateCallback;
use runloop::RunLoop;

pub struct Transaction {
    // Handle to the thread loop.
    thread: Option<RunLoop>,
}

impl Transaction {
    pub fn new<F, T>(
        timeout: u64,
        callback: StateCallback<crate::Result<T>>,
        new_device_cb: F,
    ) -> crate::Result<Self>
    where
        F: Fn(String, &dyn Fn() -> bool) + Sync + Send + 'static,
        T: 'static,
    {
        let thread = RunLoop::new_with_timeout(
            move |alive| {
                // Create a new device monitor.
                let mut monitor = Monitor::new(new_device_cb);

                // Start polling for new devices.
                try_or!(monitor.run(alive), |_| callback
                    .call(Err(errors::AuthenticatorError::Platform)));

                // Send an error, if the callback wasn't called already.
                callback.call(Err(errors::AuthenticatorError::U2FToken(
                    errors::U2FTokenError::NotAllowed,
                )));
            },
            timeout,
        )
        .map_err(|_| errors::AuthenticatorError::Platform)?;

        Ok(Self {
            thread: Some(thread),
        })
    }

    pub fn cancel(&mut self) {
        // This must never be None.
        self.thread.take().unwrap().cancel();
    }
}
