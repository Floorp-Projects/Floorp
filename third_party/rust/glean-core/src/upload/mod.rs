// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Manages the pending pings queue and directory.
//!
//! * Keeps track of pending pings, loading any unsent ping from disk on startup;
//! * Exposes `get_upload_task` API for the platform layer to request next upload task;
//! * Exposes `process_ping_upload_response` API to check the HTTP response from the ping upload
//!   and either delete the corresponding ping from disk or re-enqueue it for sending.

// !IMPORTANT!
// Remove the next line when this module's functionality is in the Glean object.
// This is here just to not have lint error for now.
#![allow(dead_code)]

use std::collections::VecDeque;
use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, RwLock, RwLockWriteGuard};
use std::thread;

use log;
use serde_json::Value as JsonValue;

use directory::PingDirectoryManager;
use request::PingRequest;

mod directory;
mod request;

/// When asking for the next ping request to upload,
/// the requester may receive one out of three possible tasks.
#[derive(PartialEq, Debug)]
pub enum PingUploadTask {
    /// A PingRequest popped from the front of the queue.
    /// See [`PingRequest`](struct.PingRequest.html) for more information.
    Upload(PingRequest),
    /// A flag signaling that the pending pings directories are not done being processed,
    /// thus the requester should wait and come back later.
    Wait,
    /// A flag signaling that the pending pings queue is empty and requester is done.
    Done,
}

/// Manages the pending pings queue and directory.
#[derive(Debug)]
pub struct PingUploadManager {
    /// A FIFO queue storing a `PingRequest` for each pending ping.
    queue: Arc<RwLock<VecDeque<PingRequest>>>,
    /// A manager for the pending pings directories.
    directory_manager: PingDirectoryManager,
    /// A flag signaling if we are done processing the pending pings directories.
    processed_pending_pings: Arc<AtomicBool>,
}

impl PingUploadManager {
    /// Create a new PingUploadManager.
    ///
    /// Spawns a new thread and processes the pending pings directory,
    /// filling up the queue with whatever pings are in there.
    ///
    /// # Arguments
    ///
    /// * `data_path` - Path to the pending pings directory.
    ///
    /// # Panics
    ///
    /// Will panic if unable to spawn a new thread.
    pub fn new<P: Into<PathBuf>>(data_path: P) -> Self {
        let queue = Arc::new(RwLock::new(VecDeque::new()));
        let directory_manager = PingDirectoryManager::new(data_path);
        let processed_pending_pings = Arc::new(AtomicBool::new(false));

        let local_queue = queue.clone();
        let local_flag = processed_pending_pings.clone();
        let local_manager = directory_manager.clone();
        let _ = thread::Builder::new()
            .name("glean.ping_directory_manager.process_dir".to_string())
            .spawn(move || {
                let mut local_queue = local_queue
                    .write()
                    .expect("Can't write to pending pings queue.");
                local_queue.extend(local_manager.process_dir());
                local_flag.store(true, Ordering::SeqCst);
            })
            .expect("Unable to spawn thread to process pings directories.");

        Self {
            queue,
            processed_pending_pings,
            directory_manager,
        }
    }

    fn has_processed_pings_dir(&self) -> bool {
        self.processed_pending_pings.load(Ordering::SeqCst)
    }

    /// Creates a `PingRequest` and adds it to the queue.
    pub fn enqueue_ping(&self, uuid: &str, path: &str, body: JsonValue) {
        let mut queue = self
            .queue
            .write()
            .expect("Can't write to pending pings queue.");
        let request = PingRequest::new(uuid, path, body);
        queue.push_back(request);
    }

    /// Clears the pending pings queue, leaves the deletion-request pings.
    pub fn clear_ping_queue(&self) -> RwLockWriteGuard<'_, VecDeque<PingRequest>> {
        let mut queue = self
            .queue
            .write()
            .expect("Can't write to pending pings queue.");

        queue.retain(|ping| ping.is_deletion_request());
        queue
    }

    /// Gets the next `PingUploadTask`.
    ///
    /// # Return value
    ///
    /// `PingUploadTask` - see [`PingUploadTask`](enum.PingUploadTask.html) for more information.
    pub fn get_upload_task(&self) -> PingUploadTask {
        if !self.has_processed_pings_dir() {
            return PingUploadTask::Wait;
        }

        let mut queue = self
            .queue
            .write()
            .expect("Can't write to pending pings queue.");
        match queue.pop_front() {
            Some(request) => PingUploadTask::Upload(request),
            None => PingUploadTask::Done,
        }
    }

    /// Processes the response from an attempt to upload a ping.
    ///
    /// Based on the HTTP status of said response,
    /// the possible outcomes are:
    ///
    /// * **200 - 299 Success**
    ///   Any status on the 2XX range is considered a succesful upload,
    ///   which means the corresponding ping file can be deleted.
    ///   _Known 2XX status:_
    ///   * 200 - OK. Request accepted into the pipeline.
    ///
    /// * **400 - 499 Unrecoverable error**
    ///   Any status on the 4XX range means something our client did is not correct.
    ///   It is unlikely that the client is going to recover from this by retrying,
    ///   so in this case the corresponding ping file can also be deleted.
    ///   _Known 4XX status:_
    ///   * 404 - not found - POST/PUT to an unknown namespace
    ///   * 405 - wrong request type (anything other than POST/PUT)
    ///   * 411 - missing content-length header
    ///   * 413 - request body too large Note that if we have badly-behaved clients that
    ///           retry on 4XX, we should send back 202 on body/path too long).
    ///   * 414 - request path too long (See above)
    ///
    /// * **Any other error**
    ///   For any other error, a warning is logged and the ping is re-enqueued.
    ///   _Known other errors:_
    ///   * 500 - internal error
    ///
    /// # Note
    ///
    /// The disk I/O performed by this function is not done off-thread,
    /// as it is expected to be called off-thread by the platform.
    ///
    /// # Arguments
    ///
    /// `uuid` - The UUID of the ping in question.
    /// `status` - The HTTP status of the response.
    pub fn process_ping_upload_response(&self, uuid: &str, status: u16) {
        match status {
            200..=299 => {
                log::info!("Ping {} successfully sent {}.", uuid, status);
                self.directory_manager.delete_file(uuid);
            }
            400..=499 => {
                log::error!(
                    "Server returned client error code {} while attempting to send ping {}.",
                    status,
                    uuid
                );
                self.directory_manager.delete_file(uuid);
            }
            _ => {
                log::error!(
                    "Server returned response code {} while attempting to send ping {}.",
                    status,
                    uuid
                );
                if let Some(request) = self.directory_manager.process_file(uuid) {
                    let mut queue = self
                        .queue
                        .write()
                        .expect("Can't write to pending pings queue.");
                    queue.push_back(request);
                }
            }
        };
    }
}

#[cfg(test)]
mod test {
    use std::thread;
    use std::time::Duration;

    use serde_json::json;

    use super::*;
    use crate::metrics::PingType;
    use crate::{tests::new_glean, PENDING_PINGS_DIRECTORY};

    const UUID: &str = "40e31919-684f-43b0-a5aa-e15c2d56a674"; // Just a random UUID.
    const PATH: &str = "/submit/app_id/ping_name/schema_version/doc_id";

    #[test]
    fn test_doesnt_error_when_there_are_no_pending_pings() {
        // Create a new upload_manager
        let dir = tempfile::tempdir().unwrap();
        let upload_manager = PingUploadManager::new(dir.path());

        // Wait for processing of pending pings directory to finish.
        while upload_manager.get_upload_task() == PingUploadTask::Wait {
            thread::sleep(Duration::from_millis(10));
        }

        // Try and get the next request.
        // Verify request was not returned
        assert_eq!(upload_manager.get_upload_task(), PingUploadTask::Done);
    }

    #[test]
    fn test_returns_ping_request_when_there_is_one() {
        // Create a new upload_manager
        let dir = tempfile::tempdir().unwrap();
        let upload_manager = PingUploadManager::new(dir.path());

        // Wait for processing of pending pings directory to finish.
        while upload_manager.get_upload_task() == PingUploadTask::Wait {
            thread::sleep(Duration::from_millis(10));
        }

        // Enqueue a ping
        upload_manager.enqueue_ping(UUID, PATH, json!({}));

        // Try and get the next request.
        // Verify request was returned
        match upload_manager.get_upload_task() {
            PingUploadTask::Upload(_) => {}
            _ => panic!("Expected upload manager to return the next request!"),
        }
    }

    #[test]
    fn test_returns_as_many_ping_requests_as_there_are() {
        // Create a new upload_manager
        let dir = tempfile::tempdir().unwrap();
        let upload_manager = PingUploadManager::new(dir.path());

        // Wait for processing of pending pings directory to finish.
        while upload_manager.get_upload_task() == PingUploadTask::Wait {
            thread::sleep(Duration::from_millis(10));
        }

        // Enqueue a ping multiple times
        let n = 10;
        for _ in 0..n {
            upload_manager.enqueue_ping(UUID, PATH, json!({}));
        }

        // Verify a request is returned for each submitted ping
        for _ in 0..n {
            match upload_manager.get_upload_task() {
                PingUploadTask::Upload(_) => {}
                _ => panic!("Expected upload manager to return the next request!"),
            }
        }

        // Verify that after all requests are returned, none are left
        assert_eq!(upload_manager.get_upload_task(), PingUploadTask::Done);
    }

    #[test]
    fn test_clearing_the_queue_works_correctly() {
        // Create a new upload_manager
        let dir = tempfile::tempdir().unwrap();
        let upload_manager = PingUploadManager::new(dir.path());

        // Wait for processing of pending pings directory to finish.
        while upload_manager.get_upload_task() == PingUploadTask::Wait {
            thread::sleep(Duration::from_millis(10));
        }

        // Enqueue a ping multiple times
        for _ in 0..10 {
            upload_manager.enqueue_ping(UUID, PATH, json!({}));
        }

        // Clear the queue
        let _ = upload_manager.clear_ping_queue();

        // Verify there really isn't any ping in the queue
        assert_eq!(upload_manager.get_upload_task(), PingUploadTask::Done);
    }

    #[test]
    fn test_clearing_the_queue_doesnt_clear_deletion_request_pings() {
        let (mut glean, _) = new_glean(None);

        // Register a ping for testing
        let ping_type = PingType::new("test", true, /* send_if_empty */ true, vec![]);
        glean.register_ping_type(&ping_type);

        // Submit the ping multiple times
        let n = 10;
        for _ in 0..n {
            glean.submit_ping(&ping_type, None).unwrap();
        }

        glean
            .internal_pings
            .deletion_request
            .submit(&glean, None)
            .unwrap();

        // Clear the queue
        let _ = glean.upload_manager.clear_ping_queue();

        let upload_task = glean.get_upload_task();
        match upload_task {
            PingUploadTask::Upload(request) => assert!(request.is_deletion_request()),
            _ => panic!("Expected upload manager to return the next request!"),
        }

        // Verify there really isn't any other pings in the queue
        assert_eq!(glean.get_upload_task(), PingUploadTask::Done);
    }

    #[test]
    fn test_fills_up_queue_successfully_from_disk() {
        let (mut glean, dir) = new_glean(None);

        // Register a ping for testing
        let ping_type = PingType::new("test", true, /* send_if_empty */ true, vec![]);
        glean.register_ping_type(&ping_type);

        // Submit the ping multiple times
        let n = 10;
        for _ in 0..n {
            glean.submit_ping(&ping_type, None).unwrap();
        }

        // Create a new upload_manager
        let upload_manager = PingUploadManager::new(dir.path());

        // Wait for processing of pending pings directory to finish.
        let mut upload_task = upload_manager.get_upload_task();
        while upload_task == PingUploadTask::Wait {
            thread::sleep(Duration::from_millis(10));
            upload_task = upload_manager.get_upload_task();
        }

        // Verify the requests were properly enqueued
        for _ in 0..n {
            match upload_task {
                PingUploadTask::Upload(_) => {}
                _ => panic!("Expected upload manager to return the next request!"),
            }

            upload_task = upload_manager.get_upload_task();
        }

        // Verify that after all requests are returned, none are left
        assert_eq!(upload_manager.get_upload_task(), PingUploadTask::Done);
    }

    #[test]
    fn test_processes_correctly_success_upload_response() {
        let (mut glean, dir) = new_glean(None);

        // Register a ping for testing
        let ping_type = PingType::new("test", true, /* send_if_empty */ true, vec![]);
        glean.register_ping_type(&ping_type);

        // Submit a ping
        glean.submit_ping(&ping_type, None).unwrap();

        // Create a new upload_manager
        let upload_manager = PingUploadManager::new(&dir.path());

        // Wait for processing of pending pings directory to finish.
        let mut upload_task = upload_manager.get_upload_task();
        while upload_task == PingUploadTask::Wait {
            thread::sleep(Duration::from_millis(10));
            upload_task = upload_manager.get_upload_task();
        }

        // Get the pending ping directory path
        let pending_pings_dir = dir.path().join(PENDING_PINGS_DIRECTORY);

        // Get the submitted PingRequest
        match upload_task {
            PingUploadTask::Upload(request) => {
                // Simulate the processing of a sucessfull request
                let uuid = request.uuid;
                upload_manager.process_ping_upload_response(&uuid, 200);
                // Verify file was deleted
                assert!(!pending_pings_dir.join(uuid).exists());
            }
            _ => panic!("Expected upload manager to return the next request!"),
        }

        // Verify that after request is returned, none are left
        assert_eq!(upload_manager.get_upload_task(), PingUploadTask::Done);
    }

    #[test]
    fn test_processes_correctly_client_error_upload_response() {
        let (mut glean, dir) = new_glean(None);

        // Register a ping for testing
        let ping_type = PingType::new("test", true, /* send_if_empty */ true, vec![]);
        glean.register_ping_type(&ping_type);

        // Submit a ping
        glean.submit_ping(&ping_type, None).unwrap();

        // Create a new upload_manager
        let upload_manager = PingUploadManager::new(&dir.path());

        // Wait for processing of pending pings directory to finish.
        let mut upload_task = upload_manager.get_upload_task();
        while upload_task == PingUploadTask::Wait {
            thread::sleep(Duration::from_millis(10));
            upload_task = upload_manager.get_upload_task();
        }

        // Get the pending ping directory path
        let pending_pings_dir = dir.path().join(PENDING_PINGS_DIRECTORY);

        // Get the submitted PingRequest
        match upload_task {
            PingUploadTask::Upload(request) => {
                // Simulate the processing of a client error
                let uuid = request.uuid;
                upload_manager.process_ping_upload_response(&uuid, 404);
                // Verify file was deleted
                assert!(!pending_pings_dir.join(uuid).exists());
            }
            _ => panic!("Expected upload manager to return the next request!"),
        }

        // Verify that after request is returned, none are left
        assert_eq!(upload_manager.get_upload_task(), PingUploadTask::Done);
    }

    #[test]
    fn test_processes_correctly_server_error_upload_response() {
        let (mut glean, dir) = new_glean(None);

        // Register a ping for testing
        let ping_type = PingType::new("test", true, /* send_if_empty */ true, vec![]);
        glean.register_ping_type(&ping_type);

        // Submit a ping
        glean.submit_ping(&ping_type, None).unwrap();

        // Create a new upload_manager
        let upload_manager = PingUploadManager::new(dir.path());

        // Wait for processing of pending pings directory to finish.
        let mut upload_task = upload_manager.get_upload_task();
        while upload_task == PingUploadTask::Wait {
            thread::sleep(Duration::from_millis(10));
            upload_task = upload_manager.get_upload_task();
        }

        // Get the submitted PingRequest
        match upload_task {
            PingUploadTask::Upload(request) => {
                // Simulate the processing of a client error
                let uuid = request.uuid;
                upload_manager.process_ping_upload_response(&uuid, 500);
                // Verify this ping was indeed re-enqueued
                match upload_manager.get_upload_task() {
                    PingUploadTask::Upload(request) => {
                        assert_eq!(uuid, request.uuid);
                    }
                    _ => panic!("Expected upload manager to return the next request!"),
                }
            }
            _ => panic!("Expected upload manager to return the next request!"),
        }

        // Verify that after request is returned, none are left
        assert_eq!(upload_manager.get_upload_task(), PingUploadTask::Done);
    }
}
