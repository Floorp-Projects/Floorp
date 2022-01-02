// Copyright 2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! A lightweight logging facade.
//!
//! The `log` crate provides a single logging API that abstracts over the
//! actual logging implementation. Libraries can use the logging API provided
//! by this crate, and the consumer of those libraries can choose the logging
//! implementation that is most suitable for its use case.
//!
//! If no logging implementation is selected, the facade falls back to a "noop"
//! implementation that ignores all log messages. The overhead in this case
//! is very small - just an integer load, comparison and jump.
//!
//! A log request consists of a _target_, a _level_, and a _body_. A target is a
//! string which defaults to the module path of the location of the log request,
//! though that default may be overridden. Logger implementations typically use
//! the target to filter requests based on some user configuration.
//!
//! # Use
//!
//! The basic use of the log crate is through the five logging macros: [`error!`],
//! [`warn!`], [`info!`], [`debug!`] and [`trace!`]
//! where `error!` represents the highest-priority log messages
//! and `trace!` the lowest. The log messages are filtered by configuring
//! the log level to exclude messages with a lower priority.
//! Each of these macros accept format strings similarly to [`println!`].
//!
//!
//! [`error!`]: ./macro.error.html
//! [`warn!`]: ./macro.warn.html
//! [`info!`]: ./macro.info.html
//! [`debug!`]: ./macro.debug.html
//! [`trace!`]: ./macro.trace.html
//! [`println!`]: https://doc.rust-lang.org/stable/std/macro.println.html
//!
//! ## In libraries
//!
//! Libraries should link only to the `log` crate, and use the provided
//! macros to log whatever information will be useful to downstream consumers.
//!
//! ### Examples
//!
//! ```edition2018
//! # #[derive(Debug)] pub struct Yak(String);
//! # impl Yak { fn shave(&mut self, _: u32) {} }
//! # fn find_a_razor() -> Result<u32, u32> { Ok(1) }
//! use log::{info, warn};
//!
//! pub fn shave_the_yak(yak: &mut Yak) {
//!     info!(target: "yak_events", "Commencing yak shaving for {:?}", yak);
//!
//!     loop {
//!         match find_a_razor() {
//!             Ok(razor) => {
//!                 info!("Razor located: {}", razor);
//!                 yak.shave(razor);
//!                 break;
//!             }
//!             Err(err) => {
//!                 warn!("Unable to locate a razor: {}, retrying", err);
//!             }
//!         }
//!     }
//! }
//! # fn main() {}
//! ```
//!
//! ## In executables
//!
//! Executables should choose a logging implementation and initialize it early in the
//! runtime of the program. Logging implementations will typically include a
//! function to do this. Any log messages generated before
//! the implementation is initialized will be ignored.
//!
//! The executable itself may use the `log` crate to log as well.
//!
//! ### Warning
//!
//! The logging system may only be initialized once.
//!
//! # Available logging implementations
//!
//! In order to produce log output executables have to use
//! a logger implementation compatible with the facade.
//! There are many available implementations to choose from,
//! here are some of the most popular ones:
//!
//! * Simple minimal loggers:
//!     * [env_logger]
//!     * [simple_logger]
//!     * [simplelog]
//!     * [pretty_env_logger]
//!     * [stderrlog]
//!     * [flexi_logger]
//! * Complex configurable frameworks:
//!     * [log4rs]
//!     * [fern]
//! * Adaptors for other facilities:
//!     * [syslog]
//!     * [slog-stdlog]
//!
//! # Implementing a Logger
//!
//! Loggers implement the [`Log`] trait. Here's a very basic example that simply
//! logs all messages at the [`Error`][level_link], [`Warn`][level_link] or
//! [`Info`][level_link] levels to stdout:
//!
//! ```edition2018
//! use log::{Record, Level, Metadata};
//!
//! struct SimpleLogger;
//!
//! impl log::Log for SimpleLogger {
//!     fn enabled(&self, metadata: &Metadata) -> bool {
//!         metadata.level() <= Level::Info
//!     }
//!
//!     fn log(&self, record: &Record) {
//!         if self.enabled(record.metadata()) {
//!             println!("{} - {}", record.level(), record.args());
//!         }
//!     }
//!
//!     fn flush(&self) {}
//! }
//!
//! # fn main() {}
//! ```
//!
//! Loggers are installed by calling the [`set_logger`] function. The maximum
//! log level also needs to be adjusted via the [`set_max_level`] function. The
//! logging facade uses this as an optimization to improve performance of log
//! messages at levels that are disabled. It's important to set it, as it
//! defaults to [`Off`][filter_link], so no log messages will ever be captured!
//! In the case of our example logger, we'll want to set the maximum log level
//! to [`Info`][filter_link], since we ignore any [`Debug`][level_link] or
//! [`Trace`][level_link] level log messages. A logging implementation should
//! provide a function that wraps a call to [`set_logger`] and
//! [`set_max_level`], handling initialization of the logger:
//!
//! ```edition2018
//! # use log::{Level, Metadata};
//! # struct SimpleLogger;
//! # impl log::Log for SimpleLogger {
//! #   fn enabled(&self, _: &Metadata) -> bool { false }
//! #   fn log(&self, _: &log::Record) {}
//! #   fn flush(&self) {}
//! # }
//! # fn main() {}
//! use log::{SetLoggerError, LevelFilter};
//!
//! static LOGGER: SimpleLogger = SimpleLogger;
//!
//! pub fn init() -> Result<(), SetLoggerError> {
//!     log::set_logger(&LOGGER)
//!         .map(|()| log::set_max_level(LevelFilter::Info))
//! }
//! ```
//!
//! Implementations that adjust their configurations at runtime should take care
//! to adjust the maximum log level as well.
//!
//! # Use with `std`
//!
//! `set_logger` requires you to provide a `&'static Log`, which can be hard to
//! obtain if your logger depends on some runtime configuration. The
//! `set_boxed_logger` function is available with the `std` Cargo feature. It is
//! identical to `set_logger` except that it takes a `Box<Log>` rather than a
//! `&'static Log`:
//!
//! ```edition2018
//! # use log::{Level, LevelFilter, Log, SetLoggerError, Metadata};
//! # struct SimpleLogger;
//! # impl log::Log for SimpleLogger {
//! #   fn enabled(&self, _: &Metadata) -> bool { false }
//! #   fn log(&self, _: &log::Record) {}
//! #   fn flush(&self) {}
//! # }
//! # fn main() {}
//! # #[cfg(feature = "std")]
//! pub fn init() -> Result<(), SetLoggerError> {
//!     log::set_boxed_logger(Box::new(SimpleLogger))
//!         .map(|()| log::set_max_level(LevelFilter::Info))
//! }
//! ```
//!
//! # Compile time filters
//!
//! Log levels can be statically disabled at compile time via Cargo features. Log invocations at
//! disabled levels will be skipped and will not even be present in the resulting binary.
//! This level is configured separately for release and debug builds. The features are:
//!
//! * `max_level_off`
//! * `max_level_error`
//! * `max_level_warn`
//! * `max_level_info`
//! * `max_level_debug`
//! * `max_level_trace`
//! * `release_max_level_off`
//! * `release_max_level_error`
//! * `release_max_level_warn`
//! * `release_max_level_info`
//! * `release_max_level_debug`
//! * `release_max_level_trace`
//!
//! These features control the value of the `STATIC_MAX_LEVEL` constant. The logging macros check
//! this value before logging a message. By default, no levels are disabled.
//!
//! Libraries should avoid using the max level features because they're global and can't be changed
//! once they're set.
//!
//! For example, a crate can disable trace level logs in debug builds and trace, debug, and info
//! level logs in release builds with the following configuration:
//!
//! ```toml
//! [dependencies]
//! log = { version = "0.4", features = ["max_level_debug", "release_max_level_warn"] }
//! ```
//! # Crate Feature Flags
//!
//! The following crate feature flags are available in addition to the filters. They are
//! configured in your `Cargo.toml`.
//!
//! * `std` allows use of `std` crate instead of the default `core`. Enables using `std::error` and
//! `set_boxed_logger` functionality.
//! * `serde` enables support for serialization and deserialization of `Level` and `LevelFilter`.
//!
//! ```toml
//! [dependencies]
//! log = { version = "0.4", features = ["std", "serde"] }
//! ```
//!
//! # Version compatibility
//!
//! The 0.3 and 0.4 versions of the `log` crate are almost entirely compatible. Log messages
//! made using `log` 0.3 will forward transparently to a logger implementation using `log` 0.4. Log
//! messages made using `log` 0.4 will forward to a logger implementation using `log` 0.3, but the
//! module path and file name information associated with the message will unfortunately be lost.
//!
//! [`Log`]: trait.Log.html
//! [level_link]: enum.Level.html
//! [filter_link]: enum.LevelFilter.html
//! [`set_logger`]: fn.set_logger.html
//! [`set_max_level`]: fn.set_max_level.html
//! [`try_set_logger_raw`]: fn.try_set_logger_raw.html
//! [`shutdown_logger_raw`]: fn.shutdown_logger_raw.html
//! [env_logger]: https://docs.rs/env_logger/*/env_logger/
//! [simple_logger]: https://github.com/borntyping/rust-simple_logger
//! [simplelog]: https://github.com/drakulix/simplelog.rs
//! [pretty_env_logger]: https://docs.rs/pretty_env_logger/*/pretty_env_logger/
//! [stderrlog]: https://docs.rs/stderrlog/*/stderrlog/
//! [flexi_logger]: https://docs.rs/flexi_logger/*/flexi_logger/
//! [syslog]: https://docs.rs/syslog/*/syslog/
//! [slog-stdlog]: https://docs.rs/slog-stdlog/*/slog_stdlog/
//! [log4rs]: https://docs.rs/log4rs/*/log4rs/
//! [fern]: https://docs.rs/fern/*/fern/

#![doc(
    html_logo_url = "https://www.rust-lang.org/logos/rust-logo-128x128-blk-v2.png",
    html_favicon_url = "https://www.rust-lang.org/favicon.ico",
    html_root_url = "https://docs.rs/log/0.4.14"
)]
#![warn(missing_docs)]
#![deny(missing_debug_implementations)]
#![cfg_attr(all(not(feature = "std"), not(test)), no_std)]
// When compiled for the rustc compiler itself we want to make sure that this is
// an unstable crate
#![cfg_attr(rustbuild, feature(staged_api, rustc_private))]
#![cfg_attr(rustbuild, unstable(feature = "rustc_private", issue = "27812"))]

#[cfg(all(not(feature = "std"), not(test)))]
extern crate core as std;

#[macro_use]
extern crate cfg_if;

use std::cmp;
#[cfg(feature = "std")]
use std::error;
use std::fmt;
use std::mem;
use std::str::FromStr;

#[macro_use]
mod macros;
mod serde;

#[cfg(feature = "kv_unstable")]
pub mod kv;

#[cfg(has_atomics)]
use std::sync::atomic::{AtomicUsize, Ordering};

#[cfg(not(has_atomics))]
use std::cell::Cell;
#[cfg(not(has_atomics))]
use std::sync::atomic::Ordering;

#[cfg(not(has_atomics))]
struct AtomicUsize {
    v: Cell<usize>,
}

#[cfg(not(has_atomics))]
impl AtomicUsize {
    const fn new(v: usize) -> AtomicUsize {
        AtomicUsize { v: Cell::new(v) }
    }

    fn load(&self, _order: Ordering) -> usize {
        self.v.get()
    }

    fn store(&self, val: usize, _order: Ordering) {
        self.v.set(val)
    }

    #[cfg(atomic_cas)]
    fn compare_exchange(
        &self,
        current: usize,
        new: usize,
        _success: Ordering,
        _failure: Ordering,
    ) -> Result<usize, usize> {
        let prev = self.v.get();
        if current == prev {
            self.v.set(new);
        }
        Ok(prev)
    }
}

// Any platform without atomics is unlikely to have multiple cores, so
// writing via Cell will not be a race condition.
#[cfg(not(has_atomics))]
unsafe impl Sync for AtomicUsize {}

// The LOGGER static holds a pointer to the global logger. It is protected by
// the STATE static which determines whether LOGGER has been initialized yet.
static mut LOGGER: &dyn Log = &NopLogger;

static STATE: AtomicUsize = AtomicUsize::new(0);

// There are three different states that we care about: the logger's
// uninitialized, the logger's initializing (set_logger's been called but
// LOGGER hasn't actually been set yet), or the logger's active.
const UNINITIALIZED: usize = 0;
const INITIALIZING: usize = 1;
const INITIALIZED: usize = 2;

static MAX_LOG_LEVEL_FILTER: AtomicUsize = AtomicUsize::new(0);

static LOG_LEVEL_NAMES: [&str; 6] = ["OFF", "ERROR", "WARN", "INFO", "DEBUG", "TRACE"];

static SET_LOGGER_ERROR: &str = "attempted to set a logger after the logging system \
                                 was already initialized";
static LEVEL_PARSE_ERROR: &str =
    "attempted to convert a string that doesn't match an existing log level";

/// An enum representing the available verbosity levels of the logger.
///
/// Typical usage includes: checking if a certain `Level` is enabled with
/// [`log_enabled!`](macro.log_enabled.html), specifying the `Level` of
/// [`log!`](macro.log.html), and comparing a `Level` directly to a
/// [`LevelFilter`](enum.LevelFilter.html).
#[repr(usize)]
#[derive(Copy, Eq, Debug, Hash)]
pub enum Level {
    /// The "error" level.
    ///
    /// Designates very serious errors.
    // This way these line up with the discriminants for LevelFilter below
    // This works because Rust treats field-less enums the same way as C does:
    // https://doc.rust-lang.org/reference/items/enumerations.html#custom-discriminant-values-for-field-less-enumerations
    Error = 1,
    /// The "warn" level.
    ///
    /// Designates hazardous situations.
    Warn,
    /// The "info" level.
    ///
    /// Designates useful information.
    Info,
    /// The "debug" level.
    ///
    /// Designates lower priority information.
    Debug,
    /// The "trace" level.
    ///
    /// Designates very low priority, often extremely verbose, information.
    Trace,
}

impl Clone for Level {
    #[inline]
    fn clone(&self) -> Level {
        *self
    }
}

impl PartialEq for Level {
    #[inline]
    fn eq(&self, other: &Level) -> bool {
        *self as usize == *other as usize
    }
}

impl PartialEq<LevelFilter> for Level {
    #[inline]
    fn eq(&self, other: &LevelFilter) -> bool {
        *self as usize == *other as usize
    }
}

impl PartialOrd for Level {
    #[inline]
    fn partial_cmp(&self, other: &Level) -> Option<cmp::Ordering> {
        Some(self.cmp(other))
    }

    #[inline]
    fn lt(&self, other: &Level) -> bool {
        (*self as usize) < *other as usize
    }

    #[inline]
    fn le(&self, other: &Level) -> bool {
        *self as usize <= *other as usize
    }

    #[inline]
    fn gt(&self, other: &Level) -> bool {
        *self as usize > *other as usize
    }

    #[inline]
    fn ge(&self, other: &Level) -> bool {
        *self as usize >= *other as usize
    }
}

impl PartialOrd<LevelFilter> for Level {
    #[inline]
    fn partial_cmp(&self, other: &LevelFilter) -> Option<cmp::Ordering> {
        Some((*self as usize).cmp(&(*other as usize)))
    }

    #[inline]
    fn lt(&self, other: &LevelFilter) -> bool {
        (*self as usize) < *other as usize
    }

    #[inline]
    fn le(&self, other: &LevelFilter) -> bool {
        *self as usize <= *other as usize
    }

    #[inline]
    fn gt(&self, other: &LevelFilter) -> bool {
        *self as usize > *other as usize
    }

    #[inline]
    fn ge(&self, other: &LevelFilter) -> bool {
        *self as usize >= *other as usize
    }
}

impl Ord for Level {
    #[inline]
    fn cmp(&self, other: &Level) -> cmp::Ordering {
        (*self as usize).cmp(&(*other as usize))
    }
}

fn ok_or<T, E>(t: Option<T>, e: E) -> Result<T, E> {
    match t {
        Some(t) => Ok(t),
        None => Err(e),
    }
}

// Reimplemented here because std::ascii is not available in libcore
fn eq_ignore_ascii_case(a: &str, b: &str) -> bool {
    fn to_ascii_uppercase(c: u8) -> u8 {
        if c >= b'a' && c <= b'z' {
            c - b'a' + b'A'
        } else {
            c
        }
    }

    if a.len() == b.len() {
        a.bytes()
            .zip(b.bytes())
            .all(|(a, b)| to_ascii_uppercase(a) == to_ascii_uppercase(b))
    } else {
        false
    }
}

impl FromStr for Level {
    type Err = ParseLevelError;
    fn from_str(level: &str) -> Result<Level, Self::Err> {
        ok_or(
            LOG_LEVEL_NAMES
                .iter()
                .position(|&name| eq_ignore_ascii_case(name, level))
                .into_iter()
                .filter(|&idx| idx != 0)
                .map(|idx| Level::from_usize(idx).unwrap())
                .next(),
            ParseLevelError(()),
        )
    }
}

impl fmt::Display for Level {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.pad(self.as_str())
    }
}

impl Level {
    fn from_usize(u: usize) -> Option<Level> {
        match u {
            1 => Some(Level::Error),
            2 => Some(Level::Warn),
            3 => Some(Level::Info),
            4 => Some(Level::Debug),
            5 => Some(Level::Trace),
            _ => None,
        }
    }

    /// Returns the most verbose logging level.
    #[inline]
    pub fn max() -> Level {
        Level::Trace
    }

    /// Converts the `Level` to the equivalent `LevelFilter`.
    #[inline]
    pub fn to_level_filter(&self) -> LevelFilter {
        LevelFilter::from_usize(*self as usize).unwrap()
    }

    /// Returns the string representation of the `Level`.
    ///
    /// This returns the same string as the `fmt::Display` implementation.
    pub fn as_str(&self) -> &'static str {
        LOG_LEVEL_NAMES[*self as usize]
    }
}

/// An enum representing the available verbosity level filters of the logger.
///
/// A `LevelFilter` may be compared directly to a [`Level`]. Use this type
/// to get and set the maximum log level with [`max_level()`] and [`set_max_level`].
///
/// [`Level`]: enum.Level.html
/// [`max_level()`]: fn.max_level.html
/// [`set_max_level`]: fn.set_max_level.html
#[repr(usize)]
#[derive(Copy, Eq, Debug, Hash)]
pub enum LevelFilter {
    /// A level lower than all log levels.
    Off,
    /// Corresponds to the `Error` log level.
    Error,
    /// Corresponds to the `Warn` log level.
    Warn,
    /// Corresponds to the `Info` log level.
    Info,
    /// Corresponds to the `Debug` log level.
    Debug,
    /// Corresponds to the `Trace` log level.
    Trace,
}

// Deriving generates terrible impls of these traits

impl Clone for LevelFilter {
    #[inline]
    fn clone(&self) -> LevelFilter {
        *self
    }
}

impl PartialEq for LevelFilter {
    #[inline]
    fn eq(&self, other: &LevelFilter) -> bool {
        *self as usize == *other as usize
    }
}

impl PartialEq<Level> for LevelFilter {
    #[inline]
    fn eq(&self, other: &Level) -> bool {
        other.eq(self)
    }
}

impl PartialOrd for LevelFilter {
    #[inline]
    fn partial_cmp(&self, other: &LevelFilter) -> Option<cmp::Ordering> {
        Some(self.cmp(other))
    }

    #[inline]
    fn lt(&self, other: &LevelFilter) -> bool {
        (*self as usize) < *other as usize
    }

    #[inline]
    fn le(&self, other: &LevelFilter) -> bool {
        *self as usize <= *other as usize
    }

    #[inline]
    fn gt(&self, other: &LevelFilter) -> bool {
        *self as usize > *other as usize
    }

    #[inline]
    fn ge(&self, other: &LevelFilter) -> bool {
        *self as usize >= *other as usize
    }
}

impl PartialOrd<Level> for LevelFilter {
    #[inline]
    fn partial_cmp(&self, other: &Level) -> Option<cmp::Ordering> {
        Some((*self as usize).cmp(&(*other as usize)))
    }

    #[inline]
    fn lt(&self, other: &Level) -> bool {
        (*self as usize) < *other as usize
    }

    #[inline]
    fn le(&self, other: &Level) -> bool {
        *self as usize <= *other as usize
    }

    #[inline]
    fn gt(&self, other: &Level) -> bool {
        *self as usize > *other as usize
    }

    #[inline]
    fn ge(&self, other: &Level) -> bool {
        *self as usize >= *other as usize
    }
}

impl Ord for LevelFilter {
    #[inline]
    fn cmp(&self, other: &LevelFilter) -> cmp::Ordering {
        (*self as usize).cmp(&(*other as usize))
    }
}

impl FromStr for LevelFilter {
    type Err = ParseLevelError;
    fn from_str(level: &str) -> Result<LevelFilter, Self::Err> {
        ok_or(
            LOG_LEVEL_NAMES
                .iter()
                .position(|&name| eq_ignore_ascii_case(name, level))
                .map(|p| LevelFilter::from_usize(p).unwrap()),
            ParseLevelError(()),
        )
    }
}

impl fmt::Display for LevelFilter {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.pad(self.as_str())
    }
}

impl LevelFilter {
    fn from_usize(u: usize) -> Option<LevelFilter> {
        match u {
            0 => Some(LevelFilter::Off),
            1 => Some(LevelFilter::Error),
            2 => Some(LevelFilter::Warn),
            3 => Some(LevelFilter::Info),
            4 => Some(LevelFilter::Debug),
            5 => Some(LevelFilter::Trace),
            _ => None,
        }
    }
    /// Returns the most verbose logging level filter.
    #[inline]
    pub fn max() -> LevelFilter {
        LevelFilter::Trace
    }

    /// Converts `self` to the equivalent `Level`.
    ///
    /// Returns `None` if `self` is `LevelFilter::Off`.
    #[inline]
    pub fn to_level(&self) -> Option<Level> {
        Level::from_usize(*self as usize)
    }

    /// Returns the string representation of the `LevelFilter`.
    ///
    /// This returns the same string as the `fmt::Display` implementation.
    pub fn as_str(&self) -> &'static str {
        LOG_LEVEL_NAMES[*self as usize]
    }
}

#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Hash, Debug)]
enum MaybeStaticStr<'a> {
    Static(&'static str),
    Borrowed(&'a str),
}

impl<'a> MaybeStaticStr<'a> {
    #[inline]
    fn get(&self) -> &'a str {
        match *self {
            MaybeStaticStr::Static(s) => s,
            MaybeStaticStr::Borrowed(s) => s,
        }
    }
}

/// The "payload" of a log message.
///
/// # Use
///
/// `Record` structures are passed as parameters to the [`log`][method.log]
/// method of the [`Log`] trait. Logger implementors manipulate these
/// structures in order to display log messages. `Record`s are automatically
/// created by the [`log!`] macro and so are not seen by log users.
///
/// Note that the [`level()`] and [`target()`] accessors are equivalent to
/// `self.metadata().level()` and `self.metadata().target()` respectively.
/// These methods are provided as a convenience for users of this structure.
///
/// # Example
///
/// The following example shows a simple logger that displays the level,
/// module path, and message of any `Record` that is passed to it.
///
/// ```edition2018
/// struct SimpleLogger;
///
/// impl log::Log for SimpleLogger {
///    fn enabled(&self, metadata: &log::Metadata) -> bool {
///        true
///    }
///
///    fn log(&self, record: &log::Record) {
///        if !self.enabled(record.metadata()) {
///            return;
///        }
///
///        println!("{}:{} -- {}",
///                 record.level(),
///                 record.target(),
///                 record.args());
///    }
///    fn flush(&self) {}
/// }
/// ```
///
/// [method.log]: trait.Log.html#tymethod.log
/// [`Log`]: trait.Log.html
/// [`log!`]: macro.log.html
/// [`level()`]: struct.Record.html#method.level
/// [`target()`]: struct.Record.html#method.target
#[derive(Clone, Debug)]
pub struct Record<'a> {
    metadata: Metadata<'a>,
    args: fmt::Arguments<'a>,
    module_path: Option<MaybeStaticStr<'a>>,
    file: Option<MaybeStaticStr<'a>>,
    line: Option<u32>,
    #[cfg(feature = "kv_unstable")]
    key_values: KeyValues<'a>,
}

// This wrapper type is only needed so we can
// `#[derive(Debug)]` on `Record`. It also
// provides a useful `Debug` implementation for
// the underlying `Source`.
#[cfg(feature = "kv_unstable")]
#[derive(Clone)]
struct KeyValues<'a>(&'a dyn kv::Source);

#[cfg(feature = "kv_unstable")]
impl<'a> fmt::Debug for KeyValues<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut visitor = f.debug_map();
        self.0.visit(&mut visitor).map_err(|_| fmt::Error)?;
        visitor.finish()
    }
}

impl<'a> Record<'a> {
    /// Returns a new builder.
    #[inline]
    pub fn builder() -> RecordBuilder<'a> {
        RecordBuilder::new()
    }

    /// The message body.
    #[inline]
    pub fn args(&self) -> &fmt::Arguments<'a> {
        &self.args
    }

    /// Metadata about the log directive.
    #[inline]
    pub fn metadata(&self) -> &Metadata<'a> {
        &self.metadata
    }

    /// The verbosity level of the message.
    #[inline]
    pub fn level(&self) -> Level {
        self.metadata.level()
    }

    /// The name of the target of the directive.
    #[inline]
    pub fn target(&self) -> &'a str {
        self.metadata.target()
    }

    /// The module path of the message.
    #[inline]
    pub fn module_path(&self) -> Option<&'a str> {
        self.module_path.map(|s| s.get())
    }

    /// The module path of the message, if it is a `'static` string.
    #[inline]
    pub fn module_path_static(&self) -> Option<&'static str> {
        match self.module_path {
            Some(MaybeStaticStr::Static(s)) => Some(s),
            _ => None,
        }
    }

    /// The source file containing the message.
    #[inline]
    pub fn file(&self) -> Option<&'a str> {
        self.file.map(|s| s.get())
    }

    /// The module path of the message, if it is a `'static` string.
    #[inline]
    pub fn file_static(&self) -> Option<&'static str> {
        match self.file {
            Some(MaybeStaticStr::Static(s)) => Some(s),
            _ => None,
        }
    }

    /// The line containing the message.
    #[inline]
    pub fn line(&self) -> Option<u32> {
        self.line
    }

    /// The structued key-value pairs associated with the message.
    #[cfg(feature = "kv_unstable")]
    #[inline]
    pub fn key_values(&self) -> &dyn kv::Source {
        self.key_values.0
    }

    /// Create a new [`RecordBuilder`](struct.RecordBuilder.html) based on this record.
    #[cfg(feature = "kv_unstable")]
    #[inline]
    pub fn to_builder(&self) -> RecordBuilder {
        RecordBuilder {
            record: Record {
                metadata: Metadata {
                    level: self.metadata.level,
                    target: self.metadata.target,
                },
                args: self.args,
                module_path: self.module_path,
                file: self.file,
                line: self.line,
                key_values: self.key_values.clone(),
            },
        }
    }
}

/// Builder for [`Record`](struct.Record.html).
///
/// Typically should only be used by log library creators or for testing and "shim loggers".
/// The `RecordBuilder` can set the different parameters of `Record` object, and returns
/// the created object when `build` is called.
///
/// # Examples
///
///
/// ```edition2018
/// use log::{Level, Record};
///
/// let record = Record::builder()
///                 .args(format_args!("Error!"))
///                 .level(Level::Error)
///                 .target("myApp")
///                 .file(Some("server.rs"))
///                 .line(Some(144))
///                 .module_path(Some("server"))
///                 .build();
/// ```
///
/// Alternatively, use [`MetadataBuilder`](struct.MetadataBuilder.html):
///
/// ```edition2018
/// use log::{Record, Level, MetadataBuilder};
///
/// let error_metadata = MetadataBuilder::new()
///                         .target("myApp")
///                         .level(Level::Error)
///                         .build();
///
/// let record = Record::builder()
///                 .metadata(error_metadata)
///                 .args(format_args!("Error!"))
///                 .line(Some(433))
///                 .file(Some("app.rs"))
///                 .module_path(Some("server"))
///                 .build();
/// ```
#[derive(Debug)]
pub struct RecordBuilder<'a> {
    record: Record<'a>,
}

impl<'a> RecordBuilder<'a> {
    /// Construct new `RecordBuilder`.
    ///
    /// The default options are:
    ///
    /// - `args`: [`format_args!("")`]
    /// - `metadata`: [`Metadata::builder().build()`]
    /// - `module_path`: `None`
    /// - `file`: `None`
    /// - `line`: `None`
    ///
    /// [`format_args!("")`]: https://doc.rust-lang.org/std/macro.format_args.html
    /// [`Metadata::builder().build()`]: struct.MetadataBuilder.html#method.build
    #[inline]
    pub fn new() -> RecordBuilder<'a> {
        RecordBuilder {
            record: Record {
                args: format_args!(""),
                metadata: Metadata::builder().build(),
                module_path: None,
                file: None,
                line: None,
                #[cfg(feature = "kv_unstable")]
                key_values: KeyValues(&Option::None::<(kv::Key, kv::Value)>),
            },
        }
    }

    /// Set [`args`](struct.Record.html#method.args).
    #[inline]
    pub fn args(&mut self, args: fmt::Arguments<'a>) -> &mut RecordBuilder<'a> {
        self.record.args = args;
        self
    }

    /// Set [`metadata`](struct.Record.html#method.metadata). Construct a `Metadata` object with [`MetadataBuilder`](struct.MetadataBuilder.html).
    #[inline]
    pub fn metadata(&mut self, metadata: Metadata<'a>) -> &mut RecordBuilder<'a> {
        self.record.metadata = metadata;
        self
    }

    /// Set [`Metadata::level`](struct.Metadata.html#method.level).
    #[inline]
    pub fn level(&mut self, level: Level) -> &mut RecordBuilder<'a> {
        self.record.metadata.level = level;
        self
    }

    /// Set [`Metadata::target`](struct.Metadata.html#method.target)
    #[inline]
    pub fn target(&mut self, target: &'a str) -> &mut RecordBuilder<'a> {
        self.record.metadata.target = target;
        self
    }

    /// Set [`module_path`](struct.Record.html#method.module_path)
    #[inline]
    pub fn module_path(&mut self, path: Option<&'a str>) -> &mut RecordBuilder<'a> {
        self.record.module_path = path.map(MaybeStaticStr::Borrowed);
        self
    }

    /// Set [`module_path`](struct.Record.html#method.module_path) to a `'static` string
    #[inline]
    pub fn module_path_static(&mut self, path: Option<&'static str>) -> &mut RecordBuilder<'a> {
        self.record.module_path = path.map(MaybeStaticStr::Static);
        self
    }

    /// Set [`file`](struct.Record.html#method.file)
    #[inline]
    pub fn file(&mut self, file: Option<&'a str>) -> &mut RecordBuilder<'a> {
        self.record.file = file.map(MaybeStaticStr::Borrowed);
        self
    }

    /// Set [`file`](struct.Record.html#method.file) to a `'static` string.
    #[inline]
    pub fn file_static(&mut self, file: Option<&'static str>) -> &mut RecordBuilder<'a> {
        self.record.file = file.map(MaybeStaticStr::Static);
        self
    }

    /// Set [`line`](struct.Record.html#method.line)
    #[inline]
    pub fn line(&mut self, line: Option<u32>) -> &mut RecordBuilder<'a> {
        self.record.line = line;
        self
    }

    /// Set [`key_values`](struct.Record.html#method.key_values)
    #[cfg(feature = "kv_unstable")]
    #[inline]
    pub fn key_values(&mut self, kvs: &'a dyn kv::Source) -> &mut RecordBuilder<'a> {
        self.record.key_values = KeyValues(kvs);
        self
    }

    /// Invoke the builder and return a `Record`
    #[inline]
    pub fn build(&self) -> Record<'a> {
        self.record.clone()
    }
}

/// Metadata about a log message.
///
/// # Use
///
/// `Metadata` structs are created when users of the library use
/// logging macros.
///
/// They are consumed by implementations of the `Log` trait in the
/// `enabled` method.
///
/// `Record`s use `Metadata` to determine the log message's severity
/// and target.
///
/// Users should use the `log_enabled!` macro in their code to avoid
/// constructing expensive log messages.
///
/// # Examples
///
/// ```edition2018
/// use log::{Record, Level, Metadata};
///
/// struct MyLogger;
///
/// impl log::Log for MyLogger {
///     fn enabled(&self, metadata: &Metadata) -> bool {
///         metadata.level() <= Level::Info
///     }
///
///     fn log(&self, record: &Record) {
///         if self.enabled(record.metadata()) {
///             println!("{} - {}", record.level(), record.args());
///         }
///     }
///     fn flush(&self) {}
/// }
///
/// # fn main(){}
/// ```
#[derive(Clone, Eq, PartialEq, Ord, PartialOrd, Hash, Debug)]
pub struct Metadata<'a> {
    level: Level,
    target: &'a str,
}

impl<'a> Metadata<'a> {
    /// Returns a new builder.
    #[inline]
    pub fn builder() -> MetadataBuilder<'a> {
        MetadataBuilder::new()
    }

    /// The verbosity level of the message.
    #[inline]
    pub fn level(&self) -> Level {
        self.level
    }

    /// The name of the target of the directive.
    #[inline]
    pub fn target(&self) -> &'a str {
        self.target
    }
}

/// Builder for [`Metadata`](struct.Metadata.html).
///
/// Typically should only be used by log library creators or for testing and "shim loggers".
/// The `MetadataBuilder` can set the different parameters of a `Metadata` object, and returns
/// the created object when `build` is called.
///
/// # Example
///
/// ```edition2018
/// let target = "myApp";
/// use log::{Level, MetadataBuilder};
/// let metadata = MetadataBuilder::new()
///                     .level(Level::Debug)
///                     .target(target)
///                     .build();
/// ```
#[derive(Eq, PartialEq, Ord, PartialOrd, Hash, Debug)]
pub struct MetadataBuilder<'a> {
    metadata: Metadata<'a>,
}

impl<'a> MetadataBuilder<'a> {
    /// Construct a new `MetadataBuilder`.
    ///
    /// The default options are:
    ///
    /// - `level`: `Level::Info`
    /// - `target`: `""`
    #[inline]
    pub fn new() -> MetadataBuilder<'a> {
        MetadataBuilder {
            metadata: Metadata {
                level: Level::Info,
                target: "",
            },
        }
    }

    /// Setter for [`level`](struct.Metadata.html#method.level).
    #[inline]
    pub fn level(&mut self, arg: Level) -> &mut MetadataBuilder<'a> {
        self.metadata.level = arg;
        self
    }

    /// Setter for [`target`](struct.Metadata.html#method.target).
    #[inline]
    pub fn target(&mut self, target: &'a str) -> &mut MetadataBuilder<'a> {
        self.metadata.target = target;
        self
    }

    /// Returns a `Metadata` object.
    #[inline]
    pub fn build(&self) -> Metadata<'a> {
        self.metadata.clone()
    }
}

/// A trait encapsulating the operations required of a logger.
pub trait Log: Sync + Send {
    /// Determines if a log message with the specified metadata would be
    /// logged.
    ///
    /// This is used by the `log_enabled!` macro to allow callers to avoid
    /// expensive computation of log message arguments if the message would be
    /// discarded anyway.
    fn enabled(&self, metadata: &Metadata) -> bool;

    /// Logs the `Record`.
    ///
    /// Note that `enabled` is *not* necessarily called before this method.
    /// Implementations of `log` should perform all necessary filtering
    /// internally.
    fn log(&self, record: &Record);

    /// Flushes any buffered records.
    fn flush(&self);
}

// Just used as a dummy initial value for LOGGER
struct NopLogger;

impl Log for NopLogger {
    fn enabled(&self, _: &Metadata) -> bool {
        false
    }

    fn log(&self, _: &Record) {}
    fn flush(&self) {}
}

#[cfg(feature = "std")]
impl<T> Log for std::boxed::Box<T>
where
    T: ?Sized + Log,
{
    fn enabled(&self, metadata: &Metadata) -> bool {
        self.as_ref().enabled(metadata)
    }

    fn log(&self, record: &Record) {
        self.as_ref().log(record)
    }
    fn flush(&self) {
        self.as_ref().flush()
    }
}

/// Sets the global maximum log level.
///
/// Generally, this should only be called by the active logging implementation.
#[inline]
pub fn set_max_level(level: LevelFilter) {
    MAX_LOG_LEVEL_FILTER.store(level as usize, Ordering::SeqCst)
}

/// Returns the current maximum log level.
///
/// The [`log!`], [`error!`], [`warn!`], [`info!`], [`debug!`], and [`trace!`] macros check
/// this value and discard any message logged at a higher level. The maximum
/// log level is set by the [`set_max_level`] function.
///
/// [`log!`]: macro.log.html
/// [`error!`]: macro.error.html
/// [`warn!`]: macro.warn.html
/// [`info!`]: macro.info.html
/// [`debug!`]: macro.debug.html
/// [`trace!`]: macro.trace.html
/// [`set_max_level`]: fn.set_max_level.html
#[inline(always)]
pub fn max_level() -> LevelFilter {
    // Since `LevelFilter` is `repr(usize)`,
    // this transmute is sound if and only if `MAX_LOG_LEVEL_FILTER`
    // is set to a usize that is a valid discriminant for `LevelFilter`.
    // Since `MAX_LOG_LEVEL_FILTER` is private, the only time it's set
    // is by `set_max_level` above, i.e. by casting a `LevelFilter` to `usize`.
    // So any usize stored in `MAX_LOG_LEVEL_FILTER` is a valid discriminant.
    unsafe { mem::transmute(MAX_LOG_LEVEL_FILTER.load(Ordering::Relaxed)) }
}

/// Sets the global logger to a `Box<Log>`.
///
/// This is a simple convenience wrapper over `set_logger`, which takes a
/// `Box<Log>` rather than a `&'static Log`. See the documentation for
/// [`set_logger`] for more details.
///
/// Requires the `std` feature.
///
/// # Errors
///
/// An error is returned if a logger has already been set.
///
/// [`set_logger`]: fn.set_logger.html
#[cfg(all(feature = "std", atomic_cas))]
pub fn set_boxed_logger(logger: Box<dyn Log>) -> Result<(), SetLoggerError> {
    set_logger_inner(|| Box::leak(logger))
}

/// Sets the global logger to a `&'static Log`.
///
/// This function may only be called once in the lifetime of a program. Any log
/// events that occur before the call to `set_logger` completes will be ignored.
///
/// This function does not typically need to be called manually. Logger
/// implementations should provide an initialization method that installs the
/// logger internally.
///
/// # Availability
///
/// This method is available even when the `std` feature is disabled. However,
/// it is currently unavailable on `thumbv6` targets, which lack support for
/// some atomic operations which are used by this function. Even on those
/// targets, [`set_logger_racy`] will be available.
///
/// # Errors
///
/// An error is returned if a logger has already been set.
///
/// # Examples
///
/// ```edition2018
/// use log::{error, info, warn, Record, Level, Metadata, LevelFilter};
///
/// static MY_LOGGER: MyLogger = MyLogger;
///
/// struct MyLogger;
///
/// impl log::Log for MyLogger {
///     fn enabled(&self, metadata: &Metadata) -> bool {
///         metadata.level() <= Level::Info
///     }
///
///     fn log(&self, record: &Record) {
///         if self.enabled(record.metadata()) {
///             println!("{} - {}", record.level(), record.args());
///         }
///     }
///     fn flush(&self) {}
/// }
///
/// # fn main(){
/// log::set_logger(&MY_LOGGER).unwrap();
/// log::set_max_level(LevelFilter::Info);
///
/// info!("hello log");
/// warn!("warning");
/// error!("oops");
/// # }
/// ```
///
/// [`set_logger_racy`]: fn.set_logger_racy.html
#[cfg(atomic_cas)]
pub fn set_logger(logger: &'static dyn Log) -> Result<(), SetLoggerError> {
    set_logger_inner(|| logger)
}

#[cfg(atomic_cas)]
fn set_logger_inner<F>(make_logger: F) -> Result<(), SetLoggerError>
where
    F: FnOnce() -> &'static dyn Log,
{
    let old_state = match STATE.compare_exchange(
        UNINITIALIZED,
        INITIALIZING,
        Ordering::SeqCst,
        Ordering::SeqCst,
    ) {
        Ok(s) | Err(s) => s,
    };
    match old_state {
        UNINITIALIZED => {
            unsafe {
                LOGGER = make_logger();
            }
            STATE.store(INITIALIZED, Ordering::SeqCst);
            Ok(())
        }
        INITIALIZING => {
            while STATE.load(Ordering::SeqCst) == INITIALIZING {
                std::sync::atomic::spin_loop_hint();
            }
            Err(SetLoggerError(()))
        }
        _ => Err(SetLoggerError(())),
    }
}

/// A thread-unsafe version of [`set_logger`].
///
/// This function is available on all platforms, even those that do not have
/// support for atomics that is needed by [`set_logger`].
///
/// In almost all cases, [`set_logger`] should be preferred.
///
/// # Safety
///
/// This function is only safe to call when no other logger initialization
/// function is called while this function still executes.
///
/// This can be upheld by (for example) making sure that **there are no other
/// threads**, and (on embedded) that **interrupts are disabled**.
///
/// It is safe to use other logging functions while this function runs
/// (including all logging macros).
///
/// [`set_logger`]: fn.set_logger.html
pub unsafe fn set_logger_racy(logger: &'static dyn Log) -> Result<(), SetLoggerError> {
    match STATE.load(Ordering::SeqCst) {
        UNINITIALIZED => {
            LOGGER = logger;
            STATE.store(INITIALIZED, Ordering::SeqCst);
            Ok(())
        }
        INITIALIZING => {
            // This is just plain UB, since we were racing another initialization function
            unreachable!("set_logger_racy must not be used with other initialization functions")
        }
        _ => Err(SetLoggerError(())),
    }
}

/// The type returned by [`set_logger`] if [`set_logger`] has already been called.
///
/// [`set_logger`]: fn.set_logger.html
#[allow(missing_copy_implementations)]
#[derive(Debug)]
pub struct SetLoggerError(());

impl fmt::Display for SetLoggerError {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str(SET_LOGGER_ERROR)
    }
}

// The Error trait is not available in libcore
#[cfg(feature = "std")]
impl error::Error for SetLoggerError {}

/// The type returned by [`from_str`] when the string doesn't match any of the log levels.
///
/// [`from_str`]: https://doc.rust-lang.org/std/str/trait.FromStr.html#tymethod.from_str
#[allow(missing_copy_implementations)]
#[derive(Debug, PartialEq)]
pub struct ParseLevelError(());

impl fmt::Display for ParseLevelError {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str(LEVEL_PARSE_ERROR)
    }
}

// The Error trait is not available in libcore
#[cfg(feature = "std")]
impl error::Error for ParseLevelError {}

/// Returns a reference to the logger.
///
/// If a logger has not been set, a no-op implementation is returned.
pub fn logger() -> &'static dyn Log {
    if STATE.load(Ordering::SeqCst) != INITIALIZED {
        static NOP: NopLogger = NopLogger;
        &NOP
    } else {
        unsafe { LOGGER }
    }
}

// WARNING: this is not part of the crate's public API and is subject to change at any time
#[doc(hidden)]
pub fn __private_api_log(
    args: fmt::Arguments,
    level: Level,
    &(target, module_path, file, line): &(&str, &'static str, &'static str, u32),
) {
    logger().log(
        &Record::builder()
            .args(args)
            .level(level)
            .target(target)
            .module_path_static(Some(module_path))
            .file_static(Some(file))
            .line(Some(line))
            .build(),
    );
}

// WARNING: this is not part of the crate's public API and is subject to change at any time
#[doc(hidden)]
pub fn __private_api_enabled(level: Level, target: &str) -> bool {
    logger().enabled(&Metadata::builder().level(level).target(target).build())
}

/// The statically resolved maximum log level.
///
/// See the crate level documentation for information on how to configure this.
///
/// This value is checked by the log macros, but not by the `Log`ger returned by
/// the [`logger`] function. Code that manually calls functions on that value
/// should compare the level against this value.
///
/// [`logger`]: fn.logger.html
pub const STATIC_MAX_LEVEL: LevelFilter = MAX_LEVEL_INNER;

cfg_if! {
    if #[cfg(all(not(debug_assertions), feature = "release_max_level_off"))] {
        const MAX_LEVEL_INNER: LevelFilter = LevelFilter::Off;
    } else if #[cfg(all(not(debug_assertions), feature = "release_max_level_error"))] {
        const MAX_LEVEL_INNER: LevelFilter = LevelFilter::Error;
    } else if #[cfg(all(not(debug_assertions), feature = "release_max_level_warn"))] {
        const MAX_LEVEL_INNER: LevelFilter = LevelFilter::Warn;
    } else if #[cfg(all(not(debug_assertions), feature = "release_max_level_info"))] {
        const MAX_LEVEL_INNER: LevelFilter = LevelFilter::Info;
    } else if #[cfg(all(not(debug_assertions), feature = "release_max_level_debug"))] {
        const MAX_LEVEL_INNER: LevelFilter = LevelFilter::Debug;
    } else if #[cfg(all(not(debug_assertions), feature = "release_max_level_trace"))] {
        const MAX_LEVEL_INNER: LevelFilter = LevelFilter::Trace;
    } else if #[cfg(feature = "max_level_off")] {
        const MAX_LEVEL_INNER: LevelFilter = LevelFilter::Off;
    } else if #[cfg(feature = "max_level_error")] {
        const MAX_LEVEL_INNER: LevelFilter = LevelFilter::Error;
    } else if #[cfg(feature = "max_level_warn")] {
        const MAX_LEVEL_INNER: LevelFilter = LevelFilter::Warn;
    } else if #[cfg(feature = "max_level_info")] {
        const MAX_LEVEL_INNER: LevelFilter = LevelFilter::Info;
    } else if #[cfg(feature = "max_level_debug")] {
        const MAX_LEVEL_INNER: LevelFilter = LevelFilter::Debug;
    } else {
        const MAX_LEVEL_INNER: LevelFilter = LevelFilter::Trace;
    }
}

#[cfg(test)]
mod tests {
    extern crate std;
    use super::{Level, LevelFilter, ParseLevelError};
    use tests::std::string::ToString;

    #[test]
    fn test_levelfilter_from_str() {
        let tests = [
            ("off", Ok(LevelFilter::Off)),
            ("error", Ok(LevelFilter::Error)),
            ("warn", Ok(LevelFilter::Warn)),
            ("info", Ok(LevelFilter::Info)),
            ("debug", Ok(LevelFilter::Debug)),
            ("trace", Ok(LevelFilter::Trace)),
            ("OFF", Ok(LevelFilter::Off)),
            ("ERROR", Ok(LevelFilter::Error)),
            ("WARN", Ok(LevelFilter::Warn)),
            ("INFO", Ok(LevelFilter::Info)),
            ("DEBUG", Ok(LevelFilter::Debug)),
            ("TRACE", Ok(LevelFilter::Trace)),
            ("asdf", Err(ParseLevelError(()))),
        ];
        for &(s, ref expected) in &tests {
            assert_eq!(expected, &s.parse());
        }
    }

    #[test]
    fn test_level_from_str() {
        let tests = [
            ("OFF", Err(ParseLevelError(()))),
            ("error", Ok(Level::Error)),
            ("warn", Ok(Level::Warn)),
            ("info", Ok(Level::Info)),
            ("debug", Ok(Level::Debug)),
            ("trace", Ok(Level::Trace)),
            ("ERROR", Ok(Level::Error)),
            ("WARN", Ok(Level::Warn)),
            ("INFO", Ok(Level::Info)),
            ("DEBUG", Ok(Level::Debug)),
            ("TRACE", Ok(Level::Trace)),
            ("asdf", Err(ParseLevelError(()))),
        ];
        for &(s, ref expected) in &tests {
            assert_eq!(expected, &s.parse());
        }
    }

    #[test]
    fn test_level_as_str() {
        let tests = &[
            (Level::Error, "ERROR"),
            (Level::Warn, "WARN"),
            (Level::Info, "INFO"),
            (Level::Debug, "DEBUG"),
            (Level::Trace, "TRACE"),
        ];
        for (input, expected) in tests {
            assert_eq!(*expected, input.as_str());
        }
    }

    #[test]
    fn test_level_show() {
        assert_eq!("INFO", Level::Info.to_string());
        assert_eq!("ERROR", Level::Error.to_string());
    }

    #[test]
    fn test_levelfilter_show() {
        assert_eq!("OFF", LevelFilter::Off.to_string());
        assert_eq!("ERROR", LevelFilter::Error.to_string());
    }

    #[test]
    fn test_cross_cmp() {
        assert!(Level::Debug > LevelFilter::Error);
        assert!(LevelFilter::Warn < Level::Trace);
        assert!(LevelFilter::Off < Level::Error);
    }

    #[test]
    fn test_cross_eq() {
        assert!(Level::Error == LevelFilter::Error);
        assert!(LevelFilter::Off != Level::Error);
        assert!(Level::Trace == LevelFilter::Trace);
    }

    #[test]
    fn test_to_level() {
        assert_eq!(Some(Level::Error), LevelFilter::Error.to_level());
        assert_eq!(None, LevelFilter::Off.to_level());
        assert_eq!(Some(Level::Debug), LevelFilter::Debug.to_level());
    }

    #[test]
    fn test_to_level_filter() {
        assert_eq!(LevelFilter::Error, Level::Error.to_level_filter());
        assert_eq!(LevelFilter::Trace, Level::Trace.to_level_filter());
    }

    #[test]
    fn test_level_filter_as_str() {
        let tests = &[
            (LevelFilter::Off, "OFF"),
            (LevelFilter::Error, "ERROR"),
            (LevelFilter::Warn, "WARN"),
            (LevelFilter::Info, "INFO"),
            (LevelFilter::Debug, "DEBUG"),
            (LevelFilter::Trace, "TRACE"),
        ];
        for (input, expected) in tests {
            assert_eq!(*expected, input.as_str());
        }
    }

    #[test]
    #[cfg(feature = "std")]
    fn test_error_trait() {
        use super::SetLoggerError;
        let e = SetLoggerError(());
        assert_eq!(
            &e.to_string(),
            "attempted to set a logger after the logging system \
             was already initialized"
        );
    }

    #[test]
    fn test_metadata_builder() {
        use super::MetadataBuilder;
        let target = "myApp";
        let metadata_test = MetadataBuilder::new()
            .level(Level::Debug)
            .target(target)
            .build();
        assert_eq!(metadata_test.level(), Level::Debug);
        assert_eq!(metadata_test.target(), "myApp");
    }

    #[test]
    fn test_metadata_convenience_builder() {
        use super::Metadata;
        let target = "myApp";
        let metadata_test = Metadata::builder()
            .level(Level::Debug)
            .target(target)
            .build();
        assert_eq!(metadata_test.level(), Level::Debug);
        assert_eq!(metadata_test.target(), "myApp");
    }

    #[test]
    fn test_record_builder() {
        use super::{MetadataBuilder, RecordBuilder};
        let target = "myApp";
        let metadata = MetadataBuilder::new().target(target).build();
        let fmt_args = format_args!("hello");
        let record_test = RecordBuilder::new()
            .args(fmt_args)
            .metadata(metadata)
            .module_path(Some("foo"))
            .file(Some("bar"))
            .line(Some(30))
            .build();
        assert_eq!(record_test.metadata().target(), "myApp");
        assert_eq!(record_test.module_path(), Some("foo"));
        assert_eq!(record_test.file(), Some("bar"));
        assert_eq!(record_test.line(), Some(30));
    }

    #[test]
    fn test_record_convenience_builder() {
        use super::{Metadata, Record};
        let target = "myApp";
        let metadata = Metadata::builder().target(target).build();
        let fmt_args = format_args!("hello");
        let record_test = Record::builder()
            .args(fmt_args)
            .metadata(metadata)
            .module_path(Some("foo"))
            .file(Some("bar"))
            .line(Some(30))
            .build();
        assert_eq!(record_test.target(), "myApp");
        assert_eq!(record_test.module_path(), Some("foo"));
        assert_eq!(record_test.file(), Some("bar"));
        assert_eq!(record_test.line(), Some(30));
    }

    #[test]
    fn test_record_complete_builder() {
        use super::{Level, Record};
        let target = "myApp";
        let record_test = Record::builder()
            .module_path(Some("foo"))
            .file(Some("bar"))
            .line(Some(30))
            .target(target)
            .level(Level::Error)
            .build();
        assert_eq!(record_test.target(), "myApp");
        assert_eq!(record_test.level(), Level::Error);
        assert_eq!(record_test.module_path(), Some("foo"));
        assert_eq!(record_test.file(), Some("bar"));
        assert_eq!(record_test.line(), Some(30));
    }

    #[test]
    #[cfg(feature = "kv_unstable")]
    fn test_record_key_values_builder() {
        use super::Record;
        use kv::{self, Visitor};

        struct TestVisitor {
            seen_pairs: usize,
        }

        impl<'kvs> Visitor<'kvs> for TestVisitor {
            fn visit_pair(
                &mut self,
                _: kv::Key<'kvs>,
                _: kv::Value<'kvs>,
            ) -> Result<(), kv::Error> {
                self.seen_pairs += 1;
                Ok(())
            }
        }

        let kvs: &[(&str, i32)] = &[("a", 1), ("b", 2)];
        let record_test = Record::builder().key_values(&kvs).build();

        let mut visitor = TestVisitor { seen_pairs: 0 };

        record_test.key_values().visit(&mut visitor).unwrap();

        assert_eq!(2, visitor.seen_pairs);
    }

    #[test]
    #[cfg(feature = "kv_unstable")]
    fn test_record_key_values_get_coerce() {
        use super::Record;

        let kvs: &[(&str, &str)] = &[("a", "1"), ("b", "2")];
        let record = Record::builder().key_values(&kvs).build();

        assert_eq!(
            "2",
            record
                .key_values()
                .get("b".into())
                .expect("missing key")
                .to_borrowed_str()
                .expect("invalid value")
        );
    }
}
