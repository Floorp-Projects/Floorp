/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::statecallback::StateCallback;
use crate::transport::device_selector::{DeviceBuildParameters, DeviceSelectorEvent};
use std::sync::mpsc::Sender;

pub struct Transaction {}

impl Transaction {
    pub fn new<F, T>(
        _timeout: u64,
        _callback: StateCallback<crate::Result<T>>,
        _status: Sender<crate::StatusUpdate>,
        _new_device_cb: F,
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
        Ok(Self {})
    }

    pub fn cancel(&mut self) {
        info!("Transaction was cancelled.");
    }
}
