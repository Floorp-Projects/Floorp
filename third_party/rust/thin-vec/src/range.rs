use std::ops::{RangeFull, Range, RangeTo, RangeFrom};
use std::collections::Bound::{self, Excluded, Included, Unbounded};

/// `RangeArgument` is implemented by Rust's built-in range types, produced
/// by range syntax like `..`, `a..`, `..b` or `c..d`.
pub trait RangeArgument<T: ?Sized> {
    /// Start index bound.
    ///
    /// Returns the start value as a `Bound`.
    fn start(&self) -> Bound<&T>;

    /// End index bound.
    ///
    /// Returns the end value as a `Bound`.
    fn end(&self) -> Bound<&T>;
}

// FIXME add inclusive ranges to RangeArgument

impl<T: ?Sized> RangeArgument<T> for RangeFull {
    fn start(&self) -> Bound<&T> {
        Unbounded
    }
    fn end(&self) -> Bound<&T> {
        Unbounded
    }
}

impl<T> RangeArgument<T> for RangeFrom<T> {
    fn start(&self) -> Bound<&T> {
        Included(&self.start)
    }
    fn end(&self) -> Bound<&T> {
        Unbounded
    }
}

impl<T> RangeArgument<T> for RangeTo<T> {
    fn start(&self) -> Bound<&T> {
        Unbounded
    }
    fn end(&self) -> Bound<&T> {
        Excluded(&self.end)
    }
}

impl<T> RangeArgument<T> for Range<T> {
    fn start(&self) -> Bound<&T> {
        Included(&self.start)
    }
    fn end(&self) -> Bound<&T> {
        Excluded(&self.end)
    }
}

/* ~one day~
impl<T> RangeArgument<T> for RangeToInclusive<T> {
    fn start(&self) -> Bound<&T> {
        Unbounded
    }
    fn end(&self) -> Bound<&T> {
        Included(&self.end)
    }
}

impl<T> RangeArgument<T> for RangeInclusive<T> {
    fn start(&self) -> Bound<&T> {
        Included(&self.start)
    }
    fn end(&self) -> Bound<&T> {
        Included(&self.end)
    }
}
*/

impl<T> RangeArgument<T> for (Bound<T>, Bound<T>) {
    fn start(&self) -> Bound<&T> {
        match *self {
            (Included(ref start), _) => Included(start),
            (Excluded(ref start), _) => Excluded(start),
            (Unbounded, _)           => Unbounded,
        }
    }

    fn end(&self) -> Bound<&T> {
        match *self {
            (_, Included(ref end)) => Included(end),
            (_, Excluded(ref end)) => Excluded(end),
            (_, Unbounded)         => Unbounded,
        }
    }
}

impl<'a, T: ?Sized + 'a> RangeArgument<T> for (Bound<&'a T>, Bound<&'a T>) {
    fn start(&self) -> Bound<&T> {
        self.0
    }

    fn end(&self) -> Bound<&T> {
        self.1
    }
}
