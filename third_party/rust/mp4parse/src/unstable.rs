// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
use num_traits::{CheckedAdd, CheckedSub, PrimInt, Zero};
use std::ops::{Add, Neg, Sub};

use super::*;

/// A zero-overhead wrapper around integer types for the sake of always
/// requiring checked arithmetic
#[repr(transparent)]
#[derive(Debug, Default, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct CheckedInteger<T>(pub T);

impl<T> From<T> for CheckedInteger<T> {
    fn from(i: T) -> Self {
        Self(i)
    }
}

// Orphan rules prevent a more general implementation, but this suffices
impl From<CheckedInteger<i64>> for i64 {
    fn from(checked: CheckedInteger<i64>) -> i64 {
        checked.0
    }
}

impl<T, U: Into<T>> Add<U> for CheckedInteger<T>
where
    T: CheckedAdd,
{
    type Output = Option<Self>;

    fn add(self, other: U) -> Self::Output {
        self.0.checked_add(&other.into()).map(Into::into)
    }
}

impl<T, U: Into<T>> Sub<U> for CheckedInteger<T>
where
    T: CheckedSub,
{
    type Output = Option<Self>;

    fn sub(self, other: U) -> Self::Output {
        self.0.checked_sub(&other.into()).map(Into::into)
    }
}

/// Implement subtraction of checked `u64`s returning i64
// This is necessary for handling Mp4parseTrackInfo::media_time gracefully
impl Sub for CheckedInteger<u64> {
    type Output = Option<CheckedInteger<i64>>;

    fn sub(self, other: Self) -> Self::Output {
        if self >= other {
            self.0
                .checked_sub(other.0)
                .and_then(|u| i64::try_from(u).ok())
                .map(CheckedInteger)
        } else {
            other
                .0
                .checked_sub(self.0)
                .and_then(|u| i64::try_from(u).ok())
                .map(i64::neg)
                .map(CheckedInteger)
        }
    }
}

#[test]
fn u64_subtraction_returning_i64() {
    // self > other
    assert_eq!(
        CheckedInteger(2u64) - CheckedInteger(1u64),
        Some(CheckedInteger(1i64))
    );

    // self == other
    assert_eq!(
        CheckedInteger(1u64) - CheckedInteger(1u64),
        Some(CheckedInteger(0i64))
    );

    // difference too large to store in i64
    assert_eq!(CheckedInteger(u64::MAX) - CheckedInteger(1u64), None);

    // self < other
    assert_eq!(
        CheckedInteger(1u64) - CheckedInteger(2u64),
        Some(CheckedInteger(-1i64))
    );

    // difference not representable due to overflow
    assert_eq!(CheckedInteger(1u64) - CheckedInteger(u64::MAX), None);
}

impl<T: std::cmp::PartialEq> PartialEq<T> for CheckedInteger<T> {
    fn eq(&self, other: &T) -> bool {
        self.0 == *other
    }
}

/// Provides the following information about a sample in the source file:
/// sample data offset (start and end), composition time in microseconds
/// (start and end) and whether it is a sync sample
#[repr(C)]
#[derive(Default, Debug, PartialEq, Eq)]
pub struct Indice {
    /// The byte offset in the file where the indexed sample begins.
    pub start_offset: CheckedInteger<u64>,
    /// The byte offset in the file where the indexed sample ends. This is
    /// equivalent to `start_offset` + the length in bytes of the indexed
    /// sample. Typically this will be the `start_offset` of the next sample
    /// in the file.
    pub end_offset: CheckedInteger<u64>,
    /// The time in ticks when the indexed sample should be displayed.
    /// Analogous to the concept of presentation time stamp (pts).
    pub start_composition: CheckedInteger<i64>,
    /// The time in ticks when the indexed sample should stop being
    /// displayed. Typically this would be the `start_composition` time of the
    /// next sample if samples were ordered by composition time.
    pub end_composition: CheckedInteger<i64>,
    /// The time in ticks that the indexed sample should be decoded at.
    /// Analogous to the concept of decode time stamp (dts).
    pub start_decode: CheckedInteger<i64>,
    /// Set if the indexed sample is a sync sample. The meaning of sync is
    /// somewhat codec specific, but essentially amounts to if the sample is a
    /// key frame.
    pub sync: bool,
}

/// Create a vector of `Indice`s with the information about track samples.
/// It uses `stsc`, `stco`, `stsz` and `stts` boxes to construct a list of
/// every sample in the file and provides offsets which can be used to read
/// raw sample data from the file.
#[allow(clippy::reversed_empty_ranges)]
pub fn create_sample_table(
    track: &Track,
    track_offset_time: CheckedInteger<i64>,
) -> Option<TryVec<Indice>> {
    let (stsc, stco, stsz, stts) = match (&track.stsc, &track.stco, &track.stsz, &track.stts) {
        (Some(a), Some(b), Some(c), Some(d)) => (a, b, c, d),
        _ => return None,
    };

    // According to spec, no sync table means every sample is sync sample.
    let has_sync_table = track.stss.is_some();

    let mut sample_size_iter = stsz.sample_sizes.iter();

    // Get 'stsc' iterator for (chunk_id, chunk_sample_count) and calculate the sample
    // offset address.

    // With large numbers of samples, the cost of many allocations dominates,
    // so it's worth iterating twice to allocate sample_table just once.
    let total_sample_count = sample_to_chunk_iter(&stsc.samples, &stco.offsets)
        .map(|(_, sample_counts)| sample_counts.to_usize())
        .try_fold(0usize, usize::checked_add)?;
    let mut sample_table = TryVec::with_capacity(total_sample_count).ok()?;

    for i in sample_to_chunk_iter(&stsc.samples, &stco.offsets) {
        let chunk_id = i.0 as usize;
        let sample_counts = i.1;
        let mut cur_position = match stco.offsets.get(chunk_id) {
            Some(&i) => i.into(),
            _ => return None,
        };
        for _ in 0..sample_counts {
            let start_offset = cur_position;
            let end_offset = match (stsz.sample_size, sample_size_iter.next()) {
                (_, Some(t)) => (start_offset + *t)?,
                (t, _) if t > 0 => (start_offset + t)?,
                _ => 0.into(),
            };
            if end_offset == 0 {
                return None;
            }
            cur_position = end_offset;

            sample_table
                .push(Indice {
                    start_offset,
                    end_offset,
                    sync: !has_sync_table,
                    ..Default::default()
                })
                .ok()?;
        }
    }

    // Mark the sync sample in sample_table according to 'stss'.
    if let Some(ref v) = track.stss {
        for iter in &v.samples {
            match iter
                .checked_sub(&1)
                .and_then(|idx| sample_table.get_mut(idx as usize))
            {
                Some(elem) => elem.sync = true,
                _ => return None,
            }
        }
    }

    let ctts_iter = track.ctts.as_ref().map(|v| v.samples.as_slice().iter());

    let mut ctts_offset_iter = TimeOffsetIterator {
        cur_sample_range: (0..0),
        cur_offset: 0,
        ctts_iter,
        track_id: track.id,
    };

    let mut stts_iter = TimeToSampleIterator {
        cur_sample_count: (0..0),
        cur_sample_delta: 0,
        stts_iter: stts.samples.as_slice().iter(),
        track_id: track.id,
    };

    // sum_delta is the sum of stts_iter delta.
    // According to spec:
    //      decode time => DT(n) = DT(n-1) + STTS(n)
    //      composition time => CT(n) = DT(n) + CTTS(n)
    // Note:
    //      composition time needs to add the track offset time from 'elst' table.
    let mut sum_delta = TrackScaledTime::<i64>(0, track.id);
    for sample in sample_table.as_mut_slice() {
        let decode_time = sum_delta;
        sum_delta = (sum_delta + stts_iter.next_delta())?;

        // ctts_offset is the current sample offset time.
        let ctts_offset = ctts_offset_iter.next_offset_time();

        let start_composition = decode_time + ctts_offset;

        let end_composition = sum_delta + ctts_offset;

        let start_decode = decode_time;

        sample.start_composition = CheckedInteger(track_offset_time.0 + start_composition?.0);
        sample.end_composition = CheckedInteger(track_offset_time.0 + end_composition?.0);
        sample.start_decode = CheckedInteger(start_decode.0);
    }

    // Correct composition end time due to 'ctts' causes composition time re-ordering.
    //
    // Composition end time is not in specification. However, gecko needs it, so we need to
    // calculate to correct the composition end time.
    if !sample_table.is_empty() {
        // Create an index table refers to sample_table and sorted by start_composisiton time.
        let mut sort_table = TryVec::with_capacity(sample_table.len()).ok()?;

        for i in 0..sample_table.len() {
            sort_table.push(i).ok()?;
        }

        sort_table.sort_by_key(|i| match sample_table.get(*i) {
            Some(v) => v.start_composition,
            _ => 0.into(),
        });

        for indices in sort_table.windows(2) {
            if let [current_index, peek_index] = *indices {
                let next_start_composition_time = sample_table[peek_index].start_composition;
                let sample = &mut sample_table[current_index];
                sample.end_composition = next_start_composition_time;
            }
        }
    }

    Some(sample_table)
}

// Convert a 'ctts' compact table to full table by iterator,
// (sample_with_the_same_offset_count, offset) => (offset), (offset), (offset) ...
//
// For example:
// (2, 10), (4, 9) into (10, 10, 9, 9, 9, 9) by calling next_offset_time().
struct TimeOffsetIterator<'a> {
    cur_sample_range: std::ops::Range<u32>,
    cur_offset: i64,
    ctts_iter: Option<std::slice::Iter<'a, TimeOffset>>,
    track_id: usize,
}

impl<'a> Iterator for TimeOffsetIterator<'a> {
    type Item = i64;

    #[allow(clippy::reversed_empty_ranges)]
    fn next(&mut self) -> Option<i64> {
        let has_sample = self.cur_sample_range.next().or_else(|| {
            // At end of current TimeOffset, find the next TimeOffset.
            let iter = match self.ctts_iter {
                Some(ref mut v) => v,
                _ => return None,
            };
            let offset_version;
            self.cur_sample_range = match iter.next() {
                Some(v) => {
                    offset_version = v.time_offset;
                    0..v.sample_count
                }
                _ => {
                    offset_version = TimeOffsetVersion::Version0(0);
                    0..0
                }
            };

            self.cur_offset = match offset_version {
                TimeOffsetVersion::Version0(i) => i64::from(i),
                TimeOffsetVersion::Version1(i) => i64::from(i),
            };

            self.cur_sample_range.next()
        });

        has_sample.and(Some(self.cur_offset))
    }
}

impl<'a> TimeOffsetIterator<'a> {
    fn next_offset_time(&mut self) -> TrackScaledTime<i64> {
        match self.next() {
            Some(v) => TrackScaledTime::<i64>(v, self.track_id),
            _ => TrackScaledTime::<i64>(0, self.track_id),
        }
    }
}

// Convert 'stts' compact table to full table by iterator,
// (sample_count_with_the_same_time, time) => (time, time, time) ... repeats
// sample_count_with_the_same_time.
//
// For example:
// (2, 3000), (1, 2999) to (3000, 3000, 2999).
struct TimeToSampleIterator<'a> {
    cur_sample_count: std::ops::Range<u32>,
    cur_sample_delta: u32,
    stts_iter: std::slice::Iter<'a, Sample>,
    track_id: usize,
}

impl<'a> Iterator for TimeToSampleIterator<'a> {
    type Item = u32;

    #[allow(clippy::reversed_empty_ranges)]
    fn next(&mut self) -> Option<u32> {
        let has_sample = self.cur_sample_count.next().or_else(|| {
            self.cur_sample_count = match self.stts_iter.next() {
                Some(v) => {
                    self.cur_sample_delta = v.sample_delta;
                    0..v.sample_count
                }
                _ => 0..0,
            };

            self.cur_sample_count.next()
        });

        has_sample.and(Some(self.cur_sample_delta))
    }
}

impl<'a> TimeToSampleIterator<'a> {
    fn next_delta(&mut self) -> TrackScaledTime<i64> {
        match self.next() {
            Some(v) => TrackScaledTime::<i64>(i64::from(v), self.track_id),
            _ => TrackScaledTime::<i64>(0, self.track_id),
        }
    }
}

// Convert 'stco' compact table to full table by iterator.
// (start_chunk_num, sample_number) => (start_chunk_num, sample_number),
//                                     (start_chunk_num + 1, sample_number),
//                                     (start_chunk_num + 2, sample_number),
//                                     ...
//                                     (next start_chunk_num, next sample_number),
//                                     ...
//
// For example:
// (1, 5), (5, 10), (9, 2) => (1, 5), (2, 5), (3, 5), (4, 5), (5, 10), (6, 10),
// (7, 10), (8, 10), (9, 2)
fn sample_to_chunk_iter<'a>(
    stsc_samples: &'a TryVec<SampleToChunk>,
    stco_offsets: &'a TryVec<u64>,
) -> SampleToChunkIterator<'a> {
    SampleToChunkIterator {
        chunks: (0..0),
        sample_count: 0,
        stsc_peek_iter: stsc_samples.as_slice().iter().peekable(),
        remain_chunk_count: stco_offsets
            .len()
            .try_into()
            .expect("stco.entry_count is u32"),
    }
}

struct SampleToChunkIterator<'a> {
    chunks: std::ops::Range<u32>,
    sample_count: u32,
    stsc_peek_iter: std::iter::Peekable<std::slice::Iter<'a, SampleToChunk>>,
    remain_chunk_count: u32, // total chunk number from 'stco'.
}

impl<'a> Iterator for SampleToChunkIterator<'a> {
    type Item = (u32, u32);

    fn next(&mut self) -> Option<(u32, u32)> {
        let has_chunk = self.chunks.next().or_else(|| {
            self.chunks = self.locate();
            self.remain_chunk_count
                .checked_sub(
                    self.chunks
                        .len()
                        .try_into()
                        .expect("len() of a Range<u32> must fit in u32"),
                )
                .and_then(|res| {
                    self.remain_chunk_count = res;
                    self.chunks.next()
                })
        });

        has_chunk.map(|id| (id, self.sample_count))
    }
}

impl<'a> SampleToChunkIterator<'a> {
    #[allow(clippy::reversed_empty_ranges)]
    fn locate(&mut self) -> std::ops::Range<u32> {
        loop {
            return match (self.stsc_peek_iter.next(), self.stsc_peek_iter.peek()) {
                (Some(next), Some(peek)) if next.first_chunk == peek.first_chunk => {
                    // Invalid entry, skip it and will continue searching at
                    // next loop iteration.
                    continue;
                }
                (Some(next), Some(peek)) if next.first_chunk > 0 && peek.first_chunk > 0 => {
                    self.sample_count = next.samples_per_chunk;
                    (next.first_chunk - 1)..(peek.first_chunk - 1)
                }
                (Some(next), None) if next.first_chunk > 0 => {
                    self.sample_count = next.samples_per_chunk;
                    // Total chunk number in 'stsc' could be different to 'stco',
                    // there could be more chunks at the last 'stsc' record.
                    match next.first_chunk.checked_add(self.remain_chunk_count) {
                        Some(r) => (next.first_chunk - 1)..r - 1,
                        _ => 0..0,
                    }
                }
                _ => 0..0,
            };
        }
    }
}

/// Calculate numerator * scale / denominator, if possible.
///
/// Applying the associativity of integer arithmetic, we divide first
/// and add the remainder after multiplying each term separately
/// to preserve precision while leaving more headroom. That is,
/// (n * s) / d is split into floor(n / d) * s + (n % d) * s / d.
///
/// Return None on overflow or if the denominator is zero.
pub fn rational_scale<T, S>(numerator: T, denominator: T, scale2: S) -> Option<T>
where
    T: PrimInt + Zero,
    S: PrimInt,
{
    if denominator.is_zero() {
        return None;
    }

    let integer = numerator / denominator;
    let remainder = numerator % denominator;
    num_traits::cast(scale2).and_then(|s| match integer.checked_mul(&s) {
        Some(integer) => remainder
            .checked_mul(&s)
            .and_then(|remainder| (remainder / denominator).checked_add(&integer)),
        None => None,
    })
}

#[derive(Debug, PartialEq, Eq)]
pub struct Microseconds<T>(pub T);

/// Convert `time` in media's global (mvhd) timescale to microseconds,
/// using provided `MediaTimeScale`
pub fn media_time_to_us(time: MediaScaledTime, scale: MediaTimeScale) -> Option<Microseconds<u64>> {
    let microseconds_per_second = 1_000_000;
    rational_scale(time.0, scale.0, microseconds_per_second).map(Microseconds)
}

/// Convert `time` in track's local (mdhd) timescale to microseconds,
/// using provided `TrackTimeScale<T>`
pub fn track_time_to_us<T>(
    time: TrackScaledTime<T>,
    scale: TrackTimeScale<T>,
) -> Option<Microseconds<T>>
where
    T: PrimInt + Zero,
{
    assert_eq!(time.1, scale.1);
    let microseconds_per_second = 1_000_000;
    rational_scale(time.0, scale.0, microseconds_per_second).map(Microseconds)
}

#[test]
fn rational_scale_overflow() {
    assert_eq!(rational_scale::<u64, u64>(17, 3, 1000), Some(5666));
    let large = 0x4000_0000_0000_0000;
    assert_eq!(rational_scale::<u64, u64>(large, 2, 2), Some(large));
    assert_eq!(rational_scale::<u64, u64>(large, 4, 4), Some(large));
    assert_eq!(rational_scale::<u64, u64>(large, 2, 8), None);
    assert_eq!(rational_scale::<u64, u64>(large, 8, 4), Some(large / 2));
    assert_eq!(rational_scale::<u64, u64>(large + 1, 4, 4), Some(large + 1));
    assert_eq!(rational_scale::<u64, u64>(large, 40, 1000), None);
}

#[test]
fn media_time_overflow() {
    let scale = MediaTimeScale(90000);
    let duration = MediaScaledTime(9_007_199_254_710_000);
    assert_eq!(
        media_time_to_us(duration, scale),
        Some(Microseconds(100_079_991_719_000_000u64))
    );
}

#[test]
fn track_time_overflow() {
    let scale = TrackTimeScale(44100u64, 0);
    let duration = TrackScaledTime(4_413_527_634_807_900u64, 0);
    assert_eq!(
        track_time_to_us(duration, scale),
        Some(Microseconds(100_079_991_719_000_000u64))
    );
}
