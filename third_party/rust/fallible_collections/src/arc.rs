//! Implement a Fallible Arc
use super::FallibleBox;
use super::TryClone;

use crate::TryReserveError;
use alloc::boxed::Box;
use alloc::sync::Arc;

/// trait to implement Fallible Arc
pub trait FallibleArc<T> {
    /// try creating a new Arc, returning a Result<Box<T>,
    /// TryReserveError> if allocation failed
    fn try_new(t: T) -> Result<Self, TryReserveError>
    where
        Self: Sized;
}

impl<T> FallibleArc<T> for Arc<T> {
    fn try_new(t: T) -> Result<Self, TryReserveError> {
        // doesn't work as the inner variable of arc are also stocked in the box
        let b = Box::try_new(t)?;
        Ok(Arc::from(b))
    }
}

/// Just a TryClone boilerplate for Arc
impl<T: ?Sized> TryClone for Arc<T> {
    fn try_clone(&self) -> Result<Self, TryReserveError> {
        Ok(self.clone())
    }
}

#[cfg(test)]
mod test {
    #[test]
    fn fallible_rc() {
        use std::sync::Arc;

        let mut x = Arc::new(3);
        *Arc::get_mut(&mut x).unwrap() = 4;
        assert_eq!(*x, 4);

        let _y = Arc::clone(&x);
        assert!(Arc::get_mut(&mut x).is_none());
    }
}
