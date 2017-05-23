use std::fmt;
use std::io;
use std::str::FromStr;
use std::sync::atomic::AtomicBool;
use std::sync::atomic::Ordering::SeqCst;

use chrono::{DateTime, Local};
use slog;
use slog::DrainExt;
use slog_atomic::{AtomicSwitch, AtomicSwitchCtrl};
use slog_stream::{stream, Format, Streamer};
use slog::Level as SlogLevel;
use slog::{LevelFilter, Logger};
use slog::{OwnedKeyValueList, Record};
use slog_stdlog;

lazy_static! {
    static ref ATOMIC_DRAIN: AtomicSwitchCtrl<io::Error> = AtomicSwitch::new(
        slog::Discard.map_err(|_| io::Error::new(io::ErrorKind::Other, "should not happen"))
    ).ctrl();
    static ref FIRST_RUN: AtomicBool = AtomicBool::new(true);
}

static DEFAULT_LEVEL: &'static LogLevel = &LogLevel::Info;

/// Logger levels from [Log.jsm]
/// (https://developer.mozilla.org/en/docs/Mozilla/JavaScript_code_modules/Log.jsm).
#[derive(Debug, Clone)]
pub enum LogLevel {
    Fatal,
    Error,
    Warn,
    Info,
    Config,
    Debug,
    Trace,
}

impl fmt::Display for LogLevel {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let s = match *self {
            LogLevel::Fatal => "FATAL",
            LogLevel::Error => "ERROR",
            LogLevel::Warn => "WARN",
            LogLevel::Info => "INFO",
            LogLevel::Config => "CONFIG",
            LogLevel::Debug => "DEBUG",
            LogLevel::Trace => "TRACE",
        };
        write!(f, "{}", s)
    }
}

impl FromStr for LogLevel {
    type Err = ();

    fn from_str(s: &str) -> Result<LogLevel, ()> {
        match s {
            "fatal" => Ok(LogLevel::Fatal),
            "error" => Ok(LogLevel::Error),
            "warn" => Ok(LogLevel::Warn),
            "info" => Ok(LogLevel::Info),
            "config" => Ok(LogLevel::Config),
            "debug" => Ok(LogLevel::Debug),
            "trace" => Ok(LogLevel::Trace),
            _ => Err(()),
        }
    }
}

trait ToSlogLevel {
    fn to_slog(&self) -> SlogLevel;
}

impl ToSlogLevel for LogLevel {
    fn to_slog(&self) -> SlogLevel {
        match *self {
            LogLevel::Fatal => SlogLevel::Critical,
            LogLevel::Error => SlogLevel::Error,
            LogLevel::Warn => SlogLevel::Warning,
            LogLevel::Info => SlogLevel::Info,
            LogLevel::Config | LogLevel::Debug => SlogLevel::Debug,
            LogLevel::Trace => SlogLevel::Trace,
        }
    }
}

trait ToGeckoLevel {
    fn to_gecko(&self) -> LogLevel;
}

impl ToGeckoLevel for SlogLevel {
    fn to_gecko(&self) -> LogLevel {
        match *self {
            SlogLevel::Critical => LogLevel::Fatal,
            SlogLevel::Error => LogLevel::Error,
            SlogLevel::Warning => LogLevel::Warn,
            SlogLevel::Info => LogLevel::Info,
            SlogLevel::Debug => LogLevel::Debug,
            SlogLevel::Trace => LogLevel::Trace,
        }
    }
}

/// Initialise logger if it has not been already.  The provided `level`
/// filters out log records below this granularity.
pub fn init(level: &Option<LogLevel>) {
    let effective_level = level.as_ref().unwrap_or(DEFAULT_LEVEL);

    let drain = filtered_gecko_log(&effective_level);
    ATOMIC_DRAIN.set(drain);

    let first_run = FIRST_RUN.load(SeqCst);
    FIRST_RUN.store(false, SeqCst);
    if first_run {
        let log = Logger::root(ATOMIC_DRAIN.drain().fuse(), o!());
        slog_stdlog::set_logger(log.clone()).unwrap();
    }
}

fn filtered_gecko_log(level: &LogLevel) -> LevelFilter<Streamer<io::Stderr, GeckoFormat>> {
    let io = stream(io::stderr(), GeckoFormat {});
    slog::level_filter(level.to_slog(), io)
}

struct GeckoFormat;

impl Format for GeckoFormat {
    fn format(&self, io: &mut io::Write, record: &Record, _: &OwnedKeyValueList) -> io::Result<()> {
        // TODO(ato): Quite sure this is the wrong way to filter records with slog,
        // but I do not comprehend how slog works.
        let module = record.module();
        if module.starts_with("geckodriver") || module.starts_with("webdriver") {
            let ts = format_ts(Local::now());
            let level = record.level().to_gecko();
            let _ = try!(write!(io, "{}\t{}\t{}\t{}\n", ts, module, level, record.msg()));
        }
        Ok(())
    }
}

/// Produces a 13-digit Unix Epoch timestamp similar to Gecko.
fn format_ts(ts: DateTime<Local>) -> String {
    format!("{}{:03}", ts.timestamp(), ts.timestamp_subsec_millis())
}

#[cfg(test)]
mod tests {
    use chrono::Local;
    use super::format_ts;

    #[test]
    fn test_format_ts() {
        let ts = Local::now();
        let s = format_ts(ts);
        assert_eq!(s.len(), 13);
    }
}
