// Copyright 2018-2019 Mozilla

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

use std::fmt::Arguments;

use log::{Level, LevelFilter, Log};

use crate::error::{ErrorKind, Result};
use crate::guid::Guid;

/// An abort signal is used to abort merging. Implementations of `AbortSignal`
/// can store an aborted flag, usually as an atomic integer or Boolean, set
/// the flag on abort, and have `AbortSignal::aborted` return the flag's value.
///
/// Since merging is synchronous, it's not possible to interrupt a merge from
/// the same thread that started it. In practice, this means a signal will
/// implement `Send` and `Sync`, too, so that another thread can set the
/// aborted flag.
///
/// The name comes from the `AbortSignal` DOM API.
pub trait AbortSignal {
    /// Indicates if the caller signaled to abort.
    fn aborted(&self) -> bool;

    /// Returns an error if the caller signaled to abort. This helper makes it
    /// easier to use the signal with the `?` operator.
    fn err_if_aborted(&self) -> Result<()> {
        if self.aborted() {
            Err(ErrorKind::Abort.into())
        } else {
            Ok(())
        }
    }
}

/// A default signal that can't be aborted.
pub struct DefaultAbortSignal;

impl AbortSignal for DefaultAbortSignal {
    fn aborted(&self) -> bool {
        false
    }
}

/// A merge driver provides methods to customize merging behavior.
pub trait Driver {
    /// Generates a new GUID for the given invalid GUID. This is used to fix up
    /// items with GUIDs that Places can't store (bug 1380606, bug 1313026).
    ///
    /// The default implementation returns an error, forbidding invalid GUIDs.
    ///
    /// Implementations of `Driver` can either use the `rand` and `base64`
    /// crates to generate a new, random GUID (9 bytes, Base64url-encoded
    /// without padding), or use an existing method like Desktop's
    /// `nsINavHistoryService::MakeGuid`. Dogear doesn't generate new GUIDs
    /// automatically to avoid depending on those crates.
    ///
    /// Implementations can also return `Ok(invalid_guid.clone())` to pass
    /// through all invalid GUIDs, as the tests do.
    fn generate_new_guid(&self, invalid_guid: &Guid) -> Result<Guid> {
        Err(ErrorKind::InvalidGuid(invalid_guid.clone()).into())
    }

    /// Returns the maximum log level for merge messages. The default
    /// implementation returns the `log` crate's global maximum level.
    fn max_log_level(&self) -> LevelFilter {
        log::max_level()
    }

    /// Returns a logger for merge messages.
    ///
    /// The default implementation returns the `log` crate's global logger.
    ///
    /// Implementations can override this method to return a custom logger,
    /// where using the global logger won't work. For example, Firefox Desktop
    /// has an existing Sync logging setup outside of the `log` crate.
    fn logger(&self) -> &dyn Log {
        log::logger()
    }
}

/// A default implementation of the merge driver.
pub struct DefaultDriver;

impl Driver for DefaultDriver {}

/// Logs a merge message.
pub fn log<D: Driver>(
    driver: &D,
    level: Level,
    args: Arguments<'_>,
    module_path: &'static str,
    file: &'static str,
    line: u32,
) {
    let meta = log::Metadata::builder()
        .level(level)
        .target(module_path)
        .build();
    if driver.logger().enabled(&meta) {
        driver.logger().log(
            &log::Record::builder()
                .args(args)
                .metadata(meta)
                .module_path(Some(module_path))
                .file(Some(file))
                .line(Some(line))
                .build(),
        );
    }
}

#[macro_export]
macro_rules! error {
    ($driver:expr, $($args:tt)+) => {
        if log::Level::Error <= $driver.max_log_level() {
            $crate::driver::log(
                $driver,
                log::Level::Error,
                format_args!($($args)+),
                module_path!(),
                file!(),
                line!(),
            );
        }
    }
}

macro_rules! warn {
    ($driver:expr, $($args:tt)+) => {
        if log::Level::Warn <= $driver.max_log_level() {
            $crate::driver::log(
                $driver,
                log::Level::Warn,
                format_args!($($args)+),
                module_path!(),
                file!(),
                line!(),
            );
        }
    }
}

#[macro_export]
macro_rules! debug {
    ($driver:expr, $($args:tt)+) => {
        if log::Level::Debug <= $driver.max_log_level() {
            $crate::driver::log(
                $driver,
                log::Level::Debug,
                format_args!($($args)+),
                module_path!(),
                file!(),
                line!(),
            );
        }
    }
}

#[macro_export]
macro_rules! trace {
    ($driver:expr, $($args:tt)+) => {
        if log::Level::Trace <= $driver.max_log_level() {
            $crate::driver::log(
                $driver,
                log::Level::Trace,
                format_args!($($args)+),
                module_path!(),
                file!(),
                line!(),
            );
        }
    }
}
