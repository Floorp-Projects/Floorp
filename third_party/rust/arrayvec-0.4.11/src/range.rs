
use std::ops::{
    RangeFull,
    RangeFrom,
    RangeTo,
    Range,
};

/// `RangeArgument` is implemented by Rust's built-in range types, produced
/// by range syntax like `..`, `a..`, `..b` or `c..d`.
///
/// Note: This is arrayvec's provisional trait, waiting for stable Rust to
/// provide an equivalent.
pub trait RangeArgument {
    #[inline]
    /// Start index (inclusive)
    fn start(&self) -> Option<usize> { None }
    #[inline]
    /// End index (exclusive)
    fn end(&self) -> Option<usize> { None }
}


impl RangeArgument for RangeFull {}

impl RangeArgument for RangeFrom<usize> {
    #[inline]
    fn start(&self) -> Option<usize> { Some(self.start) }
}

impl RangeArgument for RangeTo<usize> {
    #[inline]
    fn end(&self) -> Option<usize> { Some(self.end) }
}

impl RangeArgument for Range<usize> {
    #[inline]
    fn start(&self) -> Option<usize> { Some(self.start) }
    #[inline]
    fn end(&self) -> Option<usize> { Some(self.end) }
}

