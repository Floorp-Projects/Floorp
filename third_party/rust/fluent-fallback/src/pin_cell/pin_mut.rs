use core::cell::RefMut;
use core::fmt;
use core::ops::Deref;
use core::pin::Pin;

#[derive(Debug)]
/// A wrapper type for a mutably borrowed value from a `PinCell<T>`.
pub struct PinMut<'a, T: ?Sized> {
    pub(crate) inner: Pin<RefMut<'a, T>>,
}

impl<'a, T: ?Sized> Deref for PinMut<'a, T> {
    type Target = T;

    fn deref(&self) -> &T {
        &*self.inner
    }
}

impl<'a, T: ?Sized> PinMut<'a, T> {
    /// Get a pinned mutable reference to the value inside this wrapper.
    pub fn as_mut<'b>(orig: &'b mut PinMut<'a, T>) -> Pin<&'b mut T> {
        orig.inner.as_mut()
    }
}

/* TODO implement these APIs

impl<'a, T: ?Sized> PinMut<'a, T> {
    pub fn map<U, F>(orig: PinMut<'a, T>, f: F) -> PinMut<'a, U> where
        F: FnOnce(Pin<&mut T>) -> Pin<&mut U>,
    {
        panic!()
    }

    pub fn map_split<U, V, F>(orig: PinMut<'a, T>, f: F) -> (PinMut<'a, U>, PinMut<'a, V>) where
        F: FnOnce(Pin<&mut T>) -> (Pin<&mut U>, Pin<&mut V>)
    {
        panic!()
    }
}
*/

impl<'a, T: fmt::Display + ?Sized> fmt::Display for PinMut<'a, T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        <T as fmt::Display>::fmt(&**self, f)
    }
}

// TODO CoerceUnsized
