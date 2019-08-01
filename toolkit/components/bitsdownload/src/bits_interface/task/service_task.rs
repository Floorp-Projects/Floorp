/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
use super::{
    action::{
        Action,
        Action::{MonitorDownload, StartDownload},
        ServiceAction,
    },
    client::{with_maybe_new_bits_client, ClientInitData},
    dispatch_callback::{maybe_dispatch_request_via_callback, CallbackExpected},
    error::{BitsTaskError, ErrorStage::CommandThread},
    from_threadbound::{expect_from_threadbound_option, get_from_threadbound_option, DataType},
    string::nsCString_to_OsString,
    BitsRequest, BitsService,
};

use bits_client::{BitsClient, BitsMonitorClient, BitsProxyUsage, Guid};
use crossbeam_utils::atomic::AtomicCell;
use log::{info, warn};
use moz_task::Task;
use nserror::nsresult;
use nsstring::nsCString;
use xpcom::{
    interfaces::{nsIBitsNewRequestCallback, nsIRequestObserver, nsISupports},
    RefPtr, ThreadBoundRefPtr,
};

// D is the Data Type that the RunFn function needs to make S.
// S is the Success Type that the RunFn returns on success and that the
//   DoneFn needs to make the BitsRequest.
type RunFn<D, S> = fn(&D, &mut BitsClient) -> Result<S, BitsTaskError>;
type DoneFn<D, S> = fn(
    &D,
    S,
    &ClientInitData,
    &BitsService,
    &nsIRequestObserver,
    Option<&nsISupports>,
) -> Result<RefPtr<BitsRequest>, BitsTaskError>;

pub struct ServiceTask<D, S> {
    client_init_data: ClientInitData,
    action: ServiceAction,
    task_data: D,
    run_fn: RunFn<D, S>,
    done_fn: DoneFn<D, S>,
    bits_service: AtomicCell<Option<ThreadBoundRefPtr<BitsService>>>,
    observer: AtomicCell<Option<ThreadBoundRefPtr<nsIRequestObserver>>>,
    context: AtomicCell<Option<ThreadBoundRefPtr<nsISupports>>>,
    callback: AtomicCell<Option<ThreadBoundRefPtr<nsIBitsNewRequestCallback>>>,
    result: AtomicCell<Option<Result<S, BitsTaskError>>>,
}

impl<D, S> ServiceTask<D, S>
where
    D: Sync + Send,
    S: Sync + Send,
{
    pub fn new(
        client_init_data: ClientInitData,
        action: ServiceAction,
        task_data: D,
        run_fn: RunFn<D, S>,
        done_fn: DoneFn<D, S>,
        bits_service: RefPtr<BitsService>,
        observer: RefPtr<nsIRequestObserver>,
        context: Option<RefPtr<nsISupports>>,
        callback: RefPtr<nsIBitsNewRequestCallback>,
    ) -> ServiceTask<D, S> {
        ServiceTask {
            client_init_data,
            action,
            task_data,
            run_fn,
            done_fn,
            bits_service: AtomicCell::new(Some(ThreadBoundRefPtr::new(bits_service))),
            observer: AtomicCell::new(Some(ThreadBoundRefPtr::new(observer))),
            context: AtomicCell::new(context.map(ThreadBoundRefPtr::new)),
            callback: AtomicCell::new(Some(ThreadBoundRefPtr::new(callback))),
            result: AtomicCell::new(None),
        }
    }
}

impl<D, S> Task for ServiceTask<D, S> {
    fn run(&self) {
        let result =
            with_maybe_new_bits_client(&self.client_init_data, self.action.into(), |client| {
                (self.run_fn)(&self.task_data, client)
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
        // It also isn't safe to drop the BitsService RefPtr off-thread,
        // because BitsService refcounting is non-atomic
        let maybe_tb_service = self.bits_service.swap(None);
        // The observer and context are also an nsXPCWrappedJS that aren't safe
        // to release on the worker thread.
        let maybe_tb_observer = self.observer.swap(None);
        let maybe_tb_context = self.context.swap(None);

        let action: Action = self.action.into();
        let maybe_callback =
            expect_from_threadbound_option(&maybe_tb_callback, DataType::Callback, action);

        // Immediately invoked function expression to allow for the ? operator
        let result: Result<RefPtr<BitsRequest>, BitsTaskError> = (|| {
            let bits_service =
                expect_from_threadbound_option(&maybe_tb_service, DataType::BitsService, action)?;
            let observer =
                expect_from_threadbound_option(&maybe_tb_observer, DataType::Observer, action)?;
            let maybe_context =
                get_from_threadbound_option(&maybe_tb_context, DataType::Context, action);
            let success = self
                .result
                .swap(None)
                .ok_or_else(|| BitsTaskError::missing_result(action))??;

            (self.done_fn)(
                &self.task_data,
                success,
                &self.client_init_data,
                bits_service,
                observer,
                maybe_context,
            )
        })();
        info!("BITS Interface Task completed: {:?}", result);
        // We incremented the request count when we dispatched an action to
        // start the job. Now we will decrement since the action completed.
        // See the declaration of InitBitsService::request_count for details.
        let bits_service_result =
            expect_from_threadbound_option(&maybe_tb_service, DataType::BitsService, action);
        match bits_service_result {
            Ok(bits_service) => {
                bits_service.dec_request_count();
            }
            Err(error) => {
                warn!(
                    concat!(
                        "Unable to decrement the request count when finishing ServiceTask. ",
                        "The command thread may not be shut down. Error: {:?}"
                    ),
                    error
                );
            }
        }

        maybe_dispatch_request_via_callback(result, maybe_callback, CallbackExpected)
    }
}

struct StartDownloadData {
    download_url: nsCString,
    save_rel_path: nsCString,
    proxy: BitsProxyUsage,
    no_progress_timeout_secs: u32,
    update_interval_ms: u32,
}

struct StartDownloadSuccess {
    guid: Guid,
    monitor_client: BitsMonitorClient,
}

pub struct StartDownloadTask(ServiceTask<StartDownloadData, StartDownloadSuccess>);

impl Task for StartDownloadTask {
    fn run(&self) {
        self.0.run();
    }

    fn done(&self) -> Result<(), nsresult> {
        self.0.done()
    }
}

impl StartDownloadTask {
    pub fn new(
        client_init_data: ClientInitData,
        download_url: nsCString,
        save_rel_path: nsCString,
        proxy: BitsProxyUsage,
        no_progress_timeout_secs: u32,
        update_interval_ms: u32,
        bits_service: RefPtr<BitsService>,
        observer: RefPtr<nsIRequestObserver>,
        context: Option<RefPtr<nsISupports>>,
        callback: RefPtr<nsIBitsNewRequestCallback>,
    ) -> StartDownloadTask {
        StartDownloadTask(ServiceTask::new(
            client_init_data,
            ServiceAction::StartDownload,
            StartDownloadData {
                download_url,
                save_rel_path,
                proxy,
                no_progress_timeout_secs,
                update_interval_ms,
            },
            StartDownloadTask::run_fn,
            StartDownloadTask::done_fn,
            bits_service,
            observer,
            context,
            callback,
        ))
    }

    fn run_fn(
        data: &StartDownloadData,
        client: &mut BitsClient,
    ) -> Result<StartDownloadSuccess, BitsTaskError> {
        let url = nsCString_to_OsString(&data.download_url, StartDownload, CommandThread)?;
        let path = nsCString_to_OsString(&data.save_rel_path, StartDownload, CommandThread)?;
        let (success, monitor_client) = client
            .start_job(
                url,
                path,
                data.proxy,
                data.no_progress_timeout_secs,
                data.update_interval_ms,
            )
            .map_err(|pipe_error| BitsTaskError::from_pipe(StartDownload, pipe_error))??;
        Ok(StartDownloadSuccess {
            guid: success.guid,
            monitor_client,
        })
    }

    fn done_fn(
        _data: &StartDownloadData,
        success: StartDownloadSuccess,
        client_init_data: &ClientInitData,
        bits_service: &BitsService,
        observer: &nsIRequestObserver,
        maybe_context: Option<&nsISupports>,
    ) -> Result<RefPtr<BitsRequest>, BitsTaskError> {
        BitsRequest::new(
            success.guid.clone(),
            RefPtr::new(bits_service),
            client_init_data.monitor_timeout_ms,
            RefPtr::new(&observer),
            maybe_context.map(RefPtr::new),
            success.monitor_client,
            ServiceAction::StartDownload,
        )
    }
}

struct MonitorDownloadData {
    guid: Guid,
    update_interval_ms: u32,
}

pub struct MonitorDownloadTask(ServiceTask<MonitorDownloadData, BitsMonitorClient>);

impl Task for MonitorDownloadTask {
    fn run(&self) {
        self.0.run();
    }

    fn done(&self) -> Result<(), nsresult> {
        self.0.done()
    }
}

impl MonitorDownloadTask {
    pub fn new(
        client_init_data: ClientInitData,
        guid: Guid,
        update_interval_ms: u32,
        bits_service: RefPtr<BitsService>,
        observer: RefPtr<nsIRequestObserver>,
        context: Option<RefPtr<nsISupports>>,
        callback: RefPtr<nsIBitsNewRequestCallback>,
    ) -> MonitorDownloadTask {
        MonitorDownloadTask(ServiceTask::new(
            client_init_data,
            ServiceAction::MonitorDownload,
            MonitorDownloadData {
                guid,
                update_interval_ms,
            },
            MonitorDownloadTask::run_fn,
            MonitorDownloadTask::done_fn,
            bits_service,
            observer,
            context,
            callback,
        ))
    }

    fn run_fn(
        data: &MonitorDownloadData,
        client: &mut BitsClient,
    ) -> Result<BitsMonitorClient, BitsTaskError> {
        let result = client
            .monitor_job(data.guid.clone(), data.update_interval_ms)
            .map_err(|pipe_error| BitsTaskError::from_pipe(MonitorDownload, pipe_error));
        Ok(result??)
    }

    fn done_fn(
        data: &MonitorDownloadData,
        monitor_client: BitsMonitorClient,
        client_init_data: &ClientInitData,
        bits_service: &BitsService,
        observer: &nsIRequestObserver,
        maybe_context: Option<&nsISupports>,
    ) -> Result<RefPtr<BitsRequest>, BitsTaskError> {
        BitsRequest::new(
            data.guid.clone(),
            RefPtr::new(bits_service),
            client_init_data.monitor_timeout_ms,
            RefPtr::new(&observer),
            maybe_context.map(RefPtr::new),
            monitor_client,
            ServiceAction::MonitorDownload,
        )
    }
}
