use core::{
    mem::MaybeUninit,
    ops::{
        RangeBounds,
        Bound,
    },
};
use crate::guards::SliceMemoryGuard;

/// Guard-struct used to own uninitialized memory and provide functions for initializing it.
/// Usually, you *should not* use this struct to handle your memory.
///
/// Initializing functions spawns [SliceMemoryGuard] which will provide access to initialized memory.
/// It also means memory can be used again after [SliceMemoryGuard] is dropped.
///
/// ### Safety
///
/// If you use this struct manually, remember: `&mut [MaybeUninit<T>]`'s content will be overwriten while initialization.
/// So it is *not safe* to apply this struct to already initialized data and it can lead to *memory leaks*.
///
/// ### Example
/// ```rust
/// use inplace_it::UninitializedSliceMemoryGuard;
/// use std::mem::MaybeUninit;
///
/// // Placing uninitialized memory
/// let mut memory: [MaybeUninit<usize>; 100] = unsafe { MaybeUninit::uninit().assume_init() };
/// // Initializing guard
/// let mut uninit_memory_guard = unsafe { UninitializedSliceMemoryGuard::new(&mut memory) };
///
/// {
///     // Initializing memory
///     let mut memory_guard = uninit_memory_guard
///         // we need to call .borrow() because out init-API consumes uninit-guard
///         .borrow()
///         // then passing initialize closure and the guard is ok
///         .init(|index| index * 2);
///     // For now, memory contains content like [0, 2, 4, 6, ..., 196, 198]
///
///     // Using memory
///     // Sum of [0, 2, 4, 6, ..., 196, 198] = sum of [0, 1, 2, 3, ..., 98, 99] * 2 = ( 99 * (99+1) ) / 2 * 2
///     let sum: usize = memory_guard.iter().sum();
///     assert_eq!(sum, 99 * 100);
///     // memory_guard dropped here
/// }
///
/// // uninit_memory_guard is available again now
///
/// {
///     // Initializing memory
///     let mut memory_guard = uninit_memory_guard.init(|index| index * index);
///     // For now, memory contains content like [0, 1, 4, 9, ..., 9604, 9801]
///
///     // Using memory
///     // Sum of [0, 1, 4, 9, ..., 9604, 9801] = 99 * (99 + 1) * (2 * 99 + 1) / 6
///     let sum: usize = memory_guard.iter().sum();
///     assert_eq!(sum, 99 * (99 + 1) * (2 * 99 + 1) / 6);
///     // memory_guard dropped here
/// }
///
/// ```
///
/// [SliceMemoryGuard]: struct.SliceMemoryGuard.html
pub struct UninitializedSliceMemoryGuard<'a, T> {
    memory: &'a mut [MaybeUninit<T>],
}

impl<'a, T> UninitializedSliceMemoryGuard<'a, T> {
    /// Initialize memory guard
    #[inline]
    pub unsafe fn new(memory: &'a mut [MaybeUninit<T>]) -> Self {
        Self { memory }
    }

    /// Get the length of memory slice
    #[inline]
    pub fn len(&self) -> usize {
        self.memory.len()
    }

    /// Construct new memory guard with new bounds.
    ///
    /// Can be used to shrink memory.
    ///
    /// ### Panics
    ///
    /// Panic can be reached when given `range` is out of memory's range.
    #[inline]
    pub fn slice(self, range: impl RangeBounds<usize>) -> Self {
        let start = match range.start_bound() {
            Bound::Excluded(n) => n.saturating_add(1),
            Bound::Included(n) => *n,
            Bound::Unbounded => 0,
        };
        let end = match range.end_bound() {
            Bound::Excluded(n) => *n,
            Bound::Included(n) => n.saturating_add(1),
            Bound::Unbounded => self.memory.len(),
        };
        Self {
            memory: &mut self.memory[start..end],
        }
    }

    /// Initialize memory and make new guard of initialized memory.
    /// Given `init` closure will be used to initialize elements of memory slice.
    #[inline]
    pub fn init(self, init: impl FnMut(usize) -> T) -> SliceMemoryGuard<'a, T> {
        unsafe {
            SliceMemoryGuard::new(self.memory, init)
        }
    }

    /// Initialize memory and make new guard of initialized memory.
    /// Given `source` slice will be used to initialize elements of memory slice.
    /// Returned guard will contain sliced memory to `source`'s length.
    ///
    /// ### Panics
    ///
    /// Panic can be reached when given `source`'s range is out of memory's range.
    #[inline]
    pub fn init_copy_of(self, source: &[T]) -> SliceMemoryGuard<'a, T>
        where T: Clone
    {
        self.slice(..source.len()).init(|index| { source[index].clone() })
    }

    /// Initialize memory and make new guard of initialized memory.
    /// Given `iter` exact-size iterator will be used to initialize elements of memory slice.
    /// Returned guard will contain sliced memory to `iter`'s length.
    ///
    /// ### Panics
    ///
    /// Panic can be reached when given `iter`'s length is out of memory's range.
    #[inline]
    pub fn init_with_iter(self, mut iter: impl ExactSizeIterator<Item = T>) -> SliceMemoryGuard<'a, T> {
        self.slice(..iter.len()).init(|_index| { iter.next().unwrap() })
    }

    /// Initialize memory guard using given iterator.
    /// Automatically shrink's memory to given items' count.
    /// `Ok(guard)` will be returned in this case.
    ///
    /// If items' count is too large to place in memory, moves it into new `Vec` and continue collecting into it.
    /// `Err(vec)` will be returned in this case.
    #[inline]
    pub fn init_with_dyn_iter(self, iter: impl Iterator<Item = T>) -> Result<SliceMemoryGuard<'a, T>, Vec<T>> {
        unsafe {
            SliceMemoryGuard::new_from_iter(self.memory, iter)
        }
    }

    /// Create new uninit memory guard with less or equal lifetime to original guard's lifetime.
    /// This function should be used to reuse memory because init-API consumes the guard.
    #[inline]
    pub fn borrow(&mut self) -> UninitializedSliceMemoryGuard<T> {
        unsafe {
            UninitializedSliceMemoryGuard::new(self.memory)
        }
    }
}
