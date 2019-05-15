// Copyright 2018 Developers of the Rand project.
// Copyright 2013-2015 The Rust Project Developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use rand_core::{Error, ErrorKind};
use core::fmt;

/// An error that can occur when [`JitterRng::test_timer`] fails.
///
/// [`JitterRng::test_timer`]: crate::JitterRng::test_timer
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum TimerError {
    /// No timer available.
    NoTimer,
    /// Timer too coarse to use as an entropy source.
    CoarseTimer,
    /// Timer is not monotonically increasing.
    NotMonotonic,
    /// Variations of deltas of time too small.
    TinyVariantions,
    /// Too many stuck results (indicating no added entropy).
    TooManyStuck,
    #[doc(hidden)]
    __Nonexhaustive,
}

impl TimerError {
    fn description(&self) -> &'static str {
        match *self {
            TimerError::NoTimer => "no timer available",
            TimerError::CoarseTimer => "coarse timer",
            TimerError::NotMonotonic => "timer not monotonic",
            TimerError::TinyVariantions => "time delta variations too small",
            TimerError::TooManyStuck => "too many stuck results",
            TimerError::__Nonexhaustive => unreachable!(),
        }
    }
}

impl fmt::Display for TimerError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.description())
    }
}

#[cfg(feature = "std")]
impl ::std::error::Error for TimerError {
    fn description(&self) -> &str {
        self.description()
    }
}

impl From<TimerError> for Error {
    fn from(err: TimerError) -> Error {
        // Timer check is already quite permissive of failures so we don't
        // expect false-positive failures, i.e. any error is irrecoverable.
        #[cfg(feature = "std")] {
            Error::with_cause(ErrorKind::Unavailable, "timer jitter failed basic quality tests", err)
        }
        #[cfg(not(feature = "std"))] {
            Error::new(ErrorKind::Unavailable, "timer jitter failed basic quality tests")
        }
    }
}

