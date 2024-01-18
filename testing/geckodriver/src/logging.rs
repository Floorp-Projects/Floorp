/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Gecko-esque logger implementation for the [`log`] crate.
//!
//! The [`log`] crate provides a single logging API that abstracts over the
//! actual logging implementation.  This module uses the logging API
//! to provide a log implementation that shares many aesthetical traits with
//! [Log.sys.mjs] from Gecko.
//!
//! Using the [`error!`], [`warn!`], [`info!`], [`debug!`], and
//! [`trace!`] macros from `log` will output a timestamp field, followed by the
//! log level, and then the message.  The fields are separated by a tab
//! character, making the output suitable for further text processing with
//! `awk(1)`.
//!
//! This module shares the same API as `log`, except it provides additional
//! entry functions [`init`] and [`init_with_level`] and additional log levels
//! `Level::Fatal` and `Level::Config`.  Converting these into the
//! [`log::Level`] is lossy so that `Level::Fatal` becomes `log::Level::Error`
//! and `Level::Config` becomes `log::Level::Debug`.
//!
//! [`log`]: https://docs.rs/log/newest/log/
//! [Log.sys.mjs]: https://searchfox.org/mozilla-central/source/toolkit/modules/Log.sys.mjs
//! [`error!`]: https://docs.rs/log/newest/log/macro.error.html
//! [`warn!`]: https://docs.rs/log/newest/log/macro.warn.html
//! [`info!`]: https://docs.rs/log/newest/log/macro.info.html
//! [`debug!`]: https://docs.rs/log/newest/log/macro.debug.html
//! [`trace!`]: https://docs.rs/log/newest/log/macro.trace.html
//! [`init`]: fn.init.html
//! [`init_with_level`]: fn.init_with_level.html

use icu_segmenter::GraphemeClusterSegmenter;
use std::fmt;
use std::io;
use std::io::Write;
use std::str;
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};

use mozprofile::preferences::Pref;

static LOG_TRUNCATE: AtomicBool = AtomicBool::new(true);
static MAX_LOG_LEVEL: AtomicUsize = AtomicUsize::new(0);

const MAX_STRING_LENGTH: usize = 250;

const LOGGED_TARGETS: &[&str] = &[
    "geckodriver",
    "mozdevice",
    "mozprofile",
    "mozrunner",
    "mozversion",
    "webdriver",
];

/// Logger levels from [Log.sys.mjs].
///
/// [Log.sys.mjs]: https://searchfox.org/mozilla-central/source/toolkit/modules/Log.sys.mjs
#[repr(usize)]
#[derive(Clone, Copy, Eq, Debug, Hash, PartialEq)]
pub enum Level {
    Fatal = 70,
    Error = 60,
    Warn = 50,
    Info = 40,
    Config = 30,
    Debug = 20,
    Trace = 10,
}

impl From<usize> for Level {
    fn from(n: usize) -> Level {
        use self::Level::*;
        match n {
            70 => Fatal,
            60 => Error,
            50 => Warn,
            40 => Info,
            30 => Config,
            20 => Debug,
            10 => Trace,
            _ => Info,
        }
    }
}

impl fmt::Display for Level {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use self::Level::*;
        let s = match *self {
            Fatal => "FATAL",
            Error => "ERROR",
            Warn => "WARN",
            Info => "INFO",
            Config => "CONFIG",
            Debug => "DEBUG",
            Trace => "TRACE",
        };
        write!(f, "{}", s)
    }
}

impl str::FromStr for Level {
    type Err = ();

    fn from_str(s: &str) -> Result<Level, ()> {
        use self::Level::*;
        match s.to_lowercase().as_ref() {
            "fatal" => Ok(Fatal),
            "error" => Ok(Error),
            "warn" => Ok(Warn),
            "info" => Ok(Info),
            "config" => Ok(Config),
            "debug" => Ok(Debug),
            "trace" => Ok(Trace),
            _ => Err(()),
        }
    }
}

impl From<Level> for log::Level {
    fn from(level: Level) -> log::Level {
        use self::Level::*;
        match level {
            Fatal | Error => log::Level::Error,
            Warn => log::Level::Warn,
            Info => log::Level::Info,
            Config | Debug => log::Level::Debug,
            Trace => log::Level::Trace,
        }
    }
}

impl From<Level> for Pref {
    fn from(level: Level) -> Pref {
        use self::Level::*;
        Pref::new(match level {
            Fatal => "Fatal",
            Error => "Error",
            Warn => "Warn",
            Info => "Info",
            Config => "Config",
            Debug => "Debug",
            Trace => "Trace",
        })
    }
}

impl From<log::Level> for Level {
    fn from(log_level: log::Level) -> Level {
        use log::Level::*;
        match log_level {
            Error => Level::Error,
            Warn => Level::Warn,
            Info => Level::Info,
            Debug => Level::Debug,
            Trace => Level::Trace,
        }
    }
}

struct Logger;

impl log::Log for Logger {
    fn enabled(&self, meta: &log::Metadata) -> bool {
        LOGGED_TARGETS.iter().any(|&x| meta.target().starts_with(x))
            && meta.level() <= log::max_level()
    }

    fn log(&self, record: &log::Record) {
        if self.enabled(record.metadata()) {
            if let Some((s1, s2)) = truncate_message(record.args()) {
                println!(
                    "{}\t{}\t{}\t{} ... {}",
                    format_ts(chrono::Local::now()),
                    record.target(),
                    record.level(),
                    s1,
                    s2
                );
            } else {
                println!(
                    "{}\t{}\t{}\t{}",
                    format_ts(chrono::Local::now()),
                    record.target(),
                    record.level(),
                    record.args()
                )
            }
        }
    }

    fn flush(&self) {
        io::stdout().flush().unwrap();
    }
}

/// Initialises the logging subsystem with the default log level.
pub fn init(truncate: bool) -> Result<(), log::SetLoggerError> {
    init_with_level(Level::Info, truncate)
}

/// Initialises the logging subsystem.
pub fn init_with_level(level: Level, truncate: bool) -> Result<(), log::SetLoggerError> {
    let logger = Logger {};
    set_max_level(level);
    set_truncate(truncate);
    log::set_boxed_logger(Box::new(logger))?;
    Ok(())
}

/// Returns the current maximum log level.
pub fn max_level() -> Level {
    MAX_LOG_LEVEL.load(Ordering::Relaxed).into()
}

/// Sets the global maximum log level.
pub fn set_max_level(level: Level) {
    MAX_LOG_LEVEL.store(level as usize, Ordering::SeqCst);

    let slevel: log::Level = level.into();
    log::set_max_level(slevel.to_level_filter())
}

/// Sets the global maximum log level.
pub fn set_truncate(truncate: bool) {
    LOG_TRUNCATE.store(truncate, Ordering::SeqCst);
}

/// Returns the truncation flag.
pub fn truncate() -> bool {
    LOG_TRUNCATE.load(Ordering::Relaxed)
}

/// Produces a 13-digit Unix Epoch timestamp similar to Gecko.
fn format_ts(ts: chrono::DateTime<chrono::Local>) -> String {
    format!("{}{:03}", ts.timestamp(), ts.timestamp_subsec_millis())
}

/// Truncate a log message if it's too long
fn truncate_message(args: &fmt::Arguments) -> Option<(String, String)> {
    // Don't truncate the message if requested.
    if !truncate() {
        return None;
    }

    let message = format!("{}", args);
    if message.is_empty() || message.len() < MAX_STRING_LENGTH {
        return None;
    }
    let chars = GraphemeClusterSegmenter::new()
        .segment_str(&message)
        .collect::<Vec<_>>()
        .windows(2)
        .map(|i| &message[i[0]..i[1]])
        .collect::<Vec<&str>>();

    if chars.len() > MAX_STRING_LENGTH {
        let middle: usize = MAX_STRING_LENGTH / 2;
        let s1 = chars[0..middle].concat();
        let s2 = chars[chars.len() - middle..].concat();
        Some((s1, s2))
    } else {
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use std::str::FromStr;
    use std::sync::Mutex;

    use mozprofile::preferences::{Pref, PrefValue};

    lazy_static! {
        static ref LEVEL_MUTEX: Mutex<()> = Mutex::new(());
    }

    #[test]
    fn test_level_repr() {
        assert_eq!(Level::Fatal as usize, 70);
        assert_eq!(Level::Error as usize, 60);
        assert_eq!(Level::Warn as usize, 50);
        assert_eq!(Level::Info as usize, 40);
        assert_eq!(Level::Config as usize, 30);
        assert_eq!(Level::Debug as usize, 20);
        assert_eq!(Level::Trace as usize, 10);
    }

    #[test]
    fn test_level_from_log() {
        assert_eq!(Level::from(log::Level::Error), Level::Error);
        assert_eq!(Level::from(log::Level::Warn), Level::Warn);
        assert_eq!(Level::from(log::Level::Info), Level::Info);
        assert_eq!(Level::from(log::Level::Debug), Level::Debug);
        assert_eq!(Level::from(log::Level::Trace), Level::Trace);
    }

    #[test]
    fn test_level_into_log() {
        assert_eq!(Into::<log::Level>::into(Level::Fatal), log::Level::Error);
        assert_eq!(Into::<log::Level>::into(Level::Error), log::Level::Error);
        assert_eq!(Into::<log::Level>::into(Level::Warn), log::Level::Warn);
        assert_eq!(Into::<log::Level>::into(Level::Info), log::Level::Info);
        assert_eq!(Into::<log::Level>::into(Level::Config), log::Level::Debug);
        assert_eq!(Into::<log::Level>::into(Level::Debug), log::Level::Debug);
        assert_eq!(Into::<log::Level>::into(Level::Trace), log::Level::Trace);
    }

    #[test]
    fn test_level_into_pref() {
        let tests = [
            (Level::Fatal, "Fatal"),
            (Level::Error, "Error"),
            (Level::Warn, "Warn"),
            (Level::Info, "Info"),
            (Level::Config, "Config"),
            (Level::Debug, "Debug"),
            (Level::Trace, "Trace"),
        ];

        for &(lvl, s) in tests.iter() {
            let expected = Pref {
                value: PrefValue::String(s.to_string()),
                sticky: false,
            };
            assert_eq!(Into::<Pref>::into(lvl), expected);
        }
    }

    #[test]
    fn test_level_from_str() {
        assert_eq!(Level::from_str("fatal"), Ok(Level::Fatal));
        assert_eq!(Level::from_str("error"), Ok(Level::Error));
        assert_eq!(Level::from_str("warn"), Ok(Level::Warn));
        assert_eq!(Level::from_str("info"), Ok(Level::Info));
        assert_eq!(Level::from_str("config"), Ok(Level::Config));
        assert_eq!(Level::from_str("debug"), Ok(Level::Debug));
        assert_eq!(Level::from_str("trace"), Ok(Level::Trace));

        assert_eq!(Level::from_str("INFO"), Ok(Level::Info));

        assert!(Level::from_str("foo").is_err());
    }

    #[test]
    fn test_level_to_str() {
        assert_eq!(Level::Fatal.to_string(), "FATAL");
        assert_eq!(Level::Error.to_string(), "ERROR");
        assert_eq!(Level::Warn.to_string(), "WARN");
        assert_eq!(Level::Info.to_string(), "INFO");
        assert_eq!(Level::Config.to_string(), "CONFIG");
        assert_eq!(Level::Debug.to_string(), "DEBUG");
        assert_eq!(Level::Trace.to_string(), "TRACE");
    }

    #[test]
    fn test_max_level() {
        let _guard = LEVEL_MUTEX.lock();
        set_max_level(Level::Info);
        assert_eq!(max_level(), Level::Info);
    }

    #[test]
    fn test_set_max_level() {
        let _guard = LEVEL_MUTEX.lock();
        set_max_level(Level::Error);
        assert_eq!(max_level(), Level::Error);
        set_max_level(Level::Fatal);
        assert_eq!(max_level(), Level::Fatal);
    }

    #[test]
    fn test_init_with_level() {
        let _guard = LEVEL_MUTEX.lock();
        init_with_level(Level::Debug, false).unwrap();
        assert_eq!(max_level(), Level::Debug);
        assert!(init_with_level(Level::Warn, false).is_err());
    }

    #[test]
    fn test_format_ts() {
        let ts = chrono::Local::now();
        let s = format_ts(ts);
        assert_eq!(s.len(), 13);
    }

    #[test]
    fn test_truncate() {
        let short_message = (0..MAX_STRING_LENGTH).map(|_| "x").collect::<String>();
        // A message up to MAX_STRING_LENGTH is not truncated
        assert_eq!(truncate_message(&format_args!("{}", short_message)), None);

        let long_message = (0..MAX_STRING_LENGTH + 1).map(|_| "x").collect::<String>();
        let part = (0..MAX_STRING_LENGTH / 2).map(|_| "x").collect::<String>();

        // A message longer than MAX_STRING_LENGTH is not truncated if requested
        set_truncate(false);
        assert_eq!(truncate_message(&format_args!("{}", long_message)), None);

        // A message longer than MAX_STRING_LENGTH is truncated if requested
        set_truncate(true);
        assert_eq!(
            truncate_message(&format_args!("{}", long_message)),
            Some((part.to_owned(), part))
        );
    }
}
