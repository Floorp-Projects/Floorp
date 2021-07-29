use core::cell::Ref;
use core::fmt;
use core::ops::Deref;
use core::pin::Pin;

#[derive(Debug)]
/// A wrapper type for a immutably borrowed value from a `PinCell<T>`.
pub struct PinRef<'a, T: ?Sized> {
    pub(crate) inner: Pin<Ref<'a, T>>,
}

impl<'a, T: ?Sized> Deref for PinRef<'a, T> {
    type Target = T;

    fn deref(&self) -> &T {
        &*self.inner
    }
}

/* TODO implement these APIs

impl<'a, T: ?Sized> PinRef<'a, T> {
    pub fn clone(orig: &PinRef<'a, T>) -> PinRef<'a, T> {
        panic!()
    }

    pub fn map<U, F>(orig: PinRef<'a, T>, f: F) -> PinRef<'a, U> where
        F: FnOnce(Pin<&T>) -> Pin<&U>,
    {
        panic!()
    }

    pub fn map_split<U, V, F>(orig: PinRef<'a, T>, f: F) -> (PinRef<'a, U>, PinRef<'a, V>) where
        F: FnOnce(Pin<&T>) -> (Pin<&U>, Pin<&V>)
    {
        panic!()
    }
}
*/

impl<'a, T: fmt::Display + ?Sized> fmt::Display for PinRef<'a, T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        <T as fmt::Display>::fmt(&**self, f)
    }
}

// TODO CoerceUnsized
