/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{
    fmt::Write,
    sync::atomic::{AtomicBool, Ordering},
    time::Duration,
};

use dogear::{AbortSignal, Guid, ProblemCounts, StructureCounts, TelemetryEvent};
use log::{Level, LevelFilter, Log, Metadata, Record};
use moz_task::{Task, TaskRunnable, ThreadPtrHandle};
use nserror::nsresult;
use nsstring::{nsACString, nsCString, nsString};
use storage_variant::HashPropertyBag;
use xpcom::interfaces::{
    mozISyncedBookmarksMirrorLogger, mozISyncedBookmarksMirrorProgressListener,
};

extern "C" {
    fn NS_GeneratePlacesGUID(guid: *mut nsACString) -> nsresult;
}

fn generate_guid() -> Result<nsCString, nsresult> {
    let mut guid = nsCString::new();
    let rv = unsafe { NS_GeneratePlacesGUID(&mut *guid) };
    if rv.succeeded() {
        Ok(guid)
    } else {
        Err(rv)
    }
}

/// An abort controller is used to abort merges running on the storage thread
/// from the main thread. Its design is based on the DOM API of the same name.
pub struct AbortController {
    aborted: AtomicBool,
}

impl AbortController {
    /// Signals the store to stop merging as soon as it can.
    pub fn abort(&self) {
        self.aborted.store(true, Ordering::Release)
    }
}

impl Default for AbortController {
    fn default() -> AbortController {
        AbortController {
            aborted: AtomicBool::new(false),
        }
    }
}

impl AbortSignal for AbortController {
    fn aborted(&self) -> bool {
        self.aborted.load(Ordering::Acquire)
    }
}

/// The merger driver, created and used on the storage thread.
pub struct Driver {
    log: Logger,
    progress: Option<ThreadPtrHandle<mozISyncedBookmarksMirrorProgressListener>>,
}

impl Driver {
    #[inline]
    pub fn new(
        log: Logger,
        progress: Option<ThreadPtrHandle<mozISyncedBookmarksMirrorProgressListener>>,
    ) -> Driver {
        Driver { log, progress }
    }
}

impl dogear::Driver for Driver {
    fn generate_new_guid(&self, invalid_guid: &Guid) -> dogear::Result<Guid> {
        generate_guid()
            .map_err(|_| dogear::ErrorKind::InvalidGuid(invalid_guid.clone()).into())
            .and_then(|s| Guid::from_utf8(s.as_ref()))
    }

    #[inline]
    fn max_log_level(&self) -> LevelFilter {
        self.log.max_level
    }

    #[inline]
    fn logger(&self) -> &dyn Log {
        &self.log
    }

    fn record_telemetry_event(&self, event: TelemetryEvent) {
        if let Some(ref progress) = self.progress {
            let task = RecordTelemetryEventTask {
                progress: progress.clone(),
                event,
            };
            let _ = TaskRunnable::new(
                "bookmark_sync::Driver::record_telemetry_event",
                Box::new(task),
            )
            .and_then(|r| r.dispatch(progress.owning_thread()));
        }
    }
}

pub struct Logger {
    pub max_level: LevelFilter,
    logger: Option<ThreadPtrHandle<mozISyncedBookmarksMirrorLogger>>,
}

impl Logger {
    #[inline]
    pub fn new(
        max_level: LevelFilter,
        logger: Option<ThreadPtrHandle<mozISyncedBookmarksMirrorLogger>>,
    ) -> Logger {
        Logger { max_level, logger }
    }
}

impl Log for Logger {
    #[inline]
    fn enabled(&self, meta: &Metadata) -> bool {
        self.logger.is_some() && meta.level() <= self.max_level
    }

    fn log(&self, record: &Record) {
        if !self.enabled(record.metadata()) {
            return;
        }
        if let Some(logger) = &self.logger {
            let mut message = nsString::new();
            match write!(message, "{}", record.args()) {
                Ok(_) => {
                    let task = LogTask {
                        logger: logger.clone(),
                        level: record.metadata().level(),
                        message,
                    };
                    let _ = TaskRunnable::new("bookmark_sync::Logger::log", Box::new(task))
                        .and_then(|r| r.dispatch(logger.owning_thread()));
                }
                Err(_) => {}
            }
        }
    }

    fn flush(&self) {}
}

/// Logs a message to the mirror logger. This task is created on the async
/// thread, and dispatched to the main thread.
struct LogTask {
    logger: ThreadPtrHandle<mozISyncedBookmarksMirrorLogger>,
    level: Level,
    message: nsString,
}

impl Task for LogTask {
    fn run(&self) {
        let logger = self.logger.get().unwrap();
        match self.level {
            Level::Error => unsafe {
                logger.Error(&*self.message);
            },
            Level::Warn => unsafe {
                logger.Warn(&*self.message);
            },
            Level::Debug => unsafe {
                logger.Debug(&*self.message);
            },
            Level::Trace => unsafe {
                logger.Trace(&*self.message);
            },
            _ => {}
        }
    }

    fn done(&self) -> Result<(), nsresult> {
        Ok(())
    }
}

/// Calls a progress listener callback for a merge telemetry event. This task is
/// created on the async thread, and dispatched to the main thread.
struct RecordTelemetryEventTask {
    progress: ThreadPtrHandle<mozISyncedBookmarksMirrorProgressListener>,
    event: TelemetryEvent,
}

impl Task for RecordTelemetryEventTask {
    fn run(&self) {
        let callback = self.progress.get().unwrap();
        let _ = match &self.event {
            TelemetryEvent::FetchLocalTree(stats) => unsafe {
                callback.OnFetchLocalTree(
                    as_millis(stats.time),
                    stats.items as i64,
                    problem_counts_to_bag(&stats.problems).bag().coerce(),
                )
            },
            TelemetryEvent::FetchRemoteTree(stats) => unsafe {
                callback.OnFetchRemoteTree(
                    as_millis(stats.time),
                    stats.items as i64,
                    problem_counts_to_bag(&stats.problems).bag().coerce(),
                )
            },
            TelemetryEvent::Merge(time, counts) => unsafe {
                callback.OnMerge(
                    as_millis(*time),
                    structure_counts_to_bag(counts).bag().coerce(),
                )
            },
            TelemetryEvent::Apply(time) => unsafe { callback.OnApply(as_millis(*time)) },
        };
    }

    fn done(&self) -> std::result::Result<(), nsresult> {
        Ok(())
    }
}

fn as_millis(d: Duration) -> i64 {
    d.as_secs() as i64 * 1000 + i64::from(d.subsec_millis())
}

fn problem_counts_to_bag(problems: &ProblemCounts) -> HashPropertyBag {
    let mut bag = HashPropertyBag::new();
    bag.set("orphans", problems.orphans as i64);
    bag.set("misparentedRoots", problems.misparented_roots as i64);
    bag.set(
        "multipleParents",
        problems.multiple_parents_by_children as i64,
    );
    bag.set("missingParents", problems.missing_parent_guids as i64);
    bag.set("nonFolderParents", problems.non_folder_parent_guids as i64);
    bag.set(
        "parentChildDisagreements",
        problems.parent_child_disagreements as i64,
    );
    bag.set("missingChildren", problems.missing_children as i64);
    bag
}

fn structure_counts_to_bag(counts: &StructureCounts) -> HashPropertyBag {
    let mut bag = HashPropertyBag::new();
    bag.set("remoteRevives", counts.remote_revives as i64);
    bag.set("localDeletes", counts.local_deletes as i64);
    bag.set("localRevives", counts.local_revives as i64);
    bag.set("remoteDeletes", counts.remote_deletes as i64);
    bag.set("dupes", counts.dupes as i64);
    bag.set("items", counts.merged_nodes as i64);
    bag.set("deletes", counts.merged_deletions as i64);
    bag
}
