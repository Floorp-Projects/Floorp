/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::collections::HashMap;
use std::ffi::{CString, OsString};
use std::io;
use std::os::unix::ffi::OsStrExt;
use std::os::unix::io::RawFd;
use std::path::PathBuf;
use std::sync::Arc;
use std::thread;
use std::time::Duration;

use runloop::RunLoop;
use util::from_unix_result;

const POLL_TIMEOUT: u64 = 500;

#[derive(Debug)]
pub struct FidoDev {
    pub fd: RawFd,
    pub os_path: OsString,
}

pub struct Monitor<F>
where
    F: Fn(FidoDev, &dyn Fn() -> bool) + Sync,
{
    runloops: HashMap<OsString, RunLoop>,
    new_device_cb: Arc<F>,
}

impl<F> Monitor<F>
where
    F: Fn(FidoDev, &dyn Fn() -> bool) + Send + Sync + 'static,
{
    pub fn new(new_device_cb: F) -> Self {
        Self {
            runloops: HashMap::new(),
            new_device_cb: Arc::new(new_device_cb),
        }
    }

    pub fn run(&mut self, alive: &dyn Fn() -> bool) -> io::Result<()> {
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
                        self.add_device(FidoDev { fd, os_path });
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

    fn add_device(&mut self, fido: FidoDev) {
        if !self.runloops.contains_key(&fido.os_path) {
            let f = self.new_device_cb.clone();
            let key = fido.os_path.clone();

            let runloop = RunLoop::new(move |alive| {
                if alive() {
                    f(fido, alive);
                }
            });

            if let Ok(runloop) = runloop {
                self.runloops.insert(key, runloop);
            }
        }
    }

    fn remove_device(&mut self, path: OsString) {
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
