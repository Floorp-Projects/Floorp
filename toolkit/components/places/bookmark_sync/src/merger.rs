/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{cell::RefCell, fmt::Write, mem, sync::Arc, time::Duration};

use atomic_refcell::AtomicRefCell;
use dogear::{MergeTimings, Stats, Store, StructureCounts};
use log::LevelFilter;
use moz_task::{Task, TaskRunnable, ThreadPtrHandle, ThreadPtrHolder};
use nserror::{nsresult, NS_ERROR_FAILURE, NS_ERROR_UNEXPECTED, NS_OK};
use nsstring::nsString;
use storage::Conn;
use storage_variant::HashPropertyBag;
use thin_vec::ThinVec;
use xpcom::{
    interfaces::{
        mozIStorageConnection, mozISyncedBookmarksMirrorCallback, mozISyncedBookmarksMirrorLogger,
    },
    RefPtr,
};

use crate::driver::{AbortController, Driver, Logger};
use crate::error;
use crate::store;

#[derive(xpcom)]
#[xpimplements(mozISyncedBookmarksMerger)]
#[refcnt = "nonatomic"]
pub struct InitSyncedBookmarksMerger {
    controller: Arc<AbortController>,
    db: RefCell<Option<Conn>>,
    logger: RefCell<Option<RefPtr<mozISyncedBookmarksMirrorLogger>>>,
}

impl SyncedBookmarksMerger {
    pub fn new() -> RefPtr<SyncedBookmarksMerger> {
        SyncedBookmarksMerger::allocate(InitSyncedBookmarksMerger {
            controller: Arc::new(AbortController::default()),
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

    xpcom_method!(get_logger => GetLogger() -> *const mozISyncedBookmarksMirrorLogger);
    fn get_logger(&self) -> Result<RefPtr<mozISyncedBookmarksMirrorLogger>, nsresult> {
        match *self.logger.borrow() {
            Some(ref logger) => Ok(logger.clone()),
            None => Err(NS_OK),
        }
    }

    xpcom_method!(set_logger => SetLogger(logger: *const mozISyncedBookmarksMirrorLogger));
    fn set_logger(&self, logger: Option<&mozISyncedBookmarksMirrorLogger>) -> Result<(), nsresult> {
        self.logger.replace(logger.map(RefPtr::new));
        Ok(())
    }

    xpcom_method!(
        merge => Merge(
            local_time_seconds: libc::int64_t,
            remote_time_seconds: libc::int64_t,
            weak_uploads: *const ThinVec<::nsstring::nsString>,
            callback: *const mozISyncedBookmarksMirrorCallback
        )
    );
    fn merge(
        &self,
        local_time_seconds: libc::int64_t,
        remote_time_seconds: libc::int64_t,
        weak_uploads: Option<&ThinVec<nsString>>,
        callback: &mozISyncedBookmarksMirrorCallback,
    ) -> Result<(), nsresult> {
        let callback = RefPtr::new(callback);
        let db = match *self.db.borrow() {
            Some(ref db) => db.clone(),
            None => return Err(NS_ERROR_FAILURE),
        };
        let logger = &*self.logger.borrow();
        let async_thread = db.thread()?;
        let task = MergeTask::new(
            &db,
            Arc::clone(&self.controller),
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
        runnable.dispatch(&async_thread)
    }

    xpcom_method!(finalize => Finalize());
    fn finalize(&self) -> Result<(), nsresult> {
        self.controller.abort();
        mem::drop(self.db.borrow_mut().take());
        mem::drop(self.logger.borrow_mut().take());
        Ok(())
    }
}

struct MergeTask {
    db: Conn,
    controller: Arc<AbortController>,
    max_log_level: LevelFilter,
    logger: Option<ThreadPtrHandle<mozISyncedBookmarksMirrorLogger>>,
    local_time_millis: i64,
    remote_time_millis: i64,
    weak_uploads: Vec<nsString>,
    callback: ThreadPtrHandle<mozISyncedBookmarksMirrorCallback>,
    result: AtomicRefCell<Option<error::Result<Stats>>>,
}

impl MergeTask {
    fn new(
        db: &Conn,
        controller: Arc<AbortController>,
        logger: Option<RefPtr<mozISyncedBookmarksMirrorLogger>>,
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
                mozISyncedBookmarksMirrorLogger::LEVEL_ERROR => LevelFilter::Error,
                mozISyncedBookmarksMirrorLogger::LEVEL_WARN => LevelFilter::Warn,
                mozISyncedBookmarksMirrorLogger::LEVEL_DEBUG => LevelFilter::Debug,
                mozISyncedBookmarksMirrorLogger::LEVEL_TRACE => LevelFilter::Trace,
                _ => LevelFilter::Off,
            })
            .unwrap_or(LevelFilter::Off);
        let logger = match logger {
            Some(logger) => Some(ThreadPtrHolder::new(
                cstr!("mozISyncedBookmarksMirrorLogger"),
                logger,
            )?),
            None => None,
        };
        Ok(MergeTask {
            db: db.clone(),
            controller,
            max_log_level,
            logger,
            local_time_millis: local_time_seconds * 1000,
            remote_time_millis: remote_time_seconds * 1000,
            weak_uploads,
            callback: ThreadPtrHolder::new(cstr!("mozISyncedBookmarksMirrorCallback"), callback)?,
            result: AtomicRefCell::default(),
        })
    }
}

impl Task for MergeTask {
    fn run(&self) {
        let mut db = self.db.clone();
        let mut store = store::Store::new(
            &mut db,
            &self.controller,
            self.local_time_millis,
            self.remote_time_millis,
            &self.weak_uploads,
        );
        let log = Logger::new(self.max_log_level, self.logger.clone());
        let driver = Driver::new(log);
        *self.result.borrow_mut() = Some(store.merge_with_driver(&driver, &*self.controller));
    }

    fn done(&self) -> Result<(), nsresult> {
        let callback = self.callback.get().unwrap();
        match self.result.borrow_mut().take() {
            Some(Ok(stats)) => {
                let mut telem = HashPropertyBag::new();
                telem.set("fetchLocalTreeTime", stats.time(|t| t.fetch_local_tree));
                telem.set(
                    "fetchNewLocalContentsTime",
                    stats.time(|t| t.fetch_new_local_contents),
                );
                telem.set("fetchRemoteTreeTime", stats.time(|t| t.fetch_remote_tree));
                telem.set(
                    "fetchNewRemoteContentsTime",
                    stats.time(|t| t.fetch_new_remote_contents),
                );
                telem.set("mergeTime", stats.time(|t| t.merge));
                telem.set("mergedNodesCount", stats.count(|c| c.merged_nodes));
                telem.set("mergedDeletionsCount", stats.count(|c| c.merged_deletions));
                telem.set("remoteRevivesCount", stats.count(|c| c.remote_revives));
                telem.set("localDeletesCount", stats.count(|c| c.local_deletes));
                telem.set("localRevivesCount", stats.count(|c| c.local_revives));
                telem.set("remoteDeletesCount", stats.count(|c| c.remote_deletes));
                telem.set("dupesCount", stats.count(|c| c.dupes));
                telem.set("applyTime", stats.time(|t| t.apply));
                unsafe { callback.HandleResult(telem.bag().coerce()) }
            }
            Some(Err(err)) => {
                let message = {
                    let mut message = nsString::new();
                    match write!(message, "{}", err) {
                        Ok(_) => message,
                        Err(_) => nsString::from("Merge failed with unknown error"),
                    }
                };
                unsafe { callback.HandleError(err.into(), &*message) }
            }
            None => unsafe {
                callback.HandleError(
                    NS_ERROR_UNEXPECTED,
                    &*nsString::from("Failed to run merge on storage thread"),
                )
            },
        }
        .to_result()
    }
}

/// Extension methods that convert timings and counters into types that we
/// can store in a `HashPropertyBag`.
trait StatsExt {
    fn time(&self, func: impl FnOnce(&MergeTimings) -> Duration) -> i64;
    fn count(&self, func: impl FnOnce(&StructureCounts) -> usize) -> i64;
}

impl StatsExt for Stats {
    fn time(&self, func: impl FnOnce(&MergeTimings) -> Duration) -> i64 {
        let d = func(&self.timings);
        d.as_secs() as i64 * 1000 + i64::from(d.subsec_millis())
    }

    fn count(&self, func: impl FnOnce(&StructureCounts) -> usize) -> i64 {
        func(&self.counts) as i64
    }
}
