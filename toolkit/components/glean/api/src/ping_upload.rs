// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The Ping Upload Scheduler logic.
//!
//! Register your application's PingUploader to upload pings when scheduled.
//!
//! ## Example:
//!
//! ```rust,ignore
//! fn during_init() {
//!     ping_upload::register_uploader(Box::new(|ping_request| do_the_thing(ping_request)));
//! }
//! ```

use glean_core::upload::PingUploadTask::*;
pub use glean_core::upload::{PingRequest, UploadResult};

use once_cell::sync::OnceCell;

use std::sync::Mutex;
use std::thread;

pub type UploadFn = dyn (FnMut(&PingRequest) -> UploadResult) + Send;
static UPLOADER: OnceCell<Mutex<Box<UploadFn>>> = OnceCell::new();

/// Threadsafe. Set the uploader to be used for all pings.
/// Subsequent calls fail.
pub fn register_uploader(uploader: Box<UploadFn>) -> Result<(), ()> {
    UPLOADER.set(Mutex::new(uploader)).map_err(|_| ())
}

/// Called occasionally on any thread to nudge the Scheduler to ask Glean for
/// ping requests.
pub fn check_for_uploads() {
    if UPLOADER.get().is_none() {
        return;
    }
    thread::spawn(move || {
        loop {
            if let Some(mutex) = UPLOADER.get() {
                let uploader = &mut *mutex.lock().unwrap();
                if let Upload(request) = crate::with_glean(|glean| glean.get_upload_task()) {
                    let response = (*uploader)(&request);
                    crate::with_glean(|glean| {
                        glean.process_ping_upload_response(&request.document_id, response)
                    });
                } else {
                    // For now, don't bother distinguishing between Wait and Done.
                    // Either mean there's no task to execute right now.
                    break;
                }
            } else {
                // No uploader? Weird.
                break;
            }
        }
    });
}
