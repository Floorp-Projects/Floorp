use crate::guards::UninitializedSliceMemoryGuard;
use std::mem::MaybeUninit;
use crate::try_inplace_array;

/// `alloc_array` is used when `inplace_or_alloc_array` realize that the size of requested array of `T`
/// is too large and should be replaced in the heap.
///
/// It allocates a vector with `size` elements and fills it up with help of `init` closure
/// and then pass a reference to a slice of the vector into the `consumer` closure.
/// `consumer`'s result will be returned.
pub fn alloc_array<T, R, Consumer: FnOnce(UninitializedSliceMemoryGuard<T>) -> R>(size: usize, consumer: Consumer) -> R {
    unsafe {
        let mut memory_holder = Vec::<MaybeUninit<T>>::with_capacity(size);
        memory_holder.set_len(size);
        let result = consumer(UninitializedSliceMemoryGuard::new(&mut *memory_holder));
        memory_holder.set_len(0);
        result
    }
}

/// `inplace_or_alloc_array` is a central function of this crate.
///  It's trying to place an array of `T` on the stack and pass the guard of memory into the
/// `consumer` closure. `consumer`'s result will be returned.
///
/// If the result of array of `T` is more than 4096 then the vector will be allocated
/// in the heap and will be used instead of stack-based fixed-size array.
///
/// Sometimes size of allocated array might be more than requested. For sizes larger than 32,
/// the following formula is used: `roundUp(size/32)*32`. This is a simplification that used
/// for keeping code short, simple and able to optimize.
/// For example, for requested 50 item `[T; 64]` will be allocated.
/// For 120 items - `[T; 128]` and so on.
///
/// Note that rounding size up is working for fixed-sized arrays only. If function decides to
/// allocate a vector then its size will be equal to requested.
///
/// # Examples
///
/// ```rust
/// use inplace_it::{
///     inplace_or_alloc_array,
///     UninitializedSliceMemoryGuard,
/// };
///
/// let sum: u16 = inplace_or_alloc_array(100, |uninit_guard: UninitializedSliceMemoryGuard<u16>| {
///     assert_eq!(uninit_guard.len(), 128);
///     // For now, our memory is placed/allocated but uninitialized.
///     // Let's initialize it!
///     let guard = uninit_guard.init(|index| index as u16 * 2);
///     // For now, memory contains content like [0, 2, 4, 6, ..., 252, 254]
///     guard.iter().sum()
/// });
/// // Sum of [0, 2, 4, 6, ..., 252, 254] = sum of [0, 1, 2, 3, ..., 126, 127] * 2 = ( 127 * (127+1) ) / 2 * 2
/// assert_eq!(sum, 127 * 128);
/// ```
pub fn inplace_or_alloc_array<T, R, Consumer>(size: usize, consumer: Consumer) -> R
    where Consumer: FnOnce(UninitializedSliceMemoryGuard<T>) -> R
{
    match try_inplace_array(size, consumer) {
        Ok(result) => result,
        Err(consumer) => alloc_array(size, consumer),
    }
}
