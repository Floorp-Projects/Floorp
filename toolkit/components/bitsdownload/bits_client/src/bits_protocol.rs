/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Command, response, and status types.

use std::ffi::OsString;
use std::fmt;
use std::result;

use failure::Fail;
use guid_win::Guid;

use super::{BitsErrorContext, BitsJobProgress, BitsJobState, BitsJobTimes, BitsProxyUsage};

type HRESULT = i32;

/// An HRESULT with a descriptive message
#[derive(Clone, Debug, Fail)]
pub struct HResultMessage {
    pub hr: HRESULT,
    pub message: String,
}

impl fmt::Display for HResultMessage {
    fn fmt(&self, f: &mut fmt::Formatter) -> result::Result<(), fmt::Error> {
        self.message.fmt(f)
    }
}

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
    SetUpdateInterval(SetUpdateIntervalCommand),
    CompleteJob(CompleteJobCommand),
    CancelJob(CancelJobCommand),
}

/// Combine a [`Command`](enum.Command.html) with its success and failure result types.
#[doc(hidden)]
pub trait CommandType {
    type Success;
    type Failure: Fail;
    fn wrap(command: Self) -> Command;
}

// Start Job
#[doc(hidden)]
#[derive(Clone, Debug)]
pub struct StartJobCommand {
    pub url: OsString,
    pub save_path: OsString,
    pub proxy_usage: BitsProxyUsage,
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

#[derive(Clone, Debug, Fail)]
pub enum StartJobFailure {
    #[fail(display = "Argument validation failed: {}", _0)]
    ArgumentValidation(String),
    #[fail(display = "Create job: {}", _0)]
    Create(HResultMessage),
    #[fail(display = "Add file to job: {}", _0)]
    AddFile(HResultMessage),
    #[fail(display = "Apply settings to job: {}", _0)]
    ApplySettings(HResultMessage),
    #[fail(display = "Resume job: {}", _0)]
    Resume(HResultMessage),
    #[fail(display = "Connect to BackgroundCopyManager: {}", _0)]
    ConnectBcm(HResultMessage),
    #[fail(display = "BITS error: {}", _0)]
    OtherBITS(HResultMessage),
    #[fail(display = "Other failure: {}", _0)]
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

#[derive(Clone, Debug, Fail)]
pub enum MonitorJobFailure {
    #[fail(display = "Argument validation failed: {}", _0)]
    ArgumentValidation(String),
    #[fail(display = "Job not found")]
    NotFound,
    #[fail(display = "Get job: {}", _0)]
    GetJob(HResultMessage),
    #[fail(display = "Connect to BackgroundCopyManager: {}", _0)]
    ConnectBcm(HResultMessage),
    #[fail(display = "BITS error: {}", _0)]
    OtherBITS(HResultMessage),
    #[fail(display = "Other failure: {}", _0)]
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

#[derive(Clone, Debug, Fail)]
pub enum SuspendJobFailure {
    #[fail(display = "Job not found")]
    NotFound,
    #[fail(display = "Get job: {}", _0)]
    GetJob(HResultMessage),
    #[fail(display = "Suspend job: {}", _0)]
    SuspendJob(HResultMessage),
    #[fail(display = "Connect to BackgroundCopyManager: {}", _0)]
    ConnectBcm(HResultMessage),
    #[fail(display = "BITS error: {}", _0)]
    OtherBITS(HResultMessage),
    #[fail(display = "Other failure: {}", _0)]
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

#[derive(Clone, Debug, Fail)]
pub enum ResumeJobFailure {
    #[fail(display = "Job not found")]
    NotFound,
    #[fail(display = "Get job: {}", _0)]
    GetJob(HResultMessage),
    #[fail(display = "Resume job: {}", _0)]
    ResumeJob(HResultMessage),
    #[fail(display = "Connect to BackgroundCopyManager: {}", _0)]
    ConnectBcm(HResultMessage),
    #[fail(display = "BITS error: {}", _0)]
    OtherBITS(HResultMessage),
    #[fail(display = "Other failure: {}", _0)]
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

#[derive(Clone, Debug, Fail)]
pub enum SetJobPriorityFailure {
    #[fail(display = "Job not found")]
    NotFound,
    #[fail(display = "Get job: {}", _0)]
    GetJob(HResultMessage),
    #[fail(display = "Apply settings to job: {}", _0)]
    ApplySettings(HResultMessage),
    #[fail(display = "Connect to BackgroundCopyManager: {}", _0)]
    ConnectBcm(HResultMessage),
    #[fail(display = "BITS error: {}", _0)]
    OtherBITS(HResultMessage),
    #[fail(display = "Other failure: {}", _0)]
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

#[derive(Clone, Debug, Fail)]
pub enum SetUpdateIntervalFailure {
    #[fail(display = "Argument validation: {}", _0)]
    ArgumentValidation(String),
    #[fail(display = "Monitor not found")]
    NotFound,
    #[fail(display = "Other failure: {}", _0)]
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

#[derive(Clone, Debug, Fail)]
pub enum CompleteJobFailure {
    #[fail(display = "Job not found")]
    NotFound,
    #[fail(display = "Get job: {}", _0)]
    GetJob(HResultMessage),
    #[fail(display = "Complete job: {}", _0)]
    CompleteJob(HResultMessage),
    #[fail(display = "Job only partially completed")]
    PartialComplete,
    #[fail(display = "Connect to BackgroundCopyManager: {}", _0)]
    ConnectBcm(HResultMessage),
    #[fail(display = "BITS error: {}", _0)]
    OtherBITS(HResultMessage),
    #[fail(display = "Other failure: {}", _0)]
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

#[derive(Clone, Debug, Fail)]
pub enum CancelJobFailure {
    #[fail(display = "Job not found")]
    NotFound,
    #[fail(display = "Get job: {}", _0)]
    GetJob(HResultMessage),
    #[fail(display = "Cancel job: {}", _0)]
    CancelJob(HResultMessage),
    #[fail(display = "Connect to BackgroundCopyManager: {}", _0)]
    ConnectBcm(HResultMessage),
    #[fail(display = "BITS error: {}", _0)]
    OtherBITS(HResultMessage),
    #[fail(display = "Other failure: {}", _0)]
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
#[derive(Clone, Debug, Fail)]
#[fail(display = "Job error in context {}: {}", context_str, error)]
pub struct JobError {
    pub context: BitsErrorContext,
    pub context_str: String,
    pub error: HResultMessage,
}
