// Copyright 2014-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! A simple logger configured via environment variables which writes
//! to stdout or stderr, for use with the logging facade exposed by the
//! [`log` crate][log-crate-url].
//!
//! ## Example
//!
//! ```
//! #[macro_use] extern crate log;
//! extern crate env_logger;
//!
//! use log::Level;
//!
//! fn main() {
//!     env_logger::init();
//!
//!     debug!("this is a debug {}", "message");
//!     error!("this is printed by default");
//!
//!     if log_enabled!(Level::Info) {
//!         let x = 3 * 4; // expensive computation
//!         info!("the answer was: {}", x);
//!     }
//! }
//! ```
//!
//! Assumes the binary is `main`:
//!
//! ```{.bash}
//! $ RUST_LOG=error ./main
//! ERROR: 2017-11-09T02:12:24Z: main: this is printed by default
//! ```
//!
//! ```{.bash}
//! $ RUST_LOG=info ./main
//! ERROR: 2017-11-09T02:12:24Z: main: this is printed by default
//! INFO: 2017-11-09T02:12:24Z: main: the answer was: 12
//! ```
//!
//! ```{.bash}
//! $ RUST_LOG=debug ./main
//! DEBUG: 2017-11-09T02:12:24Z: main: this is a debug message
//! ERROR: 2017-11-09T02:12:24Z: main: this is printed by default
//! INFO: 2017-11-09T02:12:24Z: main: the answer was: 12
//! ```
//!
//! You can also set the log level on a per module basis:
//!
//! ```{.bash}
//! $ RUST_LOG=main=info ./main
//! ERROR: 2017-11-09T02:12:24Z: main: this is printed by default
//! INFO: 2017-11-09T02:12:24Z: main: the answer was: 12
//! ```
//!
//! And enable all logging:
//!
//! ```{.bash}
//! $ RUST_LOG=main ./main
//! DEBUG: 2017-11-09T02:12:24Z: main: this is a debug message
//! ERROR: 2017-11-09T02:12:24Z: main: this is printed by default
//! INFO: 2017-11-09T02:12:24Z: main: the answer was: 12
//! ```
//!
//! See the documentation for the [`log` crate][log-crate-url] for more
//! information about its API.
//!
//! ## Enabling logging
//!
//! Log levels are controlled on a per-module basis, and by default all logging
//! is disabled except for `error!`. Logging is controlled via the `RUST_LOG`
//! environment variable. The value of this environment variable is a
//! comma-separated list of logging directives. A logging directive is of the
//! form:
//!
//! ```text
//! path::to::module=level
//! ```
//!
//! The path to the module is rooted in the name of the crate it was compiled
//! for, so if your program is contained in a file `hello.rs`, for example, to
//! turn on logging for this file you would use a value of `RUST_LOG=hello`.
//! Furthermore, this path is a prefix-search, so all modules nested in the
//! specified module will also have logging enabled.
//!
//! The actual `level` is optional to specify. If omitted, all logging will
//! be enabled. If specified, it must be one of the strings `debug`, `error`,
//! `info`, `warn`, or `trace`.
//!
//! As the log level for a module is optional, the module to enable logging for
//! is also optional. If only a `level` is provided, then the global log
//! level for all modules is set to this value.
//!
//! Some examples of valid values of `RUST_LOG` are:
//!
//! * `hello` turns on all logging for the 'hello' module
//! * `info` turns on all info logging
//! * `hello=debug` turns on debug logging for 'hello'
//! * `hello,std::option` turns on hello, and std's option logging
//! * `error,hello=warn` turn on global error logging and also warn for hello
//!
//! ## Filtering results
//!
//! A `RUST_LOG` directive may include a regex filter. The syntax is to append `/`
//! followed by a regex. Each message is checked against the regex, and is only
//! logged if it matches. Note that the matching is done after formatting the
//! log string but before adding any logging meta-data. There is a single filter
//! for all modules.
//!
//! Some examples:
//!
//! * `hello/foo` turns on all logging for the 'hello' module where the log
//!   message includes 'foo'.
//! * `info/f.o` turns on all info logging where the log message includes 'foo',
//!   'f1o', 'fao', etc.
//! * `hello=debug/foo*foo` turns on debug logging for 'hello' where the log
//!   message includes 'foofoo' or 'fofoo' or 'fooooooofoo', etc.
//! * `error,hello=warn/[0-9]scopes` turn on global error logging and also
//!   warn for hello. In both cases the log message must include a single digit
//!   number followed by 'scopes'.
//!
//! ## Disabling colors
//!
//! Colors and other styles can be configured with the `RUST_LOG_STYLE`
//! environment variable. It accepts the following values:
//!
//! * `auto` (default) will attempt to print style characters, but don't force the issue.
//! If the console isn't available on Windows, or if TERM=dumb, for example, then don't print colors.
//! * `always` will always print style characters even if they aren't supported by the terminal.
//! This includes emitting ANSI colors on Windows if the console API is unavailable.
//! * `never` will never print style characters.
//! 
//! ## Tweaking the default format
//! 
//! Parts of the default format can be excluded from the log output using the [`Builder`].
//! The following example excluding the timestamp from the log output:
//! 
//! ```
//! #[macro_use] extern crate log;
//! extern crate env_logger;
//!
//! use log::Level;
//!
//! fn main() {
//!     env_logger::Builder::from_default_env()
//!         .default_format_timestamp(false)
//!         .init();
//!
//!     debug!("this is a debug {}", "message");
//!     error!("this is printed by default");
//!
//!     if log_enabled!(Level::Info) {
//!         let x = 3 * 4; // expensive computation
//!         info!("the answer was: {}", x);
//!     }
//! }
//! ```
//! 
//! [log-crate-url]: https://docs.rs/log/
//! [`Builder`]: struct.Builder.html

#![doc(html_logo_url = "http://www.rust-lang.org/logos/rust-logo-128x128-blk-v2.png",
       html_favicon_url = "http://www.rust-lang.org/favicon.ico",
       html_root_url = "https://docs.rs/env_logger/0.5.6")]
#![cfg_attr(test, deny(warnings))]

// When compiled for the rustc compiler itself we want to make sure that this is
// an unstable crate
#![cfg_attr(rustbuild, feature(staged_api, rustc_private))]
#![cfg_attr(rustbuild, unstable(feature = "rustc_private", issue = "27812"))]

#![deny(missing_debug_implementations, missing_docs, warnings)]

extern crate log;
extern crate termcolor;
extern crate humantime;
extern crate atty;

use std::env;
use std::borrow::Cow;
use std::io::prelude::*;
use std::io;
use std::mem;
use std::cell::RefCell;

use log::{Log, LevelFilter, Level, Record, SetLoggerError, Metadata};

pub mod filter;
pub mod fmt;

pub use self::fmt::{Target, WriteStyle, Color, Formatter};

const DEFAULT_FILTER_ENV: &'static str = "RUST_LOG";
const DEFAULT_WRITE_STYLE_ENV: &'static str = "RUST_LOG_STYLE";

/// Set of environment variables to configure from.
///
/// By default, the `Env` will read the following environment variables:
///
/// - `RUST_LOG`: the level filter
/// - `RUST_LOG_STYLE`: whether or not to print styles with records.
///
/// These sources can be configured using the builder methods on `Env`.
#[derive(Debug)]
pub struct Env<'a> {
    filter: Cow<'a, str>,
    write_style: Cow<'a, str>,
}

/// The env logger.
///
/// This struct implements the `Log` trait from the [`log` crate][log-crate-url],
/// which allows it to act as a logger.
///
/// The [`init()`], [`try_init()`], [`Builder::init()`] and [`Builder::try_init()`]
/// methods will each construct a `Logger` and immediately initialize it as the
/// default global logger.
///
/// If you'd instead need access to the constructed `Logger`, you can use
/// the associated [`Builder`] and install it with the
/// [`log` crate][log-crate-url] directly.
///
/// [log-crate-url]: https://docs.rs/log/
/// [`init()`]: fn.init.html
/// [`try_init()`]: fn.try_init.html
/// [`Builder::init()`]: struct.Builder.html#method.init
/// [`Builder::try_init()`]: struct.Builder.html#method.try_init
/// [`Builder`]: struct.Builder.html
pub struct Logger {
    writer: fmt::Writer,
    filter: filter::Filter,
    format: Box<Fn(&mut Formatter, &Record) -> io::Result<()> + Sync + Send>,
}

struct Format {
    default_format_timestamp: bool,
    default_format_module_path: bool,
    default_format_level: bool,
    custom_format: Option<Box<Fn(&mut Formatter, &Record) -> io::Result<()> + Sync + Send>>,
}

impl Default for Format {
    fn default() -> Self {
        Format {
            default_format_timestamp: true,
            default_format_module_path: true,
            default_format_level: true,
            custom_format: None,
        }
    }
}

impl Format {
    /// Convert the format into a callable function.
    /// 
    /// If the `custom_format` is `Some`, then any `default_format` switches are ignored.
    /// If the `custom_format` is `None`, then a default format is returned.
    /// Any `default_format` switches set to `false` won't be written by the format.
    fn into_boxed_fn(self) -> Box<Fn(&mut Formatter, &Record) -> io::Result<()> + Sync + Send> {
        if let Some(fmt) = self.custom_format {
            fmt
        }
        else {
            Box::new(move |buf, record| {
                let write_level = if self.default_format_level {
                    let level = record.level();
                    let mut level_style = buf.style();

                    match level {
                        Level::Trace => level_style.set_color(Color::White),
                        Level::Debug => level_style.set_color(Color::Blue),
                        Level::Info => level_style.set_color(Color::Green),
                        Level::Warn => level_style.set_color(Color::Yellow),
                        Level::Error => level_style.set_color(Color::Red).set_bold(true),
                    };

                    write!(buf, "{:>5} ", level_style.value(level))
                } else {
                    Ok(())
                };

                let write_ts = if self.default_format_timestamp {
                    let ts = buf.timestamp();
                    write!(buf, "{}: ", ts)
                } else {
                    Ok(())
                };

                let default_format_module_path = (self.default_format_module_path, record.module_path());
                let write_module_path = if let (true, Some(module_path)) = default_format_module_path {
                    write!(buf, "{}: ", module_path)
                } else {
                    Ok(())
                };

                let write_args = writeln!(buf, "{}", record.args());

                write_level.and(write_ts).and(write_module_path).and(write_args)
            })
        }
    }
}

/// `Builder` acts as builder for initializing a `Logger`.
///
/// It can be used to customize the log format, change the environment variable used
/// to provide the logging directives and also set the default log level filter.
///
/// # Examples
///
/// ```
/// #[macro_use]
/// extern crate log;
/// extern crate env_logger;
///
/// use std::env;
/// use std::io::Write;
/// use log::LevelFilter;
/// use env_logger::Builder;
///
/// fn main() {
///     let mut builder = Builder::from_default_env();
///
///     builder.format(|buf, record| writeln!(buf, "{} - {}", record.level(), record.args()))
///            .filter(None, LevelFilter::Info)
///            .init();
///
///     error!("error message");
///     info!("info message");
/// }
/// ```
#[derive(Default)]
pub struct Builder {
    filter: filter::Builder,
    writer: fmt::Builder,
    format: Format,
}

impl Builder {
    /// Initializes the log builder with defaults.
    /// 
    /// **NOTE:** This method won't read from any environment variables.
    /// Use the [`filter`] and [`write_style`] methods to configure the builder
    /// or use [`from_env`] or [`from_default_env`] instead.
    /// 
    /// # Examples
    /// 
    /// Create a new builder and configure filters and style:
    /// 
    /// ```
    /// # extern crate log;
    /// # extern crate env_logger;
    /// # fn main() {
    /// use log::LevelFilter;
    /// use env_logger::{Builder, WriteStyle};
    /// 
    /// let mut builder = Builder::new();
    /// 
    /// builder.filter(None, LevelFilter::Info)
    ///        .write_style(WriteStyle::Always)
    ///        .init();
    /// # }
    /// ```
    /// 
    /// [`filter`]: #method.filter
    /// [`write_style`]: #method.write_style
    /// [`from_env`]: #method.from_env
    /// [`from_default_env`]: #method.from_default_env
    pub fn new() -> Builder {
        Default::default()
    }

    /// Initializes the log builder from the environment.
    ///
    /// The variables used to read configuration from can be tweaked before
    /// passing in.
    ///
    /// # Examples
    /// 
    /// Initialise a logger reading the log filter from an environment variable
    /// called `MY_LOG`:
    /// 
    /// ```
    /// use env_logger::Builder;
    /// 
    /// let mut builder = Builder::from_env("MY_LOG");
    /// builder.init();
    /// ```
    ///
    /// Initialise a logger using the `MY_LOG` variable for filtering and
    /// `MY_LOG_STYLE` for whether or not to write styles:
    ///
    /// ```
    /// use env_logger::{Builder, Env};
    ///
    /// let env = Env::new().filter("MY_LOG").write_style("MY_LOG_STYLE");
    ///
    /// let mut builder = Builder::from_env(env);
    /// builder.init();
    /// ```
    pub fn from_env<'a, E>(env: E) -> Self
    where
        E: Into<Env<'a>>
    {
        let mut builder = Builder::new();
        let env = env.into();

        if let Some(s) = env.get_filter() {
            builder.parse(&s);
        }

        if let Some(s) = env.get_write_style() {
            builder.parse_write_style(&s);
        }

        builder
    }

    /// Initializes the log builder from the environment using default variable names.
    /// 
    /// This method is a convenient way to call `from_env(Env::default())` without
    /// having to use the `Env` type explicitly. The builder will read the following 
    /// environment variables:
    /// 
    /// - `RUST_LOG`: the level filter
    /// - `RUST_LOG_STYLE`: whether or not to print styles with records.
    /// 
    /// # Examples
    /// 
    /// Initialise a logger using the default environment variables:
    /// 
    /// ```
    /// use env_logger::Builder;
    /// 
    /// let mut builder = Builder::from_default_env();
    /// builder.init();
    /// ```
    pub fn from_default_env() -> Self {
        Self::from_env(Env::default())
    }

    /// Sets the format function for formatting the log output.
    ///
    /// This function is called on each record logged and should format the
    /// log record and output it to the given [`Formatter`].
    ///
    /// The format function is expected to output the string directly to the
    /// `Formatter` so that implementations can use the [`std::fmt`] macros
    /// to format and output without intermediate heap allocations. The default
    /// `env_logger` formatter takes advantage of this.
    /// 
    /// # Examples
    /// 
    /// Use a custom format to write only the log message:
    /// 
    /// ```
    /// use std::io::Write;
    /// use env_logger::Builder;
    /// 
    /// let mut builder = Builder::new();
    /// 
    /// builder.format(|buf, record| write!(buf, "{}", record.args()));
    /// ```
    ///
    /// [`Formatter`]: fmt/struct.Formatter.html
    /// [`String`]: https://doc.rust-lang.org/stable/std/string/struct.String.html
    /// [`std::fmt`]: https://doc.rust-lang.org/std/fmt/index.html
    pub fn format<F: 'static>(&mut self, format: F) -> &mut Self
        where F: Fn(&mut Formatter, &Record) -> io::Result<()> + Sync + Send
    {
        self.format.custom_format = Some(Box::new(format));
        self
    }

    /// Use the default format.
    /// 
    /// This method will clear any custom format set on the builder.
    pub fn default_format(&mut self) -> &mut Self {
        self.format.custom_format = None;
        self
    }

    /// Whether or not to write the level in the default format.
    pub fn default_format_level(&mut self, write: bool) -> &mut Self {
        self.format.default_format_level = write;
        self
    }

    /// Whether or not to write the module path in the default format.
    pub fn default_format_module_path(&mut self, write: bool) -> &mut Self {
        self.format.default_format_module_path = write;
        self
    }

    /// Whether or not to write the timestamp in the default format.
    pub fn default_format_timestamp(&mut self, write: bool) -> &mut Self {
        self.format.default_format_timestamp = write;
        self
    }

    /// Adds filters to the logger.
    ///
    /// The given module (if any) will log at most the specified level provided.
    /// If no module is provided then the filter will apply to all log messages.
    /// 
    /// # Examples
    /// 
    /// Only include messages for warning and above for logs in `path::to::module`:
    /// 
    /// ```
    /// # extern crate log;
    /// # extern crate env_logger;
    /// # fn main() {
    /// use log::LevelFilter;
    /// use env_logger::Builder;
    /// 
    /// let mut builder = Builder::new();
    /// 
    /// builder.filter(Some("path::to::module"), LevelFilter::Info);
    /// # }
    /// ```
    pub fn filter(&mut self,
                  module: Option<&str>,
                  level: LevelFilter) -> &mut Self {
        self.filter.filter(module, level);
        self
    }

    /// Parses the directives string in the same form as the `RUST_LOG`
    /// environment variable.
    ///
    /// See the module documentation for more details.
    pub fn parse(&mut self, filters: &str) -> &mut Self {
        self.filter.parse(filters);
        self
    }

    /// Sets the target for the log output.
    ///
    /// Env logger can log to either stdout or stderr. The default is stderr.
    /// 
    /// # Examples
    /// 
    /// Write log message to `stdout`:
    /// 
    /// ```
    /// use env_logger::{Builder, Target};
    /// 
    /// let mut builder = Builder::new();
    /// 
    /// builder.target(Target::Stdout);
    /// ```
    pub fn target(&mut self, target: fmt::Target) -> &mut Self {
        self.writer.target(target);
        self
    }

    /// Sets whether or not styles will be written.
    ///
    /// This can be useful in environments that don't support control characters
    /// for setting colors.
    /// 
    /// # Examples
    /// 
    /// Never attempt to write styles:
    /// 
    /// ```
    /// use env_logger::{Builder, WriteStyle};
    /// 
    /// let mut builder = Builder::new();
    /// 
    /// builder.write_style(WriteStyle::Never);
    /// ```
    pub fn write_style(&mut self, write_style: fmt::WriteStyle) -> &mut Self {
        self.writer.write_style(write_style);
        self
    }

    /// Parses whether or not to write styles in the same form as the `RUST_LOG_STYLE`
    /// environment variable.
    ///
    /// See the module documentation for more details.
    pub fn parse_write_style(&mut self, write_style: &str) -> &mut Self {
        self.writer.parse(write_style);
        self
    }

    /// Initializes the global logger with the built env logger.
    ///
    /// This should be called early in the execution of a Rust program. Any log
    /// events that occur before initialization will be ignored.
    ///
    /// # Errors
    ///
    /// This function will fail if it is called more than once, or if another
    /// library has already initialized a global logger.
    pub fn try_init(&mut self) -> Result<(), SetLoggerError> {
        let logger = self.build();

        log::set_max_level(logger.filter());
        log::set_boxed_logger(Box::new(logger))
    }

    /// Initializes the global logger with the built env logger.
    ///
    /// This should be called early in the execution of a Rust program. Any log
    /// events that occur before initialization will be ignored.
    ///
    /// # Panics
    ///
    /// This function will panic if it is called more than once, or if another
    /// library has already initialized a global logger.
    pub fn init(&mut self) {
        self.try_init().expect("Builder::init should not be called after logger initialized");
    }

    /// Build an env logger.
    ///
    /// This method is kept private because the only way we support building
    /// loggers is by installing them as the single global logger for the
    /// `log` crate.
    pub fn build(&mut self) -> Logger {
        Logger {
            writer: self.writer.build(),
            filter: self.filter.build(),
            format: mem::replace(&mut self.format, Default::default()).into_boxed_fn(),
        }
    }
}

impl Logger {
    /// Returns the maximum `LevelFilter` that this env logger instance is
    /// configured to output.
    pub fn filter(&self) -> LevelFilter {
        self.filter.filter()
    }

    /// Checks if this record matches the configured filter.
    pub fn matches(&self, record: &Record) -> bool {
        self.filter.matches(record)
    }
}

impl Log for Logger {
    fn enabled(&self, metadata: &Metadata) -> bool {
        self.filter.enabled(metadata)
    }

    fn log(&self, record: &Record) {
        if self.matches(record) {
            // Log records are written to a thread-local buffer before being printed
            // to the terminal. We clear these buffers afterwards, but they aren't shrinked
            // so will always at least have capacity for the largest log record formatted
            // on that thread.
            //
            // If multiple `Logger`s are used by the same threads then the thread-local
            // formatter might have different color support. If this is the case the
            // formatter and its buffer are discarded and recreated.

            thread_local! {
                static FORMATTER: RefCell<Option<Formatter>> = RefCell::new(None);
            }

            FORMATTER.with(|tl_buf| {
                // It's possible for implementations to sometimes
                // log-while-logging (e.g. a `std::fmt` implementation logs
                // internally) but it's super rare. If this happens make sure we
                // at least don't panic and ship some output to the screen.
                let mut a;
                let mut b = None;
                let tl_buf = match tl_buf.try_borrow_mut() {
                    Ok(f) => {
                        a = f;
                        &mut *a
                    }
                    Err(_) => &mut b,
                };

                // Check the buffer style. If it's different from the logger's
                // style then drop the buffer and recreate it.
                match *tl_buf {
                    Some(ref mut formatter) => {
                        if formatter.write_style() != self.writer.write_style() {
                            *formatter = Formatter::new(&self.writer)
                        }
                    },
                    ref mut tl_buf => *tl_buf = Some(Formatter::new(&self.writer))
                }

                // The format is guaranteed to be `Some` by this point
                let mut formatter = tl_buf.as_mut().unwrap();

                let _ = (self.format)(&mut formatter, record).and_then(|_| formatter.print(&self.writer));

                // Always clear the buffer afterwards
                formatter.clear();
            });
        }
    }

    fn flush(&self) {}
}

impl<'a> Env<'a> {
    /// Get a default set of environment variables.
    pub fn new() -> Self {
        Self::default()
    }

    /// Specify an environment variable to read the filter from.
    pub fn filter<E>(mut self, filter_env: E) -> Self
    where
        E: Into<Cow<'a, str>>
    {
        self.filter = filter_env.into();
        self
    }

    fn get_filter(&self) -> Option<String> {
        env::var(&*self.filter).ok()
    }

    /// Specify an environment variable to read the style from.
    pub fn write_style<E>(mut self, write_style_env: E) -> Self
    where
        E: Into<Cow<'a, str>>
    {
        self.write_style = write_style_env.into();
        self
    }

    fn get_write_style(&self) -> Option<String> {
        env::var(&*self.write_style).ok()
    }
}

impl<'a, T> From<T> for Env<'a>
where
    T: Into<Cow<'a, str>>
{
    fn from(filter_env: T) -> Self {
        Env::default().filter(filter_env.into())
    }
}

impl<'a> Default for Env<'a> {
    fn default() -> Self {
        Env {
            filter: DEFAULT_FILTER_ENV.into(),
            write_style: DEFAULT_WRITE_STYLE_ENV.into()
        }
    }
}

mod std_fmt_impls {
    use std::fmt;
    use super::*;

    impl fmt::Debug for Logger{
        fn fmt(&self, f: &mut fmt::Formatter)->fmt::Result {
            f.debug_struct("Logger")
                .field("filter", &self.filter)
                .finish()
        }
    }

    impl fmt::Debug for Builder{
        fn fmt(&self, f: &mut fmt::Formatter)->fmt::Result {
            f.debug_struct("Logger")
            .field("filter", &self.filter)
            .field("writer", &self.writer)
            .finish()
        }
    }
}

/// Attempts to initialize the global logger with an env logger.
///
/// This should be called early in the execution of a Rust program. Any log
/// events that occur before initialization will be ignored.
///
/// # Errors
///
/// This function will fail if it is called more than once, or if another
/// library has already initialized a global logger.
pub fn try_init() -> Result<(), SetLoggerError> {
    try_init_from_env(Env::default())
}

/// Initializes the global logger with an env logger.
///
/// This should be called early in the execution of a Rust program. Any log
/// events that occur before initialization will be ignored.
///
/// # Panics
///
/// This function will panic if it is called more than once, or if another
/// library has already initialized a global logger.
pub fn init() {
    try_init().expect("env_logger::init should not be called after logger initialized");
}

/// Attempts to initialize the global logger with an env logger from the given
/// environment variables.
///
/// This should be called early in the execution of a Rust program. Any log
/// events that occur before initialization will be ignored.
///
/// # Examples
///
/// Initialise a logger using the `MY_LOG` environment variable for filters
/// and `MY_LOG_STYLE` for writing colors:
///
/// ```
/// # extern crate env_logger;
/// use env_logger::{Builder, Env};
///
/// # fn run() -> Result<(), Box<::std::error::Error>> {
/// let env = Env::new().filter("MY_LOG").write_style("MY_LOG_STYLE");
///
/// env_logger::try_init_from_env(env)?;
///
/// Ok(())
/// # }
/// # fn main() { run().unwrap(); }
/// ```
///
/// # Errors
///
/// This function will fail if it is called more than once, or if another
/// library has already initialized a global logger.
pub fn try_init_from_env<'a, E>(env: E) -> Result<(), SetLoggerError>
where
    E: Into<Env<'a>>
{
    let mut builder = Builder::from_env(env);

    builder.try_init()
}

/// Initializes the global logger with an env logger from the given environment
/// variables.
///
/// This should be called early in the execution of a Rust program. Any log
/// events that occur before initialization will be ignored.
///
/// # Examples
///
/// Initialise a logger using the `MY_LOG` environment variable for filters
/// and `MY_LOG_STYLE` for writing colors:
///
/// ```
/// use env_logger::{Builder, Env};
///
/// let env = Env::new().filter("MY_LOG").write_style("MY_LOG_STYLE");
///
/// env_logger::init_from_env(env);
/// ```
///
/// # Panics
///
/// This function will panic if it is called more than once, or if another
/// library has already initialized a global logger.
pub fn init_from_env<'a, E>(env: E)
where
    E: Into<Env<'a>>
{
    try_init_from_env(env).expect("env_logger::init_from_env should not be called after logger initialized");
}
