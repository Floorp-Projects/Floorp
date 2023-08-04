// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Handling the Glean upload logic.
//!
//! This doesn't perform the actual upload but rather handles
//! retries, upload limitations and error tracking.

use std::sync::{atomic::Ordering, Arc, Mutex};
use std::thread::{self, JoinHandle};
use std::time::Duration;

use glean_core::upload::PingUploadTask;
pub use glean_core::upload::{PingRequest, UploadResult, UploadTaskAction};

pub use http_uploader::*;
use thread_state::{AtomicState, State};

mod http_uploader;

/// A description of a component used to upload pings.
pub trait PingUploader: std::fmt::Debug + Send + Sync {
    /// Uploads a ping to a server.
    ///
    /// # Arguments
    ///
    /// * `url` - the URL path to upload the data to.
    /// * `body` - the serialized text data to send.
    /// * `headers` - a vector of tuples containing the headers to send with
    ///   the request, i.e. (Name, Value).
    fn upload(&self, url: String, body: Vec<u8>, headers: Vec<(String, String)>) -> UploadResult;
}

/// The logic for uploading pings: this leaves the actual upload mechanism as
/// a detail of the user-provided object implementing [`PingUploader`].
#[derive(Debug)]
pub(crate) struct UploadManager {
    inner: Arc<Inner>,
}

#[derive(Debug)]
struct Inner {
    server_endpoint: String,
    uploader: Box<dyn PingUploader + 'static>,
    thread_running: AtomicState,
    handle: Mutex<Option<JoinHandle<()>>>,
}

impl UploadManager {
    /// Create a new instance of the upload manager.
    ///
    /// # Arguments
    ///
    /// * `server_endpoint` -  the server pings are sent to.
    /// * `new_uploader` - the instance of the uploader used to send pings.
    pub(crate) fn new(
        server_endpoint: String,
        new_uploader: Box<dyn PingUploader + 'static>,
    ) -> Self {
        Self {
            inner: Arc::new(Inner {
                server_endpoint,
                uploader: new_uploader,
                thread_running: AtomicState::new(State::Stopped),
                handle: Mutex::new(None),
            }),
        }
    }

    /// Signals Glean to upload pings at the next best opportunity.
    pub(crate) fn trigger_upload(&self) {
        // If no other upload proces is running, we're the one starting it.
        // Need atomic compare/exchange to avoid any further races
        // or we can end up with 2+ uploader threads.
        if self
            .inner
            .thread_running
            .compare_exchange(
                State::Stopped,
                State::Running,
                Ordering::SeqCst,
                Ordering::SeqCst,
            )
            .is_err()
        {
            return;
        }

        let inner = Arc::clone(&self.inner);

        // Need to lock before we start so that noone thinks we're not running.
        let mut handle = self.inner.handle.lock().unwrap();
        let thread = thread::Builder::new()
            .name("glean.upload".into())
            .spawn(move || {
                log::trace!("Started glean.upload thread");
                loop {
                    let incoming_task = glean_core::glean_get_upload_task();

                    match incoming_task {
                        PingUploadTask::Upload { request } => {
                            log::trace!("Received upload task with request {:?}", request);
                            let doc_id = request.document_id.clone();
                            let upload_url = format!("{}{}", inner.server_endpoint, request.path);
                            let headers: Vec<(String, String)> =
                                request.headers.into_iter().collect();
                            let result = inner.uploader.upload(upload_url, request.body, headers);
                            // Process the upload response.
                            match glean_core::glean_process_ping_upload_response(doc_id, result) {
                                UploadTaskAction::Next => (),
                                UploadTaskAction::End => break,
                            }

                            let status = inner.thread_running.load(Ordering::SeqCst);
                            // asked to shut down. let's do it.
                            if status == State::ShuttingDown {
                                break;
                            }
                        }
                        PingUploadTask::Wait { time } => {
                            log::trace!("Instructed to wait for {:?}ms", time);
                            thread::sleep(Duration::from_millis(time));
                        }
                        PingUploadTask::Done { .. } => {
                            log::trace!("Received PingUploadTask::Done. Exiting.");
                            // Nothing to do here, break out of the loop.
                            break;
                        }
                    }
                }

                // Clear the running flag to signal that this thread is done,
                // but only if there's no shutdown thread.
                let _ = inner.thread_running.compare_exchange(
                    State::Running,
                    State::Stopped,
                    Ordering::SeqCst,
                    Ordering::SeqCst,
                );
            });

        match thread {
            Ok(thread) => *handle = Some(thread),
            Err(err) => {
                log::warn!("Failed to spawn Glean's uploader thread. This will be retried on next upload. Error: {err}");
                // Swapping back the thread state. We're the ones setting it to `Running`, so
                // should be able to set it back.
                let state_change = self.inner.thread_running.compare_exchange(
                    State::Running,
                    State::Stopped,
                    Ordering::SeqCst,
                    Ordering::SeqCst,
                );

                if state_change.is_err() {
                    log::warn!("Failed to swap back thread state. Someone else jumped in and changed the state.");
                }
            }
        };
    }

    pub(crate) fn shutdown(&self) {
        // mark as shutting down.
        self.inner
            .thread_running
            .store(State::ShuttingDown, Ordering::SeqCst);

        // take the thread handle out.
        let mut handle = self.inner.handle.lock().unwrap();
        let thread = handle.take();

        if let Some(thread) = thread {
            thread
                .join()
                .expect("couldn't join on the uploader thread.");
        }
    }
}

mod thread_state {
    use std::sync::atomic::{AtomicU8, Ordering};

    #[derive(Debug, PartialEq)]
    #[repr(u8)]
    pub enum State {
        Stopped = 0,
        Running = 1,
        ShuttingDown = 2,
    }

    #[derive(Debug)]
    pub struct AtomicState(AtomicU8);

    impl AtomicState {
        const fn to_u8(val: State) -> u8 {
            val as u8
        }

        fn from_u8(val: u8) -> State {
            #![allow(non_upper_case_globals)]
            const U8_Stopped: u8 = State::Stopped as u8;
            const U8_Running: u8 = State::Running as u8;
            const U8_ShuttingDown: u8 = State::ShuttingDown as u8;
            match val {
                U8_Stopped => State::Stopped,
                U8_Running => State::Running,
                U8_ShuttingDown => State::ShuttingDown,
                _ => panic!("Invalid enum discriminant"),
            }
        }

        pub const fn new(v: State) -> AtomicState {
            AtomicState(AtomicU8::new(Self::to_u8(v)))
        }

        pub fn load(&self, order: Ordering) -> State {
            Self::from_u8(self.0.load(order))
        }

        pub fn store(&self, val: State, order: Ordering) {
            self.0.store(Self::to_u8(val), order)
        }

        pub fn compare_exchange(
            &self,
            current: State,
            new: State,
            success: Ordering,
            failure: Ordering,
        ) -> Result<State, State> {
            self.0
                .compare_exchange(Self::to_u8(current), Self::to_u8(new), success, failure)
                .map(Self::from_u8)
                .map_err(Self::from_u8)
        }
    }
}
