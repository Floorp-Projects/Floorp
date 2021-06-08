//! Implement a Fallible Rc
use super::FallibleBox;
use crate::TryReserveError;
use alloc::boxed::Box;
use alloc::rc::Rc;
/// trait to implement Fallible Rc
pub trait FallibleRc<T> {
    /// try creating a new Rc, returning a Result<Box<T>,
    /// TryReserveError> if allocation failed
    fn try_new(t: T) -> Result<Self, TryReserveError>
    where
        Self: Sized;
}

impl<T> FallibleRc<T> for Rc<T> {
    fn try_new(t: T) -> Result<Self, TryReserveError> {
        let b = <Box<T> as FallibleBox<T>>::try_new(t)?;
        Ok(Rc::from(b))
    }
}

#[cfg(test)]
mod test {
    #[test]
    fn fallible_rc() {
        use std::rc::Rc;

        let mut x = Rc::new(3);
        *Rc::get_mut(&mut x).unwrap() = 4;
        assert_eq!(*x, 4);

        let _y = Rc::clone(&x);
        assert!(Rc::get_mut(&mut x).is_none());
    }
}
