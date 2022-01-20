// Copyright © 2019 The Rust Fuzz Project Developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Wrappers around raw, unstructured bytes.

use crate::{Arbitrary, Error, Result};
use std::marker::PhantomData;
use std::{mem, ops};

/// A source of unstructured data.
///
/// An `Unstructured` helps `Arbitrary` implementations interpret raw data
/// (typically provided by a fuzzer) as a "DNA string" that describes how to
/// construct the `Arbitrary` type. The goal is that a small change to the "DNA
/// string" (the raw data wrapped by an `Unstructured`) results in a small
/// change to the generated `Arbitrary` instance. This helps a fuzzer
/// efficiently explore the `Arbitrary`'s input space.
///
/// `Unstructured` is deterministic: given the same raw data, the same series of
/// API calls will return the same results (modulo system resource constraints,
/// like running out of memory). However, `Unstructured` does not guarantee
/// anything beyond that: it makes not guarantee that it will yield bytes from
/// the underlying data in any particular order.
///
/// You shouldn't generally need to use an `Unstructured` unless you are writing
/// a custom `Arbitrary` implementation by hand, instead of deriving it. Mostly,
/// you should just be passing it through to nested `Arbitrary::arbitrary`
/// calls.
///
/// # Example
///
/// Imagine you were writing a color conversion crate. You might want to write
/// fuzz tests that take a random RGB color and assert various properties, run
/// functions and make sure nothing panics, etc.
///
/// Below is what translating the fuzzer's raw input into an `Unstructured` and
/// using that to generate an arbitrary RGB color might look like:
///
/// ```
/// # #[cfg(feature = "derive")] fn foo() {
/// use arbitrary::{Arbitrary, Unstructured};
///
/// /// An RGB color.
/// #[derive(Arbitrary)]
/// pub struct Rgb {
///     r: u8,
///     g: u8,
///     b: u8,
/// }
///
/// // Get the raw bytes from the fuzzer.
/// #   let get_input_from_fuzzer = || &[];
/// let raw_data: &[u8] = get_input_from_fuzzer();
///
/// // Wrap it in an `Unstructured`.
/// let mut unstructured = Unstructured::new(raw_data);
///
/// // Generate an `Rgb` color and run our checks.
/// if let Ok(rgb) = Rgb::arbitrary(&mut unstructured) {
/// #   let run_my_color_conversion_checks = |_| {};
///     run_my_color_conversion_checks(rgb);
/// }
/// # }
/// ```
pub struct Unstructured<'a> {
    data: &'a [u8],
}

impl<'a> Unstructured<'a> {
    /// Create a new `Unstructured` from the given raw data.
    ///
    /// # Example
    ///
    /// ```
    /// use arbitrary::Unstructured;
    ///
    /// let u = Unstructured::new(&[1, 2, 3, 4]);
    /// ```
    pub fn new(data: &'a [u8]) -> Self {
        Unstructured { data }
    }

    /// Get the number of remaining bytes of underlying data that are still
    /// available.
    ///
    /// # Example
    ///
    /// ```
    /// use arbitrary::{Arbitrary, Unstructured};
    ///
    /// let mut u = Unstructured::new(&[1, 2, 3]);
    ///
    /// // Initially have three bytes of data.
    /// assert_eq!(u.len(), 3);
    ///
    /// // Generating a `bool` consumes one byte from the underlying data, so
    /// // we are left with two bytes afterwards.
    /// let _ = bool::arbitrary(&mut u);
    /// assert_eq!(u.len(), 2);
    /// ```
    #[inline]
    pub fn len(&self) -> usize {
        self.data.len()
    }

    /// Is the underlying unstructured data exhausted?
    ///
    /// `unstructured.is_empty()` is the same as `unstructured.len() == 0`.
    ///
    /// # Example
    ///
    /// ```
    /// use arbitrary::{Arbitrary, Unstructured};
    ///
    /// let mut u = Unstructured::new(&[1, 2, 3, 4]);
    ///
    /// // Initially, we are not empty.
    /// assert!(!u.is_empty());
    ///
    /// // Generating a `u32` consumes all four bytes of the underlying data, so
    /// // we become empty afterwards.
    /// let _ = u32::arbitrary(&mut u);
    /// assert!(u.is_empty());
    /// ```
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Generate an arbitrary instance of `A`.
    ///
    /// This is simply a helper method that is equivalent to `<A as
    /// Arbitrary>::arbitrary(self)`. This helper is a little bit more concise,
    /// and can be used in situations where Rust's type inference will figure
    /// out what `A` should be.
    ///
    /// # Example
    ///
    /// ```
    /// # #[cfg(feature="derive")] fn foo() -> arbitrary::Result<()> {
    /// use arbitrary::{Arbitrary, Unstructured};
    ///
    /// #[derive(Arbitrary)]
    /// struct MyType {
    ///     // ...
    /// }
    ///
    /// fn do_stuff(value: MyType) {
    /// #   let _ = value;
    ///     // ...
    /// }
    ///
    /// let mut u = Unstructured::new(&[1, 2, 3, 4]);
    ///
    /// // Rust's type inference can figure out that `value` should be of type
    /// // `MyType` here:
    /// let value = u.arbitrary()?;
    /// do_stuff(value);
    /// # Ok(()) }
    /// ```
    pub fn arbitrary<A>(&mut self) -> Result<A>
    where
        A: Arbitrary<'a>,
    {
        <A as Arbitrary<'a>>::arbitrary(self)
    }

    /// Get the number of elements to insert when building up a collection of
    /// arbitrary `ElementType`s.
    ///
    /// This uses the [`<ElementType as
    /// Arbitrary>::size_hint`][crate::Arbitrary::size_hint] method to smartly
    /// choose a length such that we most likely have enough underlying bytes to
    /// construct that many arbitrary `ElementType`s.
    ///
    /// This should only be called within an `Arbitrary` implementation.
    ///
    /// # Example
    ///
    /// ```
    /// use arbitrary::{Arbitrary, Result, Unstructured};
    /// # pub struct MyCollection<T> { _t: std::marker::PhantomData<T> }
    /// # impl<T> MyCollection<T> {
    /// #     pub fn with_capacity(capacity: usize) -> Self { MyCollection { _t: std::marker::PhantomData } }
    /// #     pub fn insert(&mut self, element: T) {}
    /// # }
    ///
    /// impl<'a, T> Arbitrary<'a> for MyCollection<T>
    /// where
    ///     T: Arbitrary<'a>,
    /// {
    ///     fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self> {
    ///         // Get the number of `T`s we should insert into our collection.
    ///         let len = u.arbitrary_len::<T>()?;
    ///
    ///         // And then create a collection of that length!
    ///         let mut my_collection = MyCollection::with_capacity(len);
    ///         for _ in 0..len {
    ///             let element = T::arbitrary(u)?;
    ///             my_collection.insert(element);
    ///         }
    ///
    ///         Ok(my_collection)
    ///     }
    /// }
    /// ```
    pub fn arbitrary_len<ElementType>(&mut self) -> Result<usize>
    where
        ElementType: Arbitrary<'a>,
    {
        let byte_size = self.arbitrary_byte_size()?;
        let (lower, upper) = <ElementType as Arbitrary>::size_hint(0);
        let elem_size = upper.unwrap_or_else(|| lower * 2);
        let elem_size = std::cmp::max(1, elem_size);
        Ok(byte_size / elem_size)
    }

    fn arbitrary_byte_size(&mut self) -> Result<usize> {
        if self.data.is_empty() {
            Ok(0)
        } else if self.data.len() == 1 {
            self.data = &[];
            Ok(0)
        } else {
            // Take lengths from the end of the data, since the `libFuzzer` folks
            // found that this lets fuzzers more efficiently explore the input
            // space.
            //
            // https://github.com/rust-fuzz/libfuzzer-sys/blob/0c450753/libfuzzer/utils/FuzzedDataProvider.h#L92-L97

            // We only consume as many bytes as necessary to cover the entire
            // range of the byte string.
            let len = if self.data.len() <= std::u8::MAX as usize + 1 {
                let bytes = 1;
                let max_size = self.data.len() - bytes;
                let (rest, for_size) = self.data.split_at(max_size);
                self.data = rest;
                Self::int_in_range_impl(0..=max_size as u8, for_size.iter().copied())?.0 as usize
            } else if self.data.len() <= std::u16::MAX as usize + 1 {
                let bytes = 2;
                let max_size = self.data.len() - bytes;
                let (rest, for_size) = self.data.split_at(max_size);
                self.data = rest;
                Self::int_in_range_impl(0..=max_size as u16, for_size.iter().copied())?.0 as usize
            } else if self.data.len() <= std::u32::MAX as usize + 1 {
                let bytes = 4;
                let max_size = self.data.len() - bytes;
                let (rest, for_size) = self.data.split_at(max_size);
                self.data = rest;
                Self::int_in_range_impl(0..=max_size as u32, for_size.iter().copied())?.0 as usize
            } else {
                let bytes = 8;
                let max_size = self.data.len() - bytes;
                let (rest, for_size) = self.data.split_at(max_size);
                self.data = rest;
                Self::int_in_range_impl(0..=max_size as u64, for_size.iter().copied())?.0 as usize
            };

            Ok(len)
        }
    }

    /// Generate an integer within the given range.
    ///
    /// Do not use this to generate the size of a collection. Use
    /// `arbitrary_len` instead.
    ///
    /// # Panics
    ///
    /// Panics if `range.start >= range.end`. That is, the given range must be
    /// non-empty.
    ///
    /// # Example
    ///
    /// ```
    /// use arbitrary::{Arbitrary, Unstructured};
    ///
    /// let mut u = Unstructured::new(&[1, 2, 3, 4]);
    ///
    /// let x: i32 = u.int_in_range(-5_000..=-1_000)
    ///     .expect("constructed `u` with enough bytes to generate an `i32`");
    ///
    /// assert!(-5_000 <= x);
    /// assert!(x <= -1_000);
    /// ```
    pub fn int_in_range<T>(&mut self, range: ops::RangeInclusive<T>) -> Result<T>
    where
        T: Int,
    {
        let (result, bytes_consumed) = Self::int_in_range_impl(range, self.data.iter().cloned())?;
        self.data = &self.data[bytes_consumed..];
        Ok(result)
    }

    fn int_in_range_impl<T>(
        range: ops::RangeInclusive<T>,
        mut bytes: impl Iterator<Item = u8>,
    ) -> Result<(T, usize)>
    where
        T: Int,
    {
        let start = range.start();
        let end = range.end();
        assert!(
            start <= end,
            "`arbitrary::Unstructured::int_in_range` requires a non-empty range"
        );

        // When there is only one possible choice, don't waste any entropy from
        // the underlying data.
        if start == end {
            return Ok((*start, 0));
        }

        let range: T::Widest = end.as_widest() - start.as_widest();
        let mut result = T::Widest::ZERO;
        let mut offset: usize = 0;

        while offset < mem::size_of::<T>()
            && (range >> T::Widest::from_usize(offset * 8)) > T::Widest::ZERO
        {
            let byte = bytes.next().ok_or(Error::NotEnoughData)?;
            result = (result << 8) | T::Widest::from_u8(byte);
            offset += 1;
        }

        // Avoid division by zero.
        if let Some(range) = range.checked_add(T::Widest::ONE) {
            result = result % range;
        }

        Ok((
            T::from_widest(start.as_widest().wrapping_add(result)),
            offset,
        ))
    }

    /// Choose one of the given choices.
    ///
    /// This should only be used inside of `Arbitrary` implementations.
    ///
    /// Returns an error if there is not enough underlying data to make a
    /// choice or if no choices are provided.
    ///
    /// # Examples
    ///
    /// Selecting from an array of choices:
    ///
    /// ```
    /// use arbitrary::Unstructured;
    ///
    /// let mut u = Unstructured::new(&[1, 2, 3, 4, 5, 6, 7, 8, 9, 0]);
    /// let choices = ['a', 'b', 'c', 'd', 'e', 'f', 'g'];
    ///
    /// let choice = u.choose(&choices).unwrap();
    ///
    /// println!("chose {}", choice);
    /// ```
    ///
    /// An error is returned if no choices are provided:
    ///
    /// ```
    /// use arbitrary::Unstructured;
    ///
    /// let mut u = Unstructured::new(&[1, 2, 3, 4, 5, 6, 7, 8, 9, 0]);
    /// let choices: [char; 0] = [];
    ///
    /// let result = u.choose(&choices);
    ///
    /// assert!(result.is_err());
    /// ```
    pub fn choose<'b, T>(&mut self, choices: &'b [T]) -> Result<&'b T> {
        if choices.is_empty() {
            return Err(Error::EmptyChoose);
        }
        let idx = self.int_in_range(0..=choices.len() - 1)?;
        Ok(&choices[idx])
    }

    /// Fill a `buffer` with bytes from the underlying raw data.
    ///
    /// This should only be called within an `Arbitrary` implementation. This is
    /// a very low-level operation. You should generally prefer calling nested
    /// `Arbitrary` implementations like `<Vec<u8>>::arbitrary` and
    /// `String::arbitrary` over using this method directly.
    ///
    /// If this `Unstructured` does not have enough data to fill the whole
    /// `buffer`, an error is returned.
    ///
    /// # Example
    ///
    /// ```
    /// use arbitrary::Unstructured;
    ///
    /// let mut u = Unstructured::new(&[1, 2, 3, 4]);
    ///
    /// let mut buf = [0; 2];
    /// assert!(u.fill_buffer(&mut buf).is_ok());
    /// assert!(u.fill_buffer(&mut buf).is_ok());
    /// ```
    pub fn fill_buffer(&mut self, buffer: &mut [u8]) -> Result<()> {
        let n = std::cmp::min(buffer.len(), self.data.len());
        buffer[..n].copy_from_slice(&self.data[..n]);
        for byte in buffer[n..].iter_mut() {
            *byte = 0;
        }
        self.data = &self.data[n..];
        Ok(())
    }

    /// Provide `size` bytes from the underlying raw data.
    ///
    /// This should only be called within an `Arbitrary` implementation. This is
    /// a very low-level operation. You should generally prefer calling nested
    /// `Arbitrary` implementations like `<Vec<u8>>::arbitrary` and
    /// `String::arbitrary` over using this method directly.
    ///
    /// # Example
    ///
    /// ```
    /// use arbitrary::Unstructured;
    ///
    /// let mut u = Unstructured::new(&[1, 2, 3, 4]);
    ///
    /// assert!(u.bytes(2).unwrap() == &[1, 2]);
    /// assert!(u.bytes(2).unwrap() == &[3, 4]);
    /// ```
    pub fn bytes(&mut self, size: usize) -> Result<&'a [u8]> {
        if self.data.len() < size {
            return Err(Error::NotEnoughData);
        }

        let (for_buf, rest) = self.data.split_at(size);
        self.data = rest;
        Ok(for_buf)
    }

    /// Peek at `size` number of bytes of the underlying raw input.
    ///
    /// Does not consume the bytes, only peeks at them.
    ///
    /// Returns `None` if there are not `size` bytes left in the underlying raw
    /// input.
    ///
    /// # Example
    ///
    /// ```
    /// use arbitrary::Unstructured;
    ///
    /// let u = Unstructured::new(&[1, 2, 3]);
    ///
    /// assert_eq!(u.peek_bytes(0).unwrap(), []);
    /// assert_eq!(u.peek_bytes(1).unwrap(), [1]);
    /// assert_eq!(u.peek_bytes(2).unwrap(), [1, 2]);
    /// assert_eq!(u.peek_bytes(3).unwrap(), [1, 2, 3]);
    ///
    /// assert!(u.peek_bytes(4).is_none());
    /// ```
    pub fn peek_bytes(&self, size: usize) -> Option<&'a [u8]> {
        self.data.get(..size)
    }

    /// Consume all of the rest of the remaining underlying bytes.
    ///
    /// Returns a slice of all the remaining, unconsumed bytes.
    ///
    /// # Example
    ///
    /// ```
    /// use arbitrary::Unstructured;
    ///
    /// let mut u = Unstructured::new(&[1, 2, 3]);
    ///
    /// let mut remaining = u.take_rest();
    ///
    /// assert_eq!(remaining, [1, 2, 3]);
    /// ```
    pub fn take_rest(mut self) -> &'a [u8] {
        mem::replace(&mut self.data, &[])
    }

    /// Provide an iterator over elements for constructing a collection
    ///
    /// This is useful for implementing [`Arbitrary::arbitrary`] on collections
    /// since the implementation is simply `u.arbitrary_iter()?.collect()`
    pub fn arbitrary_iter<'b, ElementType: Arbitrary<'a>>(
        &'b mut self,
    ) -> Result<ArbitraryIter<'a, 'b, ElementType>> {
        Ok(ArbitraryIter {
            u: &mut *self,
            _marker: PhantomData,
        })
    }

    /// Provide an iterator over elements for constructing a collection from
    /// all the remaining bytes.
    ///
    /// This is useful for implementing [`Arbitrary::arbitrary_take_rest`] on collections
    /// since the implementation is simply `u.arbitrary_take_rest_iter()?.collect()`
    pub fn arbitrary_take_rest_iter<ElementType: Arbitrary<'a>>(
        self,
    ) -> Result<ArbitraryTakeRestIter<'a, ElementType>> {
        let (lower, upper) = ElementType::size_hint(0);

        let elem_size = upper.unwrap_or(lower * 2);
        let elem_size = std::cmp::max(1, elem_size);
        let size = self.len() / elem_size;
        Ok(ArbitraryTakeRestIter {
            size,
            u: Some(self),
            _marker: PhantomData,
        })
    }
}

/// Utility iterator produced by [`Unstructured::arbitrary_iter`]
pub struct ArbitraryIter<'a, 'b, ElementType> {
    u: &'b mut Unstructured<'a>,
    _marker: PhantomData<ElementType>,
}

impl<'a, 'b, ElementType: Arbitrary<'a>> Iterator for ArbitraryIter<'a, 'b, ElementType> {
    type Item = Result<ElementType>;
    fn next(&mut self) -> Option<Result<ElementType>> {
        let keep_going = self.u.arbitrary().unwrap_or(false);
        if keep_going {
            Some(Arbitrary::arbitrary(self.u))
        } else {
            None
        }
    }
}

/// Utility iterator produced by [`Unstructured::arbitrary_take_rest_iter`]
pub struct ArbitraryTakeRestIter<'a, ElementType> {
    u: Option<Unstructured<'a>>,
    size: usize,
    _marker: PhantomData<ElementType>,
}

impl<'a, ElementType: Arbitrary<'a>> Iterator for ArbitraryTakeRestIter<'a, ElementType> {
    type Item = Result<ElementType>;
    fn next(&mut self) -> Option<Result<ElementType>> {
        if let Some(mut u) = self.u.take() {
            if self.size == 1 {
                Some(Arbitrary::arbitrary_take_rest(u))
            } else if self.size == 0 {
                None
            } else {
                self.size -= 1;
                let ret = Arbitrary::arbitrary(&mut u);
                self.u = Some(u);
                Some(ret)
            }
        } else {
            None
        }
    }
}

/// A trait that is implemented for all of the primitive integers:
///
/// * `u8`
/// * `u16`
/// * `u32`
/// * `u64`
/// * `u128`
/// * `usize`
/// * `i8`
/// * `i16`
/// * `i32`
/// * `i64`
/// * `i128`
/// * `isize`
///
/// Don't implement this trait yourself.
pub trait Int:
    Copy
    + PartialOrd
    + Ord
    + ops::Sub<Self, Output = Self>
    + ops::Rem<Self, Output = Self>
    + ops::Shr<Self, Output = Self>
    + ops::Shl<usize, Output = Self>
    + ops::BitOr<Self, Output = Self>
{
    #[doc(hidden)]
    type Widest: Int;

    #[doc(hidden)]
    const ZERO: Self;

    #[doc(hidden)]
    const ONE: Self;

    #[doc(hidden)]
    fn as_widest(self) -> Self::Widest;

    #[doc(hidden)]
    fn from_widest(w: Self::Widest) -> Self;

    #[doc(hidden)]
    fn from_u8(b: u8) -> Self;

    #[doc(hidden)]
    fn from_usize(u: usize) -> Self;

    #[doc(hidden)]
    fn checked_add(self, rhs: Self) -> Option<Self>;

    #[doc(hidden)]
    fn wrapping_add(self, rhs: Self) -> Self;
}

macro_rules! impl_int {
    ( $( $ty:ty : $widest:ty ; )* ) => {
        $(
            impl Int for $ty {
                type Widest = $widest;

                const ZERO: Self = 0;

                const ONE: Self = 1;

                fn as_widest(self) -> Self::Widest {
                    self as $widest
                }

                fn from_widest(w: Self::Widest) -> Self {
                    let x = <$ty>::max_value().as_widest();
                    (w % x) as Self
                }

                fn from_u8(b: u8) -> Self {
                    b as Self
                }

                fn from_usize(u: usize) -> Self {
                    u as Self
                }

                fn checked_add(self, rhs: Self) -> Option<Self> {
                    <$ty>::checked_add(self, rhs)
                }

                fn wrapping_add(self, rhs: Self) -> Self {
                    <$ty>::wrapping_add(self, rhs)
                }
            }
        )*
    }
}

impl_int! {
    u8: u128;
    u16: u128;
    u32: u128;
    u64: u128;
    u128: u128;
    usize: u128;
    i8: i128;
    i16: i128;
    i32: i128;
    i64: i128;
    i128: i128;
    isize: i128;
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_byte_size() {
        let mut u = Unstructured::new(&[1, 2, 3, 4, 5, 6, 7, 8, 9, 6]);
        // Should take one byte off the end
        assert_eq!(u.arbitrary_byte_size().unwrap(), 6);
        assert_eq!(u.len(), 9);
        let mut v = vec![];
        v.resize(260, 0);
        v.push(1);
        v.push(4);
        let mut u = Unstructured::new(&v);
        // Should read two bytes off the end
        assert_eq!(u.arbitrary_byte_size().unwrap(), 0x104);
        assert_eq!(u.len(), 260);
    }

    #[test]
    fn int_in_range_of_one() {
        let mut u = Unstructured::new(&[1, 2, 3, 4, 5, 6, 7, 8, 9, 6]);
        let x = u.int_in_range(0..=0).unwrap();
        assert_eq!(x, 0);
        let choice = *u.choose(&[42]).unwrap();
        assert_eq!(choice, 42)
    }

    #[test]
    fn int_in_range_uses_minimal_amount_of_bytes() {
        let mut u = Unstructured::new(&[1]);
        u.int_in_range::<u8>(0..=u8::MAX).unwrap();

        let mut u = Unstructured::new(&[1]);
        u.int_in_range::<u32>(0..=u8::MAX as u32).unwrap();

        let mut u = Unstructured::new(&[1]);
        u.int_in_range::<u32>(0..=u8::MAX as u32 + 1).unwrap_err();
    }
}
