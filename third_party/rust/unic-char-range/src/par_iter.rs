// Copyright 2018 The UNIC Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use rayon;

use self::rayon::iter::plumbing::{Consumer, ProducerCallback, UnindexedConsumer};
use self::rayon::prelude::*;
use crate::step::{AFTER_SURROGATE, BEFORE_SURROGATE};
use crate::CharRange;
use core::char;
use core::ops::Range;

const SKIP_LENGTH: u32 =
    crate::step::AFTER_SURROGATE as u32 - crate::step::BEFORE_SURROGATE as u32 - 1;

#[derive(Clone, Debug)]
pub struct Iter(rayon::iter::Map<rayon::range::Iter<u32>, fn(u32) -> char>);

impl ParallelIterator for Iter {
    type Item = char;

    fn drive_unindexed<C>(self, consumer: C) -> C::Result
    where
        C: UnindexedConsumer<Self::Item>,
    {
        self.0.drive_unindexed(consumer)
    }
}

impl IndexedParallelIterator for Iter {
    fn len(&self) -> usize {
        self.0.len()
    }

    fn drive<C: Consumer<Self::Item>>(self, consumer: C) -> C::Result {
        self.0.drive(consumer)
    }

    fn with_producer<CB: ProducerCallback<Self::Item>>(self, callback: CB) -> CB::Output {
        self.0.with_producer(callback)
    }
}

impl CharRange {
    fn compact_range(&self) -> Range<u32> {
        let low = self.low as u32;
        let high = self.high as u32 + 1;
        low..(if self.high >= AFTER_SURROGATE {
            high - SKIP_LENGTH
        } else {
            high
        })
    }
}

impl IntoParallelIterator for CharRange {
    type Item = char;
    type Iter = Iter;

    fn into_par_iter(self) -> Self::Iter {
        Iter(self.compact_range().into_par_iter().map(|c| {
            let c = if c > BEFORE_SURROGATE as u32 {
                c + SKIP_LENGTH
            } else {
                c
            };
            debug_assert!(c <= BEFORE_SURROGATE as u32 || c >= AFTER_SURROGATE as u32);
            debug_assert!(c <= char::MAX as u32);
            #[allow(unsafe_code)]
            unsafe {
                char::from_u32_unchecked(c)
            }
        }))
    }
}

impl<'a> IntoParallelIterator for &'a CharRange {
    type Item = char;
    type Iter = Iter;

    fn into_par_iter(self) -> Self::Iter {
        (*self).into_par_iter()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn length_agrees() {
        assert_eq!(chars!(..).iter().count(), chars!(..).par_iter().count())
    }

    #[test]
    #[cfg(feature = "std")]
    fn content_agrees() {
        assert_eq!(
            chars!(..).iter().collect::<Vec<_>>(),
            chars!(..).par_iter().collect::<Vec<_>>()
        )
    }
}
