// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Pings directory processing utilities.

use std::cmp::Ordering;
use std::fs::{self, File};
use std::io::{BufRead, BufReader};
use std::path::{Path, PathBuf};

use serde::{Deserialize, Serialize};
use uuid::Uuid;

use super::request::HeaderMap;
use crate::{DELETION_REQUEST_PINGS_DIRECTORY, PENDING_PINGS_DIRECTORY};

/// A representation of the data extracted from a ping file,
#[derive(Clone, Debug, Default)]
pub struct PingPayload {
    /// The ping's doc_id.
    pub document_id: String,
    /// The path to upload the ping to.
    pub upload_path: String,
    /// The ping body as JSON-encoded string.
    pub json_body: String,
    /// HTTP headers to include in the upload request.
    pub headers: Option<HeaderMap>,
    /// Whether the ping body contains {client|ping}_info
    pub body_has_info_sections: bool,
    /// The ping's name. (Also likely in the upload_path.)
    pub ping_name: String,
}

/// A struct to hold the result of scanning all pings directories.
#[derive(Clone, Debug, Default)]
pub struct PingPayloadsByDirectory {
    pub pending_pings: Vec<(u64, PingPayload)>,
    pub deletion_request_pings: Vec<(u64, PingPayload)>,
}

impl PingPayloadsByDirectory {
    /// Extends the data of this instance of PingPayloadsByDirectory
    /// with the data from another instance of PingPayloadsByDirectory.
    pub fn extend(&mut self, other: PingPayloadsByDirectory) {
        self.pending_pings.extend(other.pending_pings);
        self.deletion_request_pings
            .extend(other.deletion_request_pings);
    }

    // Get the sum of the number of deletion request and regular pending pings.
    pub fn len(&self) -> usize {
        self.pending_pings.len() + self.deletion_request_pings.len()
    }
}

/// Gets the file name from a path as a &str.
///
/// # Panics
///
/// Won't panic if not able to get file name.
fn get_file_name_as_str(path: &Path) -> Option<&str> {
    match path.file_name() {
        None => {
            log::warn!("Error getting file name from path: {}", path.display());
            None
        }
        Some(file_name) => {
            let file_name = file_name.to_str();
            if file_name.is_none() {
                log::warn!("File name is not valid unicode: {}", path.display());
            }
            file_name
        }
    }
}

/// A ping's metadata, as (optionally) represented on disk.
///
/// Anything that isn't the upload path or the ping body.
#[derive(Default, Deserialize, Serialize)]
pub struct PingMetadata {
    /// HTTP headers to include when uploading the ping.
    pub headers: Option<HeaderMap>,
    /// Whether the body has {client|ping}_info sections.
    pub body_has_info_sections: Option<bool>,
    /// The name of the ping.
    pub ping_name: Option<String>,
}

/// Processes a ping's metadata.
///
/// The metadata is an optional third line in the ping file,
/// currently it contains only additonal headers to be added to each ping request.
/// Therefore, we will process the contents of this line
/// and return a HeaderMap of the persisted headers.
fn process_metadata(path: &str, metadata: &str) -> Option<PingMetadata> {
    if let Ok(metadata) = serde_json::from_str::<PingMetadata>(metadata) {
        return Some(metadata);
    } else {
        log::warn!("Error while parsing ping metadata: {}", path);
    }
    None
}

/// Manages the pings directories.
#[derive(Debug, Clone)]
pub struct PingDirectoryManager {
    /// Path to the pending pings directory.
    pending_pings_dir: PathBuf,
    /// Path to the deletion-request pings directory.
    deletion_request_pings_dir: PathBuf,
}

impl PingDirectoryManager {
    /// Creates a new directory manager.
    ///
    /// # Arguments
    ///
    /// * `data_path` - Path to the pending pings directory.
    pub fn new<P: Into<PathBuf>>(data_path: P) -> Self {
        let data_path = data_path.into();
        Self {
            pending_pings_dir: data_path.join(PENDING_PINGS_DIRECTORY),
            deletion_request_pings_dir: data_path.join(DELETION_REQUEST_PINGS_DIRECTORY),
        }
    }

    /// Attempts to delete a ping file.
    ///
    /// # Arguments
    ///
    /// * `uuid` - The UUID of the ping file to be deleted
    ///
    /// # Returns
    ///
    /// Whether the file was successfully deleted.
    ///
    /// # Panics
    ///
    /// Won't panic if unable to delete the file.
    pub fn delete_file(&self, uuid: &str) -> bool {
        let path = match self.get_file_path(uuid) {
            Some(path) => path,
            None => {
                log::warn!("Cannot find ping file to delete {}", uuid);
                return false;
            }
        };

        match fs::remove_file(&path) {
            Err(e) => {
                log::warn!("Error deleting file {}. {}", path.display(), e);
                return false;
            }
            _ => log::info!("File was deleted {}", path.display()),
        };

        true
    }

    /// Reads a ping file and returns the data from it.
    ///
    /// If the file is not properly formatted, it will be deleted and `None` will be returned.
    ///
    /// # Arguments
    ///
    /// * `document_id` - The UUID of the ping file to be processed
    pub fn process_file(&self, document_id: &str) -> Option<PingPayload> {
        let path = match self.get_file_path(document_id) {
            Some(path) => path,
            None => {
                log::warn!("Cannot find ping file to process {}", document_id);
                return None;
            }
        };
        let file = match File::open(&path) {
            Ok(file) => file,
            Err(e) => {
                log::warn!("Error reading ping file {}. {}", path.display(), e);
                return None;
            }
        };

        log::info!("Processing ping at: {}", path.display());

        // The way the ping file is structured:
        // first line should always have the path,
        // second line should have the body with the ping contents in JSON format
        // and third line might contain ping metadata e.g. additional headers.
        let mut lines = BufReader::new(file).lines();
        if let (Some(Ok(path)), Some(Ok(body)), Ok(metadata)) =
            (lines.next(), lines.next(), lines.next().transpose())
        {
            let PingMetadata {
                headers,
                body_has_info_sections,
                ping_name,
            } = metadata
                .and_then(|m| process_metadata(&path, &m))
                .unwrap_or_default();
            let ping_name =
                ping_name.unwrap_or_else(|| path.split('/').nth(3).unwrap_or("").into());
            return Some(PingPayload {
                document_id: document_id.into(),
                upload_path: path,
                json_body: body,
                headers,
                body_has_info_sections: body_has_info_sections.unwrap_or(true),
                ping_name,
            });
        } else {
            log::warn!(
                "Error processing ping file: {}. Ping file is not formatted as expected.",
                document_id
            );
        }
        self.delete_file(document_id);
        None
    }

    /// Processes both ping directories.
    pub fn process_dirs(&self) -> PingPayloadsByDirectory {
        PingPayloadsByDirectory {
            pending_pings: self.process_dir(&self.pending_pings_dir),
            deletion_request_pings: self.process_dir(&self.deletion_request_pings_dir),
        }
    }

    /// Processes one of the pings directory and return a vector with the ping data
    /// corresponding to each valid ping file in the directory.
    /// This vector will be ordered by file `modified_date`.
    ///
    /// Any files that don't match the UUID regex will be deleted
    /// to prevent files from polluting the pings directory.
    ///
    /// # Returns
    ///
    /// A vector of tuples with the file size and payload of each ping file in the directory.
    fn process_dir(&self, dir: &Path) -> Vec<(u64, PingPayload)> {
        log::trace!("Processing persisted pings.");

        let entries = match dir.read_dir() {
            Ok(entries) => entries,
            Err(_) => {
                // This may error simply because the directory doesn't exist,
                // which is expected if no pings were stored yet.
                return Vec::new();
            }
        };

        let mut pending_pings: Vec<_> = entries
            .filter_map(|entry| entry.ok())
            .filter_map(|entry| {
                let path = entry.path();
                if let Some(file_name) = get_file_name_as_str(&path) {
                    // Delete file if it doesn't match the pattern.
                    if Uuid::parse_str(file_name).is_err() {
                        log::warn!("Pattern mismatch. Deleting {}", path.display());
                        self.delete_file(file_name);
                        return None;
                    }
                    if let Some(data) = self.process_file(file_name) {
                        let metadata = match fs::metadata(&path) {
                            Ok(metadata) => metadata,
                            Err(e) => {
                                // There's a rare case where this races against a parallel deletion
                                // of all pending ping files.
                                // This could therefore fail, in which case we don't care about the
                                // result and can ignore the ping, it's already been deleted.
                                log::warn!(
                                    "Unable to read metadata for file: {}, error: {:?}",
                                    path.display(),
                                    e
                                );
                                return None;
                            }
                        };
                        return Some((metadata, data));
                    }
                };
                None
            })
            .collect();

        // This will sort the pings by date in ascending order (oldest -> newest).
        pending_pings.sort_by(|(a, _), (b, _)| {
            // We might not be able to get the modified date for a given file,
            // in which case we just put it at the end.
            if let (Ok(a), Ok(b)) = (a.modified(), b.modified()) {
                a.cmp(&b)
            } else {
                Ordering::Less
            }
        });

        pending_pings
            .into_iter()
            .map(|(metadata, data)| (metadata.len(), data))
            .collect()
    }

    /// Gets the path for a ping file based on its document_id.
    ///
    /// Will look for files in each ping directory until something is found.
    /// If nothing is found, returns `None`.
    fn get_file_path(&self, document_id: &str) -> Option<PathBuf> {
        for dir in [&self.pending_pings_dir, &self.deletion_request_pings_dir].iter() {
            let path = dir.join(document_id);
            if path.exists() {
                return Some(path);
            }
        }
        None
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::metrics::PingType;
    use crate::tests::new_glean;

    #[test]
    fn doesnt_panic_if_no_pending_pings_directory() {
        let dir = tempfile::tempdir().unwrap();
        let directory_manager = PingDirectoryManager::new(dir.path());

        // Verify that processing the directory didn't panic
        let data = directory_manager.process_dirs();
        assert_eq!(data.pending_pings.len(), 0);
        assert_eq!(data.deletion_request_pings.len(), 0);
    }

    #[test]
    fn gets_correct_data_from_valid_ping_file() {
        let (mut glean, dir) = new_glean(None);

        // Register a ping for testing
        let ping_type = PingType::new("test", true, true, true, true, true, vec![], vec![]);
        glean.register_ping_type(&ping_type);

        // Submit the ping to populate the pending_pings directory
        ping_type.submit_sync(&glean, None);

        let directory_manager = PingDirectoryManager::new(dir.path());

        // Try and process the pings directories
        let data = directory_manager.process_dirs();

        // Verify there is just the one request
        assert_eq!(data.pending_pings.len(), 1);
        assert_eq!(data.deletion_request_pings.len(), 0);

        // Verify request was returned for the "test" ping
        let ping = &data.pending_pings[0].1;
        let request_ping_type = ping.upload_path.split('/').nth(3).unwrap();
        assert_eq!(request_ping_type, ping.ping_name);
        assert_eq!(request_ping_type, "test");
    }

    #[test]
    fn non_uuid_files_are_deleted_and_ignored() {
        let (mut glean, dir) = new_glean(None);

        // Register a ping for testing
        let ping_type = PingType::new("test", true, true, true, true, true, vec![], vec![]);
        glean.register_ping_type(&ping_type);

        // Submit the ping to populate the pending_pings directory
        ping_type.submit_sync(&glean, None);

        let directory_manager = PingDirectoryManager::new(dir.path());

        let not_uuid_path = dir
            .path()
            .join(PENDING_PINGS_DIRECTORY)
            .join("not-uuid-file-name.txt");
        File::create(&not_uuid_path).unwrap();

        // Try and process the pings directories
        let data = directory_manager.process_dirs();

        // Verify there is just the one request
        assert_eq!(data.pending_pings.len(), 1);
        assert_eq!(data.deletion_request_pings.len(), 0);

        // Verify request was returned for the "test" ping
        let ping = &data.pending_pings[0].1;
        let request_ping_type = ping.upload_path.split('/').nth(3).unwrap();
        assert_eq!(request_ping_type, ping.ping_name);
        assert_eq!(request_ping_type, "test");

        // Verify that file was indeed deleted
        assert!(!not_uuid_path.exists());
    }

    #[test]
    fn wrongly_formatted_files_are_deleted_and_ignored() {
        let (mut glean, dir) = new_glean(None);

        // Register a ping for testing
        let ping_type = PingType::new("test", true, true, true, true, true, vec![], vec![]);
        glean.register_ping_type(&ping_type);

        // Submit the ping to populate the pending_pings directory
        ping_type.submit_sync(&glean, None);

        let directory_manager = PingDirectoryManager::new(dir.path());

        let wrong_contents_file_path = dir
            .path()
            .join(PENDING_PINGS_DIRECTORY)
            .join(Uuid::new_v4().to_string());
        File::create(&wrong_contents_file_path).unwrap();

        // Try and process the pings directories
        let data = directory_manager.process_dirs();

        // Verify there is just the one request
        assert_eq!(data.pending_pings.len(), 1);
        assert_eq!(data.deletion_request_pings.len(), 0);

        // Verify request was returned for the "test" ping
        let ping = &data.pending_pings[0].1;
        let request_ping_type = ping.upload_path.split('/').nth(3).unwrap();
        assert_eq!(request_ping_type, ping.ping_name);
        assert_eq!(request_ping_type, "test");

        // Verify that file was indeed deleted
        assert!(!wrong_contents_file_path.exists());
    }

    #[test]
    fn takes_deletion_request_pings_into_account_while_processing() {
        let (glean, dir) = new_glean(None);

        // Submit a deletion request ping to populate deletion request folder.
        glean
            .internal_pings
            .deletion_request
            .submit_sync(&glean, None);

        let directory_manager = PingDirectoryManager::new(dir.path());

        // Try and process the pings directories
        let data = directory_manager.process_dirs();

        assert_eq!(data.pending_pings.len(), 0);
        assert_eq!(data.deletion_request_pings.len(), 1);

        // Verify request was returned for the "deletion-request" ping
        let ping = &data.deletion_request_pings[0].1;
        let request_ping_type = ping.upload_path.split('/').nth(3).unwrap();
        assert_eq!(request_ping_type, ping.ping_name);
        assert_eq!(request_ping_type, "deletion-request");
    }
}
