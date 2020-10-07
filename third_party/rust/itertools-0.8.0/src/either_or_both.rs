use EitherOrBoth::*;

/// Value that either holds a single A or B, or both.
#[derive(Clone, PartialEq, Eq, Debug)]
pub enum EitherOrBoth<A, B> {
    /// Both values are present.
    Both(A, B),
    /// Only the left value of type `A` is present.
    Left(A),
    /// Only the right value of type `B` is present.
    Right(B),
}

impl<A, B> EitherOrBoth<A, B> {
    /// If `Left`, or `Both`, return true, otherwise, return false.
    pub fn has_left(&self) -> bool {
        self.as_ref().left().is_some()
    }

    /// If `Right`, or `Both`, return true, otherwise, return false.
    pub fn has_right(&self) -> bool {
        self.as_ref().right().is_some()
    }

    /// If `Left`, or `Both`, return `Some` with the left value, otherwise, return `None`.
    pub fn left(self) -> Option<A> {
        match self {
            Left(left) | Both(left, _) => Some(left),
            _ => None
        }
    }

    /// If `Right`, or `Both`, return `Some` with the right value, otherwise, return `None`.
    pub fn right(self) -> Option<B> {
        match self {
            Right(right) | Both(_, right) => Some(right),
            _ => None
        }
    }

    /// Converts from `&EitherOrBoth<A, B>` to `EitherOrBoth<&A, &B>`.
    pub fn as_ref(&self) -> EitherOrBoth<&A, &B> {
        match *self {
            Left(ref left) => Left(left),
            Right(ref right) => Right(right),
            Both(ref left, ref right) => Both(left, right),
        }
    }

    /// Converts from `&mut EitherOrBoth<A, B>` to `EitherOrBoth<&mut A, &mut B>`.
    pub fn as_mut(&mut self) -> EitherOrBoth<&mut A, &mut B> {
        match *self {
            Left(ref mut left) => Left(left),
            Right(ref mut right) => Right(right),
            Both(ref mut left, ref mut right) => Both(left, right),
        }
    }
}
