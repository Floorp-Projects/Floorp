/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{
    fmt::Write,
    sync::atomic::{AtomicBool, Ordering},
};

use dogear::{AbortSignal, Guid};
use log::{Level, LevelFilter, Log, Metadata, Record};
use moz_task::{Task, TaskRunnable, ThreadPtrHandle};
use nserror::nsresult;
use nsstring::{nsACString, nsCString, nsString};
use xpcom::interfaces::mozISyncedBookmarksMirrorLogger;

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
}

impl Driver {
    #[inline]
    pub fn new(log: Logger) -> Driver {
        Driver { log }
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
/// thread, and dispatched synchronously to the main thread.
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
