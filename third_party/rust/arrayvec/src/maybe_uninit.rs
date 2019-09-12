

use array::Array;
use std::mem::ManuallyDrop;

/// A combination of ManuallyDrop and “maybe uninitialized”;
/// this wraps a value that can be wholly or partially uninitialized;
/// it also has no drop regardless of the type of T.
#[repr(C)] // for cast from self ptr to value
pub union MaybeUninit<T> {
    empty: (),
    value: ManuallyDrop<T>,
}
// Why we don't use std's MaybeUninit on nightly? See the ptr method

impl<T> MaybeUninit<T> {
    /// Create a new MaybeUninit with uninitialized interior
    pub unsafe fn uninitialized() -> Self {
        MaybeUninit { empty: () }
    }

    /// Create a new MaybeUninit from the value `v`.
    pub fn from(v: T) -> Self {
        MaybeUninit { value: ManuallyDrop::new(v) }
    }

    // Raw pointer casts written so that we don't reference or access the
    // uninitialized interior value

    /// Return a raw pointer to the start of the interior array
    pub fn ptr(&self) -> *const T::Item
        where T: Array
    {
        // std MaybeUninit creates a &self.value reference here which is
        // not guaranteed to be sound in our case - we will partially
        // initialize the value, not always wholly.
        self as *const _ as *const T::Item
    }

    /// Return a mut raw pointer to the start of the interior array
    pub fn ptr_mut(&mut self) -> *mut T::Item
        where T: Array
    {
        self as *mut _ as *mut T::Item
    }
}
