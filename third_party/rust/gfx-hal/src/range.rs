//! Generic range type abstraction that allows
//! ranges to be handled a little more generically.

use std::ops::{Range, RangeFrom, RangeFull, RangeTo};

/// Abstracts the std range types.
///
/// Based upon the nightly `RangeArgument` trait.
pub trait RangeArg<T> {
    /// Start index bound.
    fn start(&self) -> Option<&T>;
    /// End index bound.
    fn end(&self) -> Option<&T>;
}

impl<T> RangeArg<T> for Range<T> {
    fn start(&self) -> Option<&T> {
        Some(&self.start)
    }
    fn end(&self) -> Option<&T> {
        Some(&self.end)
    }
}

impl<T> RangeArg<T> for RangeTo<T> {
    fn start(&self) -> Option<&T> {
        None
    }
    fn end(&self) -> Option<&T> {
        Some(&self.end)
    }
}

impl<T> RangeArg<T> for RangeFrom<T> {
    fn start(&self) -> Option<&T> {
        Some(&self.start)
    }
    fn end(&self) -> Option<&T> {
        None
    }
}

impl<T> RangeArg<T> for RangeFull {
    fn start(&self) -> Option<&T> {
        None
    }
    fn end(&self) -> Option<&T> {
        None
    }
}

impl<T> RangeArg<T> for (Option<T>, Option<T>) {
    fn start(&self) -> Option<&T> {
        self.0.as_ref()
    }
    fn end(&self) -> Option<&T> {
        self.1.as_ref()
    }
}
