/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::errors;
use crate::statecallback::StateCallback;
use crate::transport::device_selector::{
    DeviceBuildParameters, DeviceSelector, DeviceSelectorEvent,
};
use std::path::PathBuf;
use std::sync::mpsc::Sender;

pub struct Transaction {}

impl Transaction {
    pub fn new<F, T>(
        timeout: u64,
        callback: StateCallback<crate::Result<T>>,
        _status: Sender<crate::StatusUpdate>,
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
        // Just to silence "unused"-warnings
        let mut device_selector = DeviceSelector::run();
        let _ = DeviceSelectorEvent::DevicesAdded(vec![]);
        let _ = DeviceSelectorEvent::DeviceRemoved(PathBuf::new());
        let _ = device_selector.clone_sender();
        device_selector.stop();

        callback.call(Err(errors::AuthenticatorError::U2FToken(
            errors::U2FTokenError::NotSupported,
        )));

        Err(errors::AuthenticatorError::U2FToken(
            errors::U2FTokenError::NotSupported,
        ))
    }

    pub fn cancel(&mut self) {
        /* No-op. */
    }
}
