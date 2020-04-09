/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::fmt::{self, Write};

use log::{Level, LevelFilter, Log, Metadata, Record};
use moz_task::{Task, TaskRunnable, ThreadPtrHandle, ThreadPtrHolder};
use nserror::nsresult;
use nsstring::nsString;
use xpcom::{interfaces::mozIServicesLogger, RefPtr};

pub struct LogSink {
    pub max_level: LevelFilter,
    logger: Option<ThreadPtrHandle<mozIServicesLogger>>,
}

impl Default for LogSink {
    fn default() -> Self {
        LogSink {
            max_level: LevelFilter::Off,
            logger: None,
        }
    }
}

impl LogSink {
    /// Creates a log sink that adapts the Rust `log` crate to the Sync
    /// `Log.jsm` logger.
    ///
    /// This is copied from `bookmark_sync::Logger`. It would be nice to share
    /// these, but, for now, we've just duplicated it to make prototyping
    /// easier.
    #[inline]
    pub fn new(max_level: LevelFilter, logger: ThreadPtrHandle<mozIServicesLogger>) -> LogSink {
        LogSink {
            max_level,
            logger: Some(logger),
        }
    }

    /// Creates a log sink using the given Services `logger` as the
    /// underlying implementation. The `logger` will always be called
    /// asynchronously on its owning thread; it doesn't need to be
    /// thread-safe.
    pub fn with_logger(logger: Option<&mozIServicesLogger>) -> Result<LogSink, nsresult> {
        Ok(if let Some(logger) = logger {
            // Fetch the maximum log level while we're on the main thread, so
            // that `LogSink::enabled()` can check it while on the background
            // thread. Otherwise, we'd need to dispatch a `LogTask` for every
            // log message, only to discard most of them when the task calls
            // into the logger on the main thread.
            let mut raw_max_level = 0i16;
            let rv = unsafe { logger.GetMaxLevel(&mut raw_max_level) };
            let max_level = if rv.succeeded() {
                match raw_max_level as i64 {
                    mozIServicesLogger::LEVEL_ERROR => LevelFilter::Error,
                    mozIServicesLogger::LEVEL_WARN => LevelFilter::Warn,
                    mozIServicesLogger::LEVEL_DEBUG => LevelFilter::Debug,
                    mozIServicesLogger::LEVEL_TRACE => LevelFilter::Trace,
                    _ => LevelFilter::Off,
                }
            } else {
                LevelFilter::Off
            };
            LogSink::new(
                max_level,
                ThreadPtrHolder::new(cstr!("mozIServicesLogger"), RefPtr::new(logger))?,
            )
        } else {
            LogSink::default()
        })
    }

    /// Returns a reference to the underlying `mozIServicesLogger`.
    pub fn logger(&self) -> Option<&mozIServicesLogger> {
        self.logger.as_ref().and_then(|l| l.get())
    }

    /// Logs a message to the Sync logger, if one is set. This would be better
    /// implemented as a macro, as Dogear does, so that we can pass variadic
    /// arguments without manually invoking `fmt_args!()` every time we want
    /// to log a message.
    ///
    /// The `log` crate's macros aren't suitable here, because those log to the
    /// global logger. However, we don't want to set the global logger in our
    /// crate, because that will log _everything_ that uses the Rust `log` crate
    /// to the Sync logs, including WebRender and audio logging.
    pub fn debug(&self, args: fmt::Arguments) {
        let meta = Metadata::builder()
            .level(Level::Debug)
            .target(module_path!())
            .build();
        if self.enabled(&meta) {
            self.log(&Record::builder().args(args).metadata(meta).build());
        }
    }
}

impl Log for LogSink {
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
                    let _ =
                        TaskRunnable::new("extension_storage_sync::Logger::log", Box::new(task))
                            .and_then(|r| r.dispatch(logger.owning_thread()));
                }
                Err(_) => {}
            }
        }
    }

    fn flush(&self) {}
}

/// Logs a message to the mirror logger. This task is created on the background
/// thread queue, and dispatched to the main thread.
struct LogTask {
    logger: ThreadPtrHandle<mozIServicesLogger>,
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
