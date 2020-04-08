/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{cell::RefCell, fmt::Write, mem, sync::Arc};

use atomic_refcell::AtomicRefCell;
use dogear::Store;
use log::LevelFilter;
use moz_task::{Task, TaskRunnable, ThreadPtrHandle, ThreadPtrHolder};
use nserror::{nsresult, NS_ERROR_NOT_AVAILABLE, NS_OK};
use nsstring::nsString;
use storage::Conn;
use thin_vec::ThinVec;
use xpcom::{
    interfaces::{
        mozIPlacesPendingOperation, mozIServicesLogger, mozIStorageConnection,
        mozISyncedBookmarksMirrorCallback, mozISyncedBookmarksMirrorProgressListener,
    },
    RefPtr, XpCom,
};

use crate::driver::{AbortController, Driver, Logger};
use crate::error;
use crate::store;

#[derive(xpcom)]
#[xpimplements(mozISyncedBookmarksMerger)]
#[refcnt = "nonatomic"]
pub struct InitSyncedBookmarksMerger {
    db: RefCell<Option<Conn>>,
    logger: RefCell<Option<RefPtr<mozIServicesLogger>>>,
}

impl SyncedBookmarksMerger {
    pub fn new() -> RefPtr<SyncedBookmarksMerger> {
        SyncedBookmarksMerger::allocate(InitSyncedBookmarksMerger {
            db: RefCell::default(),
            logger: RefCell::default(),
        })
    }

    xpcom_method!(get_db => GetDb() -> *const mozIStorageConnection);
    fn get_db(&self) -> Result<RefPtr<mozIStorageConnection>, nsresult> {
        self.db
            .borrow()
            .as_ref()
            .map(|db| RefPtr::new(db.connection()))
            .ok_or(NS_OK)
    }

    xpcom_method!(set_db => SetDb(connection: *const mozIStorageConnection));
    fn set_db(&self, connection: Option<&mozIStorageConnection>) -> Result<(), nsresult> {
        self.db
            .replace(connection.map(|connection| Conn::wrap(RefPtr::new(connection))));
        Ok(())
    }

    xpcom_method!(get_logger => GetLogger() -> *const mozIServicesLogger);
    fn get_logger(&self) -> Result<RefPtr<mozIServicesLogger>, nsresult> {
        match *self.logger.borrow() {
            Some(ref logger) => Ok(logger.clone()),
            None => Err(NS_OK),
        }
    }

    xpcom_method!(set_logger => SetLogger(logger: *const mozIServicesLogger));
    fn set_logger(&self, logger: Option<&mozIServicesLogger>) -> Result<(), nsresult> {
        self.logger.replace(logger.map(RefPtr::new));
        Ok(())
    }

    xpcom_method!(
        merge => Merge(
            local_time_seconds: i64,
            remote_time_seconds: i64,
            weak_uploads: *const ThinVec<::nsstring::nsString>,
            callback: *const mozISyncedBookmarksMirrorCallback
        ) -> *const mozIPlacesPendingOperation
    );
    fn merge(
        &self,
        local_time_seconds: i64,
        remote_time_seconds: i64,
        weak_uploads: Option<&ThinVec<nsString>>,
        callback: &mozISyncedBookmarksMirrorCallback,
    ) -> Result<RefPtr<mozIPlacesPendingOperation>, nsresult> {
        let callback = RefPtr::new(callback);
        let db = match *self.db.borrow() {
            Some(ref db) => db.clone(),
            None => return Err(NS_ERROR_NOT_AVAILABLE),
        };
        let logger = &*self.logger.borrow();
        let async_thread = db.thread()?;
        let controller = Arc::new(AbortController::default());
        let task = MergeTask::new(
            &db,
            Arc::clone(&controller),
            logger.as_ref().cloned(),
            local_time_seconds,
            remote_time_seconds,
            weak_uploads
                .map(|w| w.as_slice().to_vec())
                .unwrap_or_default(),
            callback,
        )?;
        let runnable = TaskRunnable::new(
            "bookmark_sync::SyncedBookmarksMerger::merge",
            Box::new(task),
        )?;
        runnable.dispatch(&async_thread)?;
        let op = MergeOp::new(controller);
        Ok(RefPtr::new(op.coerce()))
    }

    xpcom_method!(reset => Reset());
    fn reset(&self) -> Result<(), nsresult> {
        mem::drop(self.db.borrow_mut().take());
        mem::drop(self.logger.borrow_mut().take());
        Ok(())
    }
}

struct MergeTask {
    db: Conn,
    controller: Arc<AbortController>,
    max_log_level: LevelFilter,
    logger: Option<ThreadPtrHandle<mozIServicesLogger>>,
    local_time_millis: i64,
    remote_time_millis: i64,
    weak_uploads: Vec<nsString>,
    progress: Option<ThreadPtrHandle<mozISyncedBookmarksMirrorProgressListener>>,
    callback: ThreadPtrHandle<mozISyncedBookmarksMirrorCallback>,
    result: AtomicRefCell<error::Result<store::ApplyStatus>>,
}

impl MergeTask {
    fn new(
        db: &Conn,
        controller: Arc<AbortController>,
        logger: Option<RefPtr<mozIServicesLogger>>,
        local_time_seconds: i64,
        remote_time_seconds: i64,
        weak_uploads: Vec<nsString>,
        callback: RefPtr<mozISyncedBookmarksMirrorCallback>,
    ) -> Result<MergeTask, nsresult> {
        let max_log_level = logger
            .as_ref()
            .and_then(|logger| {
                let mut level = 0i16;
                unsafe { logger.GetMaxLevel(&mut level) }.to_result().ok()?;
                Some(level)
            })
            .map(|level| match level as i64 {
                mozIServicesLogger::LEVEL_ERROR => LevelFilter::Error,
                mozIServicesLogger::LEVEL_WARN => LevelFilter::Warn,
                mozIServicesLogger::LEVEL_DEBUG => LevelFilter::Debug,
                mozIServicesLogger::LEVEL_TRACE => LevelFilter::Trace,
                _ => LevelFilter::Off,
            })
            .unwrap_or(LevelFilter::Off);
        let logger = match logger {
            Some(logger) => Some(ThreadPtrHolder::new(cstr!("mozIServicesLogger"), logger)?),
            None => None,
        };
        let progress = callback
            .query_interface::<mozISyncedBookmarksMirrorProgressListener>()
            .and_then(|p| {
                ThreadPtrHolder::new(cstr!("mozISyncedBookmarksMirrorProgressListener"), p).ok()
            });
        Ok(MergeTask {
            db: db.clone(),
            controller,
            max_log_level,
            logger,
            local_time_millis: local_time_seconds * 1000,
            remote_time_millis: remote_time_seconds * 1000,
            weak_uploads,
            progress,
            callback: ThreadPtrHolder::new(cstr!("mozISyncedBookmarksMirrorCallback"), callback)?,
            result: AtomicRefCell::new(Err(error::Error::DidNotRun)),
        })
    }

    fn merge(&self) -> error::Result<store::ApplyStatus> {
        let mut db = self.db.clone();
        if db.transaction_in_progress()? {
            // If a transaction is already open, we can avoid an unnecessary
            // merge, since we won't be able to apply the merged tree back to
            // Places. This is common, especially if the user makes lots of
            // changes at once. In that case, our merge task might run in the
            // middle of a `Sqlite.jsm` transaction, and fail when we try to
            // open our own transaction in `Store::apply`. Since the local
            // tree might be in an inconsistent state, we can't safely update
            // Places.
            return Err(error::Error::StorageBusy);
        }
        let log = Logger::new(self.max_log_level, self.logger.clone());
        let driver = Driver::new(log, self.progress.clone());
        let mut store = store::Store::new(
            &mut db,
            &driver,
            &self.controller,
            self.local_time_millis,
            self.remote_time_millis,
            &self.weak_uploads,
        );
        store.validate()?;
        store.prepare()?;
        let status = store.merge_with_driver(&driver, &*self.controller)?;
        Ok(status)
    }
}

impl Task for MergeTask {
    fn run(&self) {
        *self.result.borrow_mut() = self.merge();
    }

    fn done(&self) -> Result<(), nsresult> {
        let callback = self.callback.get().unwrap();
        match mem::replace(&mut *self.result.borrow_mut(), Err(error::Error::DidNotRun)) {
            Ok(status) => unsafe { callback.HandleSuccess(status.into()) },
            Err(err) => {
                let mut message = nsString::new();
                write!(message, "{}", err).unwrap();
                unsafe { callback.HandleError(err.into(), &*message) }
            }
        }
        .to_result()
    }
}

#[derive(xpcom)]
#[xpimplements(mozIPlacesPendingOperation)]
#[refcnt = "atomic"]
pub struct InitMergeOp {
    controller: Arc<AbortController>,
}

impl MergeOp {
    pub fn new(controller: Arc<AbortController>) -> RefPtr<MergeOp> {
        MergeOp::allocate(InitMergeOp { controller })
    }

    xpcom_method!(cancel => Cancel());
    fn cancel(&self) -> Result<(), nsresult> {
        self.controller.abort();
        Ok(())
    }
}
