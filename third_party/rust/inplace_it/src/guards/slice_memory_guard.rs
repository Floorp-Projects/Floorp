use core::{
    ops::{Deref, DerefMut},
    mem::{MaybeUninit, transmute},
    ptr::{drop_in_place, write},
};

/// Guard-struct used for correctly initialize uninitialized memory and `drop` it when guard goes out of scope.
/// Usually, you *should not* use this struct to handle your memory.
///
/// ### Safety
///
/// If you use this struct manually, remember: `&mut [MaybeUninit<T>]`'s content will be overwriten while initialization.
/// So it is *not safe* to apply this struct to already initialized data and it can lead to *memory leaks*.
///
/// ### Example
/// ```rust
/// use inplace_it::SliceMemoryGuard;
/// use std::mem::MaybeUninit;
///
/// // Placing uninitialized memory
/// let mut memory: [MaybeUninit<usize>; 100] = unsafe { MaybeUninit::uninit().assume_init() };
/// // Initializing guard
/// let mut memory_guard = unsafe {
///     SliceMemoryGuard::new(
///         // Borrowing memory
///         &mut memory,
///         // Forwarding initializer
///         |index| index * 2
///     )
/// };
///
/// // For now, memory contains content like [0, 2, 4, 6, ..., 196, 198]
///
/// // Using memory
/// // Sum of [0, 2, 4, 6, ..., 196, 198] = sum of [0, 1, 2, 3, ..., 98, 99] * 2 = ( 99 * (99+1) ) / 2 * 2
/// let sum: usize = memory_guard.iter().sum();
/// assert_eq!(sum, 99 * 100);
///
/// ```
pub struct SliceMemoryGuard<'a, T> {
    memory: &'a mut [MaybeUninit<T>],
}

impl<'a, T> SliceMemoryGuard<'a, T> {
    /// Initialize memory guard
    #[inline]
    pub unsafe fn new(memory: &'a mut [MaybeUninit<T>], mut init: impl FnMut(usize) -> T) -> Self {
        for (index, item) in memory.into_iter().enumerate() {
            write(item.as_mut_ptr(), init(index));
        }
        SliceMemoryGuard { memory }
    }
}

impl<'a, T> Deref for SliceMemoryGuard<'a, T> {
    type Target = [T];

    #[inline]
    fn deref(&self) -> &Self::Target {
        unsafe { transmute::<&[MaybeUninit<T>], &[T]>(&self.memory) }
    }
}

impl<'a, T> DerefMut for SliceMemoryGuard<'a, T> {
    #[inline]
    fn deref_mut(&mut self) -> &mut Self::Target {
        unsafe { transmute::<&mut [MaybeUninit<T>], &mut [T]>(&mut self.memory) }
    }
}

impl<'a, T> Drop for SliceMemoryGuard<'a, T> {
    #[inline]
    fn drop(&mut self) {
        for item in self.memory.into_iter() {
            unsafe { drop_in_place(item.as_mut_ptr()); }
        }
    }
}
