/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! An interface for managing and monitoring BITS jobs.
//!
//! BITS is a Windows service for performing downloads in the background, independent from an
//! application, usually via HTTP/HTTPS.
//!
//! [`BitsClient`](enum.BitsClient.html) is the main interface, used to issue commands.
//!
//! [`BitsMonitorClient`](enum.BitsMonitorClient.html) delivers periodic status reports about a
//! job.
//!
//! Microsoft's documentation for BITS can be found at
//! <https://docs.microsoft.com/en-us/windows/desktop/Bits/background-intelligent-transfer-service-portal>

extern crate bits;
extern crate comedy;
extern crate failure;
extern crate failure_derive;
extern crate guid_win;

pub mod bits_protocol;

mod in_process;

use std::convert;
use std::ffi;

use bits_protocol::*;
use failure::Fail;

pub use bits::status::{BitsErrorContext, BitsJobState, BitsJobTimes};
pub use bits::{BitsJobProgress, BitsJobStatus, BitsProxyUsage};
pub use bits_protocol::{JobError, JobStatus};
pub use comedy::HResult;
pub use guid_win::Guid;

// These errors would come from a Local Service client but are mostly unused currently.
// PipeError properly lives in the crate that deals with named pipes, but it isn't in use now.
#[derive(Clone, Debug, Eq, Fail, PartialEq)]
pub enum PipeError {
    #[fail(display = "Pipe is not connected")]
    NotConnected,
    #[fail(display = "Operation timed out")]
    Timeout,
    #[fail(display = "Should have written {} bytes, wrote {}", _0, _1)]
    WriteCount(usize, u32),
    #[fail(display = "Windows API error")]
    Api(#[fail(cause)] HResult),
}

impl convert::From<HResult> for PipeError {
    fn from(err: HResult) -> PipeError {
        PipeError::Api(err)
    }
}

pub use PipeError as Error;

/// A client for interacting with BITS.
///
/// Methods on `BitsClient` return a `Result<Result<_, XyzFailure>, Error>`. The outer `Result`
/// is `Err` if there was a communication error in sending the associated command or receiving
/// its response. Currently this is always `Ok` as all clients are in-process. The inner
/// `Result` is `Err` if there was an error executing the command.
///
/// A single `BitsClient` can be used with multiple BITS jobs simultaneously; generally a job
/// is not bound tightly to a client.
///
/// A `BitsClient` tracks all [`BitsMonitorClient`s](enum.BitsMonitorClient.html) that it started
/// with `start_job()` or `monitor_job()`, so that the monitor can be stopped or modified.
pub enum BitsClient {
    // The `InProcess` variant does all BITS calls directly.
    #[doc(hidden)]
    InProcess(in_process::InProcessClient),
    // Space is reserved here for the LocalService variant, which will work through an external
    // process running as Local Service.
}

use BitsClient::InProcess;

impl BitsClient {
    /// Create an in-process `BitsClient`.
    ///
    /// `job_name` will be used when creating jobs, and this `BitsClient` can only be used to
    /// manipulate jobs with that name.
    ///
    /// `save_path_prefix` will be prepended to the local `save_path` given to `start_job()`, it
    /// must name an existing directory.
    pub fn new(
        job_name: ffi::OsString,
        save_path_prefix: ffi::OsString,
    ) -> Result<BitsClient, Error> {
        Ok(InProcess(in_process::InProcessClient::new(
            job_name,
            save_path_prefix,
        )?))
    }

    /// Start a job to download a single file at `url` to local path `save_path` (relative to the
    /// `save_path_prefix` given when constructing the `BitsClient`).
    ///
    /// `save_path_prefix` combined with `save_path` must name a file (existing or not) in an
    /// existing directory, which must be under the directory named by `save_path_prefix`.
    ///
    /// `proxy_usage` determines what proxy will be used.
    ///
    /// When a successful result `Ok(result)` is returned, `result.0.guid` is the id for the
    /// new job, and `result.1` is a monitor client that can be polled for periodic updates,
    /// returning a result approximately once per `monitor_interval_millis` milliseconds.
    pub fn start_job(
        &mut self,
        url: ffi::OsString,
        save_path: ffi::OsString,
        proxy_usage: BitsProxyUsage,
        no_progress_timeout_secs: u32,
        monitor_interval_millis: u32,
    ) -> Result<Result<(StartJobSuccess, BitsMonitorClient), StartJobFailure>, Error> {
        match self {
            InProcess(client) => Ok(client
                .start_job(
                    url,
                    save_path,
                    proxy_usage,
                    no_progress_timeout_secs,
                    monitor_interval_millis,
                )
                .map(|(success, monitor)| (success, BitsMonitorClient::InProcess(monitor)))),
        }
    }

    /// Start monitoring the job with id `guid` approximately once per `monitor_interval_millis`
    /// milliseconds.
    ///
    /// The returned `Ok(monitor)` is a monitor client to be polled for periodic updates.
    ///
    /// There can only be one ongoing `BitsMonitorClient` for each job associated with a given
    /// `BitsClient`. If a monitor client already exists for the specified job, it will be stopped.
    pub fn monitor_job(
        &mut self,
        guid: Guid,
        interval_millis: u32,
    ) -> Result<Result<BitsMonitorClient, MonitorJobFailure>, Error> {
        match self {
            InProcess(client) => Ok(client
                .monitor_job(guid, interval_millis)
                .map(BitsMonitorClient::InProcess)),
        }
    }

    /// Suspend job `guid`.
    pub fn suspend_job(&mut self, guid: Guid) -> Result<Result<(), SuspendJobFailure>, Error> {
        match self {
            InProcess(client) => Ok(client.suspend_job(guid)),
        }
    }

    /// Resume job `guid`.
    pub fn resume_job(&mut self, guid: Guid) -> Result<Result<(), ResumeJobFailure>, Error> {
        match self {
            InProcess(client) => Ok(client.resume_job(guid)),
        }
    }

    /// Set the priority of job `guid`.
    ///
    /// `foreground == true` will set the priority to `BG_JOB_PRIORITY_FOREGROUND`,
    /// `false` will use the default `BG_JOB_PRIORITY_NORMAL`.
    /// See the Microsoft documentation for `BG_JOB_PRIORITY` for details.
    ///
    /// A job created by `start_job()` will be foreground priority, by default.
    pub fn set_job_priority(
        &mut self,
        guid: Guid,
        foreground: bool,
    ) -> Result<Result<(), SetJobPriorityFailure>, Error> {
        match self {
            InProcess(client) => Ok(client.set_job_priority(guid, foreground)),
        }
    }

    /// Set the "no progress timeout" of job `guid`.
    pub fn set_no_progress_timeout(
        &mut self,
        guid: Guid,
        timeout_secs: u32,
    ) -> Result<Result<(), SetNoProgressTimeoutFailure>, Error> {
        match self {
            InProcess(client) => Ok(client.set_no_progress_timeout(guid, timeout_secs)),
        }
    }

    /// Change the update interval for an ongoing monitor of job `guid`.
    pub fn set_update_interval(
        &mut self,
        guid: Guid,
        interval_millis: u32,
    ) -> Result<Result<(), SetUpdateIntervalFailure>, Error> {
        match self {
            InProcess(client) => Ok(client.set_update_interval(guid, interval_millis)),
        }
    }

    /// Stop any ongoing monitor for job `guid`.
    pub fn stop_update(
        &mut self,
        guid: Guid,
    ) -> Result<Result<(), SetUpdateIntervalFailure>, Error> {
        match self {
            InProcess(client) => Ok(client.stop_update(guid)),
        }
    }

    /// Complete the job `guid`.
    ///
    /// This also stops any ongoing monitor for the job.
    pub fn complete_job(&mut self, guid: Guid) -> Result<Result<(), CompleteJobFailure>, Error> {
        match self {
            InProcess(client) => Ok(client.complete_job(guid)),
        }
    }

    /// Cancel the job `guid`.
    ///
    /// This also stops any ongoing monitor for the job.
    pub fn cancel_job(&mut self, guid: Guid) -> Result<Result<(), CancelJobFailure>, Error> {
        match self {
            InProcess(client) => Ok(client.cancel_job(guid)),
        }
    }
}

/// The client side of a monitor for a BITS job.
///
/// It is intended to be used by calling `get_status` in a loop to receive notifications about
/// the status of a job. Because `get_status` blocks, it is recommended to run this loop on its
/// own thread.
pub enum BitsMonitorClient {
    InProcess(in_process::InProcessMonitor),
}

impl BitsMonitorClient {
    /// `get_status` will return a result approximately every `monitor_interval_millis`
    /// milliseconds, but in case a result isn't available within `timeout_millis` milliseconds
    /// this will return `Err(Error::Timeout)`. Any `Err` returned, including timeout, indicates
    /// that the monitor has been stopped; the `BitsMonitorClient` should then be discarded.
    ///
    /// As with methods on `BitsClient`, `BitsMonitorClient::get_status()` has an inner `Result`
    /// type which indicates an error returned from the server. Any `Err` here also indicates that
    /// the monitor has stopped after yielding the result.
    ///
    /// The first time `get_status` is called it will return a status without any delay.
    ///
    /// If there is an error or the transfer completes, a result may be available sooner than
    /// the monitor interval.
    pub fn get_status(
        &mut self,
        timeout_millis: u32,
    ) -> Result<Result<JobStatus, HResultMessage>, Error> {
        match self {
            BitsMonitorClient::InProcess(client) => client.get_status(timeout_millis),
        }
    }
}
