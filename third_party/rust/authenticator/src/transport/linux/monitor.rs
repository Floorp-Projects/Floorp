/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::transport::device_selector::DeviceSelectorEvent;
use libc::{c_int, c_short, c_ulong};
use libudev::EventType;
use runloop::RunLoop;
use std::collections::HashMap;
use std::error::Error;
use std::io;
use std::os::unix::io::AsRawFd;
use std::path::PathBuf;
use std::sync::{mpsc::Sender, Arc};

const UDEV_SUBSYSTEM: &str = "hidraw";
const POLLIN: c_short = 0x0001;
const POLL_TIMEOUT: c_int = 100;

fn poll(fds: &mut Vec<::libc::pollfd>) -> io::Result<()> {
    let nfds = fds.len() as c_ulong;

    let rv = unsafe { ::libc::poll((fds[..]).as_mut_ptr(), nfds, POLL_TIMEOUT) };

    if rv < 0 {
        Err(io::Error::from_raw_os_error(rv))
    } else {
        Ok(())
    }
}

pub struct Monitor<F>
where
    F: Fn(PathBuf, Sender<DeviceSelectorEvent>, Sender<crate::StatusUpdate>, &dyn Fn() -> bool)
        + Sync,
{
    runloops: HashMap<PathBuf, RunLoop>,
    new_device_cb: Arc<F>,
    selector_sender: Sender<DeviceSelectorEvent>,
    status_sender: Sender<crate::StatusUpdate>,
}

impl<F> Monitor<F>
where
    F: Fn(PathBuf, Sender<DeviceSelectorEvent>, Sender<crate::StatusUpdate>, &dyn Fn() -> bool)
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
        let ctx = libudev::Context::new()?;

        let mut enumerator = libudev::Enumerator::new(&ctx)?;
        enumerator.match_subsystem(UDEV_SUBSYSTEM)?;

        // Iterate all existing devices.
        let paths: Vec<PathBuf> = enumerator
            .scan_devices()?
            .filter_map(|dev| dev.devnode().map(|p| p.to_owned()))
            .collect();

        // Add them all in one go to avoid race conditions in DeviceSelector
        // (8 devices should be added, but the first returns already before all
        // others are known to DeviceSelector)
        self.selector_sender
            .send(DeviceSelectorEvent::DevicesAdded(paths.clone()))?;
        for path in paths {
            self.add_device(path);
        }

        let mut monitor = libudev::Monitor::new(&ctx)?;
        monitor.match_subsystem(UDEV_SUBSYSTEM)?;

        // Start listening for new devices.
        let mut socket = monitor.listen()?;
        let mut fds = vec![::libc::pollfd {
            fd: socket.as_raw_fd(),
            events: POLLIN,
            revents: 0,
        }];

        while alive() {
            // Wait for new events, break on failure.
            poll(&mut fds)?;

            if let Some(event) = socket.receive_event() {
                self.process_event(&event);
            }
        }

        // Remove all tracked devices.
        self.remove_all_devices();

        Ok(())
    }

    fn process_event(&mut self, event: &libudev::Event) {
        let path = event.device().devnode().map(|dn| dn.to_owned());

        match (event.event_type(), path) {
            (EventType::Add, Some(path)) => {
                let _ = self
                    .selector_sender
                    .send(DeviceSelectorEvent::DevicesAdded(vec![path.clone()]));
                self.add_device(path);
            }
            (EventType::Remove, Some(path)) => {
                self.remove_device(&path);
            }
            _ => { /* ignore other types and failures */ }
        }
    }

    fn add_device(&mut self, path: PathBuf) {
        let f = self.new_device_cb.clone();
        let key = path.clone();
        let selector_sender = self.selector_sender.clone();
        let status_sender = self.status_sender.clone();
        debug!("Adding device {}", path.to_string_lossy());

        let runloop = RunLoop::new(move |alive| {
            if alive() {
                f(path, selector_sender, status_sender, alive);
            }
        });

        if let Ok(runloop) = runloop {
            self.runloops.insert(key, runloop);
        }
    }

    fn remove_device(&mut self, path: &PathBuf) {
        let _ = self
            .selector_sender
            .send(DeviceSelectorEvent::DeviceRemoved(path.clone()));

        debug!("Removing device {}", path.to_string_lossy());
        if let Some(runloop) = self.runloops.remove(path) {
            runloop.cancel();
        }
    }

    fn remove_all_devices(&mut self) {
        while !self.runloops.is_empty() {
            let path = self.runloops.keys().next().unwrap().clone();
            self.remove_device(&path);
        }
    }
}

pub fn get_property_linux(path: &PathBuf, prop_name: &str) -> io::Result<String> {
    let ctx = libudev::Context::new()?;

    let mut enumerator = libudev::Enumerator::new(&ctx)?;
    enumerator.match_subsystem(UDEV_SUBSYSTEM)?;

    // Iterate all existing devices, since we don't have a syspath
    // and libudev-rs doesn't implement opening by devnode.
    for dev in enumerator.scan_devices()? {
        if dev.devnode().is_some() && dev.devnode().unwrap() == path {
            debug!(
                "get_property_linux Querying property {} from {}",
                prop_name,
                dev.syspath().display()
            );

            let value = dev
                .attribute_value(prop_name)
                .ok_or(io::ErrorKind::Other)?
                .to_string_lossy();

            debug!("get_property_linux Fetched Result, {}={}", prop_name, value);
            return Ok(value.to_string());
        }
    }

    Err(io::Error::new(
        io::ErrorKind::Other,
        "Unable to find device",
    ))
}
