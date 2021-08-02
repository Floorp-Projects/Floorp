// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! # Metrics Ping Scheduler
//!
//! The Metrics Ping Scheduler (MPS) is responsible for scheduling "metrics" pings.
//! It implements the spec described in
//! [the docs](https://mozilla.github.io/glean/book/user/pings/metrics.html#scheduling)

use crate::metrics::{DatetimeMetric, StringMetric, TimeUnit};
use crate::{local_now_with_offset, CommonMetricData, Glean, Lifetime, INTERNAL_STORAGE};
use chrono::prelude::*;
use chrono::Duration;
use once_cell::sync::Lazy;
use std::sync::{Arc, Condvar, Mutex};
use std::thread::JoinHandle;

const SCHEDULED_HOUR: u32 = 4;

// Clippy thinks an AtomicBool would be preferred, but Condvar requires a full Mutex.
// See https://github.com/rust-lang/rust-clippy/issues/1516
#[allow(clippy::mutex_atomic)]
static TASK_CONDVAR: Lazy<Arc<(Mutex<bool>, Condvar)>> =
    Lazy::new(|| Arc::new((Mutex::new(false), Condvar::new())));

/// Describes the interface for a submitter of "metrics" pings.
/// Used to decouple the implementation so we can test it.
trait MetricsPingSubmitter {
    /// Submits a metrics ping, updating the last sent time to `now`
    /// (which might not be _right now_ due to processing delays (or in tests))
    fn submit_metrics_ping(&self, glean: &Glean, reason: Option<&str>, now: DateTime<FixedOffset>);
}

/// Describes the interface for a scheduler of "metrics" pings.
/// Used to decouple the implementation so we can test it.
trait MetricsPingScheduler {
    /// Begins a recurring schedule of "metrics" ping submissions, on another thread.
    /// `now` is used with `when` to determine the first schedule interval and
    /// may not be _right now_ due to processing delays (or in tests).
    fn start_scheduler(
        &self,
        submitter: impl MetricsPingSubmitter + Send + 'static,
        now: DateTime<FixedOffset>,
        when: When,
    );
}

/// Uses Glean to submit "metrics" pings directly.
struct GleanMetricsPingSubmitter {}
impl MetricsPingSubmitter for GleanMetricsPingSubmitter {
    fn submit_metrics_ping(&self, glean: &Glean, reason: Option<&str>, now: DateTime<FixedOffset>) {
        glean.submit_ping_by_name("metrics", reason);
        // Always update the collection date, irrespective of the ping being sent.
        get_last_sent_time_metric().set(glean, Some(now));
    }
}

/// Schedule "metrics" pings directly using the default behaviour.
struct GleanMetricsPingScheduler {}
impl MetricsPingScheduler for GleanMetricsPingScheduler {
    fn start_scheduler(
        &self,
        submitter: impl MetricsPingSubmitter + Send + 'static,
        now: DateTime<FixedOffset>,
        when: When,
    ) {
        start_scheduler(submitter, now, when);
    }
}

/// Performs startup checks to decide when to schedule the next "metrics" ping collection.
/// **Must** be called before draining the preinit queue.
/// (We're at the Language Bindings' mercy for that)
pub fn schedule(glean: &Glean) {
    let now = local_now_with_offset().0;

    let (cancelled_lock, _condvar) = &**TASK_CONDVAR;
    if *cancelled_lock.lock().unwrap() {
        log::debug!("Told to schedule, but already cancelled. Are we in a test?");
    }
    *cancelled_lock.lock().unwrap() = false; // Uncancel the thread.

    let submitter = GleanMetricsPingSubmitter {};
    let scheduler = GleanMetricsPingScheduler {};

    schedule_internal(glean, submitter, scheduler, now)
}

/// Tells the scheduler task to exit quickly and cleanly.
pub fn cancel() {
    let (cancelled_lock, condvar) = &**TASK_CONDVAR; // One `*` for Lazy, the second for Arc
    *cancelled_lock.lock().unwrap() = true; // Cancel the scheduler thread.
    condvar.notify_all(); // Notify any/all listening schedulers to check whether they were cancelled.
}

fn schedule_internal(
    glean: &Glean,
    submitter: impl MetricsPingSubmitter + Send + 'static,
    scheduler: impl MetricsPingScheduler,
    now: DateTime<FixedOffset>,
) {
    let last_sent_build_metric = get_last_sent_build_metric();
    if let Some(last_sent_build) = last_sent_build_metric.get_value(glean, INTERNAL_STORAGE) {
        // If `app_build` is longer than StringMetric's max length, we will always
        // treat it as a changed build when really it isn't.
        // This will be externally-observable as InvalidOverflow errors on both the core
        // `client_info.app_build` metric and the scheduler's internal metric.
        if last_sent_build != glean.app_build {
            last_sent_build_metric.set(glean, &glean.app_build);
            log::info!("App build changed. Sending 'metrics' ping");
            submitter.submit_metrics_ping(glean, Some("upgrade"), now);
            scheduler.start_scheduler(submitter, now, When::Reschedule);
            return;
        }
    }

    let last_sent_time = get_last_sent_time_metric().get_value(glean, INTERNAL_STORAGE);
    if let Some(last_sent) = last_sent_time {
        log::info!("The 'metrics' ping was last sent on {}", last_sent);
    }

    // We aim to cover 3 cases here:
    //
    // 1. The ping was already collected on the current calendar day;
    //    only schedule one for collection on the next calendar day at the due time.
    // 2. The ping was NOT collected on the current calendar day AND we're later
    //    than today's due time; collect the ping immediately.
    // 3. The ping was NOT collected on the current calendar day BUT we still have
    //    some time to the due time; schedule for submitting the current calendar day.

    let already_sent_today = last_sent_time.map_or(false, |d| d.date() == now.date());
    if already_sent_today {
        // Case #1
        log::info!("The 'metrics' ping was already sent today, {}", now);
        scheduler.start_scheduler(submitter, now, When::Tomorrow);
    } else if now > now.date().and_hms(SCHEDULED_HOUR, 0, 0) {
        // Case #2
        log::info!("Sending the 'metrics' ping immediately, {}", now);
        submitter.submit_metrics_ping(glean, Some("overdue"), now);
        scheduler.start_scheduler(submitter, now, When::Reschedule);
    } else {
        // Case #3
        log::info!("The 'metrics' collection is scheduled for today, {}", now);
        scheduler.start_scheduler(submitter, now, When::Today);
    }
}

/// "metrics" ping scheduling deadlines.
#[derive(Debug, PartialEq)]
enum When {
    Today,
    Tomorrow,
    Reschedule,
}

impl When {
    /// Returns the duration from now until our deadline.
    /// Note that std::time::Duration doesn't do negative time spans, so if
    /// our deadline has passed, this will return zero.
    fn until(&self, now: DateTime<FixedOffset>) -> std::time::Duration {
        let fire_date = match self {
            Self::Today => now.date().and_hms(SCHEDULED_HOUR, 0, 0),
            // Doesn't actually save us from being an hour off on DST because
            // chrono doesn't know when DST changes. : (
            Self::Tomorrow | Self::Reschedule => {
                (now.date() + Duration::days(1)).and_hms(SCHEDULED_HOUR, 0, 0)
            }
        };
        // After rust-lang/rust#73544 can use std::time::Duration::ZERO
        (fire_date - now)
            .to_std()
            .unwrap_or_else(|_| std::time::Duration::from_millis(0))
    }

    /// The "metrics" ping reason corresponding to our deadline.
    fn reason(&self) -> &'static str {
        match self {
            Self::Today => "today",
            Self::Tomorrow => "tomorrow",
            Self::Reschedule => "reschedule",
        }
    }
}

fn start_scheduler(
    submitter: impl MetricsPingSubmitter + Send + 'static,
    now: DateTime<FixedOffset>,
    when: When,
) -> JoinHandle<()> {
    let pair = Arc::clone(&TASK_CONDVAR);
    std::thread::Builder::new()
        .name("glean.mps".into())
        .spawn(move || {
            let (cancelled_lock, condvar) = &*pair;
            let mut when = when;
            let mut now = now;
            loop {
                let dur = when.until(now);
                log::info!("Scheduling for {:?} after {}, reason {:?}", dur, now, when);
                let mut timed_out = false;
                {
                    match condvar.wait_timeout_while(cancelled_lock.lock().unwrap(), dur, |cancelled| !*cancelled) {
                        Err(err) => {
                            log::warn!("Condvar wait failure. MPS exiting. {}", err);
                            break;
                        }
                        Ok((cancelled, wait_result)) => {
                            if *cancelled {
                                log::info!("Metrics Ping Scheduler cancelled. Exiting.");
                                break;
                            } else if wait_result.timed_out() {
                                // Can't get the global glean while holding cancelled's lock.
                                timed_out = true;
                            } else {
                                // This should be impossible. `cancelled_lock` is acquired, and
                                // `!*cancelled` is checked by the condvar before it is allowed
                                // to return from `wait_timeout_while` (I checked).
                                // So `Ok(_)` implies `*cancelled || wait_result.timed_out`.
                                log::warn!("Spurious wakeup of the MPS condvar should be impossible.");
                            }
                        }
                    }
                }
                // Safety:
                // We are okay dropping the condvar's cancelled lock here because it only guards
                // whether we're cancelled, and we've established that we weren't when we timed out.
                // We might _now_ be cancelled at any time, in which case when we loop back over
                // we'll immediately exit. But first we need to submit our "metrics" ping.
                if timed_out {
                    log::info!("Time to submit our metrics ping, {:?}", when);
                    let glean = crate::global_glean().expect("Global Glean not present when trying to send scheduled 'metrics' ping?!").lock().unwrap();
                    submitter.submit_metrics_ping(&glean, Some(when.reason()), now);
                    when = When::Reschedule;
                }
                now = local_now_with_offset().0;
            }
        }).expect("Unable to spawn Metrics Ping Scheduler thread.")
}

fn get_last_sent_time_metric() -> DatetimeMetric {
    DatetimeMetric::new(
        CommonMetricData {
            name: "last_sent_time".into(),
            category: "mps".into(),
            send_in_pings: vec![INTERNAL_STORAGE.into()],
            lifetime: Lifetime::User,
            ..Default::default()
        },
        TimeUnit::Minute,
    )
}

fn get_last_sent_build_metric() -> StringMetric {
    StringMetric::new(CommonMetricData {
        name: "last_sent_build".into(),
        category: "mps".into(),
        send_in_pings: vec![INTERNAL_STORAGE.into()],
        lifetime: Lifetime::User,
        ..Default::default()
    })
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::tests::new_glean;
    use std::sync::atomic::{AtomicU32, Ordering};

    struct ValidatingSubmitter<F: Fn(DateTime<FixedOffset>, Option<&str>)> {
        submit_validator: F,
        validator_run_count: Arc<AtomicU32>,
    }
    struct ValidatingScheduler<F: Fn(DateTime<FixedOffset>, When)> {
        schedule_validator: F,
        validator_run_count: Arc<AtomicU32>,
    }
    impl<F: Fn(DateTime<FixedOffset>, Option<&str>)> MetricsPingSubmitter for ValidatingSubmitter<F> {
        fn submit_metrics_ping(
            &self,
            _glean: &Glean,
            reason: Option<&str>,
            now: DateTime<FixedOffset>,
        ) {
            (self.submit_validator)(now, reason);
            self.validator_run_count.fetch_add(1, Ordering::Relaxed);
        }
    }
    impl<F: Fn(DateTime<FixedOffset>, When)> MetricsPingScheduler for ValidatingScheduler<F> {
        fn start_scheduler(
            &self,
            _submitter: impl MetricsPingSubmitter + Send + 'static,
            now: DateTime<FixedOffset>,
            when: When,
        ) {
            (self.schedule_validator)(now, when);
            self.validator_run_count.fetch_add(1, Ordering::Relaxed);
        }
    }

    fn new_proxies<
        F1: Fn(DateTime<FixedOffset>, Option<&str>),
        F2: Fn(DateTime<FixedOffset>, When),
    >(
        submit_validator: F1,
        schedule_validator: F2,
    ) -> (
        ValidatingSubmitter<F1>,
        Arc<AtomicU32>,
        ValidatingScheduler<F2>,
        Arc<AtomicU32>,
    ) {
        let submitter_count = Arc::new(AtomicU32::new(0));
        let submitter = ValidatingSubmitter {
            submit_validator,
            validator_run_count: Arc::clone(&submitter_count),
        };
        let scheduler_count = Arc::new(AtomicU32::new(0));
        let scheduler = ValidatingScheduler {
            schedule_validator,
            validator_run_count: Arc::clone(&scheduler_count),
        };
        (submitter, submitter_count, scheduler, scheduler_count)
    }

    // Ensure that if we have a different build, we immediately submit an "upgrade" ping
    // and schedule a "reschedule" ping for tomorrow.
    #[test]
    fn different_app_builds_submit_and_reschedule() {
        let (mut glean, _t) = new_glean(None);

        glean.app_build = "a build".into();
        get_last_sent_build_metric().set(&glean, "a different build");

        let (submitter, submitter_count, scheduler, scheduler_count) = new_proxies(
            |_, reason| assert_eq!(reason, Some("upgrade")),
            |_, when| assert_eq!(when, When::Reschedule),
        );

        schedule_internal(&glean, submitter, scheduler, local_now_with_offset().0);
        assert_eq!(1, submitter_count.swap(0, Ordering::Relaxed));
        assert_eq!(1, scheduler_count.swap(0, Ordering::Relaxed));
    }

    // If we've already sent a ping today, ensure we don't send a ping but we
    // do schedule a ping for tomorrow. ("Case #1" in schedule_internal)
    #[test]
    fn case_1_no_submit_but_schedule_tomorrow() {
        let (glean, _t) = new_glean(None);

        let fake_now = FixedOffset::east(0).ymd(2021, 4, 30).and_hms(14, 36, 14);
        get_last_sent_time_metric().set(&glean, Some(fake_now));

        let (submitter, submitter_count, scheduler, scheduler_count) = new_proxies(
            |_, reason| panic!("Case #1 shouldn't submit a ping! reason: {:?}", reason),
            |_, when| assert_eq!(when, When::Tomorrow),
        );
        schedule_internal(&glean, submitter, scheduler, fake_now);
        assert_eq!(0, submitter_count.swap(0, Ordering::Relaxed));
        assert_eq!(1, scheduler_count.swap(0, Ordering::Relaxed));
    }

    // If we haven't sent a ping today and we're after the scheduled time,
    // ensure we send a ping and then schedule a "reschedule" ping for tomorrow.
    // ("Case #2" in schedule_internal)
    #[test]
    fn case_2_submit_ping_and_reschedule() {
        let (glean, _t) = new_glean(None);

        let fake_yesterday = FixedOffset::east(0)
            .ymd(2021, 4, 29)
            .and_hms(SCHEDULED_HOUR, 0, 1);
        get_last_sent_time_metric().set(&glean, Some(fake_yesterday));
        let fake_now = fake_yesterday + Duration::days(1);

        let (submitter, submitter_count, scheduler, scheduler_count) = new_proxies(
            |_, reason| assert_eq!(reason, Some("overdue")),
            |_, when| assert_eq!(when, When::Reschedule),
        );
        schedule_internal(&glean, submitter, scheduler, fake_now);
        assert_eq!(1, submitter_count.swap(0, Ordering::Relaxed));
        assert_eq!(1, scheduler_count.swap(0, Ordering::Relaxed));
    }

    // If we haven't sent a ping today and we're before the scheduled time,
    // ensure we don't send a ping but schedule a "today" ping for today.
    // ("Case #3" in schedule_internal)
    #[test]
    fn case_3_no_submit_but_schedule_today() {
        let (glean, _t) = new_glean(None);

        let fake_yesterday =
            FixedOffset::east(0)
                .ymd(2021, 4, 29)
                .and_hms(SCHEDULED_HOUR - 1, 0, 1);
        get_last_sent_time_metric().set(&glean, Some(fake_yesterday));
        let fake_now = fake_yesterday + Duration::days(1);

        let (submitter, submitter_count, scheduler, scheduler_count) = new_proxies(
            |_, reason| panic!("Case #3 shouldn't submit a ping! reason: {:?}", reason),
            |_, when| assert_eq!(when, When::Today),
        );
        schedule_internal(&glean, submitter, scheduler, fake_now);
        assert_eq!(0, submitter_count.swap(0, Ordering::Relaxed));
        assert_eq!(1, scheduler_count.swap(0, Ordering::Relaxed));
    }

    // `When` is responsible for date math. Let's make sure it's correct.
    #[test]
    fn when_gets_at_least_some_date_math_correct() {
        let now = FixedOffset::east(0).ymd(2021, 4, 30).and_hms(15, 2, 10);
        // `now` is after `SCHEDULED_HOUR` so should be zero:
        assert_eq!(std::time::Duration::from_secs(0), When::Today.until(now));
        // If we bring it back before `SCHEDULED_HOUR` it should give us the duration:
        let earlier = now.date().and_hms(SCHEDULED_HOUR - 1, 0, 0);
        assert_eq!(
            std::time::Duration::from_secs(3600),
            When::Today.until(earlier)
        );

        // `Tomorrow` and `Reschedule` should differ only in their `reason()`
        // 46670s is 12h57m10s (aka, the time from 15:02:10 to 04:00:00
        // (when the timezone doesn't change between them)).
        assert_eq!(
            std::time::Duration::from_secs(46670),
            When::Tomorrow.until(now)
        );
        assert_eq!(
            std::time::Duration::from_secs(46670),
            When::Reschedule.until(now)
        );
        assert_eq!(When::Tomorrow.until(now), When::Reschedule.until(now));
        assert_ne!(When::Tomorrow.reason(), When::Reschedule.reason());
    }

    // Scheduler tests mutate global state and thus must not be run in parallel.
    // Otherwise one test could cancel the other.
    // This Mutex aims to solve that.
    static SCHEDULER_TEST_MUTEX: Lazy<Mutex<()>> = Lazy::new(|| Mutex::new(()));

    // The scheduler has been designed to be cancellable. Can we cancel it?
    #[test]
    fn cancellable_tasks_can_be_cancelled() {
        // First and foremost, all scheduler tests must ensure they start uncancelled.
        // Perils of having shared state.
        let _test_lock = SCHEDULER_TEST_MUTEX.lock().unwrap();
        let (cancelled_lock, _condvar) = &**TASK_CONDVAR; // One `*` for Lazy, the second for Arc
        *cancelled_lock.lock().unwrap() = false;

        // Pick a time at least two hours from the next scheduled submission.
        // (So that this test will time out if cancellation fails).
        let now = FixedOffset::east(0)
            .ymd(2021, 4, 30)
            .and_hms(SCHEDULED_HOUR - 2, 0, 0);

        let proxy_factory = || {
            new_proxies(
                |_, reason| {
                    panic!(
                        "Shouldn't submit when testing scheduler. reason: {:?}",
                        reason
                    )
                },
                |_, _| panic!("Not even using the scheduler this time."),
            )
        };

        // Test Today.
        let (submitter, submitter_count, _, _) = proxy_factory();
        let handle = start_scheduler(submitter, now, When::Today);
        super::cancel();
        handle.join().unwrap(); // Should complete immediately.
        assert_eq!(0, submitter_count.swap(0, Ordering::Relaxed));

        // Test Tomorrow.
        let (submitter, submitter_count, _, _) = proxy_factory();
        *cancelled_lock.lock().unwrap() = false; // Uncancel.
        let handle = start_scheduler(submitter, now, When::Tomorrow);
        super::cancel();
        handle.join().unwrap(); // Should complete immediately.
        assert_eq!(0, submitter_count.swap(0, Ordering::Relaxed));

        // Test Reschedule.
        let (submitter, submitter_count, _, _) = proxy_factory();
        *cancelled_lock.lock().unwrap() = false; // Uncancel.
        let handle = start_scheduler(submitter, now, When::Reschedule);
        super::cancel();
        handle.join().unwrap(); // Should complete immediately.
        assert_eq!(0, submitter_count.swap(0, Ordering::Relaxed));
    }

    // We're not keen to wait like the scheduler is, but we can test a quick schedule.
    #[test]
    fn immediate_task_runs_immediately() {
        // First and foremost, all scheduler tests must ensure they start uncancelled.
        // Perils of having shared state.
        let _test_lock = SCHEDULER_TEST_MUTEX.lock().unwrap();
        let (cancelled_lock, _condvar) = &**TASK_CONDVAR; // One `*` for Lazy, the second for Arc
        *cancelled_lock.lock().unwrap() = false;

        // We're actually going to submit a ping from the scheduler, which requires a global glean.
        let (glean, _t) = new_glean(None);
        assert!(
            !glean.schedule_metrics_pings,
            "Real schedulers not allowed in tests!"
        );
        assert!(crate::setup_glean(glean).is_ok());

        // We're choosing a time after SCHEDULED_HOUR so `When::Today` will give us a duration of 0.
        let now = FixedOffset::east(0).ymd(2021, 4, 20).and_hms(15, 42, 0);

        let (submitter, submitter_count, _, _) = new_proxies(
            move |_, reason| {
                assert_eq!(reason, Some("today"));
                // After submitting the ping we expect, let's cancel this scheduler so the thread exits.
                // (But do it on another thread because the condvar loop is currently holding `cancelled`'s mutex)
                std::thread::spawn(super::cancel);
            },
            |_, _| panic!("Not using the scheduler this time."),
        );

        let handle = start_scheduler(submitter, now, When::Today);
        handle.join().unwrap();
        assert_eq!(1, submitter_count.swap(0, Ordering::Relaxed));
    }
}
