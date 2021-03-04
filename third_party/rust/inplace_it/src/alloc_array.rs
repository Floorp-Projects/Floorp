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

/// `inplace_or_alloc_from_iter` is helper function used to easy trying to place data from `Iterator`.
///
/// It tries to get upper bound of `size_hint` of iterator and forward it to `inplace_or_alloc_array` function.
/// It there is not upper bound hint from iterator, then it just `collect` your data into `Vec`.
///
/// If iterator contains more data that `size_hint` said
/// (and more than `try_inplace_array` function placed on stack),
/// then items will be moved and collected (by iterating) into `Vec` also.
///
/// # Examples
///
/// ```rust
/// // Some random number to demonstrate
/// let count = 42;
/// let iterator = 0..count;
///
/// let result = ::inplace_it::inplace_or_alloc_from_iter(iterator.clone(), |mem| {
///      assert_eq!(mem.len(), count);
///      assert!(mem.iter().cloned().eq(iterator));
///
///      // Some result
///      format!("{}", mem.len())
///  });
///  assert_eq!(result, format!("{}", count));
/// ```
pub fn inplace_or_alloc_from_iter<Iter, R, Consumer>(iter: Iter, consumer: Consumer) -> R
    where Iter: Iterator,
          Consumer: FnOnce(&mut [Iter::Item]) -> R,
{
    match iter.size_hint().1 {
        Some(upper_bound_hint) => {
            inplace_or_alloc_array(upper_bound_hint, |uninitialized_guard| {
                match uninitialized_guard.init_with_dyn_iter(iter) {
                    Ok(mut guard) => consumer(&mut *guard),
                    Err(mut vec) => consumer(&mut *vec),
                }
            })
        }
        None => {
            let mut vec = iter.collect::<Vec<_>>();
            consumer(&mut *vec)
        }
    }
}
