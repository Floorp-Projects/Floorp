// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use ffi;

/// Level (verbosity) of logging for a particular cubeb context.
#[derive(PartialEq, Eq, Clone, Debug, Copy, PartialOrd, Ord)]
pub enum LogLevel {
    /// Logging disabled
    Disabled,
    /// Logging lifetime operation (creation/destruction).
    Normal,
    /// Verbose logging of callbacks, can have performance implications.
    Verbose,
}

impl From<ffi::cubeb_log_level> for LogLevel {
    fn from(x: ffi::cubeb_log_level) -> Self {
        use LogLevel::*;
        match x {
            ffi::CUBEB_LOG_NORMAL => Normal,
            ffi::CUBEB_LOG_VERBOSE => Verbose,
            _ => Disabled,
        }
    }
}

pub fn log_enabled() -> bool {
    unsafe { ffi::cubeb_log_get_level() != LogLevel::Disabled as _ }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_logging_disabled_by_default() {
        assert!(!log_enabled());
    }
}
