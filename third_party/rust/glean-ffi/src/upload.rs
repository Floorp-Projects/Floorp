// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! FFI compatible types for the upload mechanism.
//!
//! These are used in the `glean_get_upload_task` and `glean_process_ping_upload_response`
//! functions.

use std::ffi::CString;
use std::os::raw::c_char;

use ffi_support::IntoFfi;

use crate::{byte_buffer::ByteBuffer, glean_str_free};
use glean_core::upload::PingUploadTask;

/// Result values of attempted ping uploads encoded for FFI use.
///
/// These are exposed as C `define`s, e.g.:
///
/// ```c
/// #define UPLOAD_RESULT_RECOVERABLE 1
/// #define UPLOAD_RESULT_UNRECOVERABLE 2
/// #define UPLOAD_RESULT_HTTP_STATUS 0x8000
/// ```
///
/// The language binding needs to replicate these constants exactly.
///
/// The `HTTP_STATUS` result can carry additional data (the HTTP response code).
/// This is encoded in the lower bits.
///
/// The FFI layer can convert from a 32-bit integer (`u32`) representing the upload result (and
/// associated HTTP response code) into the Glean-compatible `UploadResult` type.
///
/// These are defined in `glean-core/src/upload/result.rs`,
/// but for cbindgen to also export them in header files we need to define them here as constants.
///
/// Inline tests ensure they match across crates.
#[allow(dead_code)]
pub mod upload_result {
    /// A recoverable error.
    pub const UPLOAD_RESULT_RECOVERABLE: u32 = 0x1;

    /// An unrecoverable error.
    pub const UPLOAD_RESULT_UNRECOVERABLE: u32 = 0x2;

    /// A HTTP response code.
    ///
    /// The actual response code is encoded in the lower bits.
    pub const UPLOAD_RESULT_HTTP_STATUS: u32 = 0x8000;
}

/// A FFI-compatible representation for the PingUploadTask.
///
/// This is exposed as a C-compatible tagged union, like this:
///
/// ```c
/// enum FfiPingUploadTask_Tag {
///   FfiPingUploadTask_Upload,
///   FfiPingUploadTask_Wait,
///   FfiPingUploadTask_Done,
/// };
/// typedef uint8_t FfiPingUploadTask_Tag;
///
/// typedef struct {
///   FfiPingUploadTask_Tag tag;
///   char *document_id;
///   char *path;
///   char *body;
///   char *headers;
/// } FfiPingUploadTask_Upload_Body;
///
/// typedef union {
///   FfiPingUploadTask_Tag tag;
///   FfiPingUploadTask_Upload_Body upload;
/// } FfiPingUploadTask;
///
/// ```
///
/// It is therefore always valid to read the `tag` field of the returned union (always the first
/// field in memory).
///
/// Language bindings should turn this into proper language types (e.g. enums/structs) and
/// copy out data.
///
/// String fields are encoded into null-terminated UTF-8 C strings.
///
/// * The language binding should copy out the data and turn these into their equivalent string type.
/// * The language binding should _not_ free these fields individually.
///   Instead `glean_process_ping_upload_response` will receive the whole enum, taking care of
///   freeing the memory.
///
///
/// The order of variants should be the same as in `glean-core/src/upload/mod.rs`
/// and `glean-core/android/src/main/java/mozilla/telemetry/glean/net/Upload.kt`.
///
/// cbindgen:prefix-with-name
#[repr(u8)]
pub enum FfiPingUploadTask {
    Upload {
        document_id: *mut c_char,
        path: *mut c_char,
        body: ByteBuffer,
        headers: *mut c_char,
    },
    Wait(u64),
    Done,
}

impl From<PingUploadTask> for FfiPingUploadTask {
    fn from(task: PingUploadTask) -> Self {
        match task {
            PingUploadTask::Upload(request) => {
                // Safe unwraps:
                // 1. CString::new(..) should not fail as we are the ones that created the strings being transformed;
                // 2. serde_json::to_string(&request.headers) should not fail as request.headers is a HashMap of Strings.
                let document_id = CString::new(request.document_id.to_owned()).unwrap();
                let path = CString::new(request.path.to_owned()).unwrap();
                let headers =
                    CString::new(serde_json::to_string(&request.headers).unwrap()).unwrap();
                FfiPingUploadTask::Upload {
                    document_id: document_id.into_raw(),
                    path: path.into_raw(),
                    body: ByteBuffer::from_vec(request.body),
                    headers: headers.into_raw(),
                }
            }
            PingUploadTask::Wait(time) => FfiPingUploadTask::Wait(time),
            PingUploadTask::Done => FfiPingUploadTask::Done,
        }
    }
}

impl Drop for FfiPingUploadTask {
    fn drop(&mut self) {
        if let FfiPingUploadTask::Upload {
            document_id,
            path,
            body,
            headers,
        } = self
        {
            // We need to free the previously allocated strings before dropping.
            unsafe {
                glean_str_free(*document_id);
                glean_str_free(*path);
                glean_str_free(*headers);
            }
            // Unfortunately, we cannot directly call `body.destroy();` as
            // we're behind a mutable reference, so we have to manually take the
            // ownership and drop. Moreover, `ByteBuffer::new_with_size(0)`
            // does not allocate, so we are not leaking memory.
            let body = std::mem::replace(body, ByteBuffer::new_with_size(0));
            body.destroy();
        }
    }
}

unsafe impl IntoFfi for FfiPingUploadTask {
    type Value = FfiPingUploadTask;

    #[inline]
    fn ffi_default() -> FfiPingUploadTask {
        FfiPingUploadTask::Done
    }

    #[inline]
    fn into_ffi_value(self) -> FfiPingUploadTask {
        self
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn constants_match_with_glean_core() {
        assert_eq!(
            upload_result::UPLOAD_RESULT_RECOVERABLE,
            glean_core::upload::ffi_upload_result::UPLOAD_RESULT_RECOVERABLE
        );
        assert_eq!(
            upload_result::UPLOAD_RESULT_UNRECOVERABLE,
            glean_core::upload::ffi_upload_result::UPLOAD_RESULT_UNRECOVERABLE
        );
        assert_eq!(
            upload_result::UPLOAD_RESULT_HTTP_STATUS,
            glean_core::upload::ffi_upload_result::UPLOAD_RESULT_HTTP_STATUS
        );
    }
}
