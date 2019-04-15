/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
use super::{
    action::{Action, RequestAction},
    client::with_bits_client,
    dispatch_callback::{
        maybe_dispatch_via_callback, CallbackExpected, CallbackOptional, IsCallbackExpected,
    },
    error::BitsTaskError,
    from_threadbound::{expect_from_threadbound_option, DataType},
    BitsRequest,
};

use bits_client::{BitsClient, Guid};
use crossbeam_utils::atomic::AtomicCell;
use log::info;
use moz_task::Task;
use nserror::nsresult;
use xpcom::{interfaces::nsIBitsCallback, RefPtr, ThreadBoundRefPtr};

type RunFn<D> = fn(Guid, &D, &mut BitsClient) -> Result<(), BitsTaskError>;
type DoneFn = fn(&BitsRequest, bool) -> Result<(), BitsTaskError>;

pub struct RequestTask<D> {
    request: AtomicCell<Option<ThreadBoundRefPtr<BitsRequest>>>,
    guid: Guid,
    action: RequestAction,
    task_data: D,
    run_fn: RunFn<D>,
    maybe_done_fn: Option<DoneFn>,
    callback: AtomicCell<Option<ThreadBoundRefPtr<nsIBitsCallback>>>,
    callback_presence: IsCallbackExpected,
    result: AtomicCell<Option<Result<(), BitsTaskError>>>,
}

impl<D> RequestTask<D>
where
    D: Sync + Send,
{
    pub fn new(
        request: RefPtr<BitsRequest>,
        guid: Guid,
        action: RequestAction,
        task_data: D,
        run_fn: RunFn<D>,
        maybe_done_fn: Option<DoneFn>,
        callback: Option<RefPtr<nsIBitsCallback>>,
        callback_presence: IsCallbackExpected,
    ) -> RequestTask<D> {
        RequestTask {
            request: AtomicCell::new(Some(ThreadBoundRefPtr::new(request))),
            guid,
            action,
            task_data,
            run_fn,
            maybe_done_fn,
            callback: AtomicCell::new(callback.map(ThreadBoundRefPtr::new)),
            result: AtomicCell::new(None),
            callback_presence,
        }
    }
}

impl<D> Task for RequestTask<D> {
    fn run(&self) {
        let result = with_bits_client(self.action.into(), |client| {
            (self.run_fn)(self.guid.clone(), &self.task_data, client)
        });
        self.result.store(Some(result));
    }

    fn done(&self) -> Result<(), nsresult> {
        // If TaskRunnable.run() calls Task.done() to return a result
        // on the main thread before TaskRunnable.run() returns on the worker
        // thread, then the Task will get dropped on the worker thread.
        //
        // But the callback is an nsXPCWrappedJS that isn't safe to release
        // on the worker thread.  So we move it out of the Task here to ensure
        // it gets released on the main thread.
        let maybe_tb_callback = self.callback.swap(None);
        // It also isn't safe to drop the BitsRequest RefPtr off-thread,
        // because BitsRequest refcounting is non-atomic
        let maybe_tb_request = self.request.swap(None);

        let action: Action = self.action.into();
        let maybe_callback =
            expect_from_threadbound_option(&maybe_tb_callback, DataType::Callback, action);

        // Immediately invoked function expression to allow for the ? operator
        let result: Result<(), BitsTaskError> = (|| {
            let request =
                expect_from_threadbound_option(&maybe_tb_request, DataType::BitsRequest, action)?;

            let maybe_result = self.result.swap(None);

            let success = if let Some(result) = maybe_result.as_ref() {
                result.is_ok()
            } else {
                false
            };

            if let Some(done_fn) = self.maybe_done_fn {
                done_fn(request, success)?;
            }

            maybe_result.ok_or_else(|| BitsTaskError::missing_result(action))?
        })();
        info!("BITS Request Task completed: {:?}", result);
        maybe_dispatch_via_callback(result, maybe_callback, self.callback_presence)
    }
}

pub struct CompleteTask(RequestTask<()>);

impl Task for CompleteTask {
    fn run(&self) {
        self.0.run();
    }

    fn done(&self) -> Result<(), nsresult> {
        self.0.done()
    }
}

impl CompleteTask {
    pub fn new(
        request: RefPtr<BitsRequest>,
        id: Guid,
        callback: RefPtr<nsIBitsCallback>,
    ) -> CompleteTask {
        CompleteTask(RequestTask::new(
            request,
            id,
            RequestAction::Complete,
            (),
            CompleteTask::run_fn,
            Some(CompleteTask::done_fn),
            Some(callback),
            CallbackExpected,
        ))
    }

    fn run_fn(id: Guid, _data: &(), client: &mut BitsClient) -> Result<(), BitsTaskError> {
        client
            .complete_job(id)
            .map_err(|pipe_error| BitsTaskError::from_pipe(Action::Complete, pipe_error))??;
        Ok(())
    }

    fn done_fn(request: &BitsRequest, success: bool) -> Result<(), BitsTaskError> {
        if success {
            request.on_finished();
        }
        Ok(())
    }
}

pub struct CancelTask(RequestTask<()>);

impl Task for CancelTask {
    fn run(&self) {
        self.0.run();
    }

    fn done(&self) -> Result<(), nsresult> {
        self.0.done()
    }
}

impl CancelTask {
    pub fn new(
        request: RefPtr<BitsRequest>,
        id: Guid,
        callback: Option<RefPtr<nsIBitsCallback>>,
    ) -> CancelTask {
        let callback_presence = if callback.is_some() {
            CallbackExpected
        } else {
            CallbackOptional
        };

        CancelTask(RequestTask::new(
            request,
            id,
            RequestAction::Cancel,
            (),
            CancelTask::run_fn,
            Some(CancelTask::done_fn),
            callback,
            callback_presence,
        ))
    }

    fn run_fn(id: Guid, _data: &(), client: &mut BitsClient) -> Result<(), BitsTaskError> {
        client
            .cancel_job(id)
            .map_err(|pipe_error| BitsTaskError::from_pipe(Action::Cancel, pipe_error))??;
        Ok(())
    }

    fn done_fn(request: &BitsRequest, success: bool) -> Result<(), BitsTaskError> {
        request.finish_cancel_action(success);
        Ok(())
    }
}

pub struct SuspendTask(RequestTask<()>);

impl Task for SuspendTask {
    fn run(&self) {
        self.0.run();
    }

    fn done(&self) -> Result<(), nsresult> {
        self.0.done()
    }
}

impl SuspendTask {
    pub fn new(
        request: RefPtr<BitsRequest>,
        id: Guid,
        callback: Option<RefPtr<nsIBitsCallback>>,
    ) -> SuspendTask {
        let callback_presence = if callback.is_some() {
            CallbackExpected
        } else {
            CallbackOptional
        };

        SuspendTask(RequestTask::new(
            request,
            id,
            RequestAction::Suspend,
            (),
            SuspendTask::run_fn,
            None,
            callback,
            callback_presence,
        ))
    }

    fn run_fn(id: Guid, _data: &(), client: &mut BitsClient) -> Result<(), BitsTaskError> {
        client
            .suspend_job(id)
            .map_err(|pipe_error| BitsTaskError::from_pipe(Action::Suspend, pipe_error))??;
        Ok(())
    }
}

pub struct ResumeTask(RequestTask<()>);

impl Task for ResumeTask {
    fn run(&self) {
        self.0.run();
    }

    fn done(&self) -> Result<(), nsresult> {
        self.0.done()
    }
}

impl ResumeTask {
    pub fn new(
        request: RefPtr<BitsRequest>,
        id: Guid,
        callback: Option<RefPtr<nsIBitsCallback>>,
    ) -> ResumeTask {
        let callback_presence = if callback.is_some() {
            CallbackExpected
        } else {
            CallbackOptional
        };

        ResumeTask(RequestTask::new(
            request,
            id,
            RequestAction::Resume,
            (),
            ResumeTask::run_fn,
            None,
            callback,
            callback_presence,
        ))
    }

    fn run_fn(id: Guid, _data: &(), client: &mut BitsClient) -> Result<(), BitsTaskError> {
        client
            .resume_job(id)
            .map_err(|pipe_error| BitsTaskError::from_pipe(Action::Resume, pipe_error))??;
        Ok(())
    }
}

pub struct ChangeMonitorIntervalTask(RequestTask<u32>);

impl Task for ChangeMonitorIntervalTask {
    fn run(&self) {
        self.0.run();
    }

    fn done(&self) -> Result<(), nsresult> {
        self.0.done()
    }
}

impl ChangeMonitorIntervalTask {
    pub fn new(
        request: RefPtr<BitsRequest>,
        id: Guid,
        update_interval_ms: u32,
        callback: RefPtr<nsIBitsCallback>,
    ) -> ChangeMonitorIntervalTask {
        ChangeMonitorIntervalTask(RequestTask::new(
            request,
            id,
            RequestAction::SetMonitorInterval,
            update_interval_ms,
            ChangeMonitorIntervalTask::run_fn,
            None,
            Some(callback),
            CallbackExpected,
        ))
    }

    fn run_fn(
        id: Guid,
        update_interval_ms: &u32,
        client: &mut BitsClient,
    ) -> Result<(), BitsTaskError> {
        client
            .set_update_interval(id, *update_interval_ms)
            .map_err(|pipe_error| {
                BitsTaskError::from_pipe(Action::SetMonitorInterval, pipe_error)
            })??;
        Ok(())
    }
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum Priority {
    High,
    Low,
}

pub struct SetPriorityTask(RequestTask<Priority>);

impl Task for SetPriorityTask {
    fn run(&self) {
        self.0.run();
    }

    fn done(&self) -> Result<(), nsresult> {
        self.0.done()
    }
}

impl SetPriorityTask {
    pub fn new(
        request: RefPtr<BitsRequest>,
        id: Guid,
        priority: Priority,
        callback: RefPtr<nsIBitsCallback>,
    ) -> SetPriorityTask {
        SetPriorityTask(RequestTask::new(
            request,
            id,
            RequestAction::SetPriority,
            priority,
            SetPriorityTask::run_fn,
            None,
            Some(callback),
            CallbackExpected,
        ))
    }

    fn run_fn(id: Guid, priority: &Priority, client: &mut BitsClient) -> Result<(), BitsTaskError> {
        client
            .set_job_priority(id, *priority == Priority::High)
            .map_err(|pipe_error| BitsTaskError::from_pipe(Action::SetPriority, pipe_error))??;
        Ok(())
    }
}
