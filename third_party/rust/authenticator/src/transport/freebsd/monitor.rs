/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::transport::device_selector::DeviceSelectorEvent;
use devd_rs;
use runloop::RunLoop;
use std::collections::HashMap;
use std::error::Error;
use std::ffi::OsString;
use std::sync::{mpsc::Sender, Arc};
use std::{fs, io};

const POLL_TIMEOUT: usize = 100;

pub enum Event {
    Add(OsString),
    Remove(OsString),
}

impl Event {
    fn from_devd(event: devd_rs::Event) -> Option<Self> {
        match event {
            devd_rs::Event::Attach {
                ref dev,
                parent: _,
                location: _,
            } if dev.starts_with("uhid") => Some(Event::Add(("/dev/".to_owned() + dev).into())),
            devd_rs::Event::Detach {
                ref dev,
                parent: _,
                location: _,
            } if dev.starts_with("uhid") => Some(Event::Remove(("/dev/".to_owned() + dev).into())),
            _ => None,
        }
    }
}

fn convert_error(e: devd_rs::Error) -> io::Error {
    e.into()
}

pub struct Monitor<F>
where
    F: Fn(OsString, Sender<DeviceSelectorEvent>, Sender<crate::StatusUpdate>, &dyn Fn() -> bool)
        + Sync,
{
    runloops: HashMap<OsString, RunLoop>,
    new_device_cb: Arc<F>,
    selector_sender: Sender<DeviceSelectorEvent>,
    status_sender: Sender<crate::StatusUpdate>,
}

impl<F> Monitor<F>
where
    F: Fn(OsString, Sender<DeviceSelectorEvent>, Sender<crate::StatusUpdate>, &dyn Fn() -> bool)
        + Send
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
        let mut ctx = devd_rs::Context::new().map_err(convert_error)?;

        let mut initial_devs = Vec::new();
        // Iterate all existing devices.
        for dev in (fs::read_dir("/dev")?).flatten() {
            let filename_ = dev.file_name();
            let filename = filename_.to_str().unwrap_or("");
            if filename.starts_with("uhid") {
                let path = OsString::from("/dev/".to_owned() + filename);
                initial_devs.push(path.clone());
                self.add_device(path);
            }
        }
        let _ = self
            .selector_sender
            .send(DeviceSelectorEvent::DevicesAdded(initial_devs));

        // Loop until we're stopped by the controlling thread, or fail.
        while alive() {
            // Wait for new events, break on failure.
            match ctx.wait_for_event(POLL_TIMEOUT) {
                Err(devd_rs::Error::Timeout) => (),
                Err(e) => return Err(convert_error(e).into()),
                Ok(event) => {
                    if let Some(event) = Event::from_devd(event) {
                        self.process_event(event);
                    }
                }
            }
        }

        // Remove all tracked devices.
        self.remove_all_devices();

        Ok(())
    }

    fn process_event(&mut self, event: Event) {
        match event {
            Event::Add(path) => {
                let _ = self
                    .selector_sender
                    .send(DeviceSelectorEvent::DevicesAdded(vec![path.clone()]));
                self.add_device(path);
            }
            Event::Remove(path) => {
                self.remove_device(path);
            }
        }
    }

    fn add_device(&mut self, path: OsString) {
        let f = self.new_device_cb.clone();
        let selector_sender = self.selector_sender.clone();
        let status_sender = self.status_sender.clone();
        let key = path.clone();
        debug!("Adding device {}", key.to_string_lossy());

        let runloop = RunLoop::new(move |alive| {
            if alive() {
                f(path, selector_sender, status_sender, alive);
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
