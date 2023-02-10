/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::transport::device_selector::DeviceSelectorEvent;
use crate::transport::platform::fd::Fd;
use runloop::RunLoop;
use std::collections::HashMap;
use std::error::Error;
use std::ffi::OsString;
use std::sync::{mpsc::Sender, Arc};
use std::thread;
use std::time::Duration;

// XXX Should use drvctl, but it doesn't do pubsub properly yet so
// DRVGETEVENT requires write access to /dev/drvctl.  Instead, for now,
// just poll every 500ms.
const POLL_TIMEOUT: u64 = 500;

#[derive(Debug)]
pub struct WrappedOpenDevice {
    pub fd: Fd,
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
            for n in 0..100 {
                let uhidpath = OsString::from(format!("/dev/uhid{n}"));
                match Fd::open(&uhidpath, libc::O_RDWR | libc::O_CLOEXEC) {
                    Ok(uhid) => {
                        // The device is available if it can be opened.
                        let _ = self
                            .selector_sender
                            .send(DeviceSelectorEvent::DevicesAdded(vec![uhidpath.clone()]));
                        self.add_device(WrappedOpenDevice {
                            fd: uhid,
                            os_path: uhidpath,
                        });
                    }
                    Err(ref err) => match err.raw_os_error() {
                        Some(libc::EBUSY) => continue,
                        Some(libc::ENOENT) => break,
                        _ => self.remove_device(uhidpath),
                    },
                }
            }
            thread::sleep(Duration::from_millis(POLL_TIMEOUT));
        }

        // Remove all tracked devices.
        self.remove_all_devices();

        Ok(())
    }

    fn add_device(&mut self, fido: WrappedOpenDevice) {
        let f = self.new_device_cb.clone();
        let selector_sender = self.selector_sender.clone();
        let status_sender = self.status_sender.clone();
        let key = fido.os_path.clone();
        debug!("Adding device {}", key.to_string_lossy());

        let runloop = RunLoop::new(move |alive| {
            if alive() {
                f(fido, selector_sender, status_sender, alive);
            }
        });

        if let Ok(runloop) = runloop {
            self.runloops.insert(key, runloop);
        }
    }

    fn remove_device(&mut self, path: OsString) {
        let _ = self
            .selector_sender
            .send(DeviceSelectorEvent::DeviceRemoved(path.clone()));

        debug!("Removing device {}", path.to_string_lossy());
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
