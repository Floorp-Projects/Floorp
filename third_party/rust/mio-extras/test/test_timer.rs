use mio::{Events, Poll, PollOpt, Ready, Token};
use mio_extras::timer::{self, Timer};

use std::thread;
use std::time::Duration;

#[test]
fn test_basic_timer_without_poll() {
    let mut timer = Timer::default();

    // Set the timeout
    timer.set_timeout(Duration::from_millis(200), "hello");

    // Nothing when polled immediately
    assert!(timer.poll().is_none());

    // Wait for the timeout
    thread::sleep(Duration::from_millis(250));

    assert_eq!(Some("hello"), timer.poll());
    assert!(timer.poll().is_none());
}

#[test]
fn test_basic_timer_with_poll_edge_set_timeout_after_register() {
    let poll = Poll::new().unwrap();
    let mut events = Events::with_capacity(1024);
    let mut timer = Timer::default();

    poll.register(&timer, Token(0), Ready::readable(), PollOpt::edge())
        .unwrap();
    timer.set_timeout(Duration::from_millis(200), "hello");

    let elapsed = elapsed(|| {
        let num = poll.poll(&mut events, None).unwrap();

        assert_eq!(num, 1);
        let event = events.iter().next().unwrap();
        assert_eq!(Token(0), event.token());
        assert_eq!(Ready::readable(), event.readiness());
    });

    assert!(is_about(200, elapsed), "actual={:?}", elapsed);
    assert_eq!("hello", timer.poll().unwrap());
    assert_eq!(None, timer.poll());
}

#[test]
fn test_basic_timer_with_poll_edge_set_timeout_before_register() {
    let poll = Poll::new().unwrap();
    let mut events = Events::with_capacity(1024);
    let mut timer = Timer::default();

    timer.set_timeout(Duration::from_millis(200), "hello");
    poll.register(&timer, Token(0), Ready::readable(), PollOpt::edge())
        .unwrap();

    let elapsed = elapsed(|| {
        let num = poll.poll(&mut events, None).unwrap();

        assert_eq!(num, 1);
        let event = events.iter().next().unwrap();
        assert_eq!(Token(0), event.token());
        assert_eq!(Ready::readable(), event.readiness());
    });

    assert!(is_about(200, elapsed), "actual={:?}", elapsed);
    assert_eq!("hello", timer.poll().unwrap());
    assert_eq!(None, timer.poll());
}

#[test]
fn test_setting_later_timeout_then_earlier_one() {
    let poll = Poll::new().unwrap();
    let mut events = Events::with_capacity(1024);
    let mut timer = Timer::default();

    poll.register(&timer, Token(0), Ready::readable(), PollOpt::edge())
        .unwrap();

    timer.set_timeout(Duration::from_millis(600), "hello");
    timer.set_timeout(Duration::from_millis(200), "world");

    let elapsed = elapsed(|| {
        let num = poll.poll(&mut events, None).unwrap();

        assert_eq!(num, 1);
        let event = events.iter().next().unwrap();
        assert_eq!(Token(0), event.token());
        assert_eq!(Ready::readable(), event.readiness());
    });

    assert!(is_about(200, elapsed), "actual={:?}", elapsed);
    assert_eq!("world", timer.poll().unwrap());
    assert_eq!(None, timer.poll());

    let elapsed = self::elapsed(|| {
        let num = poll.poll(&mut events, None).unwrap();

        assert_eq!(num, 1);
        let event = events.iter().next().unwrap();
        assert_eq!(Token(0), event.token());
        assert_eq!(Ready::readable(), event.readiness());
    });

    assert!(is_about(400, elapsed), "actual={:?}", elapsed);
    assert_eq!("hello", timer.poll().unwrap());
    assert_eq!(None, timer.poll());
}

#[test]
fn test_timer_with_looping_wheel() {
    let poll = Poll::new().unwrap();
    let mut events = Events::with_capacity(1024);
    let mut timer = timer::Builder::default().num_slots(2).build();

    poll.register(&timer, Token(0), Ready::readable(), PollOpt::edge())
        .unwrap();

    const TOKENS: &[&str] = &["hello", "world", "some", "thing"];

    for (i, msg) in TOKENS.iter().enumerate() {
        timer.set_timeout(Duration::from_millis(500 * (i as u64 + 1)), msg);
    }

    for msg in TOKENS {
        let elapsed = elapsed(|| {
            let num = poll.poll(&mut events, None).unwrap();

            assert_eq!(num, 1);
            let event = events.iter().next().unwrap();
            assert_eq!(Token(0), event.token());
            assert_eq!(Ready::readable(), event.readiness());
        });

        assert!(
            is_about(500, elapsed),
            "actual={:?}; msg={:?}",
            elapsed,
            msg
        );
        assert_eq!(Some(msg), timer.poll());
        assert_eq!(None, timer.poll());
    }
}

#[test]
fn test_edge_without_polling() {
    let poll = Poll::new().unwrap();
    let mut events = Events::with_capacity(1024);
    let mut timer = Timer::default();

    poll.register(&timer, Token(0), Ready::readable(), PollOpt::edge())
        .unwrap();

    timer.set_timeout(Duration::from_millis(400), "hello");

    let ms = elapsed(|| {
        let num = poll.poll(&mut events, None).unwrap();
        assert_eq!(num, 1);
        let event = events.iter().next().unwrap();
        assert_eq!(Token(0), event.token());
        assert_eq!(Ready::readable(), event.readiness());
    });

    assert!(is_about(400, ms), "actual={:?}", ms);

    let ms = elapsed(|| {
        let num = poll
            .poll(&mut events, Some(Duration::from_millis(300)))
            .unwrap();
        assert_eq!(num, 0);
    });

    assert!(is_about(300, ms), "actual={:?}", ms);
}

#[test]
fn test_level_triggered() {
    let poll = Poll::new().unwrap();
    let mut events = Events::with_capacity(1024);
    let mut timer = Timer::default();

    poll.register(&timer, Token(0), Ready::readable(), PollOpt::level())
        .unwrap();

    timer.set_timeout(Duration::from_millis(400), "hello");

    let ms = elapsed(|| {
        let num = poll.poll(&mut events, None).unwrap();
        assert_eq!(num, 1);
        let event = events.iter().next().unwrap();
        assert_eq!(Token(0), event.token());
        assert_eq!(Ready::readable(), event.readiness());
    });

    assert!(is_about(400, ms), "actual={:?}", ms);

    let ms = elapsed(|| {
        let num = poll.poll(&mut events, None).unwrap();
        assert_eq!(num, 1);
        let event = events.iter().next().unwrap();
        assert_eq!(Token(0), event.token());
        assert_eq!(Ready::readable(), event.readiness());
    });

    assert!(is_about(0, ms), "actual={:?}", ms);
}

#[test]
fn test_edge_oneshot_triggered() {
    let poll = Poll::new().unwrap();
    let mut events = Events::with_capacity(1024);
    let mut timer = Timer::default();

    poll.register(
        &timer,
        Token(0),
        Ready::readable(),
        PollOpt::edge() | PollOpt::oneshot(),
    )
    .unwrap();

    timer.set_timeout(Duration::from_millis(200), "hello");

    let ms = elapsed(|| {
        let num = poll.poll(&mut events, None).unwrap();
        assert_eq!(num, 1);
    });

    assert!(is_about(200, ms), "actual={:?}", ms);

    let ms = elapsed(|| {
        let num = poll
            .poll(&mut events, Some(Duration::from_millis(300)))
            .unwrap();
        assert_eq!(num, 0);
    });

    assert!(is_about(300, ms), "actual={:?}", ms);

    poll.reregister(
        &timer,
        Token(0),
        Ready::readable(),
        PollOpt::edge() | PollOpt::oneshot(),
    )
    .unwrap();

    let ms = elapsed(|| {
        let num = poll.poll(&mut events, None).unwrap();
        assert_eq!(num, 1);
    });

    assert!(is_about(0, ms));
}

#[test]
fn test_cancel_timeout() {
    use std::time::Instant;

    let mut timer: Timer<u32> = Default::default();
    let timeout = timer.set_timeout(Duration::from_millis(200), 1);
    timer.cancel_timeout(&timeout);

    let poll = Poll::new().unwrap();
    poll.register(&timer, Token(0), Ready::readable(), PollOpt::edge())
        .unwrap();

    let mut events = Events::with_capacity(16);

    let now = Instant::now();
    let dur = Duration::from_millis(500);
    let mut i = 0;

    while Instant::now() - now < dur {
        if i > 10 {
            panic!("iterated too many times");
        }

        i += 1;

        let elapsed = Instant::now() - now;

        poll.poll(&mut events, Some(dur - elapsed)).unwrap();

        while let Some(_) = timer.poll() {
            panic!("did not expect to receive timeout");
        }
    }
}

fn elapsed<F: FnMut()>(mut f: F) -> u64 {
    use std::time::Instant;

    let now = Instant::now();

    f();

    let elapsed = now.elapsed();
    elapsed.as_secs() * 1000 + u64::from(elapsed.subsec_millis())
}

fn is_about(expect: u64, val: u64) -> bool {
    const WINDOW: i64 = 200;

    ((expect as i64) - (val as i64)).abs() <= WINDOW
}
