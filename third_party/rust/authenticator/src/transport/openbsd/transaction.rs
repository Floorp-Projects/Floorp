/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::errors;
use crate::statecallback::StateCallback;
use crate::transport::device_selector::{
    DeviceBuildParameters, DeviceSelector, DeviceSelectorEvent,
};
use crate::transport::platform::monitor::Monitor;
use runloop::RunLoop;
use std::sync::mpsc::Sender;

pub struct Transaction {
    // Handle to the thread loop.
    thread: RunLoop,
    device_selector: DeviceSelector,
}

impl Transaction {
    pub fn new<F, T>(
        timeout: u64,
        callback: StateCallback<crate::Result<T>>,
        status: Sender<crate::StatusUpdate>,
        new_device_cb: F,
    ) -> crate::Result<Self>
    where
        F: Fn(
                DeviceBuildParameters,
                Sender<DeviceSelectorEvent>,
                Sender<crate::StatusUpdate>,
                &dyn Fn() -> bool,
            ) + Sync
            + Send
            + 'static,
        T: 'static,
    {
        let device_selector = DeviceSelector::run();
        let selector_sender = device_selector.clone_sender();
        let thread = RunLoop::new_with_timeout(
            move |alive| {
                // Create a new device monitor.
                let mut monitor = Monitor::new(new_device_cb, selector_sender, status);

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
            thread,
            device_selector,
        })
    }

    pub fn cancel(&mut self) {
        info!("Transaction was cancelled.");
        self.device_selector.stop();
        self.thread.cancel();
    }
}
