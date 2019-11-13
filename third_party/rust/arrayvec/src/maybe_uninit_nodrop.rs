
use array::Array;
use nodrop::NoDrop;
use std::mem::uninitialized;

/// A combination of NoDrop and “maybe uninitialized”;
/// this wraps a value that can be wholly or partially uninitialized.
///
/// NOTE: This is known to not be a good solution, but it's the one we have kept
/// working on stable Rust. Stable improvements are encouraged, in any form,
/// but of course we are waiting for a real, stable, MaybeUninit.
pub struct MaybeUninit<T>(NoDrop<T>);
// why don't we use ManuallyDrop here: It doesn't inhibit
// enum layout optimizations that depend on T, and we support older Rust.

impl<T> MaybeUninit<T> {
    /// Create a new MaybeUninit with uninitialized interior
    pub unsafe fn uninitialized() -> Self {
        Self::from(uninitialized())
    }

    /// Create a new MaybeUninit from the value `v`.
    pub fn from(v: T) -> Self {
        MaybeUninit(NoDrop::new(v))
    }

    /// Return a raw pointer to the start of the interior array
    pub fn ptr(&self) -> *const T::Item
        where T: Array
    {
        &*self.0 as *const T as *const _
    }

    /// Return a mut raw pointer to the start of the interior array
    pub fn ptr_mut(&mut self) -> *mut T::Item
        where T: Array
    {
        &mut *self.0 as *mut T as *mut _
    }
}

