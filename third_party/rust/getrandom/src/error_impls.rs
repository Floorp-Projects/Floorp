// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
extern crate std;

use std::{io, error};
use core::convert::From;
use core::num::NonZeroU32;
use crate::error::Error;

impl From<io::Error> for Error {
    fn from(err: io::Error) -> Self {
        err.raw_os_error()
            .and_then(|code| NonZeroU32::new(code as u32))
            .map(|code| Error(code))
            // in practice this should never happen
            .unwrap_or(Error::UNKNOWN)
    }
}

impl From<Error> for io::Error {
    fn from(err: Error) -> Self {
        match err.msg() {
            Some(msg) => io::Error::new(io::ErrorKind::Other, msg),
            None => io::Error::from_raw_os_error(err.0.get() as i32),
        }
    }
}

impl error::Error for Error { }
