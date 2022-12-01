/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::transport::device_selector::DeviceSelectorEvent;
use std::collections::HashMap;
use std::ffi::OsString;
use std::io;
use std::sync::Arc;
use std::thread;
use std::time::Duration;
use std::sync::{mpsc::Sender, Arc};
use runloop::RunLoop;

use crate::transport::platform::fd::Fd;

// XXX Should use drvctl, but it doesn't do pubsub properly yet so
// DRVGETEVENT requires write access to /dev/drvctl.  Instead, for now,
// just poll every 500ms.
const POLL_TIMEOUT: u64 = 500;

pub struct Monitor<F>
where
    F: Fn(
            Fd,
            OsString,
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
            Fd,
            OsString,
            Sender<DeviceSelectorEvent>,
            Sender<crate::StatusUpdate>,
            &dyn Fn() -> bool,
        ) + Sync,
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
        while alive() {
            for n in 0..100 {
                let uhidpath = format!("/dev/uhid{}", n);
                match Fd::open(&uhidpath, libc::O_RDWR | libc::O_CLOEXEC) {
                    Ok(uhid) => {
                        self.add_device(uhid, OsString::from(&uhidpath));
                    }
                    Err(ref err) => match err.raw_os_error() {
                        Some(libc::EBUSY) => continue,
                        Some(libc::ENOENT) => break,
                        _ => self.remove_device(OsString::from(&uhidpath)),
                    },
                }
            }
            thread::sleep(Duration::from_millis(POLL_TIMEOUT));
        }
        self.remove_all_devices();
        Ok(())
    }

    fn add_device(&mut self, fd: Fd, path: OsString) {
        let f = self.new_device_cb.clone();
        let selector_sender = self.selector_sender.clone();
        let status_sender = self.status_sender.clone();
        debug!("Adding device {}", path.to_string_lossy());

        let runloop = RunLoop::new(move |alive| {
            if alive() {
                f(fd.clone(), path.clone(), selector_sender, status_sender, alive);
            }
        });

        if let Ok(runloop) = runloop {
            self.runloops.insert(path.clone(), runloop);
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
