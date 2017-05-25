//! Standard Rust log crate adapter to slog-rs
//!
//! This crate allows using `slog` features with code
//! using legacy `log` statements.
//!
//! `log` crate expects a global logger to be registered
//! (popular one is `env_logger`) as a handler for all
//! `info!(...)` and similar.
//!
//! `slog-stdlog` will register itself as `log` global handler and forward all
//! legacy logging statements to `slog`'s `Logger`. That means existing logging
//! `debug!` (even in dependencies crates) work and utilize `slog` composable
//! drains.
//!
//! See `init()` documentation for minimal working example.
//!
//! Note: Whilte `slog-scope` provides a similiar functionality, the `slog-scope` and `slog-stdlog`
//! keep track of distinct logginc scopes.
#![warn(missing_docs)]

#[macro_use]
extern crate slog;
extern crate slog_term;
extern crate log;
#[macro_use]
extern crate lazy_static;
extern crate crossbeam;

use slog::{DrainExt, ser};

use log::LogMetadata;
use std::sync::Arc;
use std::cell::RefCell;
use std::{io, fmt};
use std::io::Write;

use slog::Level;
use crossbeam::sync::ArcCell;

thread_local! {
    static TL_SCOPES: RefCell<Vec<slog::Logger>> = RefCell::new(Vec::with_capacity(8))
}

lazy_static! {
    static ref GLOBAL_LOGGER : ArcCell<slog::Logger> = ArcCell::new(
        Arc::new(
            slog::Logger::root(slog::Discard, o!())
        )
    );
}

fn set_global_logger(l: slog::Logger) {
    let _ = GLOBAL_LOGGER.set(Arc::new(l));
}

struct Logger;


fn log_to_slog_level(level: log::LogLevel) -> Level {
    match level {
        log::LogLevel::Trace => Level::Trace,
        log::LogLevel::Debug => Level::Debug,
        log::LogLevel::Info => Level::Info,
        log::LogLevel::Warn => Level::Warning,
        log::LogLevel::Error => Level::Error,
    }
}

impl log::Log for Logger {
    fn enabled(&self, _: &LogMetadata) -> bool {
        true
    }

    fn log(&self, r: &log::LogRecord) {
        let level = log_to_slog_level(r.metadata().level());

        let args = r.args();
        let target = r.target();
        let module = r.location().__module_path;
        let file = r.location().__file;
        let line = r.location().line();
        with_current_logger(|l| {
            let s = slog::RecordStatic {
                level: level,
                file: file,
                line: line,
                column: 0,
                function: "",
                module: module,
                target: target,
            };
            l.log(&slog::Record::new(&s, *args, &[]))
        })
    }
}

/// Set a `slog::Logger` as a global `log` create handler
///
/// This will forward all `log` records to `slog` logger.
///
/// ```
/// // only use `o` macro from `slog` crate
/// #[macro_use(o)]
/// extern crate slog;
/// #[macro_use]
/// extern crate log;
/// extern crate slog_stdlog;
///
/// fn main() {
///     let root = slog::Logger::root(
///         slog::Discard,
///         o!("build-id" => "8dfljdf"),
///     );
///     slog_stdlog::set_logger(root).unwrap();
///     // Note: this `info!(...)` macro comes from `log` crate
///     info!("standard logging redirected to slog");
/// }
/// ```
pub fn set_logger(logger: slog::Logger) -> Result<(), log::SetLoggerError> {
    log::set_logger(|max_log_level| {
        max_log_level.set(log::LogLevelFilter::max());
        set_global_logger(logger);
        Box::new(Logger)
    })
}

/// Set a `slog::Logger` as a global `log` create handler
///
/// This will forward `log` records that satisfy `log_level_filter` to `slog` logger.
pub fn set_logger_level(logger: slog::Logger,
                        log_level_filter: log::LogLevelFilter)
                        -> Result<(), log::SetLoggerError> {
    log::set_logger(|max_log_level| {
        max_log_level.set(log_level_filter);
        set_global_logger(logger);
        Box::new(Logger)
    })
}

/// Minimal initialization with default drain
///
/// The exact default drain is unspecified and will
/// change in future versions! Use `set_logger` instead
/// to build customized drain.
///
/// ```
/// #[macro_use]
/// extern crate log;
/// extern crate slog_stdlog;
///
/// fn main() {
///     slog_stdlog::init().unwrap();
///     // Note: this `info!(...)` macro comes from `log` crate
///     info!("standard logging redirected to slog");
/// }
/// ```
pub fn init() -> Result<(), log::SetLoggerError> {
    let drain = slog::level_filter(Level::Info, slog_term::streamer().compact().build());
    set_logger(slog::Logger::root(drain.fuse(), o!()))
}

struct ScopeGuard;


impl ScopeGuard {
    fn new(logger: slog::Logger) -> Self {
        TL_SCOPES.with(|s| {
            s.borrow_mut().push(logger);
        });

        ScopeGuard
    }
}

impl Drop for ScopeGuard {
    fn drop(&mut self) {
        TL_SCOPES.with(|s| {
            s.borrow_mut().pop().expect("TL_SCOPES should contain a logger");
        })
    }
}


/// Access the currently active logger
///
/// The reference logger will be either:
/// * global logger, or
/// * currently active scope logger
///
/// **Warning**: Calling `scope` inside `f`
/// will result in a panic.
pub fn with_current_logger<F, R>(f: F) -> R
    where F: FnOnce(&slog::Logger) -> R
{
    TL_SCOPES.with(|s| {
        let s = s.borrow();
        if s.is_empty() {
            f(&GLOBAL_LOGGER.get())
        } else {
            f(&s[s.len() - 1])
        }
    })
}

/// Access the `Logger` for the current logging scope
pub fn logger() -> slog::Logger {
    TL_SCOPES.with(|s| {
        let s = s.borrow();
        if s.is_empty() {
            (*GLOBAL_LOGGER.get()).clone()
        } else {
            s[s.len() - 1].clone()
        }
    })
}

/// Execute code in a logging scope
///
/// Logging scopes allow using different logger for legacy logging
/// statements in part of the code.
///
/// Logging scopes can be nested and are panic safe.
///
/// `logger` is the `Logger` to use during the duration of `f`.
/// `with_current_logger` can be used to build it as a child of currently active
/// logger.
///
/// `f` is a code to be executed in the logging scope.
///
/// Note: Thread scopes are thread-local. Each newly spawned thread starts
/// with a global logger, as a current logger.
pub fn scope<SF, R>(logger: slog::Logger, f: SF) -> R
    where SF: FnOnce() -> R
{
    let _guard = ScopeGuard::new(logger);
    f()
}

/// Drain logging `Record`s into `log` crate
///
/// Using `StdLog` is effectively the same as using `log::info!(...)` and
/// other standard logging statements.
///
/// Caution needs to be taken to prevent circular loop where `Logger`
/// installed via `slog-stdlog::set_logger` would log things to a `StdLog`
/// drain, which would again log things to the global `Logger` and so on
/// leading to an infinite recursion.
pub struct StdLog;

struct LazyLogString<'a> {
    info: &'a slog::Record<'a>,
    logger_values : &'a slog::OwnedKeyValueList
}

impl<'a> LazyLogString<'a> {

    fn new(info : &'a slog::Record, logger_values : &'a slog::OwnedKeyValueList) -> Self {

        LazyLogString {
            info: info,
            logger_values: logger_values,
        }
    }
}

impl<'a> fmt::Display for LazyLogString<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {

        try!(write!(f, "{}", self.info.msg()));

        let io = io::Cursor::new(Vec::new());
        let mut ser = KSV::new(io, ": ".into());

        let res = {
            || -> io::Result<()> {

            for (ref k, ref v) in self.logger_values.iter() {
                try!(ser.io().write_all(", ".as_bytes()));
                try!(v.serialize(self.info, k, &mut ser));
            }

            for &(ref k, ref v) in self.info.values().iter() {
                try!(ser.io().write_all(", ".as_bytes()));
                try!(v.serialize(self.info, k, &mut ser));
            }
            Ok(())
        }
        }().map_err(|_| fmt::Error);

        try!(res);

        let values = ser.into_inner().into_inner();


        write!(f, "{}", String::from_utf8_lossy(&values))

    }
}

impl slog::Drain for StdLog {
    type Error = io::Error;
    fn log(&self, info: &slog::Record, logger_values : &slog::OwnedKeyValueList) -> io::Result<()> {

        let level = match info.level() {
            slog::Level::Critical => log::LogLevel::Error,
            slog::Level::Error => log::LogLevel::Error,
            slog::Level::Warning => log::LogLevel::Warn,
            slog::Level::Info => log::LogLevel::Info,
            slog::Level::Debug => log::LogLevel::Debug,
            slog::Level::Trace => log::LogLevel::Trace,
        };

        let target = info.target();

        let location = log::LogLocation {
            __module_path: info.module(),
            __file: info.file(),
            __line: info.line(),
        };

        let lazy = LazyLogString::new(info, logger_values);
        // Please don't yell at me for this! :D
        // https://github.com/rust-lang-nursery/log/issues/95
        log::__log(level, target, &location, format_args!("{}", lazy));

        Ok(())
    }
}

/// Key-Separator-Value serializer
struct KSV<W: io::Write> {
    separator: String,
    io: W,
}

impl<W: io::Write> KSV<W> {
    fn new(io: W, separator: String) -> Self {
        KSV {
            io: io,
            separator: separator,
        }
    }

    fn io(&mut self) -> &mut W {
        &mut self.io
    }

    fn into_inner(self) -> W {
        self.io
    }
}

impl<W: io::Write> ser::Serializer for KSV<W> {
    fn emit_none(&mut self, key: &str) -> ser::Result {
        try!(write!(self.io, "{}{}{}", key, self.separator, "None"));
        Ok(())
    }
    fn emit_unit(&mut self, key: &str) -> ser::Result {
        try!(write!(self.io, "{}", key));
        Ok(())
    }

    fn emit_bool(&mut self, key: &str, val: bool) -> ser::Result {
        try!(write!(self.io, "{}{}{}", key, self.separator, val));
        Ok(())
    }

    fn emit_char(&mut self, key: &str, val: char) -> ser::Result {
        try!(write!(self.io, "{}{}{}", key, self.separator, val));
        Ok(())
    }

    fn emit_usize(&mut self, key: &str, val: usize) -> ser::Result {
        try!(write!(self.io, "{}{}{}", key, self.separator, val));
        Ok(())
    }
    fn emit_isize(&mut self, key: &str, val: isize) -> ser::Result {
        try!(write!(self.io, "{}{}{}", key, self.separator, val));
        Ok(())
    }

    fn emit_u8(&mut self, key: &str, val: u8) -> ser::Result {
        try!(write!(self.io, "{}{}{}", key, self.separator, val));
        Ok(())
    }
    fn emit_i8(&mut self, key: &str, val: i8) -> ser::Result {
        try!(write!(self.io, "{}{}{}", key, self.separator, val));
        Ok(())
    }
    fn emit_u16(&mut self, key: &str, val: u16) -> ser::Result {
        try!(write!(self.io, "{}{}{}", key, self.separator, val));
        Ok(())
    }
    fn emit_i16(&mut self, key: &str, val: i16) -> ser::Result {
        try!(write!(self.io, "{}{}{}", key, self.separator, val));
        Ok(())
    }
    fn emit_u32(&mut self, key: &str, val: u32) -> ser::Result {
        try!(write!(self.io, "{}{}{}", key, self.separator, val));
        Ok(())
    }
    fn emit_i32(&mut self, key: &str, val: i32) -> ser::Result {
        try!(write!(self.io, "{}{}{}", key, self.separator, val));
        Ok(())
    }
    fn emit_f32(&mut self, key: &str, val: f32) -> ser::Result {
        try!(write!(self.io, "{}{}{}", key, self.separator, val));
        Ok(())
    }
    fn emit_u64(&mut self, key: &str, val: u64) -> ser::Result {
        try!(write!(self.io, "{}{}{}", key, self.separator, val));
        Ok(())
    }
    fn emit_i64(&mut self, key: &str, val: i64) -> ser::Result {
        try!(write!(self.io, "{}{}{}", key, self.separator, val));
        Ok(())
    }
    fn emit_f64(&mut self, key: &str, val: f64) -> ser::Result {
        try!(write!(self.io, "{}{}{}", key, self.separator, val));
        Ok(())
    }
    fn emit_str(&mut self, key: &str, val: &str) -> ser::Result {
        try!(write!(self.io, "{}{}{}", key, self.separator, val));
        Ok(())
    }
    fn emit_arguments(&mut self, key: &str, val: &fmt::Arguments) -> ser::Result {
        try!(write!(self.io, "{}{}{}", key, self.separator, val));
        Ok(())
    }
}
