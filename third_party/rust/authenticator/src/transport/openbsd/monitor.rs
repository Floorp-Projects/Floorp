/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::transport::device_selector::DeviceSelectorEvent;
use crate::util::from_unix_result;
use runloop::RunLoop;
use std::collections::HashMap;
use std::error::Error;
use std::ffi::{CString, OsString};
use std::os::unix::ffi::OsStrExt;
use std::os::unix::io::RawFd;
use std::path::PathBuf;
use std::sync::{mpsc::Sender, Arc};
use std::thread;
use std::time::Duration;

const POLL_TIMEOUT: u64 = 500;

#[derive(Debug)]
pub struct WrappedOpenDevice {
    pub fd: RawFd,
    pub os_path: OsString,
}

pub struct Monitor<F>
where
    F: Fn(
            WrappedOpenDevice,
            Sender<DeviceSelectorEvent>,
            Sender<crate::StatusUpdate>,
            &dyn Fn() -> bool,
        ) + Sync,
{
    runloops: HashMap<OsString, RunLoop>,
    new_device_cb: Arc<F>,
    selector_sender: Sender<DeviceSelectorEvent>,
    status_sender: Sender<crate::StatusUpdate>,
}

impl<F> Monitor<F>
where
    F: Fn(
            WrappedOpenDevice,
            Sender<DeviceSelectorEvent>,
            Sender<crate::StatusUpdate>,
            &dyn Fn() -> bool,
        ) + Send
        + Sync
        + 'static,
{
    pub fn new(
        new_device_cb: F,
        selector_sender: Sender<DeviceSelectorEvent>,
        status_sender: Sender<crate::StatusUpdate>,
    ) -> Self {
        Self {
            runloops: HashMap::new(),
            new_device_cb: Arc::new(new_device_cb),
            selector_sender,
            status_sender,
        }
    }

    pub fn run(&mut self, alive: &dyn Fn() -> bool) -> Result<(), Box<dyn Error>> {
        // Loop until we're stopped by the controlling thread, or fail.
        while alive() {
            // Iterate the first 10 fido(4) devices.
            for path in (0..10)
                .map(|unit| PathBuf::from(&format!("/dev/fido/{}", unit)))
                .filter(|path| path.exists())
            {
                let os_path = path.as_os_str().to_os_string();
                let cstr = CString::new(os_path.as_bytes())?;

                // Try to open the device.
                let fd = unsafe { libc::open(cstr.as_ptr(), libc::O_RDWR) };
                match from_unix_result(fd) {
                    Ok(fd) => {
                        // The device is available if it can be opened.
                        let _ = self
                            .selector_sender
                            .send(DeviceSelectorEvent::DevicesAdded(vec![os_path.clone()]));
                        self.add_device(WrappedOpenDevice { fd, os_path });
                    }
                    Err(ref err) if err.raw_os_error() == Some(libc::EBUSY) => {
                        // The device is available but currently in use.
                    }
                    _ => {
                        // libc::ENODEV or any other error.
                        self.remove_device(os_path);
                    }
                }
            }

            thread::sleep(Duration::from_millis(POLL_TIMEOUT));
        }

        // Remove all tracked devices.
        self.remove_all_devices();

        Ok(())
    }

    fn add_device(&mut self, fido: WrappedOpenDevice) {
        if !self.runloops.contains_key(&fido.os_path) {
            let f = self.new_device_cb.clone();
            let key = fido.os_path.clone();
            let selector_sender = self.selector_sender.clone();
            let status_sender = self.status_sender.clone();
            let runloop = RunLoop::new(move |alive| {
                if alive() {
                    f(fido, selector_sender, status_sender, alive);
                }
            });

            if let Ok(runloop) = runloop {
                self.runloops.insert(key, runloop);
            }
        }
    }

    fn remove_device(&mut self, path: OsString) {
        let _ = self
            .selector_sender
            .send(DeviceSelectorEvent::DeviceRemoved(path.clone()));
        if let Some(runloop) = self.runloops.remove(&path) {
            runloop.cancel();
        }
    }

    fn remove_all_devices(&mut self) {
        while !self.runloops.is_empty() {
            let path = self.runloops.keys().next().unwrap().clone();
            self.remove_device(path);
        }
    }
}
