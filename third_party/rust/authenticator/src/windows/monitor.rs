/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::platform::winapi::DeviceInfoSet;
use runloop::RunLoop;
use std::collections::{HashMap, HashSet};
use std::io;
use std::iter::FromIterator;
use std::sync::Arc;
use std::thread;
use std::time::Duration;

pub struct Monitor<F>
where
    F: Fn(String, &dyn Fn() -> bool) + Sync,
{
    runloops: HashMap<String, RunLoop>,
    new_device_cb: Arc<F>,
}

impl<F> Monitor<F>
where
    F: Fn(String, &dyn Fn() -> bool) + Send + Sync + 'static,
{
    pub fn new(new_device_cb: F) -> Self {
        Self {
            runloops: HashMap::new(),
            new_device_cb: Arc::new(new_device_cb),
        }
    }

    pub fn run(&mut self, alive: &dyn Fn() -> bool) -> io::Result<()> {
        let mut stored = HashSet::new();

        while alive() {
            let device_info_set = DeviceInfoSet::new()?;
            let devices = HashSet::from_iter(device_info_set.devices());

            // Remove devices that are gone.
            for path in stored.difference(&devices) {
                self.remove_device(path);
            }

            // Add devices that were plugged in.
            for path in devices.difference(&stored) {
                self.add_device(path);
            }

            // Remember the new set.
            stored = devices;

            // Wait a little before looking for devices again.
            thread::sleep(Duration::from_millis(100));
        }

        // Remove all tracked devices.
        self.remove_all_devices();

        Ok(())
    }

    fn add_device(&mut self, path: &String) {
        let f = self.new_device_cb.clone();
        let path = path.clone();
        let key = path.clone();

        let runloop = RunLoop::new(move |alive| {
            if alive() {
                f(path, alive);
            }
        });

        if let Ok(runloop) = runloop {
            self.runloops.insert(key, runloop);
        }
    }

    fn remove_device(&mut self, path: &String) {
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
