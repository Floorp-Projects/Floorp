//! Processing push constant ranges
//!
//! This module provides utitlity functions to make push constants root signature
//! compatible. Root constants are non-overlapping, therefore, the push constant
//! ranges passed at pipeline layout creation need to be `split` into disjunct
//! ranges. The disjunct ranges can be then converted into root signature entries.

use hal::pso;
use std::{borrow::Borrow, cmp::Ordering, ops::Range};

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub struct RootConstant {
    pub stages: pso::ShaderStageFlags,
    pub range: Range<u32>,
}

impl RootConstant {
    fn is_empty(&self) -> bool {
        self.range.end <= self.range.start
    }

    // Divide a root constant into two separate ranges depending on the overlap
    // with another root constant.
    fn divide(self, other: &RootConstant) -> (RootConstant, RootConstant) {
        assert!(self.range.start <= other.range.start);
        let left = RootConstant {
            stages: self.stages,
            range: self.range.start .. other.range.start,
        };

        let right = RootConstant {
            stages: self.stages,
            range: other.range.start .. self.range.end,
        };

        (left, right)
    }
}

impl PartialOrd for RootConstant {
    fn partial_cmp(&self, other: &RootConstant) -> Option<Ordering> {
        Some(
            self.range
                .start
                .cmp(&other.range.start)
                .then(self.range.end.cmp(&other.range.end))
                .then(self.stages.cmp(&other.stages)),
        )
    }
}

impl Ord for RootConstant {
    fn cmp(&self, other: &RootConstant) -> Ordering {
        self.partial_cmp(other).unwrap()
    }
}

pub fn split<I>(ranges: I) -> Vec<RootConstant>
where
    I: IntoIterator,
    I::Item: Borrow<(pso::ShaderStageFlags, Range<u32>)>,
{
    // Frontier of unexplored root constant ranges, sorted descending
    // (less element shifting for Vec) regarding to the start of ranges.
    let mut ranges = into_vec(ranges);
    ranges.sort_by(|a, b| b.cmp(a));

    // Storing resulting disjunct root constant ranges.
    let mut disjunct = Vec::with_capacity(ranges.len());

    while let Some(cur) = ranges.pop() {
        // Run trough all unexplored ranges. After each run the frontier will be
        // resorted!
        //
        // Case 1: Single element remaining
        //      Push is to the disjunct list, done.
        // Case 2: At least two ranges, which possibly overlap
        //      Divide the first range into a left set and right set, depending
        //      on the overlap of the two ranges:
        //      Range 1: |---- left ---||--- right ---|
        //      Range 2:                |--------...
        if let Some(mut next) = ranges.pop() {
            let (left, mut right) = cur.divide(&next);
            if !left.is_empty() {
                // The left part is, by definition, disjunct to all other ranges.
                // Push all remaining pieces in the frontier, handled by the next
                // iteration.
                disjunct.push(left);
                ranges.push(next);
                if !right.is_empty() {
                    ranges.push(right);
                }
            } else if !right.is_empty() {
                // If the left part is empty this means that both ranges have the
                // same start value. The right segment is a candidate for a disjunct
                // segment but we haven't checked against other ranges so far.
                // Therefore, we push is on the frontier again, but added the
                // stage flags from the overlapping segment.
                // The second range will be shrunken to be disjunct with the pushed
                // segment as we have already processed it.
                // In the next iteration we will look again at the push right
                // segment and compare it to other elements on the list until we
                // have a small enough disjunct segment, which doesn't overlap
                // with any part of the frontier.
                right.stages |= next.stages;
                next.range.start = right.range.end;
                ranges.push(right);
                if !next.is_empty() {
                    ranges.push(next);
                }
            }
        } else {
            disjunct.push(cur);
        }
        ranges.sort_by(|a, b| b.cmp(a));
    }

    disjunct
}

fn into_vec<I>(ranges: I) -> Vec<RootConstant>
where
    I: IntoIterator,
    I::Item: Borrow<(pso::ShaderStageFlags, Range<u32>)>,
{
    ranges
        .into_iter()
        .map(|borrowable| {
            let &(stages, ref range) = borrowable.borrow();
            debug_assert_eq!(range.start % 4, 0);
            debug_assert_eq!(range.end % 4, 0);
            RootConstant {
                stages,
                range: range.start / 4 .. range.end / 4,
            }
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_single() {
        let range = &[(pso::ShaderStageFlags::VERTEX, 0 .. 12)];
        assert_eq!(into_vec(range), split(range));
    }

    #[test]
    fn test_overlap_1() {
        // Case:
        //      |----------|
        //          |------------|
        let ranges = &[
            (pso::ShaderStageFlags::VERTEX, 0 .. 12),
            (pso::ShaderStageFlags::FRAGMENT, 8 .. 16),
        ];

        let reference = vec![
            RootConstant {
                stages: pso::ShaderStageFlags::VERTEX,
                range: 0 .. 2,
            },
            RootConstant {
                stages: pso::ShaderStageFlags::VERTEX | pso::ShaderStageFlags::FRAGMENT,
                range: 2 .. 3,
            },
            RootConstant {
                stages: pso::ShaderStageFlags::FRAGMENT,
                range: 3 .. 4,
            },
        ];
        assert_eq!(reference, split(ranges));
    }

    #[test]
    fn test_overlap_2() {
        // Case:
        //      |-------------------|
        //          |------------|
        let ranges = &[
            (pso::ShaderStageFlags::VERTEX, 0 .. 20),
            (pso::ShaderStageFlags::FRAGMENT, 8 .. 16),
        ];

        let reference = vec![
            RootConstant {
                stages: pso::ShaderStageFlags::VERTEX,
                range: 0 .. 2,
            },
            RootConstant {
                stages: pso::ShaderStageFlags::VERTEX | pso::ShaderStageFlags::FRAGMENT,
                range: 2 .. 4,
            },
            RootConstant {
                stages: pso::ShaderStageFlags::VERTEX,
                range: 4 .. 5,
            },
        ];
        assert_eq!(reference, split(ranges));
    }

    #[test]
    fn test_overlap_4() {
        // Case:
        //      |--------------|
        //      |------------|
        let ranges = &[
            (pso::ShaderStageFlags::VERTEX, 0 .. 20),
            (pso::ShaderStageFlags::FRAGMENT, 0 .. 16),
        ];

        let reference = vec![
            RootConstant {
                stages: pso::ShaderStageFlags::VERTEX | pso::ShaderStageFlags::FRAGMENT,
                range: 0 .. 4,
            },
            RootConstant {
                stages: pso::ShaderStageFlags::VERTEX,
                range: 4 .. 5,
            },
        ];
        assert_eq!(reference, split(ranges));
    }

    #[test]
    fn test_equal() {
        // Case:
        //      |-----|
        //      |-----|
        let ranges = &[
            (pso::ShaderStageFlags::VERTEX, 0 .. 16),
            (pso::ShaderStageFlags::FRAGMENT, 0 .. 16),
        ];

        let reference = vec![RootConstant {
            stages: pso::ShaderStageFlags::VERTEX | pso::ShaderStageFlags::FRAGMENT,
            range: 0 .. 4,
        }];
        assert_eq!(reference, split(ranges));
    }

    #[test]
    fn test_disjunct() {
        // Case:
        //      |------|
        //               |------------|
        let ranges = &[
            (pso::ShaderStageFlags::VERTEX, 0 .. 12),
            (pso::ShaderStageFlags::FRAGMENT, 12 .. 16),
        ];
        assert_eq!(into_vec(ranges), split(ranges));
    }

    #[test]
    fn test_complex() {
        let ranges = &[
            (pso::ShaderStageFlags::VERTEX, 8 .. 40),
            (pso::ShaderStageFlags::FRAGMENT, 0 .. 20),
            (pso::ShaderStageFlags::GEOMETRY, 24 .. 40),
            (pso::ShaderStageFlags::HULL, 16 .. 28),
        ];

        let reference = vec![
            RootConstant {
                stages: pso::ShaderStageFlags::FRAGMENT,
                range: 0 .. 2,
            },
            RootConstant {
                stages: pso::ShaderStageFlags::VERTEX | pso::ShaderStageFlags::FRAGMENT,
                range: 2 .. 4,
            },
            RootConstant {
                stages: pso::ShaderStageFlags::VERTEX
                    | pso::ShaderStageFlags::FRAGMENT
                    | pso::ShaderStageFlags::HULL,
                range: 4 .. 5,
            },
            RootConstant {
                stages: pso::ShaderStageFlags::VERTEX | pso::ShaderStageFlags::HULL,
                range: 5 .. 6,
            },
            RootConstant {
                stages: pso::ShaderStageFlags::VERTEX
                    | pso::ShaderStageFlags::GEOMETRY
                    | pso::ShaderStageFlags::HULL,
                range: 6 .. 7,
            },
            RootConstant {
                stages: pso::ShaderStageFlags::VERTEX | pso::ShaderStageFlags::GEOMETRY,
                range: 7 .. 10,
            },
        ];

        assert_eq!(reference, split(ranges));
    }
}
