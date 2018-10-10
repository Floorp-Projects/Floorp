//! This crate implements a structure that can be used as a generic array type.use
//! Core Rust array types `[T; N]` can't be used generically with
//! respect to `N`, so for example this:
//!
//! ```{should_fail}
//! struct Foo<T, N> {
//!     data: [T; N]
//! }
//! ```
//!
//! won't work.
//!
//! **generic-array** exports a `GenericArray<T,N>` type, which lets
//! the above be implemented as:
//!
//! ```
//! # use generic_array::{ArrayLength, GenericArray};
//! struct Foo<T, N: ArrayLength<T>> {
//!     data: GenericArray<T,N>
//! }
//! ```
//!
//! The `ArrayLength<T>` trait is implemented by default for
//! [unsigned integer types](../typenum/uint/index.html) from
//! [typenum](../typenum/index.html).
//!
//! For ease of use, an `arr!` macro is provided - example below:
//!
//! ```
//! # #[macro_use]
//! # extern crate generic_array;
//! # extern crate typenum;
//! # fn main() {
//! let array = arr![u32; 1, 2, 3];
//! assert_eq!(array[2], 3);
//! # }
//! ```

//#![deny(missing_docs)]
#![no_std]

pub extern crate typenum;
#[cfg(feature = "serde")]
extern crate serde;

mod hex;
mod impls;

#[cfg(feature = "serde")]
pub mod impl_serde;

use core::{mem, ptr, slice};

use core::marker::PhantomData;
use core::mem::ManuallyDrop;
pub use core::mem::transmute;
use core::ops::{Deref, DerefMut};

use typenum::bit::{B0, B1};
use typenum::uint::{UInt, UTerm, Unsigned};

#[cfg_attr(test, macro_use)]
pub mod arr;
pub mod iter;
pub use iter::GenericArrayIter;

/// Trait making `GenericArray` work, marking types to be used as length of an array
pub unsafe trait ArrayLength<T>: Unsigned {
    /// Associated type representing the array type for the number
    type ArrayType;
}

unsafe impl<T> ArrayLength<T> for UTerm {
    #[doc(hidden)]
    type ArrayType = ();
}

/// Internal type used to generate a struct of appropriate size
#[allow(dead_code)]
#[repr(C)]
#[doc(hidden)]
pub struct GenericArrayImplEven<T, U> {
    parent1: U,
    parent2: U,
    _marker: PhantomData<T>,
}

impl<T: Clone, U: Clone> Clone for GenericArrayImplEven<T, U> {
    fn clone(&self) -> GenericArrayImplEven<T, U> {
        GenericArrayImplEven {
            parent1: self.parent1.clone(),
            parent2: self.parent2.clone(),
            _marker: PhantomData,
        }
    }
}

impl<T: Copy, U: Copy> Copy for GenericArrayImplEven<T, U> {}

/// Internal type used to generate a struct of appropriate size
#[allow(dead_code)]
#[repr(C)]
#[doc(hidden)]
pub struct GenericArrayImplOdd<T, U> {
    parent1: U,
    parent2: U,
    data: T,
}

impl<T: Clone, U: Clone> Clone for GenericArrayImplOdd<T, U> {
    fn clone(&self) -> GenericArrayImplOdd<T, U> {
        GenericArrayImplOdd {
            parent1: self.parent1.clone(),
            parent2: self.parent2.clone(),
            data: self.data.clone(),
        }
    }
}

impl<T: Copy, U: Copy> Copy for GenericArrayImplOdd<T, U> {}

unsafe impl<T, N: ArrayLength<T>> ArrayLength<T> for UInt<N, B0> {
    #[doc(hidden)]
    type ArrayType = GenericArrayImplEven<T, N::ArrayType>;
}

unsafe impl<T, N: ArrayLength<T>> ArrayLength<T> for UInt<N, B1> {
    #[doc(hidden)]
    type ArrayType = GenericArrayImplOdd<T, N::ArrayType>;
}

/// Struct representing a generic array - `GenericArray<T, N>` works like [T; N]
#[allow(dead_code)]
pub struct GenericArray<T, U: ArrayLength<T>> {
    data: U::ArrayType,
}

impl<T, N> Deref for GenericArray<T, N>
where
    N: ArrayLength<T>,
{
    type Target = [T];

    fn deref(&self) -> &[T] {
        unsafe { slice::from_raw_parts(self as *const Self as *const T, N::to_usize()) }
    }
}

impl<T, N> DerefMut for GenericArray<T, N>
where
    N: ArrayLength<T>,
{
    fn deref_mut(&mut self) -> &mut [T] {
        unsafe { slice::from_raw_parts_mut(self as *mut Self as *mut T, N::to_usize()) }
    }
}

struct ArrayBuilder<T, N: ArrayLength<T>> {
    array: ManuallyDrop<GenericArray<T, N>>,
    position: usize,
}

impl<T, N: ArrayLength<T>> ArrayBuilder<T, N> {
    fn new() -> ArrayBuilder<T, N> {
        ArrayBuilder {
            array: ManuallyDrop::new(unsafe { mem::uninitialized() }),
            position: 0,
        }
    }

    fn into_inner(self) -> GenericArray<T, N> {
        let array = unsafe { ptr::read(&self.array) };

        mem::forget(self);

        ManuallyDrop::into_inner(array)
    }
}

impl<T, N: ArrayLength<T>> Drop for ArrayBuilder<T, N> {
    fn drop(&mut self) {
        for value in self.array.iter_mut().take(self.position) {
            unsafe {
                ptr::drop_in_place(value);
            }
        }
    }
}

struct ArrayConsumer<T, N: ArrayLength<T>> {
    array: ManuallyDrop<GenericArray<T, N>>,
    position: usize,
}

impl<T, N: ArrayLength<T>> ArrayConsumer<T, N> {
    fn new(array: GenericArray<T, N>) -> ArrayConsumer<T, N> {
        ArrayConsumer {
            array: ManuallyDrop::new(array),
            position: 0,
        }
    }
}

impl<T, N: ArrayLength<T>> Drop for ArrayConsumer<T, N> {
    fn drop(&mut self) {
        for i in self.position..N::to_usize() {
            unsafe {
                ptr::drop_in_place(self.array.get_unchecked_mut(i));
            }
        }
    }
}

impl<T, N> GenericArray<T, N>
where
    N: ArrayLength<T>,
{
    /// Initializes a new `GenericArray` instance using the given function.
    ///
    /// If the generator function panics while initializing the array,
    /// any already initialized elements will be dropped.
    pub fn generate<F>(f: F) -> GenericArray<T, N>
    where
        F: Fn(usize) -> T,
    {
        let mut destination = ArrayBuilder::new();

        for (i, dst) in destination.array.iter_mut().enumerate() {
            unsafe {
                ptr::write(dst, f(i));
            }

            destination.position += 1;
        }

        destination.into_inner()
    }

    /// Map a function over a slice to a `GenericArray`.
    ///
    /// The length of the slice *must* be equal to the length of the array.
    #[inline]
    pub fn map_slice<S, F: Fn(&S) -> T>(s: &[S], f: F) -> GenericArray<T, N> {
        assert_eq!(s.len(), N::to_usize());

        Self::generate(|i| f(unsafe { s.get_unchecked(i) }))
    }

    /// Maps a `GenericArray` to another `GenericArray`.
    ///
    /// If the mapping function panics, any already initialized elements in the new array
    /// will be dropped, AND any unused elements in the source array will also be dropped.
    pub fn map<U, F>(self, f: F) -> GenericArray<U, N>
    where
        F: Fn(T) -> U,
        N: ArrayLength<U>,
    {
        let mut source = ArrayConsumer::new(self);
        let mut destination = ArrayBuilder::new();

        for (dst, src) in destination.array.iter_mut().zip(source.array.iter()) {
            unsafe {
                ptr::write(dst, f(ptr::read(src)));
            }

            source.position += 1;
            destination.position += 1;
        }

        destination.into_inner()
    }

    /// Maps a `GenericArray` to another `GenericArray` by reference.
    ///
    /// If the mapping function panics, any already initialized elements will be dropped.
    #[inline]
    pub fn map_ref<U, F>(&self, f: F) -> GenericArray<U, N>
    where
        F: Fn(&T) -> U,
        N: ArrayLength<U>,
    {
        GenericArray::generate(|i| f(unsafe { self.get_unchecked(i) }))
    }

    /// Combines two `GenericArray` instances and iterates through both of them,
    /// initializing a new `GenericArray` with the result of the zipped mapping function.
    ///
    /// If the mapping function panics, any already initialized elements in the new array
    /// will be dropped, AND any unused elements in the source arrays will also be dropped.
    pub fn zip<B, U, F>(self, rhs: GenericArray<B, N>, f: F) -> GenericArray<U, N>
    where
        F: Fn(T, B) -> U,
        N: ArrayLength<B> + ArrayLength<U>,
    {
        let mut left = ArrayConsumer::new(self);
        let mut right = ArrayConsumer::new(rhs);

        let mut destination = ArrayBuilder::new();

        for (dst, (lhs, rhs)) in
            destination.array.iter_mut().zip(left.array.iter().zip(
                right.array.iter(),
            ))
        {
            unsafe {
                ptr::write(dst, f(ptr::read(lhs), ptr::read(rhs)));
            }

            destination.position += 1;
            left.position += 1;
            right.position += 1;
        }

        destination.into_inner()
    }

    /// Combines two `GenericArray` instances and iterates through both of them by reference,
    /// initializing a new `GenericArray` with the result of the zipped mapping function.
    ///
    /// If the mapping function panics, any already initialized elements will be dropped.
    pub fn zip_ref<B, U, F>(&self, rhs: &GenericArray<B, N>, f: F) -> GenericArray<U, N>
    where
        F: Fn(&T, &B) -> U,
        N: ArrayLength<B> + ArrayLength<U>,
    {
        GenericArray::generate(|i| unsafe {
            f(self.get_unchecked(i), rhs.get_unchecked(i))
        })
    }

    /// Extracts a slice containing the entire array.
    #[inline]
    pub fn as_slice(&self) -> &[T] {
        self.deref()
    }

    /// Extracts a mutable slice containing the entire array.
    #[inline]
    pub fn as_mut_slice(&mut self) -> &mut [T] {
        self.deref_mut()
    }

    /// Converts slice to a generic array reference with inferred length;
    ///
    /// Length of the slice must be equal to the length of the array.
    #[inline]
    pub fn from_slice(slice: &[T]) -> &GenericArray<T, N> {
        assert_eq!(slice.len(), N::to_usize());

        unsafe { &*(slice.as_ptr() as *const GenericArray<T, N>) }
    }

    /// Converts mutable slice to a mutable generic array reference
    ///
    /// Length of the slice must be equal to the length of the array.
    #[inline]
    pub fn from_mut_slice(slice: &mut [T]) -> &mut GenericArray<T, N> {
        assert_eq!(slice.len(), N::to_usize());

        unsafe { &mut *(slice.as_mut_ptr() as *mut GenericArray<T, N>) }
    }
}

impl<T: Clone, N> GenericArray<T, N>
where
    N: ArrayLength<T>,
{
    /// Construct a `GenericArray` from a slice by cloning its content
    ///
    /// Length of the slice must be equal to the length of the array
    #[inline]
    pub fn clone_from_slice(list: &[T]) -> GenericArray<T, N> {
        Self::from_exact_iter(list.iter().cloned()).expect(
            "Slice must be the same length as the array",
        )
    }
}

impl<T, N> GenericArray<T, N>
where
    N: ArrayLength<T>,
{
    pub fn from_exact_iter<I>(iter: I) -> Option<Self>
    where
        I: IntoIterator<Item = T>,
        <I as IntoIterator>::IntoIter: ExactSizeIterator,
    {
        let iter = iter.into_iter();

        if iter.len() == N::to_usize() {
            let mut destination = ArrayBuilder::new();

            for (dst, src) in destination.array.iter_mut().zip(iter.into_iter()) {
                unsafe {
                    ptr::write(dst, src);
                }

                destination.position += 1;
            }

            let array = unsafe { ptr::read(&destination.array) };

            mem::forget(destination);

            Some(ManuallyDrop::into_inner(array))
        } else {
            None
        }
    }
}

impl<T, N> ::core::iter::FromIterator<T> for GenericArray<T, N>
where
    N: ArrayLength<T>,
    T: Default,
{
    fn from_iter<I>(iter: I) -> GenericArray<T, N>
    where
        I: IntoIterator<Item = T>,
    {
        let mut destination = ArrayBuilder::new();

        let defaults = ::core::iter::repeat(()).map(|_| T::default());

        for (dst, src) in destination.array.iter_mut().zip(
            iter.into_iter().chain(defaults),
        )
        {
            unsafe {
                ptr::write(dst, src);
            }
        }

        destination.into_inner()
    }
}

#[cfg(test)]
mod test {
    // Compile with:
    // cargo rustc --lib --profile test --release --
    //      -C target-cpu=native -C opt-level=3 --emit asm
    // and view the assembly to make sure test_assembly generates
    // SIMD instructions instead of a niave loop.

    #[inline(never)]
    pub fn black_box<T>(val: T) -> T {
        use core::{mem, ptr};

        let ret = unsafe { ptr::read_volatile(&val) };
        mem::forget(val);
        ret
    }

    #[test]
    fn test_assembly() {
        let a = black_box(arr![i32; 1, 3, 5, 7]);
        let b = black_box(arr![i32; 2, 4, 6, 8]);

        let c = a.zip_ref(&b, |l, r| l + r);

        assert_eq!(c, arr![i32; 3, 7, 11, 15]);
    }
}
