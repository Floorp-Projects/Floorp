/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Command, response, and status types.

use std::error::Error as StdError;
use std::ffi::OsString;
use std::fmt;
use std::result;

use guid_win::Guid;
use thiserror::Error;

use super::{BitsErrorContext, BitsJobProgress, BitsJobState, BitsJobTimes, BitsProxyUsage};

type HRESULT = i32;

/// An HRESULT with a descriptive message
#[derive(Clone, Debug)]
pub struct HResultMessage {
    pub hr: HRESULT,
    pub message: String,
}

impl fmt::Display for HResultMessage {
    fn fmt(&self, f: &mut fmt::Formatter) -> result::Result<(), fmt::Error> {
        self.message.fmt(f)
    }
}

impl StdError for HResultMessage {}

/// Commands which can be sent to the server.
///
/// This is currently unused as the out-of-process Local Service server is not finished.
#[doc(hidden)]
#[derive(Clone, Debug)]
pub enum Command {
    StartJob(StartJobCommand),
    MonitorJob(MonitorJobCommand),
    SuspendJob(SuspendJobCommand),
    ResumeJob(ResumeJobCommand),
    SetJobPriority(SetJobPriorityCommand),
    SetNoProgressTimeout(SetNoProgressTimeoutCommand),
    SetUpdateInterval(SetUpdateIntervalCommand),
    CompleteJob(CompleteJobCommand),
    CancelJob(CancelJobCommand),
}

/// Combine a [`Command`](enum.Command.html) with its success and failure result types.
#[doc(hidden)]
pub trait CommandType {
    type Success;
    type Failure: StdError;
    fn wrap(command: Self) -> Command;
}

// Start Job
#[doc(hidden)]
#[derive(Clone, Debug)]
pub struct StartJobCommand {
    pub url: OsString,
    pub save_path: OsString,
    pub proxy_usage: BitsProxyUsage,
    pub no_progress_timeout_secs: u32,
    pub monitor: Option<MonitorConfig>,
}

impl CommandType for StartJobCommand {
    type Success = StartJobSuccess;
    type Failure = StartJobFailure;
    fn wrap(cmd: Self) -> Command {
        Command::StartJob(cmd)
    }
}

#[doc(hidden)]
#[derive(Clone, Debug)]
pub struct MonitorConfig {
    pub pipe_name: OsString,
    pub interval_millis: u32,
}

#[derive(Clone, Debug)]
pub struct StartJobSuccess {
    pub guid: Guid,
}

#[derive(Clone, Debug, Error)]
pub enum StartJobFailure {
    #[error("Argument validation failed: {0}")]
    ArgumentValidation(String),
    #[error("Create job: {0}")]
    Create(HResultMessage),
    #[error("Add file to job: {0}")]
    AddFile(HResultMessage),
    #[error("Apply settings to job: {0}")]
    ApplySettings(HResultMessage),
    #[error("Resume job: {0}")]
    Resume(HResultMessage),
    #[error("Connect to BackgroundCopyManager: {0}")]
    ConnectBcm(HResultMessage),
    #[error("BITS error: {0}")]
    OtherBITS(HResultMessage),
    #[error("Other failure: {0}")]
    Other(String),
}

// Monitor Job
#[doc(hidden)]
#[derive(Clone, Debug)]
pub struct MonitorJobCommand {
    pub guid: Guid,
    pub monitor: MonitorConfig,
}

impl CommandType for MonitorJobCommand {
    type Success = ();
    type Failure = MonitorJobFailure;
    fn wrap(cmd: Self) -> Command {
        Command::MonitorJob(cmd)
    }
}

#[derive(Clone, Debug, Error)]
pub enum MonitorJobFailure {
    #[error("Argument validation failed: {0}")]
    ArgumentValidation(String),
    #[error("Job not found")]
    NotFound,
    #[error("Get job: {0}")]
    GetJob(HResultMessage),
    #[error("Connect to BackgroundCopyManager: {0}")]
    ConnectBcm(HResultMessage),
    #[error("BITS error: {0}")]
    OtherBITS(HResultMessage),
    #[error("Other failure: {0}")]
    Other(String),
}

// Suspend Job
#[doc(hidden)]
#[derive(Clone, Debug)]
pub struct SuspendJobCommand {
    pub guid: Guid,
}

impl CommandType for SuspendJobCommand {
    type Success = ();
    type Failure = SuspendJobFailure;
    fn wrap(cmd: Self) -> Command {
        Command::SuspendJob(cmd)
    }
}

#[derive(Clone, Debug, Error)]
pub enum SuspendJobFailure {
    #[error("Job not found")]
    NotFound,
    #[error("Get job: {0}")]
    GetJob(HResultMessage),
    #[error("Suspend job: {0}")]
    SuspendJob(HResultMessage),
    #[error("Connect to BackgroundCopyManager: {0}")]
    ConnectBcm(HResultMessage),
    #[error("BITS error: {0}")]
    OtherBITS(HResultMessage),
    #[error("Other failure: {0}")]
    Other(String),
}

// Resume Job
#[doc(hidden)]
#[derive(Clone, Debug)]
pub struct ResumeJobCommand {
    pub guid: Guid,
}

impl CommandType for ResumeJobCommand {
    type Success = ();
    type Failure = ResumeJobFailure;
    fn wrap(cmd: Self) -> Command {
        Command::ResumeJob(cmd)
    }
}

#[derive(Clone, Debug, Error)]
pub enum ResumeJobFailure {
    #[error("Job not found")]
    NotFound,
    #[error("Get job: {0}")]
    GetJob(HResultMessage),
    #[error("Resume job: {0}")]
    ResumeJob(HResultMessage),
    #[error("Connect to BackgroundCopyManager: {0}")]
    ConnectBcm(HResultMessage),
    #[error("BITS error: {0}")]
    OtherBITS(HResultMessage),
    #[error("Other failure: {0}")]
    Other(String),
}

// Set Job Priority
#[doc(hidden)]
#[derive(Clone, Debug)]
pub struct SetJobPriorityCommand {
    pub guid: Guid,
    pub foreground: bool,
}

impl CommandType for SetJobPriorityCommand {
    type Success = ();
    type Failure = SetJobPriorityFailure;
    fn wrap(cmd: Self) -> Command {
        Command::SetJobPriority(cmd)
    }
}

#[derive(Clone, Debug, Error)]
pub enum SetJobPriorityFailure {
    #[error("Job not found")]
    NotFound,
    #[error("Get job: {0}")]
    GetJob(HResultMessage),
    #[error("Apply settings to job: {0}")]
    ApplySettings(HResultMessage),
    #[error("Connect to BackgroundCopyManager: {0}")]
    ConnectBcm(HResultMessage),
    #[error("BITS error: {0}")]
    OtherBITS(HResultMessage),
    #[error("Other failure: {0}")]
    Other(String),
}

// Set No Progress Timeout
#[doc(hidden)]
#[derive(Clone, Debug)]
pub struct SetNoProgressTimeoutCommand {
    pub guid: Guid,
    pub timeout_secs: u32,
}

impl CommandType for SetNoProgressTimeoutCommand {
    type Success = ();
    type Failure = SetNoProgressTimeoutFailure;
    fn wrap(cmd: Self) -> Command {
        Command::SetNoProgressTimeout(cmd)
    }
}

#[derive(Clone, Debug, Error)]
pub enum SetNoProgressTimeoutFailure {
    #[error("Job not found")]
    NotFound,
    #[error("Get job: {0}")]
    GetJob(HResultMessage),
    #[error("Apply settings to job: {0}")]
    ApplySettings(HResultMessage),
    #[error("Connect to BackgroundCopyManager: {0}")]
    ConnectBcm(HResultMessage),
    #[error("BITS error: {0}")]
    OtherBITS(HResultMessage),
    #[error("Other failure: {0}")]
    Other(String),
}

// Set Update Interval
#[doc(hidden)]
#[derive(Clone, Debug)]
pub struct SetUpdateIntervalCommand {
    pub guid: Guid,
    pub interval_millis: u32,
}

impl CommandType for SetUpdateIntervalCommand {
    type Success = ();
    type Failure = SetUpdateIntervalFailure;
    fn wrap(cmd: Self) -> Command {
        Command::SetUpdateInterval(cmd)
    }
}

#[derive(Clone, Debug, Error)]
pub enum SetUpdateIntervalFailure {
    #[error("Argument validation: {0}")]
    ArgumentValidation(String),
    #[error("Monitor not found")]
    NotFound,
    #[error("Other failure: {0}")]
    Other(String),
}

// Complete Job
#[doc(hidden)]
#[derive(Clone, Debug)]
pub struct CompleteJobCommand {
    pub guid: Guid,
}

impl CommandType for CompleteJobCommand {
    type Success = ();
    type Failure = CompleteJobFailure;
    fn wrap(cmd: Self) -> Command {
        Command::CompleteJob(cmd)
    }
}

#[derive(Clone, Debug, Error)]
pub enum CompleteJobFailure {
    #[error("Job not found")]
    NotFound,
    #[error("Get job: {0}")]
    GetJob(HResultMessage),
    #[error("Complete job: {0}")]
    CompleteJob(HResultMessage),
    #[error("Job only partially completed")]
    PartialComplete,
    #[error("Connect to BackgroundCopyManager: {0}")]
    ConnectBcm(HResultMessage),
    #[error("BITS error: {0}")]
    OtherBITS(HResultMessage),
    #[error("Other failure: {0}")]
    Other(String),
}

// Cancel Job
#[doc(hidden)]
#[derive(Clone, Debug)]
pub struct CancelJobCommand {
    pub guid: Guid,
}

impl CommandType for CancelJobCommand {
    type Success = ();
    type Failure = CancelJobFailure;
    fn wrap(cmd: Self) -> Command {
        Command::CancelJob(cmd)
    }
}

#[derive(Clone, Debug, Error)]
pub enum CancelJobFailure {
    #[error("Job not found")]
    NotFound,
    #[error("Get job: {0}")]
    GetJob(HResultMessage),
    #[error("Cancel job: {0}")]
    CancelJob(HResultMessage),
    #[error("Connect to BackgroundCopyManager: {0}")]
    ConnectBcm(HResultMessage),
    #[error("BITS error: {0}")]
    OtherBITS(HResultMessage),
    #[error("Other failure: {0}")]
    Other(String),
}

/// Job status report
///
/// This includes a URL which updates with redirect but is otherwise the same as
/// `bits::status::BitsJobStatus`.
#[derive(Clone, Debug)]
pub struct JobStatus {
    pub state: BitsJobState,
    pub progress: BitsJobProgress,
    pub error_count: u32,
    pub error: Option<JobError>,
    pub times: BitsJobTimes,
    /// None means same as last time
    pub url: Option<OsString>,
}

/// Job error report
#[derive(Clone, Debug, Error)]
#[error("Job error in context {context_str}: {error}")]
pub struct JobError {
    pub context: BitsErrorContext,
    pub context_str: String,
    pub error: HResultMessage,
}
