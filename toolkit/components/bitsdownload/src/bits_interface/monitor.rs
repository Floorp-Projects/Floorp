/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
use bits_interface::{error::ErrorType, BitsRequest};

use bits_client::{
    bits_protocol::HResultMessage, BitsJobState, BitsMonitorClient, Guid, JobStatus, PipeError,
};
use crossbeam_utils::atomic::AtomicCell;
use log::error;
use moz_task::{get_main_thread, is_main_thread};
use nserror::{nsresult, NS_ERROR_ABORT, NS_ERROR_FAILURE, NS_OK};
use nsstring::{nsACString, nsCString};
use xpcom::{
    interfaces::{nsIEventTarget, nsIThread},
    xpcom, xpcom_method, RefPtr, ThreadBoundRefPtr,
};

/// This function takes the output of BitsMonitorClient::get_status() and uses
/// it to determine whether the the transfer has started. If the argument
/// contains an error, the transfer is considered started because we also
/// consider a transfer stopped on error.
/// This function is used to determine whether the OnStartRequest and OnProgress
/// observer functions should be called.
fn transfer_started(status: &Result<Result<JobStatus, HResultMessage>, PipeError>) -> bool {
    match status.as_ref() {
        Ok(Ok(job_status)) => match job_status.state {
            BitsJobState::Queued | BitsJobState::Connecting => false,
            _ => true,
        },
        Ok(Err(_)) => true,
        Err(_) => true,
    }
}

/// This function takes the output of BitsMonitorClient::get_status() and uses
/// it to determine whether the the transfer has stopped. If the argument
/// contains an error, the transfer is considered stopped.
/// A number of things will be done when a transfer is completed, such as
/// calling the observer's OnStopRequest method.
fn transfer_completed(status: &Result<Result<JobStatus, HResultMessage>, PipeError>) -> bool {
    match status.as_ref() {
        Ok(Ok(job_status)) => match job_status.state {
            BitsJobState::Error
            | BitsJobState::Transferred
            | BitsJobState::Acknowledged
            | BitsJobState::Cancelled => true,
            _ => false,
        },
        Ok(Err(_)) => true,
        Err(_) => true,
    }
}

/// BitsRequest implements nsIRequest, which means that it must be able to
/// provide an nsresult status code. This function provides such a status code
/// based on the output of BitsMonitorClient::get_status().
fn status_to_nsresult(status: &Result<Result<JobStatus, HResultMessage>, PipeError>) -> nsresult {
    match status.as_ref() {
        Ok(Ok(job_status)) => match job_status.state {
            BitsJobState::Cancelled => NS_ERROR_ABORT,
            BitsJobState::Transferred | BitsJobState::Acknowledged => NS_OK,
            _ => NS_ERROR_FAILURE,
        },
        Ok(Err(_)) => NS_ERROR_FAILURE,
        Err(_) => NS_ERROR_FAILURE,
    }
}

/// This function takes the output of BitsMonitorClient::get_status() and uses
/// it to determine the result value of the request. This will take the form of
/// an Optional ErrorType value with a None value indicating success.
fn status_to_request_result(
    status: &Result<Result<JobStatus, HResultMessage>, PipeError>,
) -> Option<ErrorType> {
    match status.as_ref() {
        Ok(Ok(job_status)) => match job_status.state {
            BitsJobState::Transferred | BitsJobState::Acknowledged => None,
            BitsJobState::Cancelled => Some(ErrorType::BitsStateCancelled),
            BitsJobState::Error => Some(ErrorType::BitsStateError),
            BitsJobState::TransientError => Some(ErrorType::BitsStateTransientError),
            _ => Some(ErrorType::BitsStateUnexpected),
        },
        Ok(Err(_)) => Some(ErrorType::FailedToGetJobStatus),
        Err(pipe_error) => Some(pipe_error.into()),
    }
}

/// MonitorRunnable is an nsIRunnable meant to be dispatched off thread. It will
/// perform the following actions:
///   1. Call BitsMonitorClient::get_status and store the result.
///   2. Dispatch itself back to the main thread.
///   3. Report the status to the observer.
///   4. If the transfer has finished, free its data and return, otherwise:
///   5. Dispatch itself back to its original thread and repeat from step 1.
#[xpcom(implement(nsIRunnable, nsINamed), atomic)]
pub struct MonitorRunnable {
    request: AtomicCell<Option<ThreadBoundRefPtr<BitsRequest>>>,
    id: Guid,
    timeout: u32,
    monitor_client: AtomicCell<Option<BitsMonitorClient>>,
    // This cell contains an Option, possibly containing the return value of
    // BitsMonitorClient::get_status.
    status: AtomicCell<Option<Result<Result<JobStatus, HResultMessage>, PipeError>>>,
    request_started: AtomicCell<bool>,
    in_error_state: AtomicCell<bool>,
}

impl MonitorRunnable {
    pub fn new(
        request: RefPtr<BitsRequest>,
        id: Guid,
        timeout: u32,
        monitor_client: BitsMonitorClient,
    ) -> RefPtr<MonitorRunnable> {
        MonitorRunnable::allocate(InitMonitorRunnable {
            request: AtomicCell::new(Some(ThreadBoundRefPtr::new(request))),
            id,
            timeout,
            monitor_client: AtomicCell::new(Some(monitor_client)),
            status: AtomicCell::new(None),
            request_started: AtomicCell::new(false),
            in_error_state: AtomicCell::new(false),
        })
    }

    pub fn dispatch(&self, thread: RefPtr<nsIThread>) -> Result<(), nsresult> {
        unsafe { thread.DispatchFromScript(self.coerce(), nsIEventTarget::DISPATCH_NORMAL) }
            .to_result()
    }

    fn free_mainthread_data(&self) {
        if is_main_thread() {
            // This is not safe to free unless on the main thread
            self.request.swap(None);
        } else {
            error!("Attempting to free data on the main thread, but not on the main thread");
        }
    }

    xpcom_method!(run => Run());

    /// This method is essentially a error-handling wrapper around try_run.
    /// This is done to make it easier to ensure that main-thread data is freed
    /// on the main thread.
    pub fn run(&self) -> Result<(), nsresult> {
        if self.in_error_state.load() {
            self.free_mainthread_data();
            return Err(NS_ERROR_FAILURE);
        }

        self.try_run().or_else(|error_message| {
            error!("{}", error_message);

            // Once an error has been encountered, we need to free all of our
            // data, but it all needs to be freed on the main thread.
            self.in_error_state.store(true);
            if is_main_thread() {
                self.free_mainthread_data();
                Err(NS_ERROR_FAILURE)
            } else {
                self.dispatch(get_main_thread()?)
            }
        })
    }

    /// This function performs all the primary functionality of MonitorRunnable.
    /// See the documentation for InitMonitorRunnable/MonitorRunnable for
    /// details.
    pub fn try_run(&self) -> Result<(), String> {
        if !is_main_thread() {
            let mut monitor_client = self
                .monitor_client
                .swap(None)
                .ok_or("Missing monitor client")?;
            self.status
                .store(Some(monitor_client.get_status(self.timeout)));
            self.monitor_client.store(Some(monitor_client));

            let main_thread =
                get_main_thread().map_err(|rv| format!("Unable to get main thread: {}", rv))?;

            self.dispatch(main_thread)
                .map_err(|rv| format!("Unable to dispatch to main thread: {}", rv))
        } else {
            let status = self.status.swap(None).ok_or("Missing status object")?;
            let tb_request = self.request.swap(None).ok_or("Missing request")?;

            // This block bounds the scope for request to ensure that it ends
            // before re-storing tb_request.
            let maybe_next_thread: Option<RefPtr<nsIThread>> = {
                let request = tb_request
                    .get_ref()
                    .ok_or("BitsRequest is on the wrong thread")?;

                if !self.request_started.load() && transfer_started(&status) {
                    self.request_started.store(true);
                    request.on_start();
                }

                if self.request_started.load() {
                    if let Ok(Ok(job_status)) = status.as_ref() {
                        let transferred_bytes = job_status.progress.transferred_bytes as i64;
                        let total_bytes = match job_status.progress.total_bytes {
                            Some(total) => total as i64,
                            None => -1i64,
                        };
                        request.on_progress(transferred_bytes, total_bytes);
                    }
                }

                if transfer_completed(&status) {
                    request.on_stop(Some((
                        status_to_nsresult(&status),
                        status_to_request_result(&status),
                    )));

                    // Transfer completed. No need to dispatch back to the monitor thread.
                    None
                } else {
                    Some(
                        request
                            .get_monitor_thread()
                            .ok_or("Missing monitor thread")?,
                    )
                }
            };

            self.request.store(Some(tb_request));

            match maybe_next_thread {
                Some(next_thread) => self
                    .dispatch(next_thread)
                    .map_err(|rv| format!("Unable to dispatch to thread: {}", rv)),
                None => {
                    self.free_mainthread_data();
                    Ok(())
                }
            }
        }
    }

    xpcom_method!(get_name => GetName() -> nsACString);
    fn get_name(&self) -> Result<nsCString, nsresult> {
        Ok(nsCString::from("BitsRequest::Monitor"))
    }
}
