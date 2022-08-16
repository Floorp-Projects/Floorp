use core::{
    ops::{Deref, DerefMut},
    mem::{MaybeUninit, transmute},
    ptr::{drop_in_place, write},
};
use std::ptr::copy_nonoverlapping;

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

    /// Initialize memory guard using given iterator.
    /// Automatically shrink's memory to given items' count.
    /// `Ok(guard)` will be returned in this case.
    ///
    /// If items' count is too large to place in memory, moves it into new `Vec` and continue collecting into it.
    /// `Err(vec)` will be returned in this case.
    #[inline]
    pub unsafe fn new_from_iter(memory: &'a mut [MaybeUninit<T>], mut iter: impl Iterator<Item=T>) -> Result<Self, Vec<T>> {
        // Fulfilling placed memory
        for (index, item) in memory.into_iter().enumerate() {
            match iter.next() {
                // While iterator returns new value, write it
                Some(value) => {
                    write(item.as_mut_ptr(), value);
                }
                // When it returns None then slicing memory and returning the guard
                None => {
                    return Ok(SliceMemoryGuard {
                        memory: &mut memory[0..index],
                    });
                }
            }
        }

        if let Some(next_item) = iter.next() {
            // If iterator still contains values to return, collect it into the vector
            let mut vec = Vec::<T>::with_capacity(
                // We cannot trust the `size_hint` anymore
                memory.len() + 1
            );

            // First, copying already fulfilled memory into the heap
            vec.set_len(memory.len());
            copy_nonoverlapping(memory.as_mut_ptr() as *mut T, vec.as_mut_ptr(), memory.len());

            // Then, append it with the rest iterator's items
            vec.push(next_item);
            vec.extend(iter);

            Err(vec)
        } else {
            // If iterator is done after fulfilling all available memory, just return the guard
            Ok(SliceMemoryGuard { memory })
        }
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn new_from_iter_uses_exactly_same_count_that_collects() {
        let mut memory: [MaybeUninit<usize>; 128] = unsafe { MaybeUninit::uninit().assume_init() };

        // 0 and 128 cases should work also fine
        for count in 0..128 {
            let result = unsafe { SliceMemoryGuard::new_from_iter(&mut memory, 0..count) };
            assert!(result.is_ok());
            let guard = result.unwrap();
            assert_eq!(guard.len(), count);
            assert!(guard.iter().cloned().eq(0..count));
        }

        // cases when Vec should be returned
        for count in [129, 200, 512].iter().cloned() {
            let result = unsafe { SliceMemoryGuard::new_from_iter(&mut memory, 0..count) };
            assert!(result.is_err());
            let vec = result.err().unwrap();
            assert_eq!(vec.len(), count);
            assert_eq!(vec, (0..count).collect::<Vec<_>>());
        }
    }
}
