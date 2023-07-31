// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{
    cell::RefCell,
    convert::TryFrom,
    rc::{Rc, Weak},
    time::Duration,
};

#[cfg(windows)]
use winapi::shared::minwindef::UINT;
#[cfg(windows)]
use winapi::um::timeapi::{timeBeginPeriod, timeEndPeriod};

/// A quantized `Duration`.  This currently just produces 16 discrete values
/// corresponding to whole milliseconds.  Future implementations might choose
/// a different allocation, such as a logarithmic scale.
#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
struct Period(u8);

impl Period {
    const MAX: Period = Period(16);
    const MIN: Period = Period(1);

    #[cfg(windows)]
    fn as_uint(&self) -> UINT {
        UINT::from(self.0)
    }

    #[cfg(target_os = "macos")]
    fn scaled(&self, scale: f64) -> f64 {
        scale * f64::from(self.0)
    }
}

impl From<Duration> for Period {
    fn from(p: Duration) -> Self {
        let rounded = u8::try_from(p.as_millis()).unwrap_or(Self::MAX.0);
        Self(rounded.clamp(Self::MIN.0, Self::MAX.0))
    }
}

/// This counts instances of `Period`, except those of `Period::MAX`.
#[derive(Default)]
struct PeriodSet {
    counts: [usize; (Period::MAX.0 - Period::MIN.0) as usize],
}

impl PeriodSet {
    fn idx(&mut self, p: Period) -> &mut usize {
        debug_assert!(p >= Period::MIN);
        &mut self.counts[usize::from(p.0 - Period::MIN.0)]
    }

    fn add(&mut self, p: Period) {
        if p != Period::MAX {
            *self.idx(p) += 1;
        }
    }

    fn remove(&mut self, p: Period) {
        if p != Period::MAX {
            debug_assert_ne!(*self.idx(p), 0);
            *self.idx(p) -= 1;
        }
    }

    fn min(&self) -> Option<Period> {
        for (i, v) in self.counts.iter().enumerate() {
            if *v > 0 {
                return Some(Period(u8::try_from(i).unwrap() + Period::MIN.0));
            }
        }
        None
    }
}

#[cfg(target_os = "macos")]
#[allow(non_camel_case_types)]
mod mac {
    use std::{mem::size_of, ptr::addr_of_mut};

    // These are manually extracted from the many bindings generated
    // by bindgen when provided with the simple header:
    // #include <mach/mach_init.h>
    // #include <mach/mach_time.h>
    // #include <mach/thread_policy.h>
    // #include <pthread.h>

    type __darwin_natural_t = ::std::os::raw::c_uint;
    type __darwin_mach_port_name_t = __darwin_natural_t;
    type __darwin_mach_port_t = __darwin_mach_port_name_t;
    type mach_port_t = __darwin_mach_port_t;
    type thread_t = mach_port_t;
    type natural_t = __darwin_natural_t;
    type thread_policy_flavor_t = natural_t;
    type integer_t = ::std::os::raw::c_int;
    type thread_policy_t = *mut integer_t;
    type mach_msg_type_number_t = natural_t;
    type boolean_t = ::std::os::raw::c_uint;
    type kern_return_t = ::std::os::raw::c_int;

    #[repr(C)]
    #[derive(Debug, Copy, Clone, Default)]
    struct mach_timebase_info {
        numer: u32,
        denom: u32,
    }
    type mach_timebase_info_t = *mut mach_timebase_info;
    type mach_timebase_info_data_t = mach_timebase_info;
    extern "C" {
        fn mach_timebase_info(info: mach_timebase_info_t) -> kern_return_t;
    }

    #[repr(C)]
    #[derive(Debug, Copy, Clone, Default)]
    pub struct thread_time_constraint_policy {
        period: u32,
        computation: u32,
        constraint: u32,
        preemptible: boolean_t,
    }

    const THREAD_TIME_CONSTRAINT_POLICY: thread_policy_flavor_t = 2;
    const THREAD_TIME_CONSTRAINT_POLICY_COUNT: mach_msg_type_number_t =
        (size_of::<thread_time_constraint_policy>() / size_of::<integer_t>())
            as mach_msg_type_number_t;

    // These function definitions are taken from a comment in <thread_policy.h>.
    // Why they are inaccessible is unknown, but they work as declared.
    extern "C" {
        fn thread_policy_set(
            thread: thread_t,
            flavor: thread_policy_flavor_t,
            policy_info: thread_policy_t,
            count: mach_msg_type_number_t,
        ) -> kern_return_t;
        fn thread_policy_get(
            thread: thread_t,
            flavor: thread_policy_flavor_t,
            policy_info: thread_policy_t,
            count: *mut mach_msg_type_number_t,
            get_default: *mut boolean_t,
        ) -> kern_return_t;
    }

    enum _opaque_pthread_t {} // An opaque type is fine here.
    type __darwin_pthread_t = *mut _opaque_pthread_t;
    type pthread_t = __darwin_pthread_t;

    extern "C" {
        fn pthread_self() -> pthread_t;
        fn pthread_mach_thread_np(thread: pthread_t) -> mach_port_t;
    }

    /// Set a thread time policy.
    pub fn set_thread_policy(mut policy: thread_time_constraint_policy) {
        _ = unsafe {
            thread_policy_set(
                pthread_mach_thread_np(pthread_self()),
                THREAD_TIME_CONSTRAINT_POLICY,
                addr_of_mut!(policy) as _, // horror!
                THREAD_TIME_CONSTRAINT_POLICY_COUNT,
            )
        };
    }

    pub fn get_scale() -> f64 {
        const NANOS_PER_MSEC: f64 = 1_000_000.0;
        let mut timebase_info = mach_timebase_info_data_t::default();
        unsafe {
            mach_timebase_info(&mut timebase_info);
        }
        f64::from(timebase_info.denom) * NANOS_PER_MSEC / f64::from(timebase_info.numer)
    }

    /// Create a realtime policy and set it.
    pub fn set_realtime(base: f64) {
        let policy = thread_time_constraint_policy {
            period: base as u32, // Base interval
            computation: (base * 0.5) as u32,
            constraint: (base * 1.0) as u32,
            preemptible: 1,
        };
        set_thread_policy(policy);
    }

    /// Get the default policy.
    pub fn get_default_policy() -> thread_time_constraint_policy {
        let mut policy = thread_time_constraint_policy::default();
        let mut count = THREAD_TIME_CONSTRAINT_POLICY_COUNT;
        let mut get_default = 0;
        _ = unsafe {
            thread_policy_get(
                pthread_mach_thread_np(pthread_self()),
                THREAD_TIME_CONSTRAINT_POLICY,
                addr_of_mut!(policy) as _, // horror!
                &mut count,
                &mut get_default,
            )
        };
        policy
    }
}

/// A handle for a high-resolution timer of a specific period.
pub struct Handle {
    hrt: Rc<RefCell<Time>>,
    active: Period,
    hysteresis: [Period; Self::HISTORY],
    hysteresis_index: usize,
}

impl Handle {
    const HISTORY: usize = 8;

    fn new(hrt: Rc<RefCell<Time>>, active: Period) -> Self {
        Self {
            hrt,
            active,
            hysteresis: [Period::MAX; Self::HISTORY],
            hysteresis_index: 0,
        }
    }

    /// Update shortcut.  Equivalent to dropping the current reference and
    /// calling `HrTime::get` again with the new period, except that this applies
    /// a little hysteresis that smoothes out fluctuations.
    pub fn update(&mut self, period: Duration) {
        self.hysteresis[self.hysteresis_index] = Period::from(period);
        self.hysteresis_index += 1;
        self.hysteresis_index %= self.hysteresis.len();

        let mut first = Period::MAX;
        let mut second = Period::MAX;
        for i in &self.hysteresis {
            if *i < first {
                second = first;
                first = *i;
            } else if *i < second {
                second = *i;
            }
        }

        if second != self.active {
            let mut b = self.hrt.borrow_mut();
            b.periods.remove(self.active);
            self.active = second;
            b.periods.add(self.active);
            b.update();
        }
    }
}

impl Drop for Handle {
    fn drop(&mut self) {
        self.hrt.borrow_mut().remove(self.active);
    }
}

/// Holding an instance of this indicates that high resolution timers are enabled.
pub struct Time {
    periods: PeriodSet,
    active: Option<Period>,

    #[cfg(target_os = "macos")]
    scale: f64,
    #[cfg(target_os = "macos")]
    deflt: mac::thread_time_constraint_policy,
}
impl Time {
    fn new() -> Self {
        Self {
            periods: PeriodSet::default(),
            active: None,

            #[cfg(target_os = "macos")]
            scale: mac::get_scale(),
            #[cfg(target_os = "macos")]
            deflt: mac::get_default_policy(),
        }
    }

    #[allow(clippy::unused_self)] // Only on some platforms is it unused.
    fn start(&self) {
        #[cfg(target_os = "macos")]
        {
            if let Some(p) = self.active {
                mac::set_realtime(p.scaled(self.scale));
            } else {
                mac::set_thread_policy(self.deflt.clone());
            }
        }

        #[cfg(windows)]
        {
            if let Some(p) = self.active {
                assert_eq!(0, unsafe { timeBeginPeriod(p.as_uint()) });
            }
        }
    }

    #[allow(clippy::unused_self)] // Only on some platforms is it unused.
    fn stop(&self) {
        #[cfg(windows)]
        {
            if let Some(p) = self.active {
                assert_eq!(0, unsafe { timeEndPeriod(p.as_uint()) });
            }
        }
    }

    fn update(&mut self) {
        let next = self.periods.min();
        if next != self.active {
            self.stop();
            self.active = next;
            self.start();
        }
    }

    fn add(&mut self, p: Period) {
        self.periods.add(p);
        self.update();
    }

    fn remove(&mut self, p: Period) {
        self.periods.remove(p);
        self.update();
    }

    /// Enable high resolution time.  Returns a thread-bound handle that
    /// needs to be held until the high resolution time is no longer needed.
    /// The handle can also be used to update the resolution.
    #[must_use]
    pub fn get(period: Duration) -> Handle {
        thread_local! {
            static HR_TIME: RefCell<Weak<RefCell<Time>>> = RefCell::default();
        }

        HR_TIME.with(|r| {
            let mut b = r.borrow_mut();
            let hrt = b.upgrade().unwrap_or_else(|| {
                let hrt = Rc::new(RefCell::new(Time::new()));
                *b = Rc::downgrade(&hrt);
                hrt
            });

            let p = Period::from(period);
            hrt.borrow_mut().add(p);
            Handle::new(hrt, p)
        })
    }
}

impl Drop for Time {
    fn drop(&mut self) {
        self.stop();

        #[cfg(target_os = "macos")]
        {
            if self.active.is_some() {
                mac::set_thread_policy(self.deflt);
            }
        }
    }
}

#[cfg(test)]
mod test {
    use super::Time;
    use std::{
        thread::{sleep, spawn},
        time::{Duration, Instant},
    };

    const ONE: Duration = Duration::from_millis(1);
    const ONE_AND_A_BIT: Duration = Duration::from_micros(1500);
    /// A limit for when high resolution timers are disabled.
    const GENEROUS: Duration = Duration::from_millis(30);

    fn validate_delays(max_lag: Duration) -> Result<(), ()> {
        const DELAYS: &[u64] = &[1, 2, 3, 5, 8, 10, 12, 15, 20, 25, 30];
        let durations = DELAYS.iter().map(|&d| Duration::from_millis(d));

        let mut s = Instant::now();
        for d in durations {
            sleep(d);
            let e = Instant::now();
            let actual = e - s;
            let lag = actual - d;
            println!("sleep({d:?}) \u{2192} {actual:?} \u{394}{lag:?}");
            if lag > max_lag {
                return Err(());
            }
            s = Instant::now();
        }
        Ok(())
    }

    /// Validate the delays twice.  Sometimes the first run can stall.
    /// Reliability in CI is more important than reliable timers.
    fn check_delays(max_lag: Duration) {
        if validate_delays(max_lag).is_err() {
            sleep(Duration::from_millis(50));
            validate_delays(max_lag).unwrap();
        }
    }

    /// Note that you have to run this test alone or other tests will
    /// grab the high resolution timer and this will run faster.
    #[test]
    fn baseline() {
        check_delays(GENEROUS);
    }

    #[test]
    fn one_ms() {
        let _hrt = Time::get(ONE);
        check_delays(ONE_AND_A_BIT);
    }

    #[test]
    fn multithread_baseline() {
        let thr = spawn(move || {
            baseline();
        });
        baseline();
        thr.join().unwrap();
    }

    #[test]
    fn one_ms_multi() {
        let thr = spawn(move || {
            one_ms();
        });
        one_ms();
        thr.join().unwrap();
    }

    #[test]
    fn mixed_multi() {
        let thr = spawn(move || {
            one_ms();
        });
        let _hrt = Time::get(Duration::from_millis(4));
        check_delays(Duration::from_millis(5));
        thr.join().unwrap();
    }

    #[test]
    fn update() {
        let mut hrt = Time::get(Duration::from_millis(4));
        check_delays(Duration::from_millis(5));
        hrt.update(ONE);
        check_delays(ONE_AND_A_BIT);
    }

    #[test]
    fn update_multi() {
        let thr = spawn(move || {
            update();
        });
        update();
        thr.join().unwrap();
    }

    #[test]
    fn max() {
        let _hrt = Time::get(Duration::from_secs(1));
        check_delays(GENEROUS);
    }
}
