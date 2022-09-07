// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::upper_case_acronyms)]

use crate::agentio::as_c_void;
use crate::err::{Error, Res};
use crate::once::OnceResult;
use crate::ssl::{PRFileDesc, SSLTimeFunc};

use std::boxed::Box;
use std::convert::{TryFrom, TryInto};
use std::ops::Deref;
use std::os::raw::c_void;
use std::pin::Pin;
use std::time::{Duration, Instant};

include!(concat!(env!("OUT_DIR"), "/nspr_time.rs"));

experimental_api!(SSL_SetTimeFunc(
    fd: *mut PRFileDesc,
    cb: SSLTimeFunc,
    arg: *mut c_void,
));

/// This struct holds the zero time used for converting between `Instant` and `PRTime`.
#[derive(Debug)]
struct TimeZero {
    instant: Instant,
    prtime: PRTime,
}

impl TimeZero {
    /// This function sets a baseline from an instance of `Instant`.
    /// This allows for the possibility that code that uses these APIs will create
    /// instances of `Instant` before any of this code is run.  If `Instant`s older than
    /// `BASE_TIME` are used with these conversion functions, they will fail.
    /// To avoid that, we make sure that this sets the base time using the first value
    /// it sees if it is in the past.  If it is not, then use `Instant::now()` instead.
    pub fn baseline(t: Instant) -> Self {
        let now = Instant::now();
        let prnow = unsafe { PR_Now() };

        if now <= t {
            // `t` is in the future, just use `now`.
            Self {
                instant: now,
                prtime: prnow,
            }
        } else {
            let elapsed = Interval::from(now.duration_since(now));
            // An error from these unwrap functions would require
            // ridiculously long application running time.
            let prelapsed: PRTime = elapsed.try_into().unwrap();
            Self {
                instant: t,
                prtime: prnow.checked_sub(prelapsed).unwrap(),
            }
        }
    }
}

static mut BASE_TIME: OnceResult<TimeZero> = OnceResult::new();

fn get_base() -> &'static TimeZero {
    let f = || TimeZero {
        instant: Instant::now(),
        prtime: unsafe { PR_Now() },
    };
    unsafe { BASE_TIME.call_once(f) }
}

pub(crate) fn init() {
    let _ = get_base();
}

/// Time wraps Instant and provides conversion functions into `PRTime`.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Time {
    t: Instant,
}

impl Deref for Time {
    type Target = Instant;
    fn deref(&self) -> &Self::Target {
        &self.t
    }
}

impl From<Instant> for Time {
    /// Convert from an Instant into a Time.
    fn from(t: Instant) -> Self {
        // Call `TimeZero::baseline(t)` so that time zero can be set.
        let f = || TimeZero::baseline(t);
        let _ = unsafe { BASE_TIME.call_once(f) };
        Self { t }
    }
}

impl TryFrom<PRTime> for Time {
    type Error = Error;
    fn try_from(prtime: PRTime) -> Res<Self> {
        let base = get_base();
        if let Some(delta) = prtime.checked_sub(base.prtime) {
            let d = Duration::from_micros(delta.try_into()?);
            base.instant
                .checked_add(d)
                .map_or(Err(Error::TimeTravelError), |t| Ok(Self { t }))
        } else {
            Err(Error::TimeTravelError)
        }
    }
}

impl TryInto<PRTime> for Time {
    type Error = Error;
    fn try_into(self) -> Res<PRTime> {
        let base = get_base();
        let delta = self
            .t
            .checked_duration_since(base.instant)
            .ok_or(Error::TimeTravelError)?;
        if let Ok(d) = PRTime::try_from(delta.as_micros()) {
            d.checked_add(base.prtime).ok_or(Error::TimeTravelError)
        } else {
            Err(Error::TimeTravelError)
        }
    }
}

impl From<Time> for Instant {
    #[must_use]
    fn from(t: Time) -> Self {
        t.t
    }
}

/// Interval wraps Duration and provides conversion functions into `PRTime`.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Interval {
    d: Duration,
}

impl Deref for Interval {
    type Target = Duration;
    fn deref(&self) -> &Self::Target {
        &self.d
    }
}

impl TryFrom<PRTime> for Interval {
    type Error = Error;
    fn try_from(prtime: PRTime) -> Res<Self> {
        Ok(Self {
            d: Duration::from_micros(u64::try_from(prtime)?),
        })
    }
}

impl From<Duration> for Interval {
    fn from(d: Duration) -> Self {
        Self { d }
    }
}

impl TryInto<PRTime> for Interval {
    type Error = Error;
    fn try_into(self) -> Res<PRTime> {
        Ok(PRTime::try_from(self.d.as_micros())?)
    }
}

/// `TimeHolder` maintains a `PRTime` value in a form that is accessible to the TLS stack.
#[derive(Debug)]
pub struct TimeHolder {
    t: Pin<Box<PRTime>>,
}

impl TimeHolder {
    unsafe extern "C" fn time_func(arg: *mut c_void) -> PRTime {
        let p = arg as *const PRTime;
        *p.as_ref().unwrap()
    }

    pub fn bind(&mut self, fd: *mut PRFileDesc) -> Res<()> {
        unsafe { SSL_SetTimeFunc(fd, Some(Self::time_func), as_c_void(&mut self.t)) }
    }

    pub fn set(&mut self, t: Instant) -> Res<()> {
        *self.t = Time::from(t).try_into()?;
        Ok(())
    }
}

impl Default for TimeHolder {
    fn default() -> Self {
        TimeHolder { t: Box::pin(0) }
    }
}

#[cfg(test)]
mod test {
    use super::{get_base, init, Interval, PRTime, Time};
    use crate::err::Res;
    use std::convert::{TryFrom, TryInto};
    use std::time::{Duration, Instant};

    #[test]
    fn convert_stable() {
        init();
        let now = Time::from(Instant::now());
        let pr: PRTime = now.try_into().expect("convert to PRTime with truncation");
        let t2 = Time::try_from(pr).expect("convert to Instant");
        let pr2: PRTime = t2.try_into().expect("convert to PRTime again");
        assert_eq!(pr, pr2);
        let t3 = Time::try_from(pr2).expect("convert to Instant again");
        assert_eq!(t2, t3);
    }

    #[test]
    fn past_time() {
        init();
        let base = get_base();
        assert!(Time::try_from(base.prtime - 1).is_err());
    }

    #[test]
    fn negative_time() {
        init();
        assert!(Time::try_from(-1).is_err());
    }

    #[test]
    fn negative_interval() {
        init();
        assert!(Interval::try_from(-1).is_err());
    }

    #[test]
    // We allow replace_consts here because
    // std::u64::max_value() isn't available
    // in all of our targets
    fn overflow_interval() {
        init();
        let interval = Interval::from(Duration::from_micros(u64::max_value()));
        let res: Res<PRTime> = interval.try_into();
        assert!(res.is_err());
    }
}
