// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::convert::TryFrom;
use std::mem;
use std::time::{Duration, Instant};

/// Internal structure for a timer item.
struct TimerItem<T> {
    time: Instant,
    item: T,
}

impl<T> TimerItem<T> {
    fn time(ti: &Self) -> Instant {
        ti.time
    }
}

/// A timer queue.
/// This uses a classic timer wheel arrangement, with some characteristics that might be considered peculiar.
/// Each slot in the wheel is sorted (complexity O(N) insertions, but O(logN) to find cut points).
/// Time is relative, the wheel has an origin time and it is unable to represent times that are more than
/// `granularity * capacity` past that time.
pub struct Timer<T> {
    items: Vec<Vec<TimerItem<T>>>,
    now: Instant,
    granularity: Duration,
    cursor: usize,
}

impl<T> Timer<T> {
    /// Construct a new wheel at the given granularity, starting at the given time.
    /// # Panics
    /// When `capacity` is too large to fit in `u32` or `granularity` is zero.
    pub fn new(now: Instant, granularity: Duration, capacity: usize) -> Self {
        assert!(u32::try_from(capacity).is_ok());
        assert!(granularity.as_nanos() > 0);
        let mut items = Vec::with_capacity(capacity);
        items.resize_with(capacity, Default::default);
        Self {
            items,
            now,
            granularity,
            cursor: 0,
        }
    }

    /// Return a reference to the time of the next entry.
    #[must_use]
    pub fn next_time(&self) -> Option<Instant> {
        for i in 0..self.items.len() {
            let idx = self.bucket(i);
            if let Some(t) = self.items[idx].first() {
                return Some(t.time);
            }
        }
        None
    }

    /// Get the full span of time that this can cover.
    /// Two timers cannot be more than this far apart.
    /// In practice, this value is less by one amount of the timer granularity.
    #[inline]
    #[allow(clippy::cast_possible_truncation)] // guarded by assertion
    #[must_use]
    pub fn span(&self) -> Duration {
        self.granularity * (self.items.len() as u32)
    }

    /// For the given `time`, get the number of whole buckets in the future that is.
    #[inline]
    #[allow(clippy::cast_possible_truncation)] // guarded by assertion
    fn delta(&self, time: Instant) -> usize {
        // This really should use Duration::div_duration_f??(), but it can't yet.
        ((time - self.now).as_nanos() / self.granularity.as_nanos()) as usize
    }

    #[inline]
    fn time_bucket(&self, time: Instant) -> usize {
        self.bucket(self.delta(time))
    }

    #[inline]
    fn bucket(&self, delta: usize) -> usize {
        debug_assert!(delta < self.items.len());
        (self.cursor + delta) % self.items.len()
    }

    /// Slide forward in time by `n * self.granularity`.
    #[allow(clippy::cast_possible_truncation, clippy::reversed_empty_ranges)]
    // cast_possible_truncation is ok because we have an assertion guard.
    // reversed_empty_ranges is to avoid different types on the if/else.
    fn tick(&mut self, n: usize) {
        let new = self.bucket(n);
        let iter = if new < self.cursor {
            (self.cursor..self.items.len()).chain(0..new)
        } else {
            (self.cursor..new).chain(0..0)
        };
        for i in iter {
            assert!(self.items[i].is_empty());
        }
        self.now += self.granularity * (n as u32);
        self.cursor = new;
    }

    /// Asserts if the time given is in the past or too far in the future.
    /// # Panics
    /// When `time` is in the past relative to previous calls.
    pub fn add(&mut self, time: Instant, item: T) {
        assert!(time >= self.now);
        // Skip forward quickly if there is too large a gap.
        let short_span = self.span() - self.granularity;
        if time >= (self.now + self.span() + short_span) {
            // Assert that there aren't any items.
            for i in &self.items {
                debug_assert!(i.is_empty());
            }
            self.now = time.checked_sub(short_span).unwrap();
            self.cursor = 0;
        }

        // Adjust time forward the minimum amount necessary.
        let mut d = self.delta(time);
        if d >= self.items.len() {
            self.tick(1 + d - self.items.len());
            d = self.items.len() - 1;
        }

        let bucket = self.bucket(d);
        let ins = match self.items[bucket].binary_search_by_key(&time, TimerItem::time) {
            Ok(j) | Err(j) => j,
        };
        self.items[bucket].insert(ins, TimerItem { time, item });
    }

    /// Given knowledge of the time an item was added, remove it.
    /// This requires use of a predicate that identifies matching items.
    pub fn remove<F>(&mut self, time: Instant, mut selector: F) -> Option<T>
    where
        F: FnMut(&T) -> bool,
    {
        if time < self.now {
            return None;
        }
        if time > self.now + self.span() {
            return None;
        }
        let bucket = self.time_bucket(time);
        let Ok(start_index) = self.items[bucket].binary_search_by_key(&time, TimerItem::time)
        else {
            return None;
        };
        // start_index is just one of potentially many items with the same time.
        // Search backwards for a match, ...
        for i in (0..=start_index).rev() {
            if self.items[bucket][i].time != time {
                break;
            }
            if selector(&self.items[bucket][i].item) {
                return Some(self.items[bucket].remove(i).item);
            }
        }
        // ... then forwards.
        for i in (start_index + 1)..self.items[bucket].len() {
            if self.items[bucket][i].time != time {
                break;
            }
            if selector(&self.items[bucket][i].item) {
                return Some(self.items[bucket].remove(i).item);
            }
        }
        None
    }

    /// Take the next item, unless there are no items with
    /// a timeout in the past relative to `until`.
    pub fn take_next(&mut self, until: Instant) -> Option<T> {
        for i in 0..self.items.len() {
            let idx = self.bucket(i);
            if !self.items[idx].is_empty() && self.items[idx][0].time <= until {
                return Some(self.items[idx].remove(0).item);
            }
        }
        None
    }

    /// Create an iterator that takes all items until the given time.
    /// Note: Items might be removed even if the iterator is not fully exhausted.
    pub fn take_until(&mut self, until: Instant) -> impl Iterator<Item = T> {
        let get_item = move |x: TimerItem<T>| x.item;
        if until >= self.now + self.span() {
            // Drain everything, so a clean sweep.
            let mut empty_items = Vec::with_capacity(self.items.len());
            empty_items.resize_with(self.items.len(), Vec::default);
            let mut items = mem::replace(&mut self.items, empty_items);
            self.now = until;
            self.cursor = 0;

            let tail = items.split_off(self.cursor);
            return tail.into_iter().chain(items).flatten().map(get_item);
        }

        // Only returning a partial span, so do it bucket at a time.
        let delta = self.delta(until);
        let mut buckets = Vec::with_capacity(delta + 1);

        // First, the whole buckets.
        for i in 0..delta {
            let idx = self.bucket(i);
            buckets.push(mem::take(&mut self.items[idx]));
        }
        self.tick(delta);

        // Now we need to split the last bucket, because there might be
        // some items with `item.time > until`.
        let bucket = &mut self.items[self.cursor];
        let last_idx = match bucket.binary_search_by_key(&until, TimerItem::time) {
            Ok(mut m) => {
                // If there are multiple values, the search will hit any of them.
                // Make sure to get them all.
                while m < bucket.len() && bucket[m].time == until {
                    m += 1;
                }
                m
            }
            Err(ins) => ins,
        };
        let tail = bucket.split_off(last_idx);
        buckets.push(mem::replace(bucket, tail));
        // This tomfoolery with the empty vector ensures that
        // the returned type here matches the one above precisely
        // without having to invoke the `either` crate.
        buckets.into_iter().chain(vec![]).flatten().map(get_item)
    }
}

#[cfg(test)]
mod test {
    use super::{Duration, Instant, Timer};
    use lazy_static::lazy_static;

    lazy_static! {
        static ref NOW: Instant = Instant::now();
    }

    const GRANULARITY: Duration = Duration::from_millis(10);
    const CAPACITY: usize = 10;
    #[test]
    fn create() {
        let t: Timer<()> = Timer::new(*NOW, GRANULARITY, CAPACITY);
        assert_eq!(t.span(), Duration::from_millis(100));
        assert_eq!(None, t.next_time());
    }

    #[test]
    fn immediate_entry() {
        let mut t = Timer::new(*NOW, GRANULARITY, CAPACITY);
        t.add(*NOW, 12);
        assert_eq!(*NOW, t.next_time().expect("should have an entry"));
        let values: Vec<_> = t.take_until(*NOW).collect();
        assert_eq!(vec![12], values);
    }

    #[test]
    fn same_time() {
        let mut t = Timer::new(*NOW, GRANULARITY, CAPACITY);
        let v1 = 12;
        let v2 = 13;
        t.add(*NOW, v1);
        t.add(*NOW, v2);
        assert_eq!(*NOW, t.next_time().expect("should have an entry"));
        let values: Vec<_> = t.take_until(*NOW).collect();
        assert!(values.contains(&v1));
        assert!(values.contains(&v2));
    }

    #[test]
    fn add() {
        let mut t = Timer::new(*NOW, GRANULARITY, CAPACITY);
        let near_future = *NOW + Duration::from_millis(17);
        let v = 9;
        t.add(near_future, v);
        assert_eq!(near_future, t.next_time().expect("should return a value"));
        assert_eq!(
            t.take_until(near_future.checked_sub(Duration::from_millis(1)).unwrap())
                .count(),
            0
        );
        assert!(t
            .take_until(near_future + Duration::from_millis(1))
            .any(|x| x == v));
    }

    #[test]
    fn add_future() {
        let mut t = Timer::new(*NOW, GRANULARITY, CAPACITY);
        let future = *NOW + Duration::from_millis(117);
        let v = 9;
        t.add(future, v);
        assert_eq!(future, t.next_time().expect("should return a value"));
        assert!(t.take_until(future).any(|x| x == v));
    }

    #[test]
    fn add_far_future() {
        let mut t = Timer::new(*NOW, GRANULARITY, CAPACITY);
        let far_future = *NOW + Duration::from_millis(892);
        let v = 9;
        t.add(far_future, v);
        assert_eq!(far_future, t.next_time().expect("should return a value"));
        assert!(t.take_until(far_future).any(|x| x == v));
    }

    const TIMES: &[Duration] = &[
        Duration::from_millis(40),
        Duration::from_millis(91),
        Duration::from_millis(6),
        Duration::from_millis(3),
        Duration::from_millis(22),
        Duration::from_millis(40),
    ];

    fn with_times() -> Timer<usize> {
        let mut t = Timer::new(*NOW, GRANULARITY, CAPACITY);
        for (i, time) in TIMES.iter().enumerate() {
            t.add(*NOW + *time, i);
        }
        assert_eq!(
            *NOW + *TIMES.iter().min().unwrap(),
            t.next_time().expect("should have a time")
        );
        t
    }

    #[test]
    #[allow(clippy::needless_collect)] // false positive
    fn multiple_values() {
        let mut t = with_times();
        let values: Vec<_> = t.take_until(*NOW + *TIMES.iter().max().unwrap()).collect();
        for i in 0..TIMES.len() {
            assert!(values.contains(&i));
        }
    }

    #[test]
    #[allow(clippy::needless_collect)] // false positive
    fn take_far_future() {
        let mut t = with_times();
        let values: Vec<_> = t.take_until(*NOW + Duration::from_secs(100)).collect();
        for i in 0..TIMES.len() {
            assert!(values.contains(&i));
        }
    }

    #[test]
    fn remove_each() {
        let mut t = with_times();
        for (i, time) in TIMES.iter().enumerate() {
            assert_eq!(Some(i), t.remove(*NOW + *time, |&x| x == i));
        }
        assert_eq!(None, t.next_time());
    }

    #[test]
    fn remove_future() {
        let mut t = Timer::new(*NOW, GRANULARITY, CAPACITY);
        let future = *NOW + Duration::from_millis(117);
        let v = 9;
        t.add(future, v);

        assert_eq!(Some(v), t.remove(future, |candidate| *candidate == v));
    }

    #[test]
    fn remove_too_far_future() {
        let mut t = Timer::new(*NOW, GRANULARITY, CAPACITY);
        let future = *NOW + Duration::from_millis(117);
        let too_far_future = *NOW + t.span() + Duration::from_millis(117);
        let v = 9;
        t.add(future, v);

        assert_eq!(None, t.remove(too_far_future, |candidate| *candidate == v));
    }
}
