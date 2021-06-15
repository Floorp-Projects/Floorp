//! Useful traits for manipulating sequences of data stored in `GenericArray`s

use super::*;
use core::{mem, ptr};
use core::ops::{Add, Sub};
use typenum::operator_aliases::*;

/// Defines some sequence with an associated length and iteration capabilities.
///
/// This is useful for passing N-length generic arrays as generics.
pub unsafe trait GenericSequence<T>: Sized + IntoIterator {
    /// `GenericArray` associated length
    type Length: ArrayLength<T>;

    /// Concrete sequence type used in conjuction with reference implementations of `GenericSequence`
    type Sequence: GenericSequence<T, Length = Self::Length> + FromIterator<T>;

    /// Initializes a new sequence instance using the given function.
    ///
    /// If the generator function panics while initializing the sequence,
    /// any already initialized elements will be dropped.
    fn generate<F>(f: F) -> Self::Sequence
    where
        F: FnMut(usize) -> T;

    #[doc(hidden)]
    fn inverted_zip<B, U, F>(
        self,
        lhs: GenericArray<B, Self::Length>,
        mut f: F,
    ) -> MappedSequence<GenericArray<B, Self::Length>, B, U>
    where
        GenericArray<B, Self::Length>: GenericSequence<B, Length = Self::Length>
            + MappedGenericSequence<B, U>,
        Self: MappedGenericSequence<T, U>,
        Self::Length: ArrayLength<B> + ArrayLength<U>,
        F: FnMut(B, Self::Item) -> U,
    {
        unsafe {
            let mut left = ArrayConsumer::new(lhs);

            let (left_array_iter, left_position) = left.iter_position();

            FromIterator::from_iter(
                left_array_iter
                    .zip(self.into_iter())
                    .map(|(l, right_value)| {
                        let left_value = ptr::read(l);

                        *left_position += 1;

                        f(left_value, right_value)
                    })
            )
        }
    }

    #[doc(hidden)]
    fn inverted_zip2<B, Lhs, U, F>(self, lhs: Lhs, mut f: F) -> MappedSequence<Lhs, B, U>
    where
        Lhs: GenericSequence<B, Length = Self::Length> + MappedGenericSequence<B, U>,
        Self: MappedGenericSequence<T, U>,
        Self::Length: ArrayLength<B> + ArrayLength<U>,
        F: FnMut(Lhs::Item, Self::Item) -> U,
    {
        FromIterator::from_iter(lhs.into_iter().zip(self.into_iter()).map(|(l, r)| f(l, r)))
    }
}

/// Accessor for `GenericSequence` item type, which is really `IntoIterator::Item`
///
/// For deeply nested generic mapped sequence types, like shown in `tests/generics.rs`,
/// this can be useful for keeping things organized.
pub type SequenceItem<T> = <T as IntoIterator>::Item;

unsafe impl<'a, T: 'a, S: GenericSequence<T>> GenericSequence<T> for &'a S
where
    &'a S: IntoIterator,
{
    type Length = S::Length;
    type Sequence = S::Sequence;

    #[inline]
    fn generate<F>(f: F) -> Self::Sequence
    where
        F: FnMut(usize) -> T,
    {
        S::generate(f)
    }
}

unsafe impl<'a, T: 'a, S: GenericSequence<T>> GenericSequence<T> for &'a mut S
where
    &'a mut S: IntoIterator,
{
    type Length = S::Length;
    type Sequence = S::Sequence;

    #[inline]
    fn generate<F>(f: F) -> Self::Sequence
    where
        F: FnMut(usize) -> T,
    {
        S::generate(f)
    }
}

/// Defines any `GenericSequence` which can be lengthened or extended by appending
/// or prepending an element to it.
///
/// Any lengthened sequence can be shortened back to the original using `pop_front` or `pop_back`
pub unsafe trait Lengthen<T>: Sized + GenericSequence<T> {
    /// `GenericSequence` that has one more element than `Self`
    type Longer: Shorten<T, Shorter = Self>;

    /// Returns a new array with the given element appended to the end of it.
    ///
    /// Example:
    ///
    /// ```ignore
    /// let a = arr![i32; 1, 2, 3];
    ///
    /// let b = a.append(4);
    ///
    /// assert_eq!(b, arr![i32; 1, 2, 3, 4]);
    /// ```
    fn append(self, last: T) -> Self::Longer;

    /// Returns a new array with the given element prepended to the front of it.
    ///
    /// Example:
    ///
    /// ```ignore
    /// let a = arr![i32; 1, 2, 3];
    ///
    /// let b = a.prepend(4);
    ///
    /// assert_eq!(b, arr![i32; 4, 1, 2, 3]);
    /// ```
    fn prepend(self, first: T) -> Self::Longer;
}

/// Defines a `GenericSequence` which can be shortened by removing the first or last element from it.
///
/// Additionally, any shortened sequence can be lengthened by
/// appending or prepending an element to it.
pub unsafe trait Shorten<T>: Sized + GenericSequence<T> {
    /// `GenericSequence` that has one less element than `Self`
    type Shorter: Lengthen<T, Longer = Self>;

    /// Returns a new array without the last element, and the last element.
    ///
    /// Example:
    ///
    /// ```ignore
    /// let a = arr![i32; 1, 2, 3, 4];
    ///
    /// let (init, last) = a.pop_back();
    ///
    /// assert_eq!(init, arr![i32; 1, 2, 3]);
    /// assert_eq!(last, 4);
    /// ```
    fn pop_back(self) -> (Self::Shorter, T);

    /// Returns a new array without the first element, and the first element.
    /// Example:
    ///
    /// ```ignore
    /// let a = arr![i32; 1, 2, 3, 4];
    ///
    /// let (head, tail) = a.pop_front();
    ///
    /// assert_eq!(head, 1);
    /// assert_eq!(tail, arr![i32; 2, 3, 4]);
    /// ```
    fn pop_front(self) -> (T, Self::Shorter);
}

unsafe impl<T, N: ArrayLength<T>> Lengthen<T> for GenericArray<T, N>
where
    N: Add<B1>,
    Add1<N>: ArrayLength<T>,
    Add1<N>: Sub<B1, Output = N>,
    Sub1<Add1<N>>: ArrayLength<T>,
{
    type Longer = GenericArray<T, Add1<N>>;

    fn append(self, last: T) -> Self::Longer {
        let mut longer: Self::Longer = unsafe { mem::uninitialized() };

        unsafe {
            ptr::write(longer.as_mut_ptr() as *mut _, self);
            ptr::write(&mut longer[N::to_usize()], last);
        }

        longer
    }

    fn prepend(self, first: T) -> Self::Longer {
        let mut longer: Self::Longer = unsafe { mem::uninitialized() };

        let longer_ptr = longer.as_mut_ptr();

        unsafe {
            ptr::write(longer_ptr as *mut _, first);
            ptr::write(longer_ptr.offset(1) as *mut _, self);
        }

        longer
    }
}

unsafe impl<T, N: ArrayLength<T>> Shorten<T> for GenericArray<T, N>
where
    N: Sub<B1>,
    Sub1<N>: ArrayLength<T>,
    Sub1<N>: Add<B1, Output = N>,
    Add1<Sub1<N>>: ArrayLength<T>,
{
    type Shorter = GenericArray<T, Sub1<N>>;

    fn pop_back(self) -> (Self::Shorter, T) {
        let init_ptr = self.as_ptr();
        let last_ptr = unsafe { init_ptr.offset(Sub1::<N>::to_usize() as isize) };

        let init = unsafe { ptr::read(init_ptr as _) };
        let last = unsafe { ptr::read(last_ptr as _) };

        mem::forget(self);

        (init, last)
    }

    fn pop_front(self) -> (T, Self::Shorter) {
        let head_ptr = self.as_ptr();
        let tail_ptr = unsafe { head_ptr.offset(1) };

        let head = unsafe { ptr::read(head_ptr as _) };
        let tail = unsafe { ptr::read(tail_ptr as _) };

        mem::forget(self);

        (head, tail)
    }
}

/// Defines a `GenericSequence` that can be split into two parts at a given pivot index.
pub unsafe trait Split<T, K>: GenericSequence<T>
where
    K: ArrayLength<T>,
{
    /// First part of the resulting split array
    type First: GenericSequence<T>;
    /// Second part of the resulting split array
    type Second: GenericSequence<T>;

    /// Splits an array at the given index, returning the separate parts of the array.
    fn split(self) -> (Self::First, Self::Second);
}

unsafe impl<T, N, K> Split<T, K> for GenericArray<T, N>
where
    N: ArrayLength<T>,
    K: ArrayLength<T>,
    N: Sub<K>,
    Diff<N, K>: ArrayLength<T>,
{
    type First = GenericArray<T, K>;
    type Second = GenericArray<T, Diff<N, K>>;

    fn split(self) -> (Self::First, Self::Second) {
        let head_ptr = self.as_ptr();
        let tail_ptr = unsafe { head_ptr.offset(K::to_usize() as isize) };

        let head = unsafe { ptr::read(head_ptr as _) };
        let tail = unsafe { ptr::read(tail_ptr as _) };

        mem::forget(self);

        (head, tail)
    }
}

/// Defines `GenericSequence`s which can be joined together, forming a larger array.
pub unsafe trait Concat<T, M>: GenericSequence<T>
where
    M: ArrayLength<T>,
{
    /// Sequence to be concatenated with `self`
    type Rest: GenericSequence<T, Length = M>;

    /// Resulting sequence formed by the concatenation.
    type Output: GenericSequence<T>;

    /// Concatenate, or join, two sequences.
    fn concat(self, rest: Self::Rest) -> Self::Output;
}

unsafe impl<T, N, M> Concat<T, M> for GenericArray<T, N>
where
    N: ArrayLength<T> + Add<M>,
    M: ArrayLength<T>,
    Sum<N, M>: ArrayLength<T>,
{
    type Rest = GenericArray<T, M>;
    type Output = GenericArray<T, Sum<N, M>>;

    fn concat(self, rest: Self::Rest) -> Self::Output {
        let mut output: Self::Output = unsafe { mem::uninitialized() };

        let output_ptr = output.as_mut_ptr();

        unsafe {
            ptr::write(output_ptr as *mut _, self);
            ptr::write(output_ptr.offset(N::to_usize() as isize) as *mut _, rest);
        }

        output
    }
}
