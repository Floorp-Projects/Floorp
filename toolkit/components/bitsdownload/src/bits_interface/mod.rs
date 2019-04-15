/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
mod action;
mod dispatch_callback;
mod error;
mod monitor;
mod request;
mod string;
mod task;
mod xpcom_methods;

pub use self::error::BitsTaskError;
use self::{
    action::Action,
    error::{
        ErrorStage::Pretask,
        ErrorType::{
            FailedToConstructTaskRunnable, FailedToDispatchRunnable, FailedToStartThread,
            InvalidArgument, NotInitialized,
        },
    },
    request::BitsRequest,
    string::Guid_from_nsCString,
    task::{ClientInitData, MonitorDownloadTask, StartDownloadTask},
};
use nsIBits_method; // From xpcom_method.rs

use bits_client::BitsProxyUsage;
use log::{info, warn};
use moz_task::{create_thread, Task, TaskRunnable};
use nserror::{nsresult, NS_ERROR_ALREADY_INITIALIZED, NS_OK};
use nsstring::{nsACString, nsCString};
use std::cell::Cell;
use xpcom::{
    interfaces::{
        nsIBits, nsIBitsNewRequestCallback, nsIRequestObserver, nsISupports, nsIThread,
        nsProxyUsage,
    },
    xpcom, xpcom_method, RefPtr,
};

#[no_mangle]
pub unsafe extern "C" fn new_bits_service(result: *mut *const nsIBits) {
    let service: RefPtr<BitsService> = BitsService::new();
    RefPtr::new(service.coerce::<nsIBits>()).forget(&mut *result);
}

#[derive(xpcom)]
#[xpimplements(nsIBits)]
#[refcnt = "nonatomic"]
pub struct InitBitsService {
    // This command thread will be used to send commands (ex: Suspend, Resume)
    // to a running job. It will be started up when the first job is created and
    // shutdown when all jobs have been completed or cancelled.
    command_thread: Cell<Option<RefPtr<nsIThread>>>,
    client_init_data: Cell<Option<ClientInitData>>,
    // This count will track the number of in-progress requests so that the
    // service knows when the command_thread is no longer being used and can be
    // shut down.
    // `BitsRequest::new()` will increment this and it will be decremented
    // either when cancel/complete is called, or when the request is dropped
    // (if it didn't decrement it already).
    // The count will also be incremented when an action to create a request
    // starts and decremented when the action ends and returns the result via
    // the callback. This prevents the command thread from being shut down while
    // a job is being created.
    request_count: Cell<u32>,
}

/// This implements the nsIBits interface, documented in nsIBits.idl, to enable
/// BITS job management. Specifically, this interface can start a download or
/// connect to an existing download. Doing so will create a BitsRequest through
/// which the transfer can be further manipulated.
///
/// This is a primarily asynchronous interface, which is accomplished via
/// callbacks of type nsIBitsNewRequestCallback. The callback is passed in as
/// an argument and is then passed off-thread via a Task. The Task interacts
/// with BITS and is dispatched back to the main thread with the BITS result.
/// Back on the main thread, it returns that result via the callback including,
/// if successful, a BitsRequest.
impl BitsService {
    pub fn new() -> RefPtr<BitsService> {
        BitsService::allocate(InitBitsService {
            command_thread: Cell::new(None),
            client_init_data: Cell::new(None),
            request_count: Cell::new(0),
        })
    }

    fn get_client_init(&self) -> Option<ClientInitData> {
        let maybe_init_data = self.client_init_data.take();
        self.client_init_data.set(maybe_init_data.clone());
        maybe_init_data
    }

    // Returns the handle to the command thread. If it has not been started yet,
    // the thread will be started.
    fn get_command_thread(&self) -> Result<RefPtr<nsIThread>, nsresult> {
        let mut command_thread = self.command_thread.take();
        if command_thread.is_none() {
            command_thread.replace(create_thread("BitsCommander")?);
        }
        self.command_thread.set(command_thread.clone());
        Ok(command_thread.unwrap())
    }

    // Asynchronously shuts down the command thread. The thread is not shutdown
    // until the event queue is empty, so any tasks that were dispatched before
    // this is called will still run.
    // Leaves None in self.command_thread
    fn shutdown_command_thread(&self) {
        if let Some(command_thread) = self.command_thread.take() {
            if let Err(rv) = unsafe { command_thread.AsyncShutdown() }.to_result() {
                warn!("Failed to shut down command thread: {}", rv);
                warn!("Releasing reference to thread that failed to shut down!");
            }
        }
    }

    fn dispatch_runnable_to_command_thread(
        &self,
        task: Box<dyn Task + Send + Sync>,
        task_runnable_name: &'static str,
        action: Action,
    ) -> Result<(), BitsTaskError> {
        let command_thread = self
            .get_command_thread()
            .map_err(|rv| BitsTaskError::from_nsresult(FailedToStartThread, action, Pretask, rv))?;
        let runnable = TaskRunnable::new(task_runnable_name, task).map_err(|rv| {
            BitsTaskError::from_nsresult(FailedToConstructTaskRunnable, action, Pretask, rv)
        })?;
        runnable.dispatch(&command_thread).map_err(|rv| {
            BitsTaskError::from_nsresult(FailedToDispatchRunnable, action, Pretask, rv)
        })
    }

    fn inc_request_count(&self) {
        self.request_count.set(self.request_count.get() + 1);
    }

    fn dec_request_count(&self) {
        let mut count = self.request_count.get();
        if count == 0 {
            warn!("Attempted to decrement request count, but it is 0");
            return;
        }
        count -= 1;
        self.request_count.set(count);

        if count == 0 {
            self.shutdown_command_thread();
        }
    }

    xpcom_method!(
        get_initialized => GetInitialized() -> bool
    );
    fn get_initialized(&self) -> Result<bool, nsresult> {
        Ok(self.get_client_init().is_some())
    }

    xpcom_method!(
        init => Init(
            job_name: *const nsACString,
            save_path_prefix: *const nsACString,
            monitor_timeout_ms: u32
        )
    );
    fn init(
        &self,
        job_name: &nsACString,
        save_path_prefix: &nsACString,
        monitor_timeout_ms: u32,
    ) -> Result<(), nsresult> {
        let previous_data = self.client_init_data.take();
        if previous_data.is_some() {
            self.client_init_data.set(previous_data);
            return Err(NS_ERROR_ALREADY_INITIALIZED);
        }

        info!(
            "BitsService initialized with job_name: {}, save_path_prefix: {}, timeout: {}",
            job_name, save_path_prefix, monitor_timeout_ms,
        );

        self.client_init_data.set(Some(ClientInitData::new(
            nsCString::from(job_name),
            nsCString::from(save_path_prefix),
            monitor_timeout_ms,
        )));

        Ok(())
    }

    nsIBits_method!(
        [Action::StartDownload]
        start_download => StartDownload(
            download_url: *const nsACString,
            save_rel_path: *const nsACString,
            proxy: nsProxyUsage,
            update_interval_ms: u32,
            observer: *const nsIRequestObserver,
            [optional] context: *const nsISupports,
        )
    );
    fn start_download(
        &self,
        download_url: &nsACString,
        save_rel_path: &nsACString,
        proxy: nsProxyUsage,
        update_interval_ms: u32,
        observer: &nsIRequestObserver,
        context: Option<&nsISupports>,
        callback: &nsIBitsNewRequestCallback,
    ) -> Result<(), BitsTaskError> {
        let client_init_data = self
            .get_client_init()
            .ok_or_else(|| BitsTaskError::new(NotInitialized, Action::StartDownload, Pretask))?;
        if update_interval_ms >= client_init_data.monitor_timeout_ms {
            return Err(BitsTaskError::new(
                InvalidArgument,
                Action::StartDownload,
                Pretask,
            ));
        }
        let proxy = match proxy as i64 {
            nsIBits::PROXY_NONE => BitsProxyUsage::NoProxy,
            nsIBits::PROXY_PRECONFIG => BitsProxyUsage::Preconfig,
            nsIBits::PROXY_AUTODETECT => BitsProxyUsage::AutoDetect,
            _ => {
                return Err(BitsTaskError::new(
                    InvalidArgument,
                    Action::StartDownload,
                    Pretask,
                ));
            }
        };

        let task: Box<StartDownloadTask> = Box::new(StartDownloadTask::new(
            client_init_data,
            nsCString::from(download_url),
            nsCString::from(save_rel_path),
            proxy,
            update_interval_ms,
            RefPtr::new(self),
            RefPtr::new(observer),
            context.map(RefPtr::new),
            RefPtr::new(callback),
        ));

        let dispatch_result = self.dispatch_runnable_to_command_thread(
            task,
            "BitsService::start_download",
            Action::StartDownload,
        );

        if dispatch_result.is_ok() {
            // Increment the request count when we dispatch an action to start
            // a job, decrement it when the action completes. See the
            // declaration of InitBitsService::request_count for details.
            self.inc_request_count();
        }

        dispatch_result
    }

    nsIBits_method!(
        [Action::MonitorDownload]
        monitor_download => MonitorDownload(
            id: *const nsACString,
            update_interval_ms: u32,
            observer: *const nsIRequestObserver,
            [optional] context: *const nsISupports,
        )
    );
    fn monitor_download(
        &self,
        id: &nsACString,
        update_interval_ms: u32,
        observer: &nsIRequestObserver,
        context: Option<&nsISupports>,
        callback: &nsIBitsNewRequestCallback,
    ) -> Result<(), BitsTaskError> {
        let client_init_data = self
            .get_client_init()
            .ok_or_else(|| BitsTaskError::new(NotInitialized, Action::MonitorDownload, Pretask))?;
        if update_interval_ms >= client_init_data.monitor_timeout_ms {
            return Err(BitsTaskError::new(
                InvalidArgument,
                Action::MonitorDownload,
                Pretask,
            ));
        }
        let guid = Guid_from_nsCString(&nsCString::from(id), Action::MonitorDownload, Pretask)?;

        let task: Box<MonitorDownloadTask> = Box::new(MonitorDownloadTask::new(
            client_init_data,
            guid,
            update_interval_ms,
            RefPtr::new(self),
            RefPtr::new(observer),
            context.map(RefPtr::new),
            RefPtr::new(callback),
        ));

        let dispatch_result = self.dispatch_runnable_to_command_thread(
            task,
            "BitsService::monitor_download",
            Action::MonitorDownload,
        );

        if dispatch_result.is_ok() {
            // Increment the request count when we dispatch an action to start
            // a job, decrement it when the action completes. See the
            // declaration of InitBitsService::request_count for details.
            self.inc_request_count();
        }

        dispatch_result
    }
}

impl Drop for BitsService {
    fn drop(&mut self) {
        self.shutdown_command_thread();
    }
}
