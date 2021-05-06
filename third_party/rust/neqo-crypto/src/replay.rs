// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::err::{Error, Res};
use crate::ssl::PRFileDesc;
use crate::time::{Interval, PRTime, Time};

use std::convert::{TryFrom, TryInto};
use std::ops::{Deref, DerefMut};
use std::os::raw::c_uint;
use std::ptr::{null_mut, NonNull};
use std::time::{Duration, Instant};

// This is an opaque struct in NSS.
#[allow(clippy::empty_enum, clippy::upper_case_acronyms)]
#[allow(unknown_lints, renamed_and_removed_lints, clippy::unknown_clippy_lints)] // Until we require rust 1.51.
pub enum SSLAntiReplayContext {}

experimental_api!(SSL_CreateAntiReplayContext(
    now: PRTime,
    window: PRTime,
    k: c_uint,
    bits: c_uint,
    ctx: *mut *mut SSLAntiReplayContext,
));
experimental_api!(SSL_ReleaseAntiReplayContext(ctx: *mut SSLAntiReplayContext));
experimental_api!(SSL_SetAntiReplayContext(
    fd: *mut PRFileDesc,
    ctx: *mut SSLAntiReplayContext,
));

scoped_ptr!(
    AntiReplayContext,
    SSLAntiReplayContext,
    SSL_ReleaseAntiReplayContext
);

/// `AntiReplay` is used by servers when processing 0-RTT handshakes.
/// It limits the exposure of servers to replay attack by rejecting 0-RTT
/// if it appears to be a replay.  There is a false-positive rate that can be
/// managed by tuning the parameters used to create the context.
#[allow(clippy::module_name_repetitions)]
pub struct AntiReplay {
    ctx: AntiReplayContext,
}

impl AntiReplay {
    /// Make a new anti-replay context.
    /// See the documentation in NSS for advice on how to set these values.
    ///
    /// # Errors
    /// Returns an error if `now` is in the past relative to our baseline or
    /// NSS is unable to generate an anti-replay context.
    pub fn new(now: Instant, window: Duration, k: usize, bits: usize) -> Res<Self> {
        let mut ctx: *mut SSLAntiReplayContext = null_mut();
        unsafe {
            SSL_CreateAntiReplayContext(
                Time::from(now).try_into()?,
                Interval::from(window).try_into()?,
                c_uint::try_from(k)?,
                c_uint::try_from(bits)?,
                &mut ctx,
            )
        }?;

        match NonNull::new(ctx) {
            Some(ctx_nn) => Ok(Self {
                ctx: AntiReplayContext::new(ctx_nn),
            }),
            None => Err(Error::InternalError),
        }
    }

    /// Configure the provided socket with this anti-replay context.
    pub(crate) fn config_socket(&self, fd: *mut PRFileDesc) -> Res<()> {
        unsafe { SSL_SetAntiReplayContext(fd, *self.ctx) }
    }
}
