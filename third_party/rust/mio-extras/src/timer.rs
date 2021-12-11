//! Timer optimized for I/O related operations
use crate::convert;
use lazycell::LazyCell;
use mio::{Evented, Poll, PollOpt, Ready, Registration, SetReadiness, Token};
use slab::Slab;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;
use std::time::{Duration, Instant};
use std::{cmp, fmt, io, iter, thread, u64, usize};

/// A timer.
///
/// Typical usage goes like this:
///
/// * register the timer with a `mio::Poll`.
/// * set a timeout, by calling `Timer::set_timeout`.  Here you provide some
///   state to be associated with this timeout.
/// * poll the `Poll`, to learn when a timeout has occurred.
/// * retrieve state associated with the timeout by calling `Timer::poll`.
///
/// You can omit use of the `Poll` altogether, if you like, and just poll the
/// `Timer` directly.
pub struct Timer<T> {
    // Size of each tick in milliseconds
    tick_ms: u64,
    // Slab of timeout entries
    entries: Slab<Entry<T>>,
    // Timeout wheel. Each tick, the timer will look at the next slot for
    // timeouts that match the current tick.
    wheel: Vec<WheelEntry>,
    // Tick 0's time instant
    start: Instant,
    // The current tick
    tick: Tick,
    // The next entry to possibly timeout
    next: Token,
    // Masks the target tick to get the slot
    mask: u64,
    // Set on registration with Poll
    inner: LazyCell<Inner>,
}

/// Used to create a `Timer`.
pub struct Builder {
    // Approximate duration of each tick
    tick: Duration,
    // Number of slots in the timer wheel
    num_slots: usize,
    // Max number of timeouts that can be in flight at a given time.
    capacity: usize,
}

/// A timeout, as returned by `Timer::set_timeout`.
///
/// Use this as the argument to `Timer::cancel_timeout`, to cancel this timeout.
#[derive(Clone, Debug)]
pub struct Timeout {
    // Reference into the timer entry slab
    token: Token,
    // Tick that it should match up with
    tick: u64,
}

struct Inner {
    registration: Registration,
    set_readiness: SetReadiness,
    wakeup_state: WakeupState,
    wakeup_thread: thread::JoinHandle<()>,
}

impl Drop for Inner {
    fn drop(&mut self) {
        // 1. Set wakeup state to TERMINATE_THREAD
        self.wakeup_state.store(TERMINATE_THREAD, Ordering::Release);
        // 2. Wake him up
        self.wakeup_thread.thread().unpark();
    }
}

#[derive(Copy, Clone, Debug)]
struct WheelEntry {
    next_tick: Tick,
    head: Token,
}

// Doubly linked list of timer entries. Allows for efficient insertion /
// removal of timeouts.
struct Entry<T> {
    state: T,
    links: EntryLinks,
}

#[derive(Copy, Clone)]
struct EntryLinks {
    tick: Tick,
    prev: Token,
    next: Token,
}

type Tick = u64;

const TICK_MAX: Tick = u64::MAX;

// Manages communication with wakeup thread
type WakeupState = Arc<AtomicUsize>;

const TERMINATE_THREAD: usize = 0;
const EMPTY: Token = Token(usize::MAX);

impl Builder {
    /// Set the tick duration.  Default is 100ms.
    pub fn tick_duration(mut self, duration: Duration) -> Builder {
        self.tick = duration;
        self
    }

    /// Set the number of slots.  Default is 256.
    pub fn num_slots(mut self, num_slots: usize) -> Builder {
        self.num_slots = num_slots;
        self
    }

    /// Set the capacity.  Default is 65536.
    pub fn capacity(mut self, capacity: usize) -> Builder {
        self.capacity = capacity;
        self
    }

    /// Build a `Timer` with the parameters set on this `Builder`.
    pub fn build<T>(self) -> Timer<T> {
        Timer::new(
            convert::millis(self.tick),
            self.num_slots,
            self.capacity,
            Instant::now(),
        )
    }
}

impl Default for Builder {
    fn default() -> Builder {
        Builder {
            tick: Duration::from_millis(100),
            num_slots: 1 << 8,
            capacity: 1 << 16,
        }
    }
}

impl<T> Timer<T> {
    fn new(tick_ms: u64, num_slots: usize, capacity: usize, start: Instant) -> Timer<T> {
        let num_slots = num_slots.next_power_of_two();
        let capacity = capacity.next_power_of_two();
        let mask = (num_slots as u64) - 1;
        let wheel = iter::repeat(WheelEntry {
            next_tick: TICK_MAX,
            head: EMPTY,
        })
        .take(num_slots)
        .collect();

        Timer {
            tick_ms,
            entries: Slab::with_capacity(capacity),
            wheel,
            start,
            tick: 0,
            next: EMPTY,
            mask,
            inner: LazyCell::new(),
        }
    }

    /// Set a timeout.
    ///
    /// When the timeout occurs, the given state becomes available via `poll`.
    pub fn set_timeout(&mut self, delay_from_now: Duration, state: T) -> Timeout {
        let delay_from_start = self.start.elapsed() + delay_from_now;
        self.set_timeout_at(delay_from_start, state)
    }

    fn set_timeout_at(&mut self, delay_from_start: Duration, state: T) -> Timeout {
        let mut tick = duration_to_tick(delay_from_start, self.tick_ms);
        trace!(
            "setting timeout; delay={:?}; tick={:?}; current-tick={:?}",
            delay_from_start,
            tick,
            self.tick
        );

        // Always target at least 1 tick in the future
        if tick <= self.tick {
            tick = self.tick + 1;
        }

        self.insert(tick, state)
    }

    fn insert(&mut self, tick: Tick, state: T) -> Timeout {
        // Get the slot for the requested tick
        let slot = (tick & self.mask) as usize;
        let curr = self.wheel[slot];

        // Insert the new entry
        let entry = Entry::new(state, tick, curr.head);
        let token = Token(self.entries.insert(entry));

        if curr.head != EMPTY {
            // If there was a previous entry, set its prev pointer to the new
            // entry
            self.entries[curr.head.into()].links.prev = token;
        }

        // Update the head slot
        self.wheel[slot] = WheelEntry {
            next_tick: cmp::min(tick, curr.next_tick),
            head: token,
        };

        self.schedule_readiness(tick);

        trace!("inserted timout; slot={}; token={:?}", slot, token);

        // Return the new timeout
        Timeout { token, tick }
    }

    /// Cancel a timeout.
    ///
    /// If the timeout has not yet occurred, the return value holds the
    /// associated state.
    pub fn cancel_timeout(&mut self, timeout: &Timeout) -> Option<T> {
        let links = match self.entries.get(timeout.token.into()) {
            Some(e) => e.links,
            None => return None,
        };

        // Sanity check
        if links.tick != timeout.tick {
            return None;
        }

        self.unlink(&links, timeout.token);
        Some(self.entries.remove(timeout.token.into()).state)
    }

    /// Poll for an expired timer.
    ///
    /// The return value holds the state associated with the first expired
    /// timer, if any.
    pub fn poll(&mut self) -> Option<T> {
        let target_tick = current_tick(self.start, self.tick_ms);
        self.poll_to(target_tick)
    }

    fn poll_to(&mut self, mut target_tick: Tick) -> Option<T> {
        trace!(
            "tick_to; target_tick={}; current_tick={}",
            target_tick,
            self.tick
        );

        if target_tick < self.tick {
            target_tick = self.tick;
        }

        while self.tick <= target_tick {
            let curr = self.next;

            trace!("ticking; curr={:?}", curr);

            if curr == EMPTY {
                self.tick += 1;

                let slot = self.slot_for(self.tick);
                self.next = self.wheel[slot].head;

                // Handle the case when a slot has a single timeout which gets
                // canceled before the timeout expires. In this case, the
                // slot's head is EMPTY but there is a value for next_tick. Not
                // resetting next_tick here causes the timer to get stuck in a
                // loop.
                if self.next == EMPTY {
                    self.wheel[slot].next_tick = TICK_MAX;
                }
            } else {
                let slot = self.slot_for(self.tick);

                if curr == self.wheel[slot].head {
                    self.wheel[slot].next_tick = TICK_MAX;
                }

                let links = self.entries[curr.into()].links;

                if links.tick <= self.tick {
                    trace!("triggering; token={:?}", curr);

                    // Unlink will also advance self.next
                    self.unlink(&links, curr);

                    // Remove and return the token
                    return Some(self.entries.remove(curr.into()).state);
                } else {
                    let next_tick = self.wheel[slot].next_tick;
                    self.wheel[slot].next_tick = cmp::min(next_tick, links.tick);
                    self.next = links.next;
                }
            }
        }

        // No more timeouts to poll
        if let Some(inner) = self.inner.borrow() {
            trace!("unsetting readiness");
            let _ = inner.set_readiness.set_readiness(Ready::empty());

            if let Some(tick) = self.next_tick() {
                self.schedule_readiness(tick);
            }
        }

        None
    }

    fn unlink(&mut self, links: &EntryLinks, token: Token) {
        trace!(
            "unlinking timeout; slot={}; token={:?}",
            self.slot_for(links.tick),
            token
        );

        if links.prev == EMPTY {
            let slot = self.slot_for(links.tick);
            self.wheel[slot].head = links.next;
        } else {
            self.entries[links.prev.into()].links.next = links.next;
        }

        if links.next != EMPTY {
            self.entries[links.next.into()].links.prev = links.prev;

            if token == self.next {
                self.next = links.next;
            }
        } else if token == self.next {
            self.next = EMPTY;
        }
    }

    fn schedule_readiness(&self, tick: Tick) {
        if let Some(inner) = self.inner.borrow() {
            // Coordinate setting readiness w/ the wakeup thread
            let mut curr = inner.wakeup_state.load(Ordering::Acquire);

            loop {
                if curr as Tick <= tick {
                    // Nothing to do, wakeup is already scheduled
                    return;
                }

                // Attempt to move the wakeup time forward
                trace!("advancing the wakeup time; target={}; curr={}", tick, curr);
                let actual =
                    inner
                        .wakeup_state
                        .compare_and_swap(curr, tick as usize, Ordering::Release);

                if actual == curr {
                    // Signal to the wakeup thread that the wakeup time has
                    // been changed.
                    trace!("unparking wakeup thread");
                    inner.wakeup_thread.thread().unpark();
                    return;
                }

                curr = actual;
            }
        }
    }

    // Next tick containing a timeout
    fn next_tick(&self) -> Option<Tick> {
        if self.next != EMPTY {
            let slot = self.slot_for(self.entries[self.next.into()].links.tick);

            if self.wheel[slot].next_tick == self.tick {
                // There is data ready right now
                return Some(self.tick);
            }
        }

        self.wheel.iter().map(|e| e.next_tick).min()
    }

    fn slot_for(&self, tick: Tick) -> usize {
        (self.mask & tick) as usize
    }
}

impl<T> Default for Timer<T> {
    fn default() -> Timer<T> {
        Builder::default().build()
    }
}

impl<T> Evented for Timer<T> {
    fn register(
        &self,
        poll: &Poll,
        token: Token,
        interest: Ready,
        opts: PollOpt,
    ) -> io::Result<()> {
        if self.inner.borrow().is_some() {
            return Err(io::Error::new(
                io::ErrorKind::Other,
                "timer already registered",
            ));
        }

        let (registration, set_readiness) = Registration::new2();
        poll.register(&registration, token, interest, opts)?;
        let wakeup_state = Arc::new(AtomicUsize::new(usize::MAX));
        let thread_handle = spawn_wakeup_thread(
            Arc::clone(&wakeup_state),
            set_readiness.clone(),
            self.start,
            self.tick_ms,
        );

        self.inner
            .fill(Inner {
                registration,
                set_readiness,
                wakeup_state,
                wakeup_thread: thread_handle,
            })
            .expect("timer already registered");

        if let Some(next_tick) = self.next_tick() {
            self.schedule_readiness(next_tick);
        }

        Ok(())
    }

    fn reregister(
        &self,
        poll: &Poll,
        token: Token,
        interest: Ready,
        opts: PollOpt,
    ) -> io::Result<()> {
        match self.inner.borrow() {
            Some(inner) => poll.reregister(&inner.registration, token, interest, opts),
            None => Err(io::Error::new(
                io::ErrorKind::Other,
                "receiver not registered",
            )),
        }
    }

    fn deregister(&self, poll: &Poll) -> io::Result<()> {
        match self.inner.borrow() {
            Some(inner) => poll.deregister(&inner.registration),
            None => Err(io::Error::new(
                io::ErrorKind::Other,
                "receiver not registered",
            )),
        }
    }
}

impl fmt::Debug for Inner {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.debug_struct("Inner")
            .field("registration", &self.registration)
            .field("wakeup_state", &self.wakeup_state.load(Ordering::Relaxed))
            .finish()
    }
}

fn spawn_wakeup_thread(
    state: WakeupState,
    set_readiness: SetReadiness,
    start: Instant,
    tick_ms: u64,
) -> thread::JoinHandle<()> {
    thread::spawn(move || {
        let mut sleep_until_tick = state.load(Ordering::Acquire) as Tick;

        loop {
            if sleep_until_tick == TERMINATE_THREAD as Tick {
                return;
            }

            let now_tick = current_tick(start, tick_ms);

            trace!(
                "wakeup thread: sleep_until_tick={:?}; now_tick={:?}",
                sleep_until_tick,
                now_tick
            );

            if now_tick < sleep_until_tick {
                // Calling park_timeout with u64::MAX leads to undefined
                // behavior in pthread, causing the park to return immediately
                // and causing the thread to tightly spin. Instead of u64::MAX
                // on large values, simply use a blocking park.
                match tick_ms.checked_mul(sleep_until_tick - now_tick) {
                    Some(sleep_duration) => {
                        trace!(
                            "sleeping; tick_ms={}; now_tick={}; sleep_until_tick={}; duration={:?}",
                            tick_ms,
                            now_tick,
                            sleep_until_tick,
                            sleep_duration
                        );
                        thread::park_timeout(Duration::from_millis(sleep_duration));
                    }
                    None => {
                        trace!(
                            "sleeping; tick_ms={}; now_tick={}; blocking sleep",
                            tick_ms,
                            now_tick
                        );
                        thread::park();
                    }
                }
                sleep_until_tick = state.load(Ordering::Acquire) as Tick;
            } else {
                let actual =
                    state.compare_and_swap(sleep_until_tick as usize, usize::MAX, Ordering::AcqRel)
                        as Tick;

                if actual == sleep_until_tick {
                    trace!("setting readiness from wakeup thread");
                    let _ = set_readiness.set_readiness(Ready::readable());
                    sleep_until_tick = usize::MAX as Tick;
                } else {
                    sleep_until_tick = actual as Tick;
                }
            }
        }
    })
}

fn duration_to_tick(elapsed: Duration, tick_ms: u64) -> Tick {
    // Calculate tick rounding up to the closest one
    let elapsed_ms = convert::millis(elapsed);
    elapsed_ms.saturating_add(tick_ms / 2) / tick_ms
}

fn current_tick(start: Instant, tick_ms: u64) -> Tick {
    duration_to_tick(start.elapsed(), tick_ms)
}

impl<T> Entry<T> {
    fn new(state: T, tick: u64, next: Token) -> Entry<T> {
        Entry {
            state,
            links: EntryLinks {
                tick,
                prev: EMPTY,
                next,
            },
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use std::time::{Duration, Instant};

    #[test]
    pub fn test_timeout_next_tick() {
        let mut t = timer();
        let mut tick;

        t.set_timeout_at(Duration::from_millis(100), "a");

        tick = ms_to_tick(&t, 50);
        assert_eq!(None, t.poll_to(tick));

        tick = ms_to_tick(&t, 100);
        assert_eq!(Some("a"), t.poll_to(tick));
        assert_eq!(None, t.poll_to(tick));

        tick = ms_to_tick(&t, 150);
        assert_eq!(None, t.poll_to(tick));

        tick = ms_to_tick(&t, 200);
        assert_eq!(None, t.poll_to(tick));

        assert_eq!(count(&t), 0);
    }

    #[test]
    pub fn test_clearing_timeout() {
        let mut t = timer();
        let mut tick;

        let to = t.set_timeout_at(Duration::from_millis(100), "a");
        assert_eq!("a", t.cancel_timeout(&to).unwrap());

        tick = ms_to_tick(&t, 100);
        assert_eq!(None, t.poll_to(tick));

        tick = ms_to_tick(&t, 200);
        assert_eq!(None, t.poll_to(tick));

        assert_eq!(count(&t), 0);
    }

    #[test]
    pub fn test_multiple_timeouts_same_tick() {
        let mut t = timer();
        let mut tick;

        t.set_timeout_at(Duration::from_millis(100), "a");
        t.set_timeout_at(Duration::from_millis(100), "b");

        let mut rcv = vec![];

        tick = ms_to_tick(&t, 100);
        rcv.push(t.poll_to(tick).unwrap());
        rcv.push(t.poll_to(tick).unwrap());

        assert_eq!(None, t.poll_to(tick));

        rcv.sort();
        assert!(rcv == ["a", "b"], "actual={:?}", rcv);

        tick = ms_to_tick(&t, 200);
        assert_eq!(None, t.poll_to(tick));

        assert_eq!(count(&t), 0);
    }

    #[test]
    pub fn test_multiple_timeouts_diff_tick() {
        let mut t = timer();
        let mut tick;

        t.set_timeout_at(Duration::from_millis(110), "a");
        t.set_timeout_at(Duration::from_millis(220), "b");
        t.set_timeout_at(Duration::from_millis(230), "c");
        t.set_timeout_at(Duration::from_millis(440), "d");
        t.set_timeout_at(Duration::from_millis(560), "e");

        tick = ms_to_tick(&t, 100);
        assert_eq!(Some("a"), t.poll_to(tick));
        assert_eq!(None, t.poll_to(tick));

        tick = ms_to_tick(&t, 200);
        assert_eq!(Some("c"), t.poll_to(tick));
        assert_eq!(Some("b"), t.poll_to(tick));
        assert_eq!(None, t.poll_to(tick));

        tick = ms_to_tick(&t, 300);
        assert_eq!(None, t.poll_to(tick));

        tick = ms_to_tick(&t, 400);
        assert_eq!(Some("d"), t.poll_to(tick));
        assert_eq!(None, t.poll_to(tick));

        tick = ms_to_tick(&t, 500);
        assert_eq!(None, t.poll_to(tick));

        tick = ms_to_tick(&t, 600);
        assert_eq!(Some("e"), t.poll_to(tick));
        assert_eq!(None, t.poll_to(tick));
    }

    #[test]
    pub fn test_catching_up() {
        let mut t = timer();

        t.set_timeout_at(Duration::from_millis(110), "a");
        t.set_timeout_at(Duration::from_millis(220), "b");
        t.set_timeout_at(Duration::from_millis(230), "c");
        t.set_timeout_at(Duration::from_millis(440), "d");

        let tick = ms_to_tick(&t, 600);
        assert_eq!(Some("a"), t.poll_to(tick));
        assert_eq!(Some("c"), t.poll_to(tick));
        assert_eq!(Some("b"), t.poll_to(tick));
        assert_eq!(Some("d"), t.poll_to(tick));
        assert_eq!(None, t.poll_to(tick));
    }

    #[test]
    pub fn test_timeout_hash_collision() {
        let mut t = timer();
        let mut tick;

        t.set_timeout_at(Duration::from_millis(100), "a");
        t.set_timeout_at(Duration::from_millis(100 + TICK * SLOTS as u64), "b");

        tick = ms_to_tick(&t, 100);
        assert_eq!(Some("a"), t.poll_to(tick));
        assert_eq!(1, count(&t));

        tick = ms_to_tick(&t, 200);
        assert_eq!(None, t.poll_to(tick));
        assert_eq!(1, count(&t));

        tick = ms_to_tick(&t, 100 + TICK * SLOTS as u64);
        assert_eq!(Some("b"), t.poll_to(tick));
        assert_eq!(0, count(&t));
    }

    #[test]
    pub fn test_clearing_timeout_between_triggers() {
        let mut t = timer();
        let mut tick;

        let a = t.set_timeout_at(Duration::from_millis(100), "a");
        let _ = t.set_timeout_at(Duration::from_millis(100), "b");
        let _ = t.set_timeout_at(Duration::from_millis(200), "c");

        tick = ms_to_tick(&t, 100);
        assert_eq!(Some("b"), t.poll_to(tick));
        assert_eq!(2, count(&t));

        t.cancel_timeout(&a);
        assert_eq!(1, count(&t));

        assert_eq!(None, t.poll_to(tick));

        tick = ms_to_tick(&t, 200);
        assert_eq!(Some("c"), t.poll_to(tick));
        assert_eq!(0, count(&t));
    }

    const TICK: u64 = 100;
    const SLOTS: usize = 16;
    const CAPACITY: usize = 32;

    fn count<T>(timer: &Timer<T>) -> usize {
        timer.entries.len()
    }

    fn timer() -> Timer<&'static str> {
        Timer::new(TICK, SLOTS, CAPACITY, Instant::now())
    }

    fn ms_to_tick<T>(timer: &Timer<T>, ms: u64) -> u64 {
        ms / timer.tick_ms
    }
}
