/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::{
    action::{Action, ServiceAction},
    error::{
        ErrorStage::{MainThread, Pretask},
        ErrorType,
        ErrorType::{
            BitsStateCancelled, FailedToDispatchRunnable, FailedToStartThread, InvalidArgument,
            OperationAlreadyInProgress, TransferAlreadyComplete,
        },
    },
    monitor::MonitorRunnable,
    task::{
        CancelTask, ChangeMonitorIntervalTask, CompleteTask, Priority, ResumeTask, SetPriorityTask,
        SuspendTask,
    },
    BitsService, BitsTaskError,
};
use nsIBitsRequest_method; // From xpcom_method.rs

use bits_client::{BitsMonitorClient, Guid};
use log::{error, info, warn};
use moz_task::create_thread;
use nserror::{nsresult, NS_ERROR_NOT_IMPLEMENTED, NS_OK};
use nsstring::{nsACString, nsCString};
use std::{cell::Cell, fmt, ptr};
use xpcom::{
    interfaces::{
        nsIBits, nsIBitsCallback, nsILoadGroup, nsIProgressEventSink, nsIRequestObserver,
        nsISupports, nsIThread, nsLoadFlags,
    },
    xpcom, xpcom_method, RefPtr, XpCom,
};

/// This structure exists to resolve a race condition. If cancel is called, we
/// don't want to immediately set the request state to cancelled, because the
/// cancel action could fail. But it's possible that on_stop() could be called
/// before the cancel action resolves, and the correct status should be sent to
/// OnStopRequest.
/// This is how this race condition will be resolved:
///   1.  cancel() is called, which sets the CancelAction to InProgress and
///       stores in it the status that should be set if it succeeds.
///   2.  cancel() dispatches the cancel task off thread.
/// At this point, things unfold in one of two ways, depending on the race
/// condition. Either:
///   3.  The cancel task returns to the main thread and calls
///       BitsRequest::finish_cancel_action.
///   4.  If the cancel action succeeded, the appropriate status codes are set
///       and the CancelAction is set to RequestEndPending.
///       If the cancel action failed, the CancelAction is set to NotInProgress.
///   5.  The MonitorRunnable detects that the transfer has ended and calls
///       BitsRequest::on_stop, passing different status codes.
///   6.  BitsRequest::on_stop checks the CancelAction and
///       If the cancel action succeeded and RequestEndPending is set, the
///       status codes that were set by BitsRequest::finish_cancel_action are
///       left untouched.
///       If the cancel action failed and NotInProgress is set, the status codes
///       passed to BitsRequest::on_stop are set.
///   7.  onStopRequest is called with the correct status code.
/// Or, if MonitorRunnable calls on_stop before the cancel task can finish:
///   3.  The MonitorRunnable detects that the transfer has ended and calls
///       BitsRequest::on_stop, passing status codes to it.
///   4.  BitsRequest::on_stop checks the CancelAction, sees it is set to
///       InProgress, and sets it to RequestEndedWhileInProgress, carrying over
///       the status code from InProgress.
///   5.  BitsRequest::on_stop sets the status to the value passed to it, which
///       will be overwritten if the cancel action succeeds, but kept if it
///       fails.
///   6.  BitsRequest::on_stop returns early, without calling OnStopRequest.
///   7.  The cancel task returns to the main thread and calls
///       BitsRequest::finish_cancel_action.
///   8.  If the cancel action succeeded, the status codes are set from the
///       value stored in RequestEndedWhileInProgress.
///       If the cancel action failed, the status codes are not changed.
///   9.  The CancelAction is set to NotInProgress.
///   10. BitsRequest::finish_cancel_action calls BitsRequest::on_stop without
///       passing it any status codes.
///   11. onStopRequest is called with the correct status code.
#[derive(Clone, Copy, PartialEq)]
enum CancelAction {
    NotInProgress,
    InProgress(Option<nsresult>),
    RequestEndedWhileInProgress(Option<nsresult>),
    RequestEndPending,
}

#[derive(xpcom)]
#[xpimplements(nsIBitsRequest)]
#[refcnt = "nonatomic"]
pub struct InitBitsRequest {
    bits_id: Guid,
    bits_service: RefPtr<BitsService>,
    // Stores the value to be returned by nsIRequest::IsPending.
    download_pending: Cell<bool>,
    // Stores the value to be returned by nsIRequest::GetStatus.
    download_status_nsresult: Cell<nsresult>,
    // Stores an ErrorType if the request has failed, or None to represent the
    // success state.
    download_status_error_type: Cell<Option<ErrorType>>,
    // This option will be None only after OnStopRequest has been fired.
    monitor_thread: Cell<Option<RefPtr<nsIThread>>>,
    monitor_timeout_ms: u32,
    observer: RefPtr<nsIRequestObserver>,
    context: Option<RefPtr<nsISupports>>,
    // started indicates whether or not OnStartRequest has been fired.
    started: Cell<bool>,
    // finished indicates whether or not we have called
    // BitsService::dec_request_count() to (assuming that there are no other
    // requests) shutdown the command thread.
    finished: Cell<bool>,
    cancel_action: Cell<CancelAction>,
}

impl fmt::Debug for BitsRequest {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "BitsRequest {{ id: {} }}", self.bits_id)
    }
}

/// This implements the nsIBitsRequest interface, documented in nsIBits.idl, to
/// enable BITS job management. This interface deals only with BITS jobs that
/// already exist. Jobs can be created via BitsService, which will create a
/// BitsRequest for that job.
///
/// This is a primarily asynchronous interface, which is accomplished via
/// callbacks of type nsIBitsCallback. The callback is passed in as an argument
/// and is then passed off-thread via a Task. The Task interacts with BITS and
/// is dispatched back to the main thread with the BITS result. Back on the main
/// thread, it returns that result via the callback.
impl BitsRequest {
    pub fn new(
        id: Guid,
        bits_service: RefPtr<BitsService>,
        monitor_timeout_ms: u32,
        observer: RefPtr<nsIRequestObserver>,
        context: Option<RefPtr<nsISupports>>,
        monitor_client: BitsMonitorClient,
        action: ServiceAction,
    ) -> Result<RefPtr<BitsRequest>, BitsTaskError> {
        let action: Action = action.into();
        let monitor_thread = create_thread(&format!("BitsMonitor {}", id)).map_err(|rv| {
            BitsTaskError::from_nsresult(FailedToStartThread, action, MainThread, rv)
        })?;

        // BitsRequest.drop() will call dec_request_count
        bits_service.inc_request_count();
        let request: RefPtr<BitsRequest> = BitsRequest::allocate(InitBitsRequest {
            bits_id: id.clone(),
            bits_service,
            download_pending: Cell::new(true),
            download_status_nsresult: Cell::new(NS_OK),
            download_status_error_type: Cell::new(None),
            monitor_thread: Cell::new(Some(monitor_thread.clone())),
            monitor_timeout_ms,
            observer,
            context,
            started: Cell::new(false),
            finished: Cell::new(false),
            cancel_action: Cell::new(CancelAction::NotInProgress),
        });

        let monitor_runnable =
            MonitorRunnable::new(request.clone(), id, monitor_timeout_ms, monitor_client);

        if let Err(rv) = monitor_runnable.dispatch(monitor_thread.clone()) {
            request.shutdown_monitor_thread();
            return Err(BitsTaskError::from_nsresult(
                FailedToDispatchRunnable,
                action,
                MainThread,
                rv,
            ));
        }

        Ok(request)
    }

    pub fn get_monitor_thread(&self) -> Option<RefPtr<nsIThread>> {
        let monitor_thread = self.monitor_thread.take();
        self.monitor_thread.set(monitor_thread.clone());
        monitor_thread
    }

    fn has_monitor_thread(&self) -> bool {
        let maybe_monitor_thread = self.monitor_thread.take();
        let transferred = maybe_monitor_thread.is_some();
        self.monitor_thread.set(maybe_monitor_thread);
        transferred
    }

    /// If this returns an true, it means that:
    ///   - The monitor thread and monitor runnable may have been shut down
    ///   - The BITS job is not in the TRANSFERRING state
    ///   - The download either completed, failed, or was cancelled
    ///   - The BITS job may or may not still need complete() or cancel() to be
    ///     called on it
    fn request_has_transferred(&self) -> bool {
        self.request_has_completed() || !self.has_monitor_thread()
    }

    /// If this returns an error, it means that:
    ///   - complete() or cancel() has been called on the BITS job.
    ///   - BitsService::dec_request_count has already been called.
    ///   - The BitsClient object that this request was using may have been
    ///     dropped.
    fn request_has_completed(&self) -> bool {
        self.finished.get()
    }

    fn shutdown_monitor_thread(&self) {
        if let Some(monitor_thread) = self.monitor_thread.take() {
            if let Err(rv) = unsafe { monitor_thread.AsyncShutdown() }.to_result() {
                warn!("Failed to shut down monitor thread: {:?}", rv);
                warn!("Releasing reference to thread that failed to shut down!");
            }
        }
    }

    /**
     * To be called when the transfer starts. Fires observer.OnStartRequest exactly once.
     */
    pub fn on_start(&self) {
        if self.started.get() {
            return;
        }
        self.started.set(true);
        if let Err(rv) = unsafe { self.observer.OnStartRequest(self.coerce()) }.to_result() {
            // This behavior is specified by nsIRequestObserver.
            // See nsIRequestObserver.idl
            info!(
                "Cancelling download because OnStartRequest rejected with: {:?}",
                rv
            );
            if let Err(rv) = self.cancel(None, None) {
                warn!("Failed to cancel download: {:?}", rv);
            }
        }
    }

    pub fn on_progress(&self, transferred_bytes: i64, total_bytes: i64) {
        if let Some(progress_event_sink) = self.observer.query_interface::<nsIProgressEventSink>() {
            let context: *const nsISupports = match self.context.as_ref() {
                Some(context) => &**context,
                None => ptr::null(),
            };
            unsafe {
                progress_event_sink.OnProgress(
                    self.coerce(),
                    context,
                    transferred_bytes,
                    total_bytes,
                );
            }
        }
    }

    /// To be called when the transfer stops (fails or completes). Fires
    /// observer.OnStopRequest exactly once, though the call may be delayed to
    /// resolve a race condition.
    ///
    /// The status values, if passed, will be stored in download_status_nsresult
    /// and download_status_error_type, unless they have been overridden by a
    /// cancel action.
    ///
    /// See the documentation for CancelAction for details.
    pub fn on_stop(&self, maybe_status: Option<(nsresult, Option<ErrorType>)>) {
        if !self.has_monitor_thread() {
            // If the request has already stopped, don't stop it again
            return;
        }

        match self.cancel_action.get() {
            CancelAction::InProgress(saved_status)
            | CancelAction::RequestEndedWhileInProgress(saved_status) => {
                if let Some((status, result)) = maybe_status {
                    self.download_status_nsresult.set(status);
                    self.download_status_error_type.set(result);
                }

                info!("Deferring OnStopRequest until Cancel Task completes");
                self.cancel_action
                    .set(CancelAction::RequestEndedWhileInProgress(saved_status));
                return;
            }
            CancelAction::NotInProgress => {
                if let Some((status, result)) = maybe_status {
                    self.download_status_nsresult.set(status);
                    self.download_status_error_type.set(result);
                }
            }
            CancelAction::RequestEndPending => {
                // Don't set the status variables if the end of this request was
                // the result of a cancel action. The cancel action already set
                // those values and they should not be changed.
                // See the CancelAction documentation for details.
            }
        }

        self.download_pending.set(false);
        self.shutdown_monitor_thread();
        unsafe {
            self.observer
                .OnStopRequest(self.coerce(), self.download_status_nsresult.get());
        }
    }

    /// To be called after a cancel or complete task has run successfully. If
    /// this is the only BitsRequest running, this will shut down
    /// BitsService's command thread, destroying the BitsClient.
    pub fn on_finished(&self) {
        if self.finished.get() {
            return;
        }
        self.finished.set(true);
        self.bits_service.dec_request_count();
    }

    // Return the same thing for GetBitsId() and GetName().
    xpcom_method!(
        maybe_get_bits_id => GetBitsId() -> nsACString
    );
    xpcom_method!(
        maybe_get_bits_id => GetName() -> nsACString
    );
    fn maybe_get_bits_id(&self) -> Result<nsCString, nsresult> {
        Ok(self.get_bits_id())
    }
    pub fn get_bits_id(&self) -> nsCString {
        nsCString::from(self.bits_id.to_string())
    }

    xpcom_method!(
        get_bits_transfer_error_nsIBitsRequest => GetTransferError() -> i32
    );
    #[allow(non_snake_case)]
    fn get_bits_transfer_error_nsIBitsRequest(&self) -> Result<i32, nsresult> {
        let error_type = match self.download_status_error_type.get() {
            None => nsIBits::ERROR_TYPE_SUCCESS as i32,
            Some(error_type) => error_type.bits_code(),
        };
        Ok(error_type)
    }

    xpcom_method!(
        is_pending => IsPending() -> bool
    );
    fn is_pending(&self) -> Result<bool, nsresult> {
        Ok(self.download_pending.get())
    }

    xpcom_method!(
        get_status_nsIRequest => GetStatus() -> nsresult
    );
    #[allow(non_snake_case)]
    fn get_status_nsIRequest(&self) -> Result<nsresult, nsresult> {
        Ok(self.get_status())
    }
    pub fn get_status(&self) -> nsresult {
        self.download_status_nsresult.get()
    }

    nsIBitsRequest_method!(
        [Action::SetMonitorInterval]
        change_monitor_interval => ChangeMonitorInterval(update_interval_ms: u32)
    );
    fn change_monitor_interval(
        &self,
        update_interval_ms: u32,
        callback: &nsIBitsCallback,
    ) -> Result<(), BitsTaskError> {
        if update_interval_ms == 0 || update_interval_ms >= self.monitor_timeout_ms {
            return Err(BitsTaskError::new(
                InvalidArgument,
                Action::SetMonitorInterval,
                Pretask,
            ));
        }
        if self.request_has_transferred() {
            return Err(BitsTaskError::new(
                TransferAlreadyComplete,
                Action::SetMonitorInterval,
                Pretask,
            ));
        }

        let task: Box<ChangeMonitorIntervalTask> = Box::new(ChangeMonitorIntervalTask::new(
            RefPtr::new(self),
            self.bits_id.clone(),
            update_interval_ms,
            RefPtr::new(callback),
        ));

        self.bits_service.dispatch_runnable_to_command_thread(
            task,
            "BitsRequest::change_monitor_interval",
            Action::SetMonitorInterval,
        )
    }

    nsIBitsRequest_method!(
        [Action::Cancel]
        cancel_nsIBitsRequest => CancelAsync(status: nsresult)
    );
    #[allow(non_snake_case)]
    fn cancel_nsIBitsRequest(
        &self,
        status: nsresult,
        callback: &nsIBitsCallback,
    ) -> Result<(), BitsTaskError> {
        self.cancel(Some(status), Some(RefPtr::new(callback)))
    }
    xpcom_method!(
        cancel_nsIRequest => Cancel(status: nsresult)
    );
    #[allow(non_snake_case)]
    fn cancel_nsIRequest(&self, status: nsresult) -> Result<(), BitsTaskError> {
        self.cancel(Some(status), None)
    }

    fn cancel(
        &self,
        status: Option<nsresult>,
        callback: Option<RefPtr<nsIBitsCallback>>,
    ) -> Result<(), BitsTaskError> {
        if let Some(cancel_reason) = status.as_ref() {
            if cancel_reason.succeeded() {
                return Err(BitsTaskError::new(InvalidArgument, Action::Cancel, Pretask));
            }
        }
        if self.request_has_completed() {
            return Err(BitsTaskError::new(
                TransferAlreadyComplete,
                Action::Cancel,
                Pretask,
            ));
        }

        if self.cancel_action.get() != CancelAction::NotInProgress {
            return Err(BitsTaskError::new(
                OperationAlreadyInProgress,
                Action::Cancel,
                Pretask,
            ));
        }
        self.cancel_action.set(CancelAction::InProgress(status));

        let task: Box<CancelTask> = Box::new(CancelTask::new(
            RefPtr::new(self),
            self.bits_id.clone(),
            callback,
        ));

        self.bits_service.dispatch_runnable_to_command_thread(
            task,
            "BitsRequest::cancel",
            Action::Cancel,
        )
    }

    /// This function must be called when a cancel action completes.
    ///
    /// See the documentation for CancelAction for details.
    pub fn finish_cancel_action(&self, cancelled_successfully: bool) {
        let (maybe_status, transfer_ended) = match self.cancel_action.get() {
            CancelAction::InProgress(maybe_status) => (maybe_status, false),
            CancelAction::RequestEndedWhileInProgress(maybe_status) => (maybe_status, true),
            _ => {
                error!("End of cancel action, but cancel action is not in progress!");
                return;
            }
        };
        info!(
            "Finishing cancel action. cancel success = {}",
            cancelled_successfully
        );
        if cancelled_successfully {
            if let Some(status) = maybe_status {
                self.download_status_nsresult.set(status);
            }
            self.download_status_error_type
                .set(Some(BitsStateCancelled));
        }

        let next_stage = if cancelled_successfully && !transfer_ended {
            // This signals on_stop not to allow the status codes set above to
            // be overridden by the ones passed to it.
            CancelAction::RequestEndPending
        } else {
            CancelAction::NotInProgress
        };
        self.cancel_action.set(next_stage);

        if cancelled_successfully {
            self.on_finished();
        }

        if transfer_ended {
            info!("Running deferred OnStopRequest");
            self.on_stop(None);
        }
    }

    nsIBitsRequest_method!(
        [Action::SetPriority]
        set_priority_high => SetPriorityHigh()
    );
    fn set_priority_high(&self, callback: &nsIBitsCallback) -> Result<(), BitsTaskError> {
        self.set_priority(Priority::High, callback)
    }

    nsIBitsRequest_method!(
        [Action::SetPriority]
        set_priority_low => SetPriorityLow()
    );
    fn set_priority_low(&self, callback: &nsIBitsCallback) -> Result<(), BitsTaskError> {
        self.set_priority(Priority::Low, callback)
    }

    fn set_priority(
        &self,
        priority: Priority,
        callback: &nsIBitsCallback,
    ) -> Result<(), BitsTaskError> {
        if self.request_has_transferred() {
            return Err(BitsTaskError::new(
                TransferAlreadyComplete,
                Action::SetPriority,
                Pretask,
            ));
        }

        let task: Box<SetPriorityTask> = Box::new(SetPriorityTask::new(
            RefPtr::new(self),
            self.bits_id.clone(),
            priority,
            RefPtr::new(callback),
        ));

        self.bits_service.dispatch_runnable_to_command_thread(
            task,
            "BitsRequest::set_priority",
            Action::SetPriority,
        )
    }

    nsIBitsRequest_method!(
        [Action::Complete]
        complete => Complete()
    );
    fn complete(&self, callback: &nsIBitsCallback) -> Result<(), BitsTaskError> {
        if self.request_has_completed() {
            return Err(BitsTaskError::new(
                TransferAlreadyComplete,
                Action::Complete,
                Pretask,
            ));
        }

        let task: Box<CompleteTask> = Box::new(CompleteTask::new(
            RefPtr::new(self),
            self.bits_id.clone(),
            RefPtr::new(callback),
        ));

        self.bits_service.dispatch_runnable_to_command_thread(
            task,
            "BitsRequest::complete",
            Action::Complete,
        )
    }

    nsIBitsRequest_method!(
        [Action::Suspend]
        suspend_nsIBitsRequest => SuspendAsync()
    );
    #[allow(non_snake_case)]
    fn suspend_nsIBitsRequest(&self, callback: &nsIBitsCallback) -> Result<(), BitsTaskError> {
        self.suspend(Some(RefPtr::new(callback)))
    }
    xpcom_method!(
        suspend_nsIRequest => Suspend()
    );
    #[allow(non_snake_case)]
    fn suspend_nsIRequest(&self) -> Result<(), BitsTaskError> {
        self.suspend(None)
    }

    fn suspend(&self, callback: Option<RefPtr<nsIBitsCallback>>) -> Result<(), BitsTaskError> {
        if self.request_has_transferred() {
            return Err(BitsTaskError::new(
                TransferAlreadyComplete,
                Action::Suspend,
                Pretask,
            ));
        }

        let task: Box<SuspendTask> = Box::new(SuspendTask::new(
            RefPtr::new(self),
            self.bits_id.clone(),
            callback,
        ));

        self.bits_service.dispatch_runnable_to_command_thread(
            task,
            "BitsRequest::suspend",
            Action::Suspend,
        )
    }

    nsIBitsRequest_method!(
        [Action::Resume]
        resume_nsIBitsRequest => ResumeAsync()
    );
    #[allow(non_snake_case)]
    fn resume_nsIBitsRequest(&self, callback: &nsIBitsCallback) -> Result<(), BitsTaskError> {
        self.resume(Some(RefPtr::new(callback)))
    }
    xpcom_method!(
        resume_nsIRequest => Resume()
    );
    #[allow(non_snake_case)]
    fn resume_nsIRequest(&self) -> Result<(), BitsTaskError> {
        self.resume(None)
    }

    fn resume(&self, callback: Option<RefPtr<nsIBitsCallback>>) -> Result<(), BitsTaskError> {
        if self.request_has_transferred() {
            return Err(BitsTaskError::new(
                TransferAlreadyComplete,
                Action::Resume,
                Pretask,
            ));
        }

        let task: Box<ResumeTask> = Box::new(ResumeTask::new(
            RefPtr::new(self),
            self.bits_id.clone(),
            callback,
        ));

        self.bits_service.dispatch_runnable_to_command_thread(
            task,
            "BitsRequest::resume",
            Action::Resume,
        )
    }

    /**
     * As stated in nsIBits.idl, nsIBits interfaces are not expected to
     * implement the loadGroup or loadFlags attributes. This implementation
     * provides only null implementations only for these methods.
     */
    xpcom_method!(
        get_load_group => GetLoadGroup() -> *const nsILoadGroup
    );
    fn get_load_group(&self) -> Result<RefPtr<nsILoadGroup>, nsresult> {
        Err(NS_ERROR_NOT_IMPLEMENTED)
    }

    xpcom_method!(
        set_load_group => SetLoadGroup(_load_group: *const nsILoadGroup)
    );
    fn set_load_group(&self, _load_group: &nsILoadGroup) -> Result<(), nsresult> {
        Err(NS_ERROR_NOT_IMPLEMENTED)
    }

    xpcom_method!(
        get_load_flags => GetLoadFlags() -> nsLoadFlags
    );
    fn get_load_flags(&self) -> Result<nsLoadFlags, nsresult> {
        Err(NS_ERROR_NOT_IMPLEMENTED)
    }

    xpcom_method!(
        set_load_flags => SetLoadFlags(_load_flags: nsLoadFlags)
    );
    fn set_load_flags(&self, _load_flags: nsLoadFlags) -> Result<(), nsresult> {
        Err(NS_ERROR_NOT_IMPLEMENTED)
    }
}

impl Drop for BitsRequest {
    fn drop(&mut self) {
        // Make sure that the monitor thread gets cleaned up.
        self.shutdown_monitor_thread();
        // Make sure we tell BitsService that we are done with the command thread.
        self.on_finished();
    }
}
