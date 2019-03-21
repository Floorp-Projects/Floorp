// Licensed under the Apache License, Version 2.0
// <LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your option.
// All files in the project carrying such notice may not be copied, modified, or distributed
// except according to those terms.

//! Data types for reporting a job's status

use filetime_win::FileTime;
use winapi::shared::winerror::HRESULT;
use winapi::um::bits::{BG_ERROR_CONTEXT, BG_JOB_STATE};

#[cfg(feature = "status_serde")]
use serde_derive::{Deserialize, Serialize};

#[derive(Clone, Debug)]
#[cfg_attr(feature = "status_serde", derive(Serialize, Deserialize))]
pub struct BitsJobStatus {
    pub state: BitsJobState,
    pub progress: BitsJobProgress,
    pub error_count: u32,
    pub error: Option<BitsJobError>,
    pub times: BitsJobTimes,
}

#[derive(Clone, Debug)]
#[cfg_attr(feature = "status_serde", derive(Serialize, Deserialize))]
pub struct BitsJobError {
    pub context: BitsErrorContext,
    pub context_str: String,
    pub error: HRESULT,
    pub error_str: String,
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
#[cfg_attr(feature = "status_serde", derive(Serialize, Deserialize))]
pub enum BitsErrorContext {
    None,
    Unknown,
    GeneralQueueManager,
    QueueManagerNotification,
    LocalFile,
    RemoteFile,
    GeneralTransport,
    RemoteApplication,
    /// No other values are documented
    Other(BG_ERROR_CONTEXT),
}

impl From<BG_ERROR_CONTEXT> for BitsErrorContext {
    fn from(ec: BG_ERROR_CONTEXT) -> BitsErrorContext {
        use self::BitsErrorContext::*;
        use winapi::um::bits;
        match ec {
            bits::BG_ERROR_CONTEXT_NONE => None,
            bits::BG_ERROR_CONTEXT_UNKNOWN => Unknown,
            bits::BG_ERROR_CONTEXT_GENERAL_QUEUE_MANAGER => GeneralQueueManager,
            bits::BG_ERROR_CONTEXT_QUEUE_MANAGER_NOTIFICATION => QueueManagerNotification,
            bits::BG_ERROR_CONTEXT_LOCAL_FILE => LocalFile,
            bits::BG_ERROR_CONTEXT_REMOTE_FILE => RemoteFile,
            bits::BG_ERROR_CONTEXT_GENERAL_TRANSPORT => GeneralTransport,
            bits::BG_ERROR_CONTEXT_REMOTE_APPLICATION => RemoteApplication,
            ec => Other(ec),
        }
    }
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
#[cfg_attr(feature = "status_serde", derive(Serialize, Deserialize))]
pub enum BitsJobState {
    Queued,
    Connecting,
    Transferring,
    Suspended,
    Error,
    TransientError,
    Transferred,
    Acknowledged,
    Cancelled,
    /// No other values are documented
    Other(BG_JOB_STATE),
}

impl From<BG_JOB_STATE> for BitsJobState {
    fn from(s: BG_JOB_STATE) -> BitsJobState {
        use self::BitsJobState::*;
        use winapi::um::bits;
        match s {
            bits::BG_JOB_STATE_QUEUED => Queued,
            bits::BG_JOB_STATE_CONNECTING => Connecting,
            bits::BG_JOB_STATE_TRANSFERRING => Transferring,
            bits::BG_JOB_STATE_SUSPENDED => Suspended,
            bits::BG_JOB_STATE_ERROR => Error,
            bits::BG_JOB_STATE_TRANSIENT_ERROR => TransientError,
            bits::BG_JOB_STATE_TRANSFERRED => Transferred,
            bits::BG_JOB_STATE_ACKNOWLEDGED => Acknowledged,
            bits::BG_JOB_STATE_CANCELLED => Cancelled,
            s => Other(s),
        }
    }
}

#[derive(Copy, Clone, Debug)]
#[cfg_attr(feature = "status_serde", derive(Serialize, Deserialize))]
pub struct BitsJobProgress {
    pub total_bytes: Option<u64>,
    pub transferred_bytes: u64,
    pub total_files: u32,
    pub transferred_files: u32,
}

#[derive(Copy, Clone, Debug)]
#[cfg_attr(feature = "status_serde", derive(Serialize, Deserialize))]
pub struct BitsJobTimes {
    pub creation: FileTime,
    pub modification: FileTime,
    pub transfer_completion: Option<FileTime>,
}
