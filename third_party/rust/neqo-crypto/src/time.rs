// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::upper_case_acronyms)]

use std::{
    ops::Deref,
    os::raw::c_void,
    pin::Pin,
    sync::OnceLock,
    time::{Duration, Instant},
};

use crate::{
    agentio::as_c_void,
    err::{Error, Res},
    ssl::{PRFileDesc, SSLTimeFunc},
};

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

static BASE_TIME: OnceLock<TimeZero> = OnceLock::new();

fn get_base() -> &'static TimeZero {
    BASE_TIME.get_or_init(|| TimeZero {
        instant: Instant::now(),
        prtime: unsafe { PR_Now() },
    })
}

pub(crate) fn init() {
    _ = get_base();
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
        // Initialize `BASE_TIME` using `TimeZero::baseline(t)`.
        BASE_TIME.get_or_init(|| TimeZero::baseline(t));
        Self { t }
    }
}

impl TryFrom<PRTime> for Time {
    type Error = Error;
    fn try_from(prtime: PRTime) -> Res<Self> {
        let base = get_base();
        let delta = prtime
            .checked_sub(base.prtime)
            .ok_or(Error::TimeTravelError)?;
        let d = Duration::from_micros(u64::try_from(delta.abs())?);
        let t = if delta >= 0 {
            base.instant.checked_add(d)
        } else {
            base.instant.checked_sub(d)
        };
        let t = t.ok_or(Error::TimeTravelError)?;
        Ok(Self { t })
    }
}

impl TryInto<PRTime> for Time {
    type Error = Error;
    fn try_into(self) -> Res<PRTime> {
        let base = get_base();

        if let Some(delta) = self.t.checked_duration_since(base.instant) {
            if let Ok(d) = PRTime::try_from(delta.as_micros()) {
                d.checked_add(base.prtime).ok_or(Error::TimeTravelError)
            } else {
                Err(Error::TimeTravelError)
            }
        } else {
            // Try to go backwards from the base time.
            let backwards = base.instant - self.t; // infallible
            if let Ok(d) = PRTime::try_from(backwards.as_micros()) {
                base.prtime.checked_sub(d).ok_or(Error::TimeTravelError)
            } else {
                Err(Error::TimeTravelError)
            }
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
    use std::time::{Duration, Instant};

    use super::{get_base, init, Interval, PRTime, Time};
    use crate::err::Res;

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
    fn past_prtime() {
        const DELTA: Duration = Duration::from_secs(1);
        init();
        let base = get_base();
        let delta_micros = PRTime::try_from(DELTA.as_micros()).unwrap();
        println!("{} - {}", base.prtime, delta_micros);
        let t = Time::try_from(base.prtime - delta_micros).unwrap();
        assert_eq!(Instant::from(t) + DELTA, base.instant);
    }

    #[test]
    fn past_instant() {
        const DELTA: Duration = Duration::from_secs(1);
        init();
        let base = get_base();
        let t = Time::from(base.instant.checked_sub(DELTA).unwrap());
        assert_eq!(Instant::from(t) + DELTA, base.instant);
    }

    #[test]
    fn negative_interval() {
        init();
        assert!(Interval::try_from(-1).is_err());
    }

    #[test]
    // We allow replace_consts here because
    // std::u64::MAX isn't available
    // in all of our targets
    fn overflow_interval() {
        init();
        let interval = Interval::from(Duration::from_micros(u64::MAX));
        let res: Res<PRTime> = interval.try_into();
        assert!(res.is_err());
    }
}
