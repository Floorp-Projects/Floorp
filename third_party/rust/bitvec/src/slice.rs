/*! `BitSlice` Wide Reference

This module defines semantic operations on `[u1]`, in contrast to the mechanical
operations defined in `BitPtr`.

The `&BitSlice` handle has the same size and general layout as the standard Rust
slice handle `&[T]`. Its binary layout is wholly incompatible with the layout of
Rust slices, and must never be interchanged except through the provided APIs.
!*/

use crate::{
	BigEndian,
	BitIdx,
	BitPtr,
	Bits,
	Cursor,
};
use core::{
	cmp::{
		Eq,
		Ord,
		Ordering,
		PartialEq,
		PartialOrd,
	},
	convert::{
		AsMut,
		AsRef,
		From,
	},
	fmt::{
		self,
		Debug,
		DebugList,
		Display,
		Formatter,
	},
	hash::{
		Hash,
		Hasher,
	},
	iter::{
		DoubleEndedIterator,
		ExactSizeIterator,
		FusedIterator,
		Iterator,
		IntoIterator,
	},
	marker::PhantomData,
	mem,
	ops::{
		AddAssign,
		BitAndAssign,
		BitOrAssign,
		BitXorAssign,
		Index,
		IndexMut,
		Neg,
		Not,
		Range,
		RangeFrom,
		RangeFull,
		RangeInclusive,
		RangeTo,
		RangeToInclusive,
		ShlAssign,
		ShrAssign,
	},
	ptr,
	slice,
	str,
};

#[cfg(feature = "alloc")]
use crate::BitVec;

#[cfg(all(feature = "alloc", not(feature = "std")))]
use alloc::borrow::ToOwned;

#[cfg(feature = "std")]
use std::borrow::ToOwned;

/** A compact slice of bits, whose cursor and storage types can be customized.

`BitSlice` is a newtype wrapper over [`[()]`], with a specialized reference
handle. As an unsized slice, it can only ever be held by reference. The
reference type is **binary incompatible** with any other Rust slice handles.

`BitSlice` can only be dynamically allocated by this library. Creation of any
other `BitSlice` collections will result in catastrophically incorrect behavior.

A `BitSlice` reference can be created through the [`bitvec!`] macro, from a
[`BitVec`] collection, or from any slice of elements by using the appropriate
[`From`] implementation.

`BitSlice`s are a view into a block of memory at bit-level resolution. They are
represented by a crate-internal pointer structure that ***cannot*** be used with
other Rust code except through the provided conversion APIs.

```rust
use bitvec::*;

let store: &[u8] = &[0x69];
//  slicing a bitvec
let bslice: &BitSlice = store.into();
//  coercing an array to a bitslice
let bslice: &BitSlice = (&[1u8, 254u8][..]).into();
```

Bit slices are either mutable or shared. The shared slice type is
`&BitSlice<C, T>`, while the mutable slice type is `&mut BitSlice<C, T>`. For
example, you can mutate bits in the memory to which a mutable `BitSlice` points:

```rust
use bitvec::*;
let mut base = [0u8, 0, 0, 0];
{
 let bs: &mut BitSlice = (&mut base[..]).into();
 bs.set(13, true);
 eprintln!("{:?}", bs.as_ref());
 assert!(bs[13]);
}
assert_eq!(base[1], 4);
```

# Type Parameters

- `C: Cursor`: An implementor of the `Cursor` trait. This type is used to
  convert semantic indices into concrete bit positions in elements, and store or
  retrieve bit values from the storage type.
- `T: Bits`: An implementor of the `Bits` trait: `u8`, `u16`, `u32`, `u64`. This
  is the actual type in memory the slice will use to store data.

# Safety

The `&BitSlice` reference handle has the same *size* as standard Rust slice
handles, but it is ***extremely binary incompatible*** with them. Attempting to
treat `&BitSlice<_, T>` as `&[T]` in any manner except through the provided APIs
is ***catastrophically*** unsafe and unsound.

[`BitVec`]: ../struct.BitVec.html
[`From`]: https://doc.rust-lang.org/stable/std/convert/trait.From.html
[`bitvec!`]: ../macro.bitvec.html
[`[()]`]: https://doc.rust-lang.org/stable/std/primitive.slice.html
**/
#[repr(transparent)]
pub struct BitSlice<C = BigEndian, T = u8>
where C: Cursor, T: Bits {
	/// Cursor type for selecting bits inside an element.
	_kind: PhantomData<C>,
	/// Element type of the slice.
	///
	/// eddyb recommends using `PhantomData<T>` and `[()]` instead of `[T]`
	/// alone.
	_type: PhantomData<T>,
	/// Slice of elements `T` over which the `BitSlice` has usage.
	_elts: [()],
}

impl<C, T> BitSlice<C, T>
where C: Cursor, T: Bits {
	/// Produces the empty slice. This is equivalent to `&[]` for Rust slices.
	///
	/// # Returns
	///
	/// An empty `&BitSlice` handle.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let bv: &BitSlice = BitSlice::empty();
	/// ```
	pub fn empty<'a>() -> &'a Self {
		BitPtr::empty().into()
	}

	/// Produces the empty mutable slice. This is equivalent to `&mut []` for
	/// Rust slices.
	///
	/// # Returns
	///
	/// An empty `&mut BitSlice` handle.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let bv: &mut BitSlice = BitSlice::empty_mut();
	/// ```
	pub fn empty_mut<'a>() -> &'a mut Self {
		BitPtr::empty().into()
	}

	/// Returns the number of bits contained in the `BitSlice`.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// The number of live bits in the slice domain.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0];
	/// let bv: &BitSlice = store.into();
	/// assert_eq!(bv.len(), 8);
	/// ```
	pub fn len(&self) -> usize {
		self.bitptr().bits()
	}

	/// Tests if the slice is empty.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// Whether the slice has no live bits.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let bv: &BitSlice = BitSlice::empty();
	/// assert!(bv.is_empty());
	/// let bv: &BitSlice = (&[0u8] as &[u8]).into();;
	/// assert!(!bv.is_empty());
	/// ```
	pub fn is_empty(&self) -> bool {
		self.len() == 0
	}

	/// Gets the first element of the slice, if present.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// `None` if the slice is empty, or `Some(bit)` if it is not.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// assert!(BitSlice::<BigEndian, u8>::empty().first().is_none());
	/// let bv: &BitSlice = (&[128u8] as &[u8]).into();
	/// assert!(bv.first().unwrap());
	/// ```
	pub fn first(&self) -> Option<bool> {
		if self.is_empty() { None }
		else { Some(self[0]) }
	}

	/// Returns the first and all the rest of the bits of the slice, or `None`
	/// if it is empty.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// If the slice is empty, this returns `None`, otherwise, it returns `Some`
	/// of:
	///
	/// - the first bit
	/// - a `&BitSlice` of all the rest of the bits (this may be empty)
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// assert!(BitSlice::<BigEndian, u8>::empty().split_first().is_none());
	///
	/// let store: &[u8] = &[128];
	/// let bv: &BitSlice = store.into();
	/// let (h, t) = bv.split_first().unwrap();
	/// assert!(h);
	/// assert!(t.not_any());
	///
	/// let bv = &bv[0 .. 1];
	/// let (h, t) = bv.split_first().unwrap();
	/// assert!(h);
	/// assert!(t.is_empty());
	/// ```
	pub fn split_first(&self) -> Option<(bool, &Self)> {
		if self.is_empty() {
			return None;
		}
		Some((self[0], &self[1 ..]))
	}

	/// Returns the first and all the rest of the bits of the slice, or `None`
	/// if it is empty.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// If the slice is empty, this returns `None`, otherwise, it returns `Some`
	/// of:
	///
	/// - the first bit
	/// - a `&mut BitSlice` of all the rest of the bits (this may be empty)
	pub fn split_first_mut(&mut self) -> Option<(bool, &mut Self)> {
		if self.is_empty() {
			return None;
		}
		Some((self[0], &mut self[1 ..]))
	}

	/// Returns the last and all the rest of the bits in the slice, or `None`
	/// if it is empty.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// If the slice is empty, this returns `None`, otherwise, it returns `Some`
	/// of:
	///
	/// - the last bit
	/// - a `&BitSlice` of all the rest of the bits (this may be empty)
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// assert!(BitSlice::<BigEndian, u8>::empty().split_last().is_none());
	///
	/// let bv: &BitSlice = (&[1u8] as &[u8]).into();
	/// let (t, h) = bv.split_last().unwrap();
	/// assert!(t);
	/// assert!(h.not_any());
	///
	/// let bv = &bv[7 .. 8];
	/// let (t, h) = bv.split_last().unwrap();
	/// assert!(t);
	/// assert!(h.is_empty());
	/// ```
	pub fn split_last(&self) -> Option<(bool, &Self)> {
		if self.is_empty() {
			return None;
		}
		let len = self.len();
		Some((self[len - 1], &self[.. len - 1]))
	}

	/// Returns the last and all the rest of the bits in the slice, or `None`
	/// if it is empty.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// If the slice is empty, this returns `None`, otherwise, it returns `Some`
	/// of:
	///
	/// - the last bit
	/// - a `&BitSlice` of all the rest of the bits (this may be empty)
	pub fn split_last_mut(&mut self) -> Option<(bool, &mut Self)> {
		if self.is_empty() {
			return None;
		}
		let len = self.len();
		Some((self[len - 1], &mut self[.. len - 1]))
	}

	/// Gets the last element of the slice, or `None` if it is empty.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// `None` if the slice is empty, or `Some(bit)` if it is not.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// assert!(BitSlice::<BigEndian, u8>::empty().last().is_none());
	/// let bv: &BitSlice = (&[1u8] as &[u8]).into();
	/// assert!(bv.last().unwrap());
	/// ```
	pub fn last(&self) -> Option<bool> {
		if self.is_empty() { None }
		else { Some(self[self.len() - 1]) }
	}

	/// Gets the bit value at the given position.
	///
	/// # Parameters
	///
	/// - `&self`
	/// - `index`: The bit index to retrieve.
	///
	/// # Returns
	///
	/// The bit at the specified index, if any.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let bv: &BitSlice = (&[8u8] as &[u8]).into();
	/// assert!(bv.get(4).unwrap());
	/// assert!(!bv.get(3).unwrap());
	/// assert!(bv.get(10).is_none());
	/// ```
	pub fn get(&self, index: usize) -> Option<bool> {
		if index >= self.len() {
			return None;
		}
		Some(self[index])
	}

	/// Sets the bit value at the given position.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `index`: The bit index to set. It must be in the domain
	///   `0 .. self.len()`.
	/// - `value`: The value to be set, `true` for `1` and `false` for `0`.
	///
	/// # Panics
	///
	/// This method panics if `index` is outside the slice domain.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &mut [u8] = &mut [8u8];
	/// let bv: &mut BitSlice = store.into();
	/// assert!(!bv[3]);
	/// bv.set(3, true);
	/// assert!(bv[3]);
	/// ```
	pub fn set(&mut self, index: usize, value: bool) {
		let len = self.len();
		assert!(index < len, "Index out of range: {} >= {}", index, len);

		let h = self.bitptr().head();
		//  Find the index of the containing element, and of the bit within it.
		let (elt, bit) = h.offset::<T>(index as isize);
		self.as_mut()[elt as usize].set::<C>(bit, value);
	}

	/// Retrieves a read pointer to the start of the underlying data slice.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// A pointer to the first element, partial or not, in the underlying store.
	///
	/// # Safety
	///
	/// The caller must ensure that the slice outlives the pointer this function
	/// returns, or else it will dangle and point to garbage.
	///
	/// Modifying the container referenced by this slice may cause its buffer to
	/// reallocate, which would also make any pointers to it invalid.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0; 4];
	/// let bv: &BitSlice = store.into();
	/// assert_eq!(store.as_ptr(), bv.as_ptr());
	/// ```
	pub fn as_ptr(&self) -> *const T {
		self.bitptr().pointer()
	}

	/// Retrieves a write pointer to the start of the underlying data slice.
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// A pointer to the first element, partial or not, in the underlying store.
	///
	/// # Safety
	///
	/// The caller must ensure that the slice outlives the pointer this function
	/// returns, or else it will dangle and point to garbage.
	///
	/// Modifying the container referenced by this slice may cause its buffer to
	/// reallocate, which would also make any pointers to it invalid.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &mut [u8] = &mut[0; 4];
	/// let store_ptr = store.as_mut_ptr();
	/// let bv: &mut BitSlice = store.into();
	/// assert_eq!(store_ptr, bv.as_mut_ptr());
	/// ```
	pub fn as_mut_ptr(&mut self) -> *mut T {
		self.bitptr().pointer() as *mut T
	}

	/// Swaps two bits in the slice.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `a`: The first index to be swapped.
	/// - `b`: The second index to be swapped.
	///
	/// # Panics
	///
	/// Panics if either `a` or `b` are out of bounds.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &mut [u8] = &mut[32u8];
	/// let bv: &mut BitSlice = store.into();
	/// assert!(!bv[0]);
	/// assert!(bv[2]);
	/// bv.swap(0, 2);
	/// assert!(bv[0]);
	/// assert!(!bv[2]);
	/// ```
	pub fn swap(&mut self, a: usize, b: usize) {
		assert!(a < self.len(), "Index {} out of bounds: {}", a, self.len());
		assert!(b < self.len(), "Index {} out of bounds: {}", b, self.len());
		let bit_a = self[a];
		let bit_b = self[b];
		self.set(a, bit_b);
		self.set(b, bit_a);
	}

	/// Reverses the order of bits in the slice, in place.
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &mut [u8] = &mut[0b1010_1010];
	/// {
	///   let bv: &mut BitSlice = store.into();
	///   bv[1 .. 7].reverse();
	/// }
	/// eprintln!("{:b}", store[0]);
	/// assert_eq!(store[0], 0b1101_0100);
	/// ```
	pub fn reverse(&mut self) {
		let mut cur: &mut Self = self;
		loop {
			let len = cur.len();
			if len < 2 {
				return;
			}
			let (h, t) = (cur[0], cur[len - 1]);
			cur.set(0, t);
			cur.set(len - 1, h);
			cur = &mut cur[1 .. len - 1];
		}
	}

	/// Provides read-only iteration across the slice domain.
	///
	/// The iterator returned from this method implements `ExactSizeIterator`
	/// and `DoubleEndedIterator` just as the consuming `.into_iter()` method’s
	/// iterator does.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// An iterator over all bits in the slice domain, in `C` and `T` ordering.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[64];
	/// let bv: &BitSlice = store.into();
	/// let mut iter = bv[.. 2].iter();
	/// assert!(!iter.next().unwrap());
	/// assert!(iter.next().unwrap());
	/// assert!(iter.next().is_none());
	/// ```
	pub fn iter(&self) -> Iter<C, T> {
		self.into_iter()
	}

	/// Produces a sliding iterator over consecutive windows in the slice. Each
	/// windows has the width `size`. The windows overlap. If the slice is
	/// shorter than `size`, the produced iterator is empty.
	///
	/// # Parameters
	///
	/// - `&self`
	/// - `size`: The width of each window.
	///
	/// # Returns
	///
	/// An iterator which yields sliding views into the slice.
	///
	/// # Panics
	///
	/// This function panics if the `size` is zero.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0b0100_1011];
	/// let bv: &BitSlice = store.into();
	/// let mut windows = bv.windows(4);
	/// assert_eq!(windows.next(), Some(&bv[0 .. 4]));
	/// assert_eq!(windows.next(), Some(&bv[1 .. 5]));
	/// assert_eq!(windows.next(), Some(&bv[2 .. 6]));
	/// assert_eq!(windows.next(), Some(&bv[3 .. 7]));
	/// assert_eq!(windows.next(), Some(&bv[4 .. 8]));
	/// assert!(windows.next().is_none());
	/// ```
	pub fn windows(&self, size: usize) -> Windows<C, T> {
		assert_ne!(size, 0, "Window width cannot be zero");
		Windows {
			inner: self,
			width: size,
		}
	}

	/// Produces a galloping iterator over consecutive chunks in the slice. Each
	/// chunk, except possibly the last, has the width `size`. The chunks do not
	/// overlap. If the slice is shorter than `size`, the produced iterator
	/// produces only one chunk.
	///
	/// # Parameters
	///
	/// - `&self`
	/// - `size`: The width of each chunk.
	///
	/// # Returns
	///
	/// An iterator which yields consecutive chunks of the slice.
	///
	/// # Panics
	///
	/// This function panics if the `size` is zero.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0b0100_1011];
	/// let bv: &BitSlice = store.into();
	/// let mut chunks = bv.chunks(3);
	/// assert_eq!(chunks.next(), Some(&bv[0 .. 3]));
	/// assert_eq!(chunks.next(), Some(&bv[3 .. 6]));
	/// assert_eq!(chunks.next(), Some(&bv[6 .. 8]));
	/// assert!(chunks.next().is_none());
	/// ```
	pub fn chunks(&self, size: usize) -> Chunks<C, T> {
		assert_ne!(size, 0, "Chunk width cannot be zero");
		Chunks {
			inner: self,
			width: size,
		}
	}

	/// Produces a galloping iterator over consecutive chunks in the slice. Each
	/// chunk, except possibly the last, has the width `size`. The chunks do not
	/// overlap. If the slice is shorter than `size`, the produced iterator
	/// produces only one chunk.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `size`: The width of each chunk.
	///
	/// # Returns
	///
	/// An iterator which yields consecutive mutable chunks of the slice.
	///
	/// # Panics
	///
	/// This function panics if the `size` is zero.
	pub fn chunks_mut(&mut self, size: usize) -> ChunksMut<C, T> {
		assert_ne!(size, 0, "Chunk width cannot be zero");
		ChunksMut {
			inner: self,
			width: size,
		}
	}

	/// Produces a galloping iterator over consecutive chunks in the slice. Each
	/// chunk has the width `size`. If `size` does not evenly divide the slice,
	/// then the remainder is not part of the iteration, and can be accessed
	/// separately with the `.remainder()` method.
	///
	/// # Parameters
	///
	/// - `&self`
	/// - `size`: The width of each chunk.
	///
	/// # Returns
	///
	/// An iterator which yields consecutive chunks of the slice.
	///
	/// # Panics
	///
	/// This function panics if `size` is zero.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0b0100_1011];
	/// let bv: &BitSlice = store.into();
	/// let mut chunks_exact = bv.chunks_exact(3);
	/// assert_eq!(chunks_exact.next(), Some(&bv[0 .. 3]));
	/// assert_eq!(chunks_exact.next(), Some(&bv[3 .. 6]));
	/// assert!(chunks_exact.next().is_none());
	/// assert_eq!(chunks_exact.remainder(), &bv[6 .. 8]);
	/// ```
	pub fn chunks_exact(&self, size: usize) -> ChunksExact<C, T> {
		assert_ne!(size, 0, "Chunk size cannot be zero");
		let rem = self.len() % size;
		let len = self.len() - rem;
		let (inner, extra) = self.split_at(len);
		ChunksExact {
			inner,
			extra,
			width: size,
		}
	}

	/// Produces a galloping iterator over consecutive chunks in the slice. Each
	/// chunk has the width `size`. If `size` does not evenly divide the slice,
	/// then the remainder is not part of the iteration, and can be accessed
	/// separately with the `.remainder()` method.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `size`: The width of each chunk.
	///
	/// # Returns
	///
	/// An iterator which yields consecutive mutable chunks of the slice.
	///
	/// # Panics
	///
	/// This function panics if `size` is zero.
	pub fn chunks_exact_mut(&mut self, size: usize) -> ChunksExactMut<C, T> {
		assert_ne!(size, 0, "Chunk size cannot be zero");
		let rem = self.len() % size;
		let len = self.len() - rem;
		let (inner, extra) = self.split_at_mut(len);
		ChunksExactMut {
			inner,
			extra,
			width: size,
		}
	}

	/// Produces a galloping iterator over consecutive chunks in the slice, from
	/// the back to the front. Each chunk, except possibly the front, has the
	/// width `size`. The chunks do not overlap. If the slice is shorter than
	/// `size`, then the iterator produces one item.
	///
	/// # Parameters
	///
	/// - `&self`
	/// - `size`: The width of each chunk.
	///
	/// # Returns
	///
	/// An iterator which yields consecutive chunks of the slice, from the back
	/// to the front.
	///
	/// # Panics
	///
	/// This function panics if `size` is zero.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0b0100_1011];
	/// let bv: &BitSlice = store.into();
	/// let mut rchunks = bv.rchunks(3);
	/// assert_eq!(rchunks.next(), Some(&bv[5 .. 8]));
	/// assert_eq!(rchunks.next(), Some(&bv[2 .. 5]));
	/// assert_eq!(rchunks.next(), Some(&bv[0 .. 2]));
	/// assert!(rchunks.next().is_none());
	/// ```
	pub fn rchunks(&self, size: usize) -> RChunks<C, T> {
		assert_ne!(size, 0, "Chunk size cannot be zero");
		RChunks {
			inner: self,
			width: size,
		}
	}

	/// Produces a galloping iterator over consecutive chunks in the slice, from
	/// the back to the front. Each chunk, except possibly the front, has the
	/// width `size`. The chunks do not overlap. If the slice is shorter than
	/// `size`, then the iterator produces one item.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `size`: The width of each chunk.
	///
	/// # Returns
	///
	/// An iterator which yields consecutive mutable chunks of the slice, from
	/// the back to the front.
	///
	/// # Panics
	///
	/// This function panics if `size` is zero.
	pub fn rchunks_mut(&mut self, size: usize) -> RChunksMut<C, T> {
		assert_ne!(size, 0, "Chunk size cannot be zero");
		RChunksMut {
			inner: self,
			width: size,
		}
	}

	/// Produces a galloping iterator over consecutive chunks in the slice, from
	/// the back to the front. Each chunk has the width `size`. If `size` does
	/// not evenly divide the slice, then the remainder is not part of the
	/// iteration, and can be accessed separately with the `.remainder()`
	/// method.
	///
	/// # Parameters
	///
	/// - `&self`
	/// - `size`: The width of each chunk.
	///
	/// # Returns
	///
	/// An iterator which yields consecutive chunks of the slice, from the back
	/// to the front.
	///
	/// # Panics
	///
	/// This function panics if `size` is zero.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0b0100_1011];
	/// let bv: &BitSlice = store.into();
	/// let mut rchunks_exact = bv.rchunks_exact(3);
	/// assert_eq!(rchunks_exact.next(), Some(&bv[5 .. 8]));
	/// assert_eq!(rchunks_exact.next(), Some(&bv[2 .. 5]));
	/// assert!(rchunks_exact.next().is_none());
	/// assert_eq!(rchunks_exact.remainder(), &bv[0 .. 2]);
	/// ```
	pub fn rchunks_exact(&self, size: usize) -> RChunksExact<C, T> {
		assert_ne!(size, 0, "Chunk size cannot be zero");
		let (extra, inner) = self.split_at(self.len() % size);
		RChunksExact {
			inner,
			extra,
			width: size,
		}
	}

	/// Produces a galloping iterator over consecutive chunks in the slice, from
	/// the back to the front. Each chunk has the width `size`. If `size` does
	/// not evenly divide the slice, then the remainder is not part of the
	/// iteration, and can be accessed separately with the `.remainder()`
	/// method.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `size`: The width of each chunk.
	///
	/// # Returns
	///
	/// An iterator which yields consecutive mutable chunks of the slice, from
	/// the back to the front.
	///
	/// # Panics
	///
	/// This function panics if `size` is zero.
	pub fn rchunks_exact_mut(&mut self, size: usize) -> RChunksExactMut<C, T> {
		assert_ne!(size, 0, "Chunk size cannot be zero");
		let (extra, inner) = self.split_at_mut(self.len() % size);
		RChunksExactMut {
			inner,
			extra,
			width: size,
		}
	}

	/// Divides one slice into two at an index.
	///
	/// The first will contain all indices from `[0, mid)` (excluding the index
	/// `mid` itself) and the second will contain all indices from `[mid, len)`
	/// (excluding the index `len` itself).
	///
	/// # Parameters
	///
	/// - `&self`
	/// - `mid`: The index at which to split
	///
	/// # Returns
	///
	/// - The bits up to but not including `mid`.
	/// - The bits from mid onwards.
	///
	/// # Panics
	///
	/// Panics if `mid > self.len()`.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x0F];
	/// let bv: &BitSlice = store.into();
	///
	/// let (l, r) = bv.split_at(0);
	/// assert!(l.is_empty());
	/// assert_eq!(r, bv);
	///
	/// let (l, r) = bv.split_at(4);
	/// assert_eq!(l, &bv[0 .. 4]);
	/// assert_eq!(r, &bv[4 .. 8]);
	///
	/// let (l, r) = bv.split_at(8);
	/// assert_eq!(l, bv);
	/// assert!(r.is_empty());
	/// ```
	pub fn split_at(&self, mid: usize) -> (&Self, &Self) {
		assert!(mid <= self.len(), "Index {} out of bounds: {}", mid, self.len());
		if mid == self.len() {
			(&self, Self::empty())
		}
		else {
			(&self[.. mid], &self[mid ..])
		}
	}

	/// Divides one slice into two at an index.
	///
	/// The first will contain all indices from `[0, mid)` (excluding the index
	/// `mid` itself) and the second will contain all indices from `[mid, len)`
	/// (excluding the index `len` itself).
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `mid`: The index at which to split
	///
	/// # Returns
	///
	/// - The bits up to but not including `mid`.
	/// - The bits from mid onwards.
	///
	/// # Panics
	///
	/// Panics if `mid > self.len()`.
	pub fn split_at_mut(&mut self, mid: usize) -> (&mut Self, &mut Self) {
		let (head, tail) = self.split_at(mid);
		let h_mut = {
			let (p, e, h, t) = head.bitptr().raw_parts();
			BitPtr::new(p, e, h, t)
		};
		let t_mut = {
			let (p, e, h, t) = tail.bitptr().raw_parts();
			BitPtr::new(p, e, h, t)
		};
		(h_mut.into(), t_mut.into())
	}

	/// Tests if the slice begins with the given prefix.
	///
	/// # Parameters
	///
	/// - `&self`
	/// - `prefix`: Any `BitSlice` against which `self` is tested. This is not
	///   required to have the same cursor or storage types as `self`.
	///
	/// # Returns
	///
	/// Whether `self` begins with `prefix`. This is true only if `self` is at
	/// least as long as `prefix` and their bits are semantically equal.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0xA6];
	/// let bv: &BitSlice = store.into();;
	/// assert!(bv.starts_with(&bv[.. 3]));
	/// assert!(!bv.starts_with(&bv[3 ..]));
	/// ```
	pub fn starts_with<D, U>(&self, prefix: &BitSlice<D, U>) -> bool
	where D: Cursor, U: Bits {
		let plen = prefix.len();
		self.len() >= plen && prefix == &self[.. plen]
	}

	/// Tests if the slice ends with the given suffix.
	///
	/// # Parameters
	///
	/// - `&self`
	/// - `suffix`: Any `BitSlice` against which `self` is tested. This is not
	///   required to have the same cursor or storage types as `self`.
	///
	/// # Returns
	///
	/// Whether `self` ends with `suffix`. This is true only if `self` is at
	/// least as long as `suffix` and their bits are semantically equal.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0xA6];
	/// let bv: &BitSlice = store.into();
	/// assert!(bv.ends_with(&bv[5 ..]));
	/// assert!(!bv.ends_with(&bv[.. 5]));
	/// ```
	pub fn ends_with<D, U>(&self, suffix: &BitSlice<D, U>) -> bool
	where D: Cursor, U: Bits {
		let slen = suffix.len();
		let len = self.len();
		len >= slen && suffix == &self[len - slen ..]
	}

	/// Rotates the slice, in place, to the left.
	///
	/// After calling this method, the bits from `[.. by]` will be at the back
	/// of the slice, and the bits from `[by ..]` will be at the front. This
	/// operates fully in-place.
	///
	/// In-place rotation of bits requires this method to take `O(k × n)` time.
	/// It is impossible to use machine intrinsics to perform galloping rotation
	/// on bits.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `by`: The number of bits by which to rotate left. This must be in the
	///   range `0 ..= self.len()`. If it is `0` or `self.len()`, then this
	///   method is a no-op.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &mut [u8] = &mut [0xF0];
	/// let bv: &mut BitSlice = store.into();
	/// bv.rotate_left(2);
	/// assert_eq!(bv.as_ref()[0], 0xC3);
	/// ```
	pub fn rotate_left(&mut self, by: usize) {
		let len = self.len();
		assert!(by <= len, "Slices cannot be rotated by more than their length");
		if by == len {
			return;
		}

		for _ in 0 .. by {
			let tmp = self[0];
			for n in 1 .. len {
				let bit = self[n];
				self.set(n - 1, bit);
			}
			self.set(len - 1, tmp);
		}
	}

	/// Rotates the slice, in place, to the right.
	///
	/// After calling this method, the bits from `[self.len() - by ..]` will be
	/// at the front of the slice, and the bits from `[.. self.len() - by]` will
	/// be at the back. This operates fully in-place.
	///
	/// In-place rotation of bits requires this method to take `O(k × n)` time.
	/// It is impossible to use machine intrinsics to perform galloping rotation
	/// on bits.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `by`: The number of bits by which to rotate right. This must be in the
	///   range `0 ..= self.len()`. If it is `0` or `self.len`, then this method
	///   is a no-op.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &mut [u8] = &mut [0xF0];
	/// let bv: &mut BitSlice = store.into();
	/// bv.rotate_right(2);
	/// assert_eq!(bv.as_ref()[0], 0x3C);
	/// ```
	pub fn rotate_right(&mut self, by: usize) {
		let len = self.len();
		assert!(by <= len, "Slices cannot be rotated by more than their length");
		if by == len {
			return;
		}

		for _ in 0 .. by {
			let tmp = self[len - 1];
			for n in (0 .. len - 1).rev() {
				let bit = self[n];
				self.set(n + 1, bit);
			}
			self.set(0, tmp);
		}
	}

	/// Tests if *all* bits in the slice domain are set (logical `∧`).
	///
	/// # Truth Table
	///
	/// ```text
	/// 0 0 => 0
	/// 0 1 => 0
	/// 1 0 => 0
	/// 1 1 => 1
	/// ```
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// Whether all bits in the slice domain are set.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0xFD];
	/// let bv: &BitSlice = store.into();
	/// assert!(bv[.. 4].all());
	/// assert!(!bv[4 ..].all());
	/// ```
	pub fn all(&self) -> bool {
		match self.inner() {
			Inner::Minor(head, elt, tail) => {
				for n in *head .. *tail {
					if !elt.get::<C>(n.into()) {
						return false;
					}
				}
			},
			Inner::Major(head, body, tail) => {
				if let Some(elt) = head {
					for n in *self.bitptr().head() .. T::SIZE {
						if !elt.get::<C>(n.into()) {
							return false;
						}
					}
				}
				for elt in body {
					if *elt != T::from(!0) {
						return false;
					}
				}
				if let Some(elt) = tail {
					for n in 0 .. *self.bitptr().tail() {
						if !elt.get::<C>(n.into()) {
							return false;
						}
					}
				}
			},
		}
		true
	}

	/// Tests if *any* bit in the slice is set (logical `∨`).
	///
	/// # Truth Table
	///
	/// ```text
	/// 0 0 => 0
	/// 0 1 => 1
	/// 1 0 => 1
	/// 1 1 => 1
	/// ```
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// Whether any bit in the slice domain is set.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x40];
	/// let bv: &BitSlice = store.into();
	/// assert!(bv[.. 4].any());
	/// assert!(!bv[4 ..].any());
	/// ```
	pub fn any(&self) -> bool {
		match self.inner() {
			Inner::Minor(head, elt, tail) => {
				for n in *head .. *tail {
					if elt.get::<C>(n.into()) {
						return true;
					}
				}
			},
			Inner::Major(head, body, tail) => {
				if let Some(elt) = head {
					for n in *self.bitptr().head() .. T::SIZE {
						if elt.get::<C>(n.into()) {
							return true;
						}
					}
				}
				for elt in body {
					if *elt != T::from(0) {
						return true;
					}
				}
				if let Some(elt) = tail {
					for n in 0 .. *self.bitptr().tail() {
						if elt.get::<C>(n.into()) {
							return true;
						}
					}
				}
			},
		}
		false
	}

	/// Tests if *any* bit in the slice is unset (logical `¬∧`).
	///
	/// # Truth Table
	///
	/// ```text
	/// 0 0 => 1
	/// 0 1 => 1
	/// 1 0 => 1
	/// 1 1 => 0
	/// ```
	///
	/// # Parameters
	///
	/// - `&self
	///
	/// # Returns
	///
	/// Whether any bit in the slice domain is unset.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0xFD];
	/// let bv: &BitSlice = store.into();
	/// assert!(!bv[.. 4].not_all());
	/// assert!(bv[4 ..].not_all());
	/// ```
	pub fn not_all(&self) -> bool {
		!self.all()
	}

	/// Tests if *all* bits in the slice are unset (logical `¬∨`).
	///
	/// # Truth Table
	///
	/// ```text
	/// 0 0 => 1
	/// 0 1 => 0
	/// 1 0 => 0
	/// 1 1 => 0
	/// ```
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// Whether all bits in the slice domain are unset.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x40];
	/// let bv: &BitSlice = store.into();
	/// assert!(!bv[.. 4].not_any());
	/// assert!(bv[4 ..].not_any());
	/// ```
	pub fn not_any(&self) -> bool {
		!self.any()
	}

	/// Tests whether the slice has some, but not all, bits set and some, but
	/// not all, bits unset.
	///
	/// This is `false` if either `all()` or `not_any()` are `true`.
	///
	/// # Truth Table
	///
	/// ```text
	/// 0 0 => 0
	/// 0 1 => 1
	/// 1 0 => 1
	/// 1 1 => 0
	/// ```
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// Whether the slice domain has mixed content.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0b111_000_10];
	/// let bv: &BitSlice = store.into();
	/// assert!(!bv[0 .. 3].some());
	/// assert!(!bv[3 .. 6].some());
	/// assert!(bv[6 ..].some());
	/// ```
	pub fn some(&self) -> bool {
		self.any() && self.not_all()
	}

	/// Counts how many bits are set high.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// The number of high bits in the slice domain.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0xFD, 0x25];
	/// let bv: &BitSlice = store.into();
	/// assert_eq!(bv.count_ones(), 10);
	/// ```
	pub fn count_ones(&self) -> usize {
		match self.inner() {
			Inner::Minor(head, elt, tail) => {
				(*head .. *tail).map(|n| elt.get::<C>(n.into())).count()
			},
			Inner::Major(head, body, tail) => {
				head.map(|t| (*self.bitptr().head() .. T::SIZE)
					.map(|n| t.get::<C>(n.into())).filter(|b| *b).count()
				).unwrap_or(0) +
				body.iter().map(T::count_ones).sum::<usize>() +
				tail.map(|t| (0 .. *self.bitptr().tail())
					.map(|n| t.get::<C>(n.into())).filter(|b| *b).count()
				).unwrap_or(0)
			},
		}
	}

	/// Counts how many bits are set low.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// The number of low bits in the slice domain.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0xFD, 0x25];
	/// let bv: &BitSlice = store.into();
	/// assert_eq!(bv.count_zeros(), 6);
	/// ```
	pub fn count_zeros(&self) -> usize {
		match self.inner() {
			Inner::Minor(head, elt, tail) => {
				(*head .. *tail).map(|n| !elt.get::<C>(n.into())).count()
			},
			Inner::Major(head, body, tail) => {
				head.map(|t| (*self.bitptr().head() .. T::SIZE)
					.map(|n| t.get::<C>(n.into())).filter(|b| !*b).count()
				).unwrap_or(0) +
				body.iter().map(T::count_zeros).sum::<usize>() +
				tail.map(|t| (0 .. *self.bitptr().tail())
					.map(|n| t.get::<C>(n.into())).filter(|b| !*b).count()
				).unwrap_or(0)
			},
		}
	}

	/// Set all bits in the slice to a value.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `value`: The bit value to which all bits in the slice will be set.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &mut [u8] = &mut [0];
	/// let bv: &mut BitSlice = store.into();
	/// bv[2 .. 6].set_all(true);
	/// assert_eq!(bv.as_ref(), &[0b0011_1100]);
	/// bv[3 .. 5].set_all(false);
	/// assert_eq!(bv.as_ref(), &[0b0010_0100]);
	/// bv[.. 1].set_all(true);
	/// assert_eq!(bv.as_ref(), &[0b1010_0100]);
	/// ```
	pub fn set_all(&mut self, value: bool) {
		match self.inner() {
			Inner::Minor(head, _, tail) => {
				let elt = &mut self.as_mut()[0];
				for n in *head .. *tail {
					elt.set::<C>(n.into(), value);
				}
			},
			Inner::Major(_, _, _) => {
				let (h, t) = (self.bitptr().head(), self.bitptr().tail());
				if let Some(head) = self.head_mut() {
					for n in *h .. T::SIZE {
						head.set::<C>(n.into(), value);
					}
				}
				for elt in self.body_mut() {
					*elt = T::from(0);
				}
				if let Some(tail) = self.tail_mut() {
					for n in *t .. T::SIZE {
						tail.set::<C>(n.into(), value);
					}
				}
			}
		}
	}

	/// Provides mutable traversal of the collection.
	///
	/// It is impossible to implement `IndexMut` on `BitSlice`, because bits do
	/// not have addresses, so there can be no `&mut u1`. This method allows the
	/// client to receive an enumerated bit, and provide a new bit to set at
	/// each index.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `func`: A function which receives a `(usize, bool)` pair of index and
	///   value, and returns a bool. It receives the bit at each position, and
	///   the return value is written back at that position.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	/// ```
	pub fn for_each<F>(&mut self, func: F)
	where F: Fn(usize, bool) -> bool {
		for idx in 0 .. self.len() {
			let tmp = self[idx];
			self.set(idx, func(idx, tmp));
		}
	}

	pub fn as_slice(&self) -> &[T] {
		//  Get the `BitPtr` structure.
		let bp = self.bitptr();
		//  Get the pointer and element counts from it.
		let (ptr, len) = (bp.pointer(), bp.elements());
		//  Create a slice from them.
		unsafe { slice::from_raw_parts(ptr, len) }
	}

	/// Accesses the underlying store.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let mut bv: BitVec = bitvec![0, 0, 0, 0, 0, 0, 0, 0, 1];
	/// for elt in bv.as_mut_slice() {
	///   *elt += 2;
	/// }
	/// assert_eq!(&[2, 0b1000_0010], bv.as_slice());
	/// ```
	pub fn as_mut_slice(&mut self) -> &mut [T] {
		//  Get the `BitPtr` structure.
		let bp = self.bitptr();
		//  Get the pointer and element counts from it.
		let (ptr, len) = (bp.pointer() as *mut T, bp.elements());
		//  Create a slice from them.
		unsafe { slice::from_raw_parts_mut(ptr, len) }
	}

	pub fn head(&self) -> Option<&T> {
		//  Transmute into the correct lifetime.
		unsafe { mem::transmute(self.bitptr().head_elt()) }
	}

	pub fn head_mut(&mut self) -> Option<&mut T> {
		unsafe { mem::transmute(self.bitptr().head_elt()) }
	}

	pub fn body(&self) -> &[T] {
		//  Transmute into the correct lifetime.
		unsafe { mem::transmute(self.bitptr().body_elts()) }
	}

	pub fn body_mut(&mut self) -> &mut [T] {
		//  Reattach the correct lifetime and mutability
		#[allow(mutable_transmutes)]
		unsafe { mem::transmute(self.bitptr().body_elts()) }
	}

	pub fn tail(&self) -> Option<&T> {
		//  Transmute into the correct lifetime.
		unsafe { mem::transmute(self.bitptr().tail_elt()) }
	}

	pub fn tail_mut(&mut self) -> Option<&mut T> {
		unsafe { mem::transmute(self.bitptr().tail_elt()) }
	}

	/// Accesses the underlying pointer structure.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// The [`BitPtr`] structure of the slice handle.
	///
	/// [`BitPtr`]: ../pointer/struct.BitPtr.html
	pub fn bitptr(&self) -> BitPtr<T> {
		self.into()
	}

	/// Splits the slice domain into its logical parts.
	///
	/// Produces either the single-element partial domain, or the edge and
	/// center elements of a multiple-element domain.
	fn inner(&self) -> Inner<T> {
		let bp = self.bitptr();
		let (h, t) = (bp.head(), bp.tail());
		//  single-element, cursors not at both edges
		if self.as_ref().len() == 1 && !(*h == 0 && *t == T::SIZE) {
			Inner::Minor(h, &self.as_ref()[0], t)
		}
		else {
			Inner::Major(self.head(), self.body(), self.tail())
		}
	}
}

enum Inner<'a, T: 'a + Bits> {
	Minor(BitIdx, &'a T, BitIdx),
	Major(Option<&'a T>, &'a [T], Option<&'a T>),
}

/// Creates an owned `BitVec<C, T>` from a borrowed `BitSlice<C, T>`.
#[cfg(feature = "alloc")]
impl<C, T> ToOwned for BitSlice<C, T>
where C: Cursor, T: Bits {
	type Owned = BitVec<C, T>;

	/// Clones a borrowed `BitSlice` into an owned `BitVec`.
	///
	/// # Examples
	///
	/// ```rust
	/// # #[cfg(feature = "alloc")] {
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0; 2];
	/// let src: &BitSlice = store.into();
	/// let dst = src.to_owned();
	/// assert_eq!(src, dst);
	/// # }
	/// ```
	fn to_owned(&self) -> Self::Owned {
		self.into()
	}
}

impl<C, T> Eq for BitSlice<C, T>
where C: Cursor, T: Bits {}

impl<C, T> Ord for BitSlice<C, T>
where C: Cursor, T: Bits {
	fn cmp(&self, rhs: &Self) -> Ordering {
		self.partial_cmp(rhs)
			.unwrap_or_else(|| unreachable!("`BitSlice` has a total ordering"))
	}
}

/// Tests if two `BitSlice`s are semantically — not bitwise — equal.
///
/// It is valid to compare two slices of different endianness or element types.
///
/// The equality condition requires that they have the same number of total bits
/// and that each pair of bits in semantic order are identical.
impl<A, B, C, D> PartialEq<BitSlice<C, D>> for BitSlice<A, B>
where A: Cursor, B: Bits, C: Cursor, D: Bits {
	/// Performas a comparison by `==`.
	///
	/// # Parameters
	///
	/// - `&self`
	/// - `rhs`: Another `BitSlice` against which to compare. This slice can
	///   have different cursor or storage types.
	///
	/// # Returns
	///
	/// If the two slices are equal, by comparing the lengths and bit values at
	/// each semantic index.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let lstore: &[u8] = &[8, 16, 32, 0];
	/// let rstore: &[u32] = &[0x10080400];
	/// let lbv: &BitSlice<LittleEndian, u8> = lstore.into();
	/// let rbv: &BitSlice<BigEndian, u32> = rstore.into();
	///
	/// assert_eq!(lbv, rbv);
	/// ```
	fn eq(&self, rhs: &BitSlice<C, D>) -> bool {
		if self.len() != rhs.len() {
			return false;
		}
		self.iter().zip(rhs.iter()).all(|(l, r)| l == r)
	}
}

/// Allow comparison against the allocated form.
#[cfg(feature = "alloc")]
impl<A, B, C, D> PartialEq<BitVec<C, D>> for BitSlice<A, B>
where A: Cursor, B: Bits, C: Cursor, D: Bits {
	fn eq(&self, rhs: &BitVec<C, D>) -> bool {
		<Self as PartialEq<BitSlice<C, D>>>::eq(self, &*rhs)
	}
}

#[cfg(feature = "alloc")]
impl<A, B, C, D> PartialEq<BitVec<C, D>> for &BitSlice<A, B>
where A: Cursor, B: Bits, C: Cursor, D: Bits {
	fn eq(&self, rhs: &BitVec<C, D>) -> bool {
		<BitSlice<A, B> as PartialEq<BitSlice<C, D>>>::eq(self, &*rhs)
	}
}

/// Compares two `BitSlice`s by semantic — not bitwise — ordering.
///
/// The comparison sorts by testing each index for one slice to have a set bit
/// where the other has an unset bit. If the slices are different, the slice
/// with the set bit sorts greater than the slice with the unset bit.
///
/// If one of the slices is exhausted before they differ, the longer slice is
/// greater.
impl<A, B, C, D> PartialOrd<BitSlice<C, D>> for BitSlice<A, B>
where A: Cursor, B: Bits, C: Cursor, D: Bits {
	/// Performs a comparison by `<` or `>`.
	///
	/// # Parameters
	///
	/// - `&self`
	/// - `rhs`: Another `BitSlice` against which to compare. This slice can
	///   have different cursor or storage types.
	///
	/// # Returns
	///
	/// The relative ordering of `self` against `rhs`. `self` is greater if it
	/// has a `true` bit at an index where `rhs` has a `false`; `self` is lesser
	/// if it has a `false` bit at an index where `rhs` has a `true`; if the two
	/// slices do not disagree then they are compared by length.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x45];
	/// let slice: &BitSlice = store.into();
	/// let a = &slice[0 .. 3]; // 010
	/// let b = &slice[0 .. 4]; // 0100
	/// let c = &slice[0 .. 5]; // 01000
	/// let d = &slice[4 .. 8]; // 0101
	///
	/// assert!(a < b);
	/// assert!(b < c);
	/// assert!(c < d);
	/// ```
	fn partial_cmp(&self, rhs: &BitSlice<C, D>) -> Option<Ordering> {
		for (l, r) in self.iter().zip(rhs.iter()) {
			match (l, r) {
				(true, false) => return Some(Ordering::Greater),
				(false, true) => return Some(Ordering::Less),
				_ => continue,
			}
		}
		self.len().partial_cmp(&rhs.len())
	}
}

#[cfg(feature = "alloc")]
impl<A, B, C, D> PartialOrd<BitVec<C, D>> for BitSlice<A, B>
where A: Cursor, B: Bits, C: Cursor, D: Bits {
	fn partial_cmp(&self, rhs: &BitVec<C, D>) -> Option<Ordering> {
		self.partial_cmp(&**rhs)
	}
}

#[cfg(feature = "alloc")]
impl<A, B, C, D> PartialOrd<BitVec<C, D>> for &BitSlice<A, B>
where A: Cursor, B: Bits, C: Cursor, D: Bits {
	fn partial_cmp(&self, rhs: &BitVec<C, D>) -> Option<Ordering> {
		(*self).partial_cmp(&**rhs)
	}
}

/// Provides write access to all elements in the underlying storage, including
/// the partial head and tail elements if present.
impl<C, T> AsMut<[T]> for BitSlice<C, T>
where C: Cursor, T: Bits {
	/// Accesses the underlying store.
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// A mutable slice of all storage elements.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &mut [u8] = &mut [0, 128];
	/// let bv: &mut BitSlice = store.into();
	/// let bv = &mut bv[1 .. 9];
	///
	/// for elt in bv.as_mut() {
	///   *elt += 2;
	/// }
	///
	/// assert_eq!(&[2, 130], bv.as_ref());
	/// ```
	fn as_mut(&mut self) -> &mut [T] {
		self.as_mut_slice()
	}
}

/// Provides read access to all elements in the underlying storage, including
/// the partial head and tail elements if present.
impl<C, T> AsRef<[T]> for BitSlice<C, T>
where C: Cursor, T: Bits {
	/// Accesses the underlying store.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// An immutable slice of all storage elements.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0, 128];
	/// let bv: &BitSlice = store.into();
	/// let bv = &bv[1 .. 9];
	/// assert_eq!(&[0, 128], bv.as_ref());
	/// ```
	fn as_ref(&self) -> &[T] {
		self.as_slice()
	}
}

/// Builds a `BitSlice` from a slice of elements. The resulting `BitSlice` will
/// always completely fill the original slice.
impl<'a, C, T> From<&'a [T]> for &'a BitSlice<C, T>
where C: Cursor, T: 'a + Bits {
	/// Wraps a `&[T: Bits]` in a `&BitSlice<C: Cursor, T>`. The endianness must
	/// be specified at the call site. The element type cannot be changed.
	///
	/// # Parameters
	///
	/// - `src`: The elements over which the new `BitSlice` will operate.
	///
	/// # Returns
	///
	/// A `BitSlice` representing the original element slice.
	///
	/// # Panics
	///
	/// The source slice must not exceed the maximum number of elements that a
	/// `BitSlice` can contain. This value is documented in [`BitPtr`].
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let src: &[u8] = &[1, 2, 3];
	/// let bits: &BitSlice = src.into();
	/// assert_eq!(bits.len(), 24);
	/// assert_eq!(bits.as_ref().len(), 3);
	/// assert!(bits[7]);  // src[0] == 0b0000_0001
	/// assert!(bits[14]); // src[1] == 0b0000_0010
	/// assert!(bits[22]); // src[2] == 0b0000_0011
	/// assert!(bits[23]);
	/// ```
	///
	/// [`BitPtr`]: ../pointer/struct.BitPtr.html
	fn from(src: &'a [T]) -> Self {
		BitPtr::new(src.as_ptr(), src.len(), 0, T::SIZE).into()
	}
}

/// Builds a mutable `BitSlice` from a slice of mutable elements. The resulting
/// `BitSlice` will always completely fill the original slice.
impl<'a, C, T> From<&'a mut [T]> for &'a mut BitSlice<C, T>
where C: Cursor, T: 'a + Bits {
	/// Wraps a `&mut [T: Bits]` in a `&mut BitSlice<C: Cursor, T>`. The
	/// endianness must be specified by the call site. The element type cannot
	/// be changed.
	///
	/// # Parameters
	///
	/// - `src`: The elements over which the new `BitSlice` will operate.
	///
	/// # Returns
	///
	/// A `BitSlice` representing the original element slice.
	///
	/// # Panics
	///
	/// The source slice must not exceed the maximum number of elements that a
	/// `BitSlice` can contain. This value is documented in [`BitPtr`].
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let src: &mut [u8] = &mut [1, 2, 3];
	/// let bits: &mut BitSlice<LittleEndian, _> = src.into();
	/// //  The first bit is the LSb of the first element.
	/// assert!(bits[0]);
	/// bits.set(0, false);
	/// assert!(!bits[0]);
	/// assert_eq!(bits.as_ref(), &[0, 2, 3]);
	/// ```
	///
	/// [`BitPtr`]: ../pointer/struct.BitPtr.html
	fn from(src: &'a mut [T]) -> Self {
		BitPtr::new(src.as_ptr(), src.len(), 0, T::SIZE).into()
	}
}

/// Converts a `BitPtr` representation into a `BitSlice` handle.
impl<'a, C, T> From<BitPtr<T>> for &'a BitSlice<C, T>
where C: Cursor, T: 'a + Bits {
	/// Converts a `BitPtr` representation into a `BitSlice` handle.
	///
	/// # Parameters
	///
	/// - `src`: The `BitPtr` representation for the slice.
	///
	/// # Returns
	///
	/// A `BitSlice` handle for the slice domain the `BitPtr` represents.
	///
	/// # Examples
	///
	/// This example is crate-internal, and cannot be used by clients.
	///
	/// ```rust
	/// # #[cfg(feature = "testing")] {
	/// use bitvec::testing::*;
	///
	/// let store: &[u8] = &[1, 2, 3];
	/// let bp = BitPtr::new(store.as_ptr(), 3, 2, 6);
	/// let bv: &BitSlice = bp.into();
	/// assert_eq!(bv.len(), 20);
	/// assert_eq!(bv.as_ref(), store);
	/// # }
	/// ```
	fn from(src: BitPtr<T>) -> Self { unsafe {
		let (ptr, len) = mem::transmute::<BitPtr<T>, (*const (), usize)>(src);
		let store = slice::from_raw_parts(ptr, len);
		mem::transmute::<&[()], &'a BitSlice<C, T>>(store)
	} }
}

/// Converts a `BitPtr` representation into a `BitSlice` handle.
impl<C, T> From<BitPtr<T>> for &mut BitSlice<C, T>
where C: Cursor, T: Bits {
	/// Converts a `BitPtr` representation into a `BitSlice` handle.
	///
	/// # Parameters
	///
	/// - `src`: The `BitPtr` representation for the slice.
	///
	/// # Returns
	///
	/// A `BitSlice` handle for the slice domain the `BitPtr` represents.
	///
	/// # Examples
	///
	/// This example is crate-internal, and cannot be used by clients.
	///
	/// ```rust
	/// # #[cfg(feature = "testing")] {
	/// use bitvec::testing::*;
	///
	/// let store: &mut [u8] = &mut [1, 2, 3];
	/// let bp = BitPtr::new(store.as_ptr(), 3, 2, 6);
	/// let bv: &mut BitSlice = bp.into();
	/// assert_eq!(bv.len(), 20);
	/// assert_eq!(bv.as_ref(), store);
	/// # }
	/// ```
	fn from(src: BitPtr<T>) -> Self { unsafe {
		let (ptr, len) = mem::transmute::<BitPtr<T>, (*mut (), usize)>(src);
		let store = slice::from_raw_parts_mut(ptr, len);
		mem::transmute::<&mut [()], &mut BitSlice<C, T>>(store)
	} }
}

/// Prints the `BitSlice` for debugging.
///
/// The output is of the form `BitSlice<C, T> [ELT, *]` where `<C, T>` is the
/// cursor and element type, with square brackets on each end of the bits and
/// all the elements of the array printed in binary. The printout is always in
/// semantic order, and may not reflect the underlying buffer. To see the
/// underlying buffer, use `.as_ref()`.
///
/// The alternate character `{:#?}` prints each element on its own line, rather
/// than having all elements on the same line.
impl<C, T> Debug for BitSlice<C, T>
where C: Cursor, T: Bits {
	/// Renders the `BitSlice` type header and contents for debug.
	///
	/// # Examples
	///
	/// ```rust
	/// # #[cfg(feature = "alloc")] {
	/// use bitvec::*;
	/// let bits: &BitSlice<LittleEndian, u16> = &bitvec![
	///   LittleEndian, u16;
	///   0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 0, 1,
	///   0, 1
	/// ];
	/// assert_eq!(
    ///     "BitSlice<LittleEndian, u16> [0101000011110101, 01]",
	///     &format!("{:?}", bits)
	/// );
	/// # }
	/// ```
	fn fmt(&self, f: &mut Formatter) -> fmt::Result {
		f.write_str("BitSlice<")?;
		f.write_str(C::TYPENAME)?;
		f.write_str(", ")?;
		f.write_str(T::TYPENAME)?;
		f.write_str("> ")?;
		Display::fmt(self, f)
	}
}

/// Prints the `BitSlice` for displaying.
///
/// This prints each element in turn, formatted in binary in semantic order (so
/// the first bit seen is printed first and the last bit seen is printed last).
/// Each element of storage is separated by a space for ease of reading.
///
/// The alternate character `{:#}` prints each element on its own line.
///
/// To see the in-memory representation, use `.as_ref()` to get access to the
/// raw elements and print that slice instead.
impl<C, T> Display for BitSlice<C, T>
where C: Cursor, T: Bits {
	/// Renders the `BitSlice` contents for display.
	///
	/// # Parameters
	///
	/// - `&self`
	/// - `f`: The formatter into which `self` is written.
	///
	/// # Returns
	///
	/// The result of the formatting operation.
	///
	/// # Examples
	///
	/// ```rust
	/// # #[cfg(feature = "alloc")] {
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0b01001011, 0b0100_0000];
	/// let bits: &BitSlice = store.into();
	/// assert_eq!("[01001011, 01]", &format!("{}", &bits[.. 10]));
	/// # }
	/// ```
	fn fmt(&self, f: &mut Formatter) -> fmt::Result {
		struct Part<'a>(&'a str);
		impl<'a> Debug for Part<'a> {
			fn fmt(&self, f: &mut Formatter) -> fmt::Result {
				f.write_str(&self.0)
			}
		}
		let mut dbg = f.debug_list();
		//  Empty slice
		if self.is_empty() {
			return dbg.finish();
		}
		else {
			//  Unfortunately, T::SIZE cannot be used as the size for the array,
			//  due to limitations in the type system. As such, set
			//  it to the maximum used size.
			//
			//  This allows the writes to target a static buffer, rather
			//  than a dynamic string, making the formatter usable in
			//  `no-std` contexts.
			let mut w: [u8; 64] = [0; 64];
			let writer =
			|l: &mut DebugList, w: &mut [u8; 64], e: &T, from: u8, to: u8| {
				let (from, to) = (from as usize, to as usize);
				for n in from .. to {
					w[n] = if e.get::<C>((n as u8).into()) { b'1' }
					else { b'0' };
				}
				l.entry(&Part(unsafe {
					str::from_utf8_unchecked(&w[from .. to])
				}));
			};
			match self.inner() {
				//  Single-element slice
				Inner::Minor(head, elt, tail) => {
					writer(&mut dbg, &mut w, elt, *head, *tail)
				},
				//  Multi-element slice
				Inner::Major(head, body, tail) => {
					if let Some(head) = head {
						let hc = self.bitptr().head();
						writer(&mut dbg, &mut w, head, *hc, T::SIZE);
					}
					for elt in body {
						writer(&mut dbg, &mut w, elt, 0, T::SIZE);
					}
					if let Some(tail) = tail {
						let tc = self.bitptr().tail();
						writer(&mut dbg, &mut w, tail, 0, *tc);
					}
				},
			}
			dbg.finish()
		}
	}
}

/// Writes the contents of the `BitSlice`, in semantic bit order, into a hasher.
impl<C, T> Hash for BitSlice<C, T>
where C: Cursor, T: Bits {
	/// Writes each bit of the `BitSlice`, as a full `bool`, into the hasher.
	///
	/// # Parameters
	///
	/// - `&self`
	/// - `hasher`: The hashing state into which the slice will be written.
	///
	/// # Type Parameters
	///
	/// - `H: Hasher`: The type of the hashing algorithm which receives the bits
	///   of `self`.
	fn hash<H>(&self, hasher: &mut H)
	where H: Hasher {
		for bit in self {
			hasher.write_u8(bit as u8);
		}
	}
}

/// Produces a read-only iterator over all the bits in the `BitSlice`.
///
/// This iterator follows the ordering in the `BitSlice` type, and implements
/// `ExactSizeIterator` as `BitSlice` has a known, fixed, length, and
/// `DoubleEndedIterator` as it has known ends.
impl<'a, C, T> IntoIterator for &'a BitSlice<C, T>
where C: Cursor, T: 'a + Bits {
	type Item = bool;
	type IntoIter = Iter<'a, C, T>;

	/// Iterates over the slice.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// An iterator over the slice domain.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0b1010_1100];
	/// let bits: &BitSlice = store.into();
	/// let mut count = 0;
	/// for bit in bits {
	///   if bit { count += 1; }
	/// }
	/// assert_eq!(count, 4);
	/// ```
	fn into_iter(self) -> Self::IntoIter {
		Iter {
			inner: self
		}
	}
}

/// Performs unsigned addition in place on a `BitSlice`.
///
/// If the addend bitstream is shorter than `self`, the addend is zero-extended
/// at the left (so that its final bit matches with `self`’s final bit). If the
/// addend is longer, the excess front length is unused.
///
/// Addition proceeds from the right ends of each slice towards the left.
/// Because this trait is forbidden from returning anything, the final carry-out
/// bit is discarded.
///
/// Note that, unlike `BitVec`, there is no subtraction implementation until I
/// find a subtraction algorithm that does not require modifying the subtrahend.
///
/// Subtraction can be implemented by negating the intended subtrahend yourself
/// and then using addition, or by using `BitVec`s instead of `BitSlice`s.
///
/// # Type Parameters
///
/// - `I: IntoIterator<Item=bool, IntoIter: DoubleEndedIterator>`: The bitstream
///   to add into `self`. It must be finite and double-ended, since addition
///   operates in reverse.
impl<C, T, I> AddAssign<I> for BitSlice<C, T>
where C: Cursor, T: Bits,
	I: IntoIterator<Item=bool>, I::IntoIter: DoubleEndedIterator {
	/// Performs unsigned wrapping addition in place.
	///
	/// # Examples
	///
	/// This example shows addition of a slice wrapping from max to zero.
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &mut [u8] = &mut [0b1110_1111, 0b0000_0001];
	/// let bv: &mut BitSlice = store.into();
	/// let (nums, one) = bv.split_at_mut(12);
	/// let (accum, steps) = nums.split_at_mut(4);
	/// *accum += &*one;
	/// assert_eq!(accum, &steps[.. 4]);
	/// *accum += &*one;
	/// assert_eq!(accum, &steps[4 ..]);
	/// ```
	fn add_assign(&mut self, addend: I) {
		use core::iter::repeat;
		//  zero-extend the addend if it’s shorter than self
		let mut addend_iter = addend.into_iter().rev().chain(repeat(false));
		let mut c = false;
		for place in (0 .. self.len()).rev() {
			//  See `BitVec::AddAssign`
			static JUMP: [u8; 8] = [0, 2, 2, 1, 2, 1, 1, 3];
			let a = self[place];
			let b = addend_iter.next().unwrap(); // addend is an infinite source
			let idx = ((c as u8) << 2) | ((a as u8) << 1) | (b as u8);
			let yz = JUMP[idx as usize];
			let (y, z) = (yz & 2 != 0, yz & 1 != 0);
			self.set(place, y);
			c = z;
		}
	}
}

/// Performs the Boolean `AND` operation against another bitstream and writes
/// the result into `self`. If the other bitstream ends before `self,`, the
/// remaining bits of `self` are cleared.
///
/// # Type Parameters
///
/// - `I: IntoIterator<Item=bool>`: A stream of bits, which may be a `BitSlice`
///   or some other bit producer as desired.
impl<C, T, I> BitAndAssign<I> for BitSlice<C, T>
where C: Cursor, T: Bits, I: IntoIterator<Item=bool> {
	/// `AND`s a bitstream into a slice.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `rhs`: The bitstream to `AND` into `self`.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &mut [u8] = &mut [0b0101_0100];
	/// let other: &    [u8] = &    [0b0011_0000];
	/// let lhs: &mut BitSlice = store.into();
	/// let rhs: &    BitSlice = other.into();
	/// lhs[.. 6] &= &rhs[.. 4];
	/// assert_eq!(store[0], 0b0001_0000);
	/// ```
	fn bitand_assign(&mut self, rhs: I) {
		use core::iter;
		rhs.into_iter()
			.chain(iter::repeat(false))
			.enumerate()
			.take(self.len())
			.for_each(|(idx, bit)| {
				let val = self[idx] & bit;
				self.set(idx, val);
			});
	}
}

/// Performs the Boolean `OR` operation against another bitstream and writes the
/// result into `self`. If the other bitstream ends before `self`, the remaining
/// bits of `self` are not affected.
///
/// # Type Parameters
///
/// - `I: IntoIterator<Item=bool>`: A stream of bits, which may be a `BitSlice`
///   or some other bit producer as desired.
impl<C, T, I> BitOrAssign<I> for BitSlice<C, T>
where C: Cursor, T: Bits, I: IntoIterator<Item=bool> {
	/// `OR`s a bitstream into a slice.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `rhs`: The bitstream to `OR` into `self`.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	/// let store: &mut [u8] = &mut [0b0101_0100];
	/// let other: &    [u8] = &    [0b0011_0000];
	/// let lhs: &mut BitSlice = store.into();
	/// let rhs: &    BitSlice = other.into();
	/// lhs[.. 6] |= &rhs[.. 4];
	/// assert_eq!(store[0], 0b0111_0100);
	/// ```
	fn bitor_assign(&mut self, rhs: I) {
		for (idx, bit) in rhs.into_iter().enumerate().take(self.len()) {
			let val = self[idx] | bit;
			self.set(idx, val);
		}
	}
}

/// Performs the Boolean `XOR` operation against another bitstream and writes
/// the result into `self`. If the other bitstream ends before `self`, the
/// remaining bits of `self` are not affected.
///
/// # Type Parameters
///
/// - `I: IntoIterator<Item=bool>`: A stream of bits, which may be a `BitSlice`
///   or some other bit producer as desired.
impl<C, T, I> BitXorAssign<I> for BitSlice<C, T>
where C: Cursor, T: Bits, I: IntoIterator<Item=bool> {
	/// `XOR`s a bitstream into a slice.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `rhs`: The bitstream to `XOR` into `self`.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &mut [u8] = &mut [0b0101_0100];
	/// let other: &    [u8] = &    [0b0011_0000];
	/// let lhs: &mut BitSlice = store.into();
	/// let rhs: &    BitSlice = other.into();
	/// lhs[.. 6] ^= &rhs[.. 4];
	/// assert_eq!(store[0], 0b0110_0100);
	/// ```
	fn bitxor_assign(&mut self, rhs: I) {
		rhs.into_iter()
			.enumerate()
			.take(self.len())
			.for_each(|(idx, bit)| {
				let val = self[idx] ^ bit;
				self.set(idx, val);
			})
	}
}

/// Indexes a single bit by semantic count. The index must be less than the
/// length of the `BitSlice`.
impl<C, T> Index<usize> for BitSlice<C, T>
where C: Cursor, T: Bits {
	type Output = bool;

	/// Looks up a single bit by semantic index.
	///
	/// # Parameters
	///
	/// - `&self`
	/// - `index`: The semantic index of the bit to look up.
	///
	/// # Returns
	///
	/// The value of the bit at the requested index.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0b0010_0000];
	/// let bits: &BitSlice = store.into();
	/// assert!(bits[2]);
	/// assert!(!bits[3]);
	/// ```
	fn index(&self, index: usize) -> &Self::Output {
		let len = self.len();
		assert!(index < len, "Index out of range: {} >= {}", index, len);

		let h = self.bitptr().head();
		let (elt, bit) = h.offset::<T>(index as isize);
		if self.as_ref()[elt as usize].get::<C>(bit) { &true } else { &false }
	}
}

impl<C, T> Index<Range<usize>> for BitSlice<C, T>
where C: Cursor, T: Bits {
	type Output = Self;

	fn index(&self, Range { start, end }: Range<usize>) -> &Self::Output {
		let len = self.len();
		assert!(
			start <= len,
			"Index {} out of range: {}",
			start,
			len,
		);
		assert!(end <= len, "Index {} out of range: {}", end, len);
		assert!(end >= start, "Ranges can only run from low to high");
		let (data, _, head, _) = self.bitptr().raw_parts();
		//  Find the number of elements to drop from the front, and the index of
		//  the new head
		let (skip, new_head) = head.offset::<T>(start as isize);
		//  Find the number of elements contained in the new span, and the index
		//  of the new tail.
		let (new_elts, new_tail) = new_head.span::<T>(end - start);
		BitPtr::new(
			unsafe { data.offset(skip) },
			new_elts,
			new_head,
			new_tail,
		).into()
	}
}

impl<C, T> IndexMut<Range<usize>> for BitSlice<C, T>
where C: Cursor, T: Bits {
	fn index_mut(
		&mut self,
		Range { start, end }: Range<usize>,
	) -> &mut Self::Output {
		//  Get an immutable slice, and then type-hack mutability back in.
		(&self[start .. end]).bitptr().into()
	}
}

impl<C, T> Index<RangeInclusive<usize>> for BitSlice<C, T>
where C: Cursor, T: Bits {
	type Output = Self;

	fn index(&self, index: RangeInclusive<usize>) -> &Self::Output {
		&self[*index.start() .. *index.end() + 1]
	}
}

impl<C, T> IndexMut<RangeInclusive<usize>> for BitSlice<C, T>
where C: Cursor, T: Bits {
	fn index_mut(&mut self, index: RangeInclusive<usize>) -> &mut Self::Output {
		&mut self[*index.start() .. *index.end() + 1]
	}
}

impl<C, T> Index<RangeFrom<usize>> for BitSlice<C, T>
where C: Cursor, T: Bits {
	type Output = Self;

	fn index(&self, RangeFrom { start }: RangeFrom<usize>) -> &Self::Output {
		&self[start .. self.len()]
	}
}

impl<C, T> IndexMut<RangeFrom<usize>> for BitSlice<C, T>
where C: Cursor, T: Bits {
	fn index_mut(
		&mut self,
		RangeFrom { start }: RangeFrom<usize>,
	) -> &mut Self::Output {
		let len = self.len();
		&mut self[start .. len]
	}
}

impl<C, T> Index<RangeFull> for BitSlice<C, T>
where C: Cursor, T: Bits {
	type Output = Self;

	fn index(&self, _: RangeFull) -> &Self::Output {
		self
	}
}

impl<C, T> IndexMut<RangeFull> for BitSlice<C, T>
where C: Cursor, T: Bits {
	fn index_mut(&mut self, _: RangeFull) -> &mut Self::Output {
		self
	}
}

impl<C, T> Index<RangeTo<usize>> for BitSlice<C, T>
where C: Cursor, T: Bits {
	type Output = Self;

	fn index(&self, RangeTo { end }: RangeTo<usize>) -> &Self::Output {
		&self[0 .. end]
	}
}

impl<C, T> IndexMut<RangeTo<usize>> for BitSlice<C, T>
where C: Cursor, T: Bits {
	fn index_mut(
		&mut self,
		RangeTo { end }: RangeTo<usize>,
	) -> &mut Self::Output {
		&mut self[0 .. end]
	}
}

impl<C, T> Index<RangeToInclusive<usize>> for BitSlice<C, T>
where C: Cursor, T: Bits {
	type Output = Self;

	fn index(
		&self,
		RangeToInclusive { end }: RangeToInclusive<usize>,
	) -> &Self::Output {
		&self[0 .. end + 1]
	}
}

impl<C, T> IndexMut<RangeToInclusive<usize>> for BitSlice<C, T>
where C: Cursor, T: Bits {
	fn index_mut(
		&mut self,
		RangeToInclusive { end }: RangeToInclusive<usize>,
	) -> &mut Self::Output {
		&mut self[0 .. end + 1]
	}
}

/// Performs fixed-width 2’s-complement negation of a `BitSlice`.
///
/// Unlike the `!` operator (`Not` trait), the unary `-` operator treats the
/// `BitSlice` as if it represents a signed 2’s-complement integer of fixed
/// width. The negation of a number in 2’s complement is defined as its
/// inversion (using `!`) plus one, and on fixed-width numbers has the following
/// discontinuities:
///
/// - A slice whose bits are all zero is considered to represent the number zero
///   which negates as itself.
/// - A slice whose bits are all one is considered to represent the most
///   negative number, which has no correpsonding positive number, and thus
///   negates as zero.
///
/// This behavior was chosen so that all possible values would have *some*
/// output, and so that repeated application converges at idempotence. The most
/// negative input can never be reached by negation, but `--MOST_NEG` converges
/// at the least unreasonable fallback value, 0.
///
/// Because `BitSlice` cannot move, the negation is performed in place.
impl<'a, C, T> Neg for &'a mut BitSlice<C, T>
where C: Cursor, T: 'a + Bits {
	type Output = Self;

	/// Perform 2’s-complement fixed-width negation.
	///
	/// Negation is accomplished by inverting the bits and adding one. This has
	/// one edge case: `1000…`, the most negative number for its width, will
	/// negate to zero instead of itself. It thas no corresponding positive
	/// number to which it can negate.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Examples
	///
	/// The contortions shown here are a result of this operator applying to a
	/// mutable reference, and this example balancing access to the original
	/// `BitVec` for comparison with aquiring a mutable borrow *as a slice* to
	/// ensure that the `BitSlice` implementation is used, not the `BitVec`.
	///
	/// Negate an arbitrary positive number (first bit unset).
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &mut [u8] = &mut [0b0110_1010];
	/// let bv: &mut BitSlice = store.into();
	/// eprintln!("{:?}", bv.split_at(4));
	/// let num = &mut bv[.. 4];
	/// -num;
	/// eprintln!("{:?}", bv.split_at(4));
	/// assert_eq!(&bv[.. 4], &bv[4 ..]);
	/// ```
	///
	/// Negate an arbitrary negative number. This example will use the above
	/// result to demonstrate round-trip correctness.
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &mut [u8] = &mut [0b1010_0110];
	/// let bv: &mut BitSlice = store.into();
	/// let num = &mut bv[.. 4];
	/// -num;
	/// assert_eq!(&bv[.. 4], &bv[4 ..]);
	/// ```
	///
	/// Negate the most negative number, which will become zero, and show
	/// convergence at zero.
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &mut [u8] = &mut [128];
	/// let bv: &mut BitSlice = store.into();
	/// let num = &mut bv[..];
	/// -num;
	/// assert!(bv.not_any());
	/// let num = &mut bv[..];
	/// -num;
	/// assert!(bv.not_any());
	/// ```
	fn neg(self) -> Self::Output {
		//  negative zero is zero. The invert-and-add will result in zero, but
		//  this case can be detected quickly.
		if self.is_empty() || self.not_any() {
			return self;
		}
		//  The most negative number (leading one, all zeroes else) negates to
		//  zero.
		if self[0] {
			//  Testing the whole range, rather than [1 ..], is more likely to
			//  hit the fast path.
			self.set(0, false);
			if self.not_any() {
				return self;
			}
			self.set(0, true);
		}
		let _ = Not::not(&mut *self);
		let one: &[T] = &[T::from(!0)];
		let one_bs: &BitSlice<C, T> = one.into();
		AddAssign::add_assign(&mut *self, &one_bs[.. 1]);
		self
	}
}

/// Flips all bits in the slice, in place.
impl<'a, C, T> Not for &'a mut BitSlice<C, T>
where C: Cursor, T: 'a + Bits {
	type Output = Self;

	/// Inverts all bits in the slice.
	///
	/// This will not affect bits outside the slice in slice storage elements.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &mut [u8] = &mut [0; 2];
	/// let bv: &mut BitSlice = store.into();
	/// let bits = &mut bv[2 .. 14];
	/// let new_bits = !bits;
	/// //  The `bits` binding is consumed by the `!` operator, and a new reference
	/// //  is returned.
	/// // assert_eq!(bits.as_ref(), &[!0, !0]);
	/// assert_eq!(new_bits.as_ref(), &[0x3F, 0xFC]);
	/// ```
	fn not(self) -> Self::Output {
		match self.inner() {
			Inner::Minor(head, _, tail) => {
				let elt = &mut self.as_mut()[0];
				for n in *head .. *tail {
					let tmp = elt.get::<C>(n.into());
					elt.set::<C>(n.into(), !tmp);
				}
			},
			Inner::Major(_, _, _) => {
				let head_bit = self.bitptr().head();
				let tail_bit = self.bitptr().tail();
				if let Some(head) = self.head_mut() {
					for n in *head_bit .. T::SIZE {
						let tmp = head.get::<C>(n.into());
						head.set::<C>(n.into(), !tmp);
					}
				}
				for elt in self.body_mut() {
					*elt = !*elt;
				}
				if let Some(tail) = self.tail_mut() {
					for n in 0 .. *tail_bit {
						let tmp = tail.get::<C>(n.into());
						tail.set::<C>(n.into(), !tmp);
					}
				}
			},
		}
		self
	}
}

__bitslice_shift!(u8, u16, u32, u64, i8, i16, i32, i64);

/// Shifts all bits in the array to the left — **DOWN AND TOWARDS THE FRONT**.
///
/// On primitives, the left-shift operator `<<` moves bits away from the origin
/// and towards the ceiling. This is because we label the bits in a primitive
/// with the minimum on the right and the maximum on the left, which is
/// big-endian bit order. This increases the value of the primitive being
/// shifted.
///
/// **THAT IS NOT HOW `BitSlice` WORKS!**
///
/// `BitSlice` defines its layout with the minimum on the left and the maximum
/// on the right! Thus, left-shifting moves bits towards the **minimum**.
///
/// In BigEndian order, the effect in memory will be what you expect the `<<`
/// operator to do.
///
/// **In LittleEndian order, the effect will be equivalent to using `>>` on**
/// **the primitives in memory!**
///
/// # Notes
///
/// In order to preserve the effecs in memory that this operator traditionally
/// expects, the bits that are emptied by this operation are zeroed rather than
/// left to their old value.
///
/// The shift amount is modulated against the array length, so it is not an
/// error to pass a shift amount greater than the array length.
///
/// A shift amount of zero is a no-op, and returns immediately.
impl<C, T> ShlAssign<usize> for BitSlice<C, T>
where C: Cursor, T: Bits {
	/// Shifts a slice left, in place.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `shamt`: The shift amount. If this is greater than the length, then
	///   the slice is zeroed immediately.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &mut [u8] = &mut[0x4B, 0xA5];
	/// let bv: &mut BitSlice = store.into();
	/// let bits = &mut bv[2 .. 14];
	/// *bits <<= 3;
	/// assert_eq!(bits.as_ref(), &[0b01_011_101, 0b001_000_01]);
	/// ```
	fn shl_assign(&mut self, shamt: usize) {
		if shamt == 0 {
			return;
		}
		let len = self.len();
		if shamt >= len {
			self.set_all(false);
			return;
		}
		//  If the shift amount is an even multiple of the element width, use
		//  `ptr::copy` instead of a bitwise crawl.
		if shamt & T::MASK as usize == 0 {
			//  Compute the shift distance measured in elements.
			let offset = shamt >> T::BITS;
			//  Compute the number of elements that will remain.
			let rem = self.as_ref().len() - offset;
			//  Clear the bits after the tail cursor before the move.
			for n in *self.bitptr().tail() .. T::SIZE {
				self.as_mut()[len - 1].set::<C>(n.into(), false);
			}
			//  Memory model: suppose we have this slice of sixteen elements,
			//  that is shifted five elements to the left. We have three
			//  pointers and two lengths to manage.
			//  - rem is 11
			//  - offset is 5
			//  - head is [0]
			//  - body is [5; 11]
			//  - tail is [11]
			//  [ 0 1 2 3 4 5 6 7 8 9 a b c d e f ]
			//              ^-------before------^
			//    ^-------after-------^ 0 0 0 0 0
			//  Pointer to the front of the slice
			let head: *mut T = self.as_mut_ptr();
			//  Pointer to the front of the section that will move and be
			//  retained
			let body: *const T = &self.as_ref()[offset];
			//  Pointer to the back of the slice that will be zero-filled.
			let tail: *mut T = &mut self.as_mut()[rem];
			unsafe {
				ptr::copy(body, head, rem);
				ptr::write_bytes(tail, 0, offset);
			}
			return;
		}
		//  Otherwise, crawl.
		for (to, from) in (shamt .. len).enumerate() {
			let val = self[from];
			self.set(to, val);
		}
		for bit in (len - shamt) .. len {
			self.set(bit, false);
		}
	}
}

/// Shifts all bits in the array to the right — **UP AND TOWARDS THE BACK**.
///
/// On primitives, the right-shift operator `>>` moves bits towards the origin
/// and away from the ceiling. This is because we label the bits in a primitive
/// with the minimum on the right and the maximum on the left, which is
/// big-endian bit order. This decreases the value of the primitive being
/// shifted.
///
/// **THAT IS NOT HOW `BitSlice` WORKS!**
///
/// `BitSlice` defines its layout with the minimum on the left and the maximum
/// on the right! Thus, right-shifting moves bits towards the **maximum**.
///
/// In Big-Endian order, the effect in memory will be what you expect the `>>`
/// operator to do.
///
/// **In LittleEndian order, the effect will be equivalent to using `<<` on**
/// **the primitives in memory!**
///
/// # Notes
///
/// In order to preserve the effects in memory that this operator traditionally
/// expects, the bits that are emptied by this operation are zeroed rather than
/// left to their old value.
///
/// The shift amount is modulated against the array length, so it is not an
/// error to pass a shift amount greater than the array length.
///
/// A shift amount of zero is a no-op, and returns immediately.
impl<C, T> ShrAssign<usize> for BitSlice<C, T>
where C: Cursor, T: Bits {
	/// Shifts a slice right, in place.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `shamt`: The shift amount. If this is greater than the length, then
	///   the slice is zeroed immediately.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &mut [u8] = &mut[0x4B, 0xA5];
	/// let bv: &mut BitSlice = store.into();
	/// let bits = &mut bv[2 .. 14];
	/// *bits >>= 3;
	/// assert_eq!(bits.as_ref(), &[0b01_000_00_1, 0b011_101_01])
	/// ```
	fn shr_assign(&mut self, shamt: usize) {
		if shamt == 0 {
			return;
		}
		let len = self.len();
		if shamt >= len {
			self.set_all(false);
			return;
		}
		//  IF the shift amount is an even multiple of the element width, use
		//  `ptr::copy` instead of a bitwise crawl.
		if shamt & T::MASK as usize == 0 {
			//  Compute the shift amount measured in elements.
			let offset = shamt >> T::BITS;
			// Compute the number of elements that will remain.
			let rem = self.as_ref().len() - offset;
			//  Clear the bits ahead of the head cursor before the move.
			for n in 0 .. *self.bitptr().head() {
				self.as_mut()[0].set::<C>(n.into(), false);
			}
			//  Memory model: suppose we have this slice of sixteen elements,
			//  that is shifted five elements to the right. We have two pointers
			//  and two lengths to manage.
			//  - rem is 11
			//  - offset is 5
			//  - head is [0; 11]
			//  - body is [5]
			//  [ 0 1 2 3 4 5 6 7 8 9 a b c d e f ]
			//    ^-------before------^
			//    0 0 0 0 0 ^-------after-------^
			let head: *mut T = self.as_mut_ptr();
			let body: *mut T = &mut self.as_mut()[offset];
			unsafe {
				ptr::copy(head, body, rem);
				ptr::write_bytes(head, 0, offset);
			}
			return;
		}
		//  Otherwise, crawl.
		for (from, to) in (shamt .. len).enumerate().rev() {
			let val = self[from];
			self.set(to, val);
		}
		for bit in 0 .. shamt {
			self.set(bit.into(), false);
		}
	}
}

/// State keeper for chunked iteration over a `BitSlice`.
///
/// # Type Parameters
///
/// - `C: Cursor`: The bit-order type of the underlying `BitSlice`.
/// - `T: 'a + Bits`: The storage type of the underlying `BitSlice`.
///
/// # Lifetimes
///
/// - `'a`: The lifetime of the underlying `BitSlice`.
#[derive(Clone, Debug)]
pub struct Chunks<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// The `BitSlice` being iterated.
	inner: &'a BitSlice<C, T>,
	/// The width of the chunks.
	width: usize,
}

impl<'a, C, T> DoubleEndedIterator for Chunks<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// Produces the next chunk from the back of the slice.
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// The last chunk in the slice, if any.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[1];
	/// let bv: &BitSlice = store.into();
	/// let mut chunks = bv.chunks(5);
	/// assert_eq!(chunks.next_back(), Some(&bv[5 ..]));
	/// assert_eq!(chunks.next_back(), Some(&bv[.. 5]));
	/// assert!(chunks.next_back().is_none());
	/// ```
	fn next_back(&mut self) -> Option<Self::Item> {
		if self.inner.is_empty() {
			return None;
		}
		let len = self.inner.len();
		let rem = len % self.width;
		let size = if rem == 0 { self.width } else { rem };
		let (head, tail) = self.inner.split_at(len - size);
		self.inner = head;
		Some(tail)
	}
}

/// Mark that the iterator has an exact size.
impl<'a, C, T> ExactSizeIterator for Chunks<'a, C, T>
where C: Cursor, T: 'a + Bits {}

/// Mark that the iterator will not resume after halting.
impl<'a, C, T> FusedIterator for Chunks<'a, C, T>
where C: Cursor, T: 'a + Bits {}

impl<'a, C, T> Iterator for Chunks<'a, C, T>
where C: Cursor, T: 'a + Bits {
	type Item = &'a BitSlice<C, T>;

	/// Advances the iterator by one, returning the first chunk in it (if any).
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// The leading chunk in the iterator, if any.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x80];
	/// let bv: &BitSlice = store.into();
	/// let mut chunks = bv.chunks(5);
	/// assert_eq!(chunks.next(), Some(&bv[.. 5]));
	/// assert_eq!(chunks.next(), Some(&bv[5 ..]));
	/// assert!(chunks.next().is_none());
	/// ```
	fn next(&mut self) -> Option<Self::Item> {
		use core::cmp::min;
		if self.inner.is_empty() {
			return None;
		}
		let size = min(self.inner.len(), self.width);
		let (head, tail) = self.inner.split_at(size);
		self.inner = tail;
		Some(head)
	}

	/// Hints at the number of chunks remaining in the iterator.
	///
	/// Because the exact size is always known, this always produces
	/// `(len, Some(len))`.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// - `usize`: The minimum chunks remaining.
	/// - `Option<usize>`: The maximum chunks remaining.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bv: &BitSlice = store.into();
	/// let mut chunks = bv.chunks(5);
	/// assert_eq!(chunks.size_hint(), (2, Some(2)));
	/// chunks.next();
	/// assert_eq!(chunks.size_hint(), (1, Some(1)));
	/// chunks.next();
	/// assert_eq!(chunks.size_hint(), (0, Some(0)));
	/// ```
	fn size_hint(&self) -> (usize, Option<usize>) {
		if self.inner.is_empty() {
			return (0, Some(0));
		}
		let len = self.inner.len();
		let (n, r) = (len / self.width, len % self.width);
		let len = n + (r > 0) as usize;
		(len, Some(len))
	}

	/// Counts how many chunks are live in the iterator, consuming it.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// The number of chunks remaining in the iterator.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bv: &BitSlice = store.into();
	/// assert_eq!(bv.chunks(3).count(), 3);
	/// ```
	fn count(self) -> usize {
		self.len()
	}

	/// Advances the iterator by `n` chunks, starting from zero.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `n`: The number of chunks to skip, before producing the next bit after
	///   skips. If this overshoots the iterator’s remaining length, then the
	///   iterator is marked empty before returning `None`.
	///
	/// # Returns
	///
	/// If `n` does not overshoot the iterator’s bounds, this produces the `n`th
	/// bit after advancing the iterator to it, discarding the intermediate
	/// chunks.
	///
	/// If `n` does overshoot the iterator’s bounds, this empties the iterator
	/// and returns `None`.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bv: &BitSlice = store.into();
	/// let mut chunks = bv.chunks(3);
	/// assert_eq!(chunks.nth(1), Some(&bv[3 .. 6]));
	/// assert_eq!(chunks.nth(0), Some(&bv[6 ..]));
	/// assert!(chunks.nth(0).is_none());
	/// ```
	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		use core::cmp::min;
		let (start, ovf) = n.overflowing_mul(self.width);
		let len = self.inner.len();
		if start >= len || ovf {
			self.inner = BitSlice::empty();
			return None;
		}
		let end = start.checked_add(self.width)
			.map(|s| min(s, len))
			.unwrap_or(len);
		let out = &self.inner[start .. end];
		self.inner = &self.inner[end ..];
		Some(out)
	}

	/// Consumes the iterator, returning only the final chunk.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// The last chunk in the iterator slice, if any.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bv: &BitSlice = store.into();
	/// assert_eq!(bv.chunks(3).last(), Some(&bv[6 ..]));
	/// ```
	fn last(mut self) -> Option<Self::Item> {
		self.next_back()
	}
}

/// State keeper for mutable chunked iteration over a `BitSlice`.
///
/// # Type Parameters
///
/// - `C: Cursor`: The bit-order type of the underlying `BitSlice`.
/// - `T: 'a + Bits`: The storage type of the underlying `BitSlice`.
///
/// # Lifetimes
///
/// - `'a`: The lifetime of the underlying `BitSlice`.
#[derive(Debug)]
pub struct ChunksMut<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// The `BitSlice` being iterated.
	inner: &'a mut BitSlice<C, T>,
	/// The width of the chunks.
	width: usize,
}

impl<'a, C, T> DoubleEndedIterator for ChunksMut<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// Produces the next chunk from the back of the slice.
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// The last chunk in the slice, if any.
	fn next_back(&mut self) -> Option<Self::Item> {
		if self.inner.is_empty() {
			return None;
		}
		let len = self.inner.len();
		let rem = len % self.width;
		let size = if rem == 0 { self.width } else { rem };
		let tmp = mem::replace(&mut self.inner, BitSlice::empty_mut());
		let (head, tail) = tmp.split_at_mut(len - size);
		self.inner = head;
		Some(tail)
	}
}

impl<'a, C, T> ExactSizeIterator for ChunksMut<'a, C, T>
where C: Cursor, T: 'a + Bits {}

impl<'a, C, T> FusedIterator for ChunksMut<'a, C, T>
where C: Cursor, T: 'a + Bits {}

impl<'a, C, T> Iterator for ChunksMut<'a, C, T>
where C: Cursor, T: 'a + Bits {
	type Item = &'a mut BitSlice<C, T>;

	/// Advances the iterator by one, returning the first chunk in it (if any).
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// The leading chunk in the iterator, if any.
	fn next(&mut self) -> Option<Self::Item> {
		use core::cmp::min;
		if self.inner.is_empty() {
			return None;
		}
		let size = min(self.inner.len(), self.width);
		let tmp = mem::replace(&mut self.inner, BitSlice::empty_mut());
		let (head, tail) = tmp.split_at_mut(size);
		self.inner = tail;
		Some(head)
	}

	/// Hints at the number of chunks remaining in the iterator.
	///
	/// Because the exact size is always known, this always produces
	/// `(len, Some(len))`.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// - `usize`: The minimum chunks remaining.
	/// - `Option<usize>`: The maximum chunks remaining.
	fn size_hint(&self) -> (usize, Option<usize>) {
		if self.inner.is_empty() {
			return (0, Some(0));
		}
		let len = self.inner.len();
		let (n, r) = (len / self.width, len % self.width);
		let len = n + (r > 0) as usize;
		(len, Some(len))
	}

	/// Counts how many chunks are live in the iterator, consuming it.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// The number of chunks remaining in the iterator.
	fn count(self) -> usize {
		self.len()
	}

	/// Advances the iterator by `n` chunks, starting from zero.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `n`: The number of chunks to skip, before producing the next bit after
	///   skips. If this overshoots the iterator’s remaining length, then the
	///   iterator is marked empty before returning `None`.
	///
	/// # Returns
	///
	/// If `n` does not overshoot the iterator’s bounds, this produces the `n`th
	/// bit after advancing the iterator to it, discarding the intermediate
	/// chunks.
	///
	/// If `n` does overshoot the iterator’s bounds, this empties the iterator
	/// and returns `None`.
	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		use core::cmp::min;
		let (start, ovf) = n.overflowing_mul(self.width);
		let len = self.inner.len();
		if start >= len || ovf {
			self.inner = BitSlice::empty_mut();
			return None;
		}
		let end = start.checked_add(self.width)
			.map(|s| min(s, len))
			.unwrap_or(len);
		let tmp = mem::replace(&mut self.inner, BitSlice::empty_mut());
		let (head, tail) = tmp.split_at_mut(start);
		let (_, nth) = head.split_at_mut(end - start);
		self.inner = tail;
		Some(nth)
	}

	/// Consumes the iterator, returning only the final chunk.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// The last chunk in the iterator slice, if any.
	fn last(mut self) -> Option<Self::Item> {
		self.next_back()
	}
}

/// State keeper for exact chunked iteration over a `BitSlice`.
///
/// # Type Parameters
///
/// - `C: Cursor`: The bit-order type of the underlying `BitSlice`.
/// - `T: 'a + Bits`: The storage type of the underlying `BitSlice`.
///
/// # Lifetimes
///
/// - `'a`: The lifetime of the underlying `BitSlice`.
#[derive(Clone, Debug)]
pub struct ChunksExact<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// The `BitSlice` being iterated.
	inner: &'a BitSlice<C, T>,
	/// The excess of the original `BitSlice`, which is not iterated.
	extra: &'a BitSlice<C, T>,
	/// The width of the chunks.
	width: usize,
}

impl<'a, C, T> ChunksExact<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// Produces the remainder of the original slice, which will not be included
	/// in the iteration.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// The remaining slice that iteration will not include.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bits: &BitSlice = store.into();
	/// let chunks_exact = bits.chunks_exact(3);
	/// assert_eq!(chunks_exact.remainder(), &bits[6 ..]);
	/// ```
	pub fn remainder(&self) -> &'a BitSlice<C, T> {
		self.extra
	}
}

impl<'a, C, T> DoubleEndedIterator for ChunksExact<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// Produces the next chunk from the back of the slice.
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// The last chunk in the slice, if any.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[1];
	/// let bv: &BitSlice = store.into();
	/// let mut chunks_exact = bv.chunks_exact(3);
	/// assert_eq!(chunks_exact.next_back(), Some(&bv[3 .. 6]));
	/// assert_eq!(chunks_exact.next_back(), Some(&bv[0 .. 3]));
	/// assert!(chunks_exact.next_back().is_none());
	/// ```
	fn next_back(&mut self) -> Option<Self::Item> {
		if self.inner.len() < self.width {
			self.inner = BitSlice::empty();
			return None;
		}
		let (head, tail) = self.inner.split_at(self.inner.len() - self.width);
		self.inner = head;
		Some(tail)
	}
}

/// Mark that the iterator has an exact size.
impl<'a, C, T> ExactSizeIterator for ChunksExact<'a, C, T>
where C: Cursor, T: 'a + Bits {}

/// Mark that the iterator will not resume after halting.
impl<'a, C, T> FusedIterator for ChunksExact<'a, C, T>
where C: Cursor, T: 'a + Bits {}

impl<'a, C, T> Iterator for ChunksExact<'a, C, T>
where C: Cursor, T: 'a + Bits {
	type Item = &'a BitSlice<C, T>;

	/// Advances the iterator by one, returning the first chunk in it (if any).
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// The leading chunk in the iterator, if any.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x80];
	/// let bv: &BitSlice = store.into();
	/// let mut chunks_exact = bv.chunks_exact(3);
	/// assert_eq!(chunks_exact.next(), Some(&bv[0 .. 3]));
	/// assert_eq!(chunks_exact.next(), Some(&bv[3 .. 6]));
	/// assert!(chunks_exact.next().is_none());
	/// ```
	fn next(&mut self) -> Option<Self::Item> {
		if self.inner.len() < self.width {
			self.inner = BitSlice::empty();
			return None;
		}
		let (head, tail) = self.inner.split_at(self.width);
		self.inner = tail;
		Some(head)
	}

	/// Hints at the number of chunks remaining in the iterator.
	///
	/// Because the exact size is always known, this always produces
	/// `(len, Some(len))`.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// - `usize`: The minimum chunks remaining.
	/// - `Option<usize>`: The maximum chunks remaining.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bv: &BitSlice = store.into();
	/// let mut chunks_exact = bv.chunks_exact(3);
	/// assert_eq!(chunks_exact.size_hint(), (2, Some(2)));
	/// chunks_exact.next();
	/// assert_eq!(chunks_exact.size_hint(), (1, Some(1)));
	/// chunks_exact.next();
	/// assert_eq!(chunks_exact.size_hint(), (0, Some(0)));
	/// ```
	fn size_hint(&self) -> (usize, Option<usize>) {
		let len = self.inner.len() / self.width;
		(len, Some(len))
	}

	/// Counts how many chunks are live in the iterator, consuming it.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// The number of chunks remaining in the iterator.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bv: &BitSlice = store.into();
	/// assert_eq!(bv.chunks_exact(3).count(), 2);
	/// ```
	fn count(self) -> usize {
		self.len()
	}

	/// Advances the iterator by `n` chunks, starting from zero.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `n`: The number of chunks to skip, before producing the next bit after
	///   skips. If this overshoots the iterator’s remaining length, then the
	///   iterator is marked empty before returning `None`.
	///
	/// # Returns
	///
	/// If `n` does not overshoot the iterator’s bounds, this produces the `n`th
	/// bit after advancing the iterator to it, discarding the intermediate
	/// chunks.
	///
	/// If `n` does overshoot the iterator’s bounds, this empties the iterator
	/// and returns `None`.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[2];
	/// let bv: &BitSlice = store.into();
	/// let mut chunks_exact = bv.chunks_exact(3);
	/// assert_eq!(chunks_exact.nth(1), Some(&bv[3 .. 6]));
	/// assert!(chunks_exact.nth(0).is_none());
	/// ```
	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		let (start, ovf) = n.overflowing_mul(self.width);
		if start >= self.inner.len() || ovf {
			self.inner = BitSlice::empty();
			return None;
		}
		let (_, tail) = self.inner.split_at(start);
		self.inner = tail;
		self.next()
	}

	/// Consumes the iterator, returning only the final chunk.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// The last chunk in the iterator slice, if any.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bv: &BitSlice = store.into();
	/// assert_eq!(bv.chunks_exact(3).last(), Some(&bv[3 .. 6]));
	/// ```
	fn last(mut self) -> Option<Self::Item> {
		self.next_back()
	}
}

/// State keeper for mutable exact chunked iteration over a `BitSlice`.
///
/// # Type Parameters
///
/// - `C: Cursor`: The bit-order type of the underlying `BitSlice`.
/// - `T: 'a + Bits`: The storage type of the underlying `BitSlice`.
///
/// # Lifetimes
///
/// - `'a`: The lifetime of the underlying `BitSlice`.
#[derive(Debug)]
pub struct ChunksExactMut<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// The `BitSlice` being iterated.
	inner: &'a mut BitSlice<C, T>,
	/// The excess of the original `BitSlice`, which is not iterated.
	extra: &'a mut BitSlice<C, T>,
	/// The width of the chunks.
	width: usize,
}

impl<'a, C, T> ChunksExactMut<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// Produces the remainder of the original slice, which will not be included
	/// in the iteration.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// The remaining slice that iteration will not include.
	pub fn into_remainder(self) -> &'a mut BitSlice<C, T> {
		self.extra
	}
}

impl<'a, C, T> DoubleEndedIterator for ChunksExactMut<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// Produces the next chunk from the back of th eslice.
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// The last chunk in the slice, if any.
	fn next_back(&mut self) -> Option<Self::Item> {
		unimplemented!()
	}
}

impl<'a, C, T> ExactSizeIterator for ChunksExactMut<'a, C, T>
where C: Cursor, T: 'a + Bits {}

impl<'a, C, T> FusedIterator for ChunksExactMut<'a, C, T>
where C: Cursor, T: 'a + Bits {}

impl<'a, C, T> Iterator for ChunksExactMut<'a, C, T>
where C: Cursor, T: 'a + Bits {
	type Item = &'a mut BitSlice<C, T>;

	/// Advances the iterator by one, returning the first chunk in it (if any).
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// The leading chunk in the iterator, if any.
	fn next(&mut self) -> Option<Self::Item> {
		if self.inner.len() < self.width {
			self.inner = BitSlice::empty_mut();
			return None;
		}
		let tmp = mem::replace(&mut self.inner, BitSlice::empty_mut());
		let (head, tail) = tmp.split_at_mut(self.width);
		self.inner = tail;
		Some(head)
	}

	/// Hints at the number of chunks remaining in the iterator.
	///
	/// Because the exact size is always known, this always produces
	/// `(len, Some(len))`.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// - `usize`: The minimum chunks remaining.
	/// - `Option<usize>`: The maximum chunks remaining.
	fn size_hint(&self) -> (usize, Option<usize>) {
		let len = self.inner.len() / self.width;
		(len, Some(len))
	}

	/// Counts how many chunks are live in the iterator, consuming it.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// The number of chunks remaining in the iterator.
	fn count(self) -> usize {
		self.len()
	}

	/// Advances the iterator by `n` chunks, starting from zero.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `n`: The number of chunks to skip, before producing the next bit after
	///   skips. If this overshoots the iterator’s remaining length, then the
	///   iterator is marked empty before returning `None`.
	///
	/// # Returns
	///
	/// If `n` does not overshoot the iterator’s bounds, this produces the `n`th
	/// bit after advancing the iterator to it, discarding the intermediate
	/// chunks.
	///
	/// If `n` does overshoot the iterator’s bounds, this empties the iterator
	/// and returns `None`.
	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		let (start, ovf) = n.overflowing_mul(self.width);
		if start >= self.inner.len() || ovf {
			self.inner = BitSlice::empty_mut();
			return None;
		}
		let tmp = mem::replace(&mut self.inner, BitSlice::empty_mut());
		let (_, tail) = tmp.split_at_mut(start);
		self.inner = tail;
		self.next()
	}

	/// Consumes the iterator, returning only the final chunk.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// The last chunk in the iterator slice, if any.
	fn last(mut self) -> Option<Self::Item> {
		self.next_back()
	}
}

/// State keeper for iteration over a `BitSlice`.
///
/// # Type Parameters
///
/// - `C: Cursor`: The bit-order type of the underlying `BitSlice`.
/// - `T: 'a + Bits`: The storage type of the underlying `BitSlice`.
///
/// # Lifetimes
///
/// - `'a`: The lifetime of the underlying `BitSlice`.
#[derive(Clone, Debug)]
pub struct Iter<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// The `BitSlice` being iterated.
	inner: &'a BitSlice<C, T>,
}

impl<'a, C, T> Iter<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// Accesses the `BitPtr` representation of the slice.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// The `BitPtr` representation of the remaining slice.
	pub(crate) fn bitptr(&self) -> BitPtr<T> {
		self.inner.bitptr()
	}
}

impl<'a, C, T> DoubleEndedIterator for Iter<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// Produces the next bit from the back of the slice.
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// The last bit in the slice, if any.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[1];
	/// let bv: &BitSlice = store.into();
	/// let bv = &bv[6 ..];
	/// let mut iter = bv.iter();
	/// assert!(iter.next_back().unwrap());
	/// assert!(!iter.next_back().unwrap());
	/// assert!(iter.next_back().is_none());
	/// ```
	fn next_back(&mut self) -> Option<Self::Item> {
		if self.inner.is_empty() {
			return None;
		}
		let len = self.inner.len();
		let out = self.inner[len - 1];
		self.inner = &self.inner[.. len - 1];
		Some(out)
	}
}

/// Mark that the iterator has an exact size.
impl<'a, C, T> ExactSizeIterator for Iter<'a, C, T>
where C: Cursor, T: 'a + Bits {}

/// Mark that the iterator will not resume after halting.
impl<'a, C, T> FusedIterator for Iter<'a, C, T>
where C: Cursor, T: 'a + Bits {}

impl<'a, C, T> Iterator for Iter<'a, C, T>
where C: Cursor, T: 'a + Bits {
	type Item = bool;

	/// Advances the iterator by one, returning the first bit in it (if any).
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// The leading bit in the iterator, if any.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x80];
	/// let bv: &BitSlice = store.into();
	/// let bv = &bv[.. 2];
	/// let mut iter = bv.iter();
	/// assert!(iter.next().unwrap());
	/// assert!(!iter.next().unwrap());
	/// assert!(iter.next().is_none());
	/// ```
	fn next(&mut self) -> Option<Self::Item> {
		if self.inner.is_empty() {
			return None;
		}
		let out = self.inner[0];
		self.inner = &self.inner[1 ..];
		Some(out)
	}

	/// Hints at the number of bits remaining in the iterator.
	///
	/// Because the exact size is always known, this always produces
	/// `(len, Some(len))`.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// - `usize`: The minimum bits remaining.
	/// - `Option<usize>`: The maximum bits remaining.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bv: &BitSlice = store.into();
	/// let bv = &bv[.. 2];
	/// let mut iter = bv.iter();
	/// assert_eq!(iter.size_hint(), (2, Some(2)));
	/// iter.next();
	/// assert_eq!(iter.size_hint(), (1, Some(1)));
	/// iter.next();
	/// assert_eq!(iter.size_hint(), (0, Some(0)));
	/// ```
	fn size_hint(&self) -> (usize, Option<usize>) {
		let len = self.inner.len();
		(len, Some(len))
	}

	/// Counts how many bits are live in the iterator, consuming it.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// The number of bits remaining in the iterator.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bv: &BitSlice = store.into();
	/// assert_eq!(bv.iter().count(), 8);
	/// ```
	fn count(self) -> usize {
		self.len()
	}

	/// Advances the iterator by `n` bits, starting from zero.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `n`: The number of bits to skip, before producing the next bit after
	///   skips. If this overshoots the iterator’s remaining length, then the
	///   iterator is marked empty before returning `None`.
	///
	/// # Returns
	///
	/// If `n` does not overshoot the iterator’s bounds, this produces the `n`th
	/// bit after advancing the iterator to it, discarding the intermediate
	/// bits.
	///
	/// If `n` does overshoot the iterator’s bounds, this empties the iterator
	/// and returns `None`.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[2];
	/// let bv: &BitSlice = store.into();
	/// let mut iter = bv.iter();
	/// assert!(iter.nth(6).unwrap());
	/// assert!(!iter.nth(0).unwrap());
	/// assert!(iter.nth(0).is_none());
	/// ```
	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		if n >= self.len() {
			self.inner = BitSlice::empty();
			return None;
		}
		self.inner = &self.inner[n ..];
		self.next()
	}

	/// Consumes the iterator, returning only the final bit.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// The last bit in the iterator slice, if any.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bv: &BitSlice = store.into();
	/// assert!(bv.iter().last().unwrap());
	/// ```
	fn last(mut self) -> Option<Self::Item> {
		self.next_back()
	}
}

/// State keeper for reverse chunked iteration over a `BitSlice`.
///
/// # Type Parameters
///
/// - `C: Cursor`: The bit-order type of the underlying `BitSlice`.
/// - `T: 'a + Bits`: The storage type of the underlying `BitSlice`.
///
/// # Lifetimes
///
/// - `'a`: The lifetime of the underlying `BitSlice`.
#[derive(Clone, Debug)]
pub struct RChunks<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// The `BitSlice` being iterated.
	inner: &'a BitSlice<C, T>,
	/// The width of the chunks.
	width: usize,
}

impl<'a, C, T> DoubleEndedIterator for RChunks<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// Produces the next chunk from the front of the slice.
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// The last chunk in the slice, if any.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[1];
	/// let bv: &BitSlice = store.into();
	/// let mut rchunks = bv.rchunks(5);
	/// assert_eq!(rchunks.next_back(), Some(&bv[.. 3]));
	/// assert_eq!(rchunks.next_back(), Some(&bv[3 ..]));
	/// assert!(rchunks.next_back().is_none());
	/// ```
	fn next_back(&mut self) -> Option<Self::Item> {
		if self.inner.is_empty() {
			return None;
		}
		let len = self.inner.len();
		let rem = len % self.width;
		let size = if rem == 0 { self.width } else { rem };
		let (head, tail) = self.inner.split_at(size);
		self.inner = tail;
		Some(head)
	}
}

/// Mark that the iterator has an exact size.
impl<'a, C, T> ExactSizeIterator for RChunks<'a, C, T>
where C: Cursor, T: 'a + Bits {}

/// Mark that the iterator will not resume after halting.
impl<'a, C, T> FusedIterator for RChunks<'a, C, T>
where C: Cursor, T: 'a + Bits {}

impl<'a, C, T> Iterator for RChunks<'a, C, T>
where C: Cursor, T: 'a + Bits {
	type Item = &'a BitSlice<C, T>;

	/// Advances the iterator by one, returning the first chunk in it (if any).
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// The leading chunk in the iterator, if any.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x80];
	/// let bv: &BitSlice = store.into();
	/// let mut rchunks = bv.rchunks(5);
	/// assert_eq!(rchunks.next(), Some(&bv[3 ..]));
	/// assert_eq!(rchunks.next(), Some(&bv[.. 3]));
	/// assert!(rchunks.next().is_none());
	/// ```
	fn next(&mut self) -> Option<Self::Item> {
		use core::cmp::min;
		if self.inner.is_empty() {
			return None;
		}
		let len = self.inner.len();
		let size = min(len, self.width);
		let (head, tail) = self.inner.split_at(len - size);
		self.inner = head;
		Some(tail)
	}

	/// Hints at the number of chunks remaining in the iterator.
	///
	/// Because the exact size is always known, this always produces
	/// `(len, Some(len))`.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// - `usize`: The minimum chunks remaining.
	/// - `Option<usize>`: The maximum chunks remaining.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bv: &BitSlice = store.into();
	/// let mut rchunks = bv.rchunks(5);
	/// assert_eq!(rchunks.size_hint(), (2, Some(2)));
	/// rchunks.next();
	/// assert_eq!(rchunks.size_hint(), (1, Some(1)));
	/// rchunks.next();
	/// assert_eq!(rchunks.size_hint(), (0, Some(0)));
	/// ```
	fn size_hint(&self) -> (usize, Option<usize>) {
		if self.inner.is_empty() {
			return (0, Some(0));
		}
		let len = self.inner.len();
		let (len, rem) = (len / self.width, len % self.width);
		let len = len + (rem > 0) as usize;
		(len, Some(len))
	}

	/// Counts how many chunks are live in the iterator, consuming it.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// The number of chunks remaining in the iterator.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bv: &BitSlice = store.into();
	/// assert_eq!(bv.rchunks(3).count(), 3);
	/// ```
	fn count(self) -> usize {
		self.len()
	}

	/// Advances the iterator by `n` chunks, starting from zero.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `n`: The number of chunks to skip, before producing the next bit after
	///   skips. If this overshoots the iterator’s remaining length, then the
	///   iterator is marked empty before returning `None`.
	///
	/// # Returns
	///
	/// If `n` does not overshoot the iterator’s bounds, this produces the `n`th
	/// bit after advancing the iterator to it, discarding the intermediate
	/// chunks.
	///
	/// If `n` does overshoot the iterator’s bounds, this empties the iterator
	/// and returns `None`.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[2];
	/// let bv: &BitSlice = store.into();
	/// let mut rchunks = bv.rchunks(3);
	/// assert_eq!(rchunks.nth(2), Some(&bv[0 .. 2]));
	/// assert!(rchunks.nth(0).is_none());
	/// ```
	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		let (end, ovf) = n.overflowing_mul(self.width);
		if end >= self.inner.len() || ovf {
			self.inner = BitSlice::empty();
			return None;
		}
		// Can't underflow because of the check above
		let end = self.inner.len() - end;
		let start = end.checked_sub(self.width).unwrap_or(0);
		let nth = &self.inner[start .. end];
		self.inner = &self.inner[.. start];
		Some(nth)
	}

	/// Consumes the iterator, returning only the final chunk.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// The last chunk in the iterator slice, if any.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bv: &BitSlice = store.into();
	/// assert_eq!(bv.rchunks(3).last(), Some(&bv[.. 2]));
	/// ```
	fn last(mut self) -> Option<Self::Item> {
		self.next_back()
	}
}

/// State keeper for mutable reverse chunked iteration over a `BitSlice`.
///
/// # Type Parameters
///
/// - `C: Cursor`: The bit-order type of the underlying `BitSlice`.
/// - `T: 'a + Bits`: The storage type of the underlying `BitSlice`.
///
/// # Lifetimes
///
/// - `'a`: The lifetime of the underlying `BitSlice`.
#[derive(Debug)]
pub struct RChunksMut<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// The `BitSlice` being iterated.
	inner: &'a mut BitSlice<C, T>,
	/// The width of the chunks.
	width: usize,
}

impl<'a, C, T> DoubleEndedIterator for RChunksMut<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// Produces the next chunk from the front of the slice.
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// The last chunk in the slice, if any.
	fn next_back(&mut self) -> Option<Self::Item> {
		if self.inner.is_empty() {
			return None;
		}
		let rem = self.inner.len() % self.width;
		let size = if rem == 0 { self.width } else { rem };
		let tmp = mem::replace(&mut self.inner, BitSlice::empty_mut());
		let (head, tail) = tmp.split_at_mut(size);
		self.inner = tail;
		Some(head)
	}
}

impl<'a, C, T> ExactSizeIterator for RChunksMut<'a, C, T>
where C: Cursor, T: 'a + Bits {}

impl<'a, C, T> FusedIterator for RChunksMut<'a, C, T>
where C: Cursor, T: 'a + Bits {}

impl<'a, C, T> Iterator for RChunksMut<'a, C, T>
where C: Cursor, T: 'a + Bits {
	type Item = &'a mut BitSlice<C, T>;

	/// Advances the iterator by one, returning the first chunk in it (if any).
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// The leading chunk in the iterator, if any.
	fn next(&mut self) -> Option<Self::Item> {
		use core::cmp::min;
		if self.inner.is_empty() {
			return None;
		}
		let size = min(self.inner.len(), self.width);
		let tmp = mem::replace(&mut self.inner, BitSlice::empty_mut());
		let tlen = tmp.len();
		let (head, tail) = tmp.split_at_mut(tlen - size);
		self.inner = head;
		Some(tail)
	}

	/// Hints at the number of chunks remaining in the iterator.
	///
	/// Because the exact size is always known, this always produces
	/// `(len, Some(len))`.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// - `usize`: The minimum chunks remaining.
	/// - `Option<usize>`: The maximum chunks remaining.
	fn size_hint(&self) -> (usize, Option<usize>) {
		if self.inner.is_empty() {
			return (0, Some(0));
		}
		let len = self.inner.len();
		let (len, rem) = (len / self.width, len % self.width);
		let len = len + (rem > 0) as usize;
		(len, Some(len))
	}

	/// Counts how many chunks are live in the iterator, consuming it.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// The number of chunks remaining in the iterator.
	fn count(self) -> usize {
		self.len()
	}

	/// Advances the iterator by `n` chunks, starting from zero.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `n`: The number of chunks to skip, before producing the next bit after
	///   skips. If this overshoots the iterator’s remaining length, then the
	///   iterator is marked empty before returning `None`.
	///
	/// # Returns
	///
	/// If `n` does not overshoot the iterator’s bounds, this produces the `n`th
	/// bit after advancing the iterator to it, discarding the intermediate
	/// chunks.
	///
	/// If `n` does overshoot the iterator’s bounds, this empties the iterator
	/// and returns `None`.
	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		let (end, ovf) = n.overflowing_mul(self.width);
		if end >= self.inner.len() || ovf {
			self.inner = BitSlice::empty_mut();
			return None;
		}
		// Can't underflow because of the check above
		let end = self.inner.len() - end;
		let start = end.checked_sub(self.width).unwrap_or(0);
		let tmp = mem::replace(&mut self.inner, BitSlice::empty_mut());
		let (head, tail) = tmp.split_at_mut(start);
		let (nth, _) = tail.split_at_mut(end - start);
		self.inner = head;
		Some(nth)
	}

	/// Consumes the iterator, returning only the final chunk.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// The last chunk in the iterator slice, if any.
	fn last(mut self) -> Option<Self::Item> {
		self.next_back()
	}
}

/// State keeper for reverse exact iteration over a `BitSlice`.
///
/// # Type Parameters
///
/// - `C: Cursor`: The bit-order type of the underlying `BitSlice`.
/// - `T: 'a + Bits`: The storage type of the underlying `BitSlice`.
///
/// # Lifetimes
///
/// - `'a`: The lifetime of the underlying `BitSlice`.
#[derive(Clone, Debug)]
pub struct RChunksExact<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// The `BitSlice` being iterated.
	inner: &'a BitSlice<C, T>,
	/// The excess of the original `BitSlice`, which is not iterated.
	extra: &'a BitSlice<C, T>,
	/// The width of the chunks.
	width: usize,
}

impl<'a, C, T> RChunksExact<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// Produces the remainder of the original slice, which will not be included
	/// in the iteration.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// The remaining slice that the iteration will not include.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bits: &BitSlice = store.into();
	/// let rchunks_exact = bits.rchunks_exact(3);
	/// assert_eq!(rchunks_exact.remainder(), &bits[.. 2]);
	/// ```
	pub fn remainder(&self) -> &'a BitSlice<C, T> {
		self.extra
	}
}

impl<'a, C, T> DoubleEndedIterator for RChunksExact<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// Produces the next chunk from the front of the slice.
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// The last chunk in the slice, if any.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[1];
	/// let bv: &BitSlice = store.into();
	/// let mut rchunks_exact = bv.rchunks_exact(3);
	/// assert_eq!(rchunks_exact.next_back(), Some(&bv[2 .. 5]));
	/// assert_eq!(rchunks_exact.next_back(), Some(&bv[5 .. 8]));
	/// assert!(rchunks_exact.next_back().is_none());
	/// ```
	fn next_back(&mut self) -> Option<Self::Item> {
		if self.inner.len() < self.width {
			self.inner = BitSlice::empty();
			return None;
		}
		let (head, tail) = self.inner.split_at(self.width);
		self.inner = tail;
		Some(head)
	}
}

/// Mark that the iterator has an exact size.
impl<'a, C, T> ExactSizeIterator for RChunksExact<'a, C, T>
where C: Cursor, T: 'a + Bits {}

/// Mark that the iterator will not resume after halting.
impl<'a, C, T> FusedIterator for RChunksExact<'a, C, T>
where C: Cursor, T: 'a + Bits {}

impl<'a, C, T> Iterator for RChunksExact<'a, C, T>
where C: Cursor, T: 'a + Bits {
	type Item = &'a BitSlice<C, T>;

	/// Advances the iterator by one, returning the first chunk in it (if any).
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// The leading chunk in the iterator, if any.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x80];
	/// let bv: &BitSlice = store.into();
	/// let mut rchunks_exact = bv.rchunks_exact(3);
	/// assert_eq!(rchunks_exact.next(), Some(&bv[5 .. 8]));
	/// assert_eq!(rchunks_exact.next(), Some(&bv[2 .. 5]));
	/// assert!(rchunks_exact.next().is_none());
	/// ```
	fn next(&mut self) -> Option<Self::Item> {
		if self.inner.len() < self.width {
			self.inner = BitSlice::empty();
			return None;
		}
		let (head, tail) = self.inner.split_at(self.inner.len() - self.width);
		self.inner = head;
		Some(tail)
	}

	/// Hints at the number of chunks remaining in the iterator.
	///
	/// Because the exact size is always known, this always produces
	/// `(len, Some(len))`.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// - `usize`: The minimum chunks remaining.
	/// - `Option<usize>`: The maximum chunks remaining.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bv: &BitSlice = store.into();
	/// let mut rchunks_exact = bv.rchunks_exact(3);
	/// assert_eq!(rchunks_exact.size_hint(), (2, Some(2)));
	/// rchunks_exact.next();
	/// assert_eq!(rchunks_exact.size_hint(), (1, Some(1)));
	/// rchunks_exact.next();
	/// assert_eq!(rchunks_exact.size_hint(), (0, Some(0)));
	/// ```
	fn size_hint(&self) -> (usize, Option<usize>) {
		let n = self.inner.len() / self.width;
		(n, Some(n))
	}

	/// Counts how many chunks are live in the iterator, consuming it.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// The number of chunks remaining in the iterator.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bv: &BitSlice = store.into();
	/// assert_eq!(bv.rchunks_exact(3).count(), 2);
	/// ```
	fn count(self) -> usize {
		self.len()
	}

	/// Advances the iterator by `n` chunks, starting from zero.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `n`: The number of chunks to skip, before producing the next bit after
	///   skips. If this overshoots the iterator’s remaining length, then the
	///   iterator is marked empty before returning `None`.
	///
	/// # Returns
	///
	/// If `n` does not overshoot the iterator’s bounds, this produces the `n`th
	/// bit after advancing the iterator to it, discarding the intermediate
	/// chunks.
	///
	/// If `n` does overshoot the iterator’s bounds, this empties the iterator
	/// and returns `None`.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bv: &BitSlice = store.into();
	/// let mut rchunks_exact = bv.rchunks_exact(3);
	/// assert_eq!(rchunks_exact.nth(1), Some(&bv[2 .. 5]));
	/// assert!(rchunks_exact.nth(0).is_none());
	/// ```
	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		let (end, ovf) = n.overflowing_mul(self.width);
		if end >= self.inner.len() || ovf {
			self.inner = BitSlice::empty();
			return None;
		}
		let (head, _) = self.inner.split_at(self.inner.len() - end);
		self.inner = head;
		self.next()
	}

	/// Consumes the iterator, returning only the final bit.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// The last bit in the iterator slice, if any.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bv: &BitSlice = store.into();
	/// assert!(bv.iter().last().unwrap());
	/// ```
	fn last(mut self) -> Option<Self::Item> {
		self.next_back()
	}
}

/// State keeper for mutable reverse exact chunked iteration over a `BitSlice`.
///
/// # Type Parameters
///
/// - `C: Cursor`: The bit-order type of the underlying `BitSlice`.
/// - `T: 'a + Bits`: The storage type of the underlying `BitSlice`.
///
/// # Lifetimes
///
/// - `'a`: The lifetime of the underlying `BitSlice`.
#[derive(Debug)]
pub struct RChunksExactMut<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// The `BitSlice` being iterated.
	inner: &'a mut BitSlice<C, T>,
	/// The excess of the original `BitSlice`, which is not iterated.
	extra: &'a mut BitSlice<C, T>,
	/// The width of the chunks.
	width: usize,
}

impl<'a, C, T> RChunksExactMut<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// Produces the remainder of the original slice, which will not be included
	/// in the iteration.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// The remaining slice that iteration will not include.
	pub fn into_remainder(self) -> &'a mut BitSlice<C, T> {
		self.extra
	}
}

impl<'a, C, T> DoubleEndedIterator for RChunksExactMut<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// Produces the next chunk from the front of the slice.
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// The last chunk in the slice, if any.
	fn next_back(&mut self) -> Option<Self::Item> {
		if self.inner.len() < self.width {
			self.inner = BitSlice::empty_mut();
			return None;
		}
		let tmp = mem::replace(&mut self.inner, BitSlice::empty_mut());
		let (head, tail) = tmp.split_at_mut(self.width);;
		self.inner = tail;
		Some(head)
	}
}

impl<'a, C, T> ExactSizeIterator for RChunksExactMut<'a, C, T>
where C: Cursor, T: 'a + Bits {}

impl<'a, C, T> FusedIterator for RChunksExactMut<'a, C, T>
where C: Cursor, T: 'a + Bits {}

impl<'a, C, T> Iterator for RChunksExactMut<'a, C, T>
where C: Cursor, T: 'a + Bits {
	type Item = &'a mut BitSlice<C, T>;

	/// Advances the iterator by one, returning the first chunk in it (if any).
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// The leading chunk in the iterator, if any.
	fn next(&mut self) -> Option<Self::Item> {
		if self.inner.len() < self.width {
			self.inner = BitSlice::empty_mut();
			return None;
		}
		let tmp = mem::replace(&mut self.inner, BitSlice::empty_mut());
		let tlen = tmp.len();
		let (head, tail) = tmp.split_at_mut(tlen - self.width);
		self.inner = head;
		Some(tail)
	}

	/// Hints at the number of chunks remaining in the iterator.
	///
	/// Because the exact size is always known, this always produces
	/// `(len, Some(len))`.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// - `usize`: The minimum chunks remaining.
	/// - `Option<usize>`: The maximum chunks remaining.
	fn size_hint(&self) -> (usize, Option<usize>) {
		let n = self.inner.len() / self.width;
		(n, Some(n))
	}

	/// Counts how many chunks are live in the iterator, consuming it.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// The number of chunks remaining in the iterator.
	fn count(self) -> usize {
		self.len()
	}

	/// Advances the iterator by `n` chunks, starting from zero.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `n`: The number of chunks to skip, before producing the next bit after
	///   skips. If this overshoots the iterator’s remaining length, then the
	///   iterator is marked empty before returning `None`.
	///
	/// # Returns
	///
	/// If `n` does not overshoot the iterator’s bounds, this produces the `n`th
	/// bit after advancing the iterator to it, discarding the intermediate
	/// chunks.
	///
	/// If `n` does overshoot the iterator’s bounds, this empties the iterator
	/// and returns `None`.
	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		let (end, ovf) = n.overflowing_mul(self.width);
		if end >= self.inner.len() || ovf {
			self.inner = BitSlice::empty_mut();
			return None;
		}
		let tmp = mem::replace(&mut self.inner, BitSlice::empty_mut());
		let tlen = tmp.len();
		let (head, _) = tmp.split_at_mut(tlen - end);
		self.inner = head;
		self.next()
	}

	/// Consumes the iterator, returning only the final bit.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// The last bit in the iterator slice, if any.
	fn last(mut self) -> Option<Self::Item> {
		self.next_back()
	}
}

/// State keeper for sliding-window iteration over a `BitSlice`.
///
/// # Type Parameters
///
/// - `C: Cursor`: The bit-order type of the underlying `BitSlice`.
/// - `T: 'a + Bits`: The storage type of the underlying `BitSlice`.
///
/// # Lifetimes
///
/// - `'a`: The lifetime of the underlying `BitSlice`.
#[derive(Clone, Debug)]
pub struct Windows<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// The `BitSlice` being iterated.
	inner: &'a BitSlice<C, T>,
	/// The width of the windows.
	width: usize,
}

impl<'a, C, T> DoubleEndedIterator for Windows<'a, C, T>
where C: Cursor, T: 'a + Bits {
	/// Produces the next window from the back of the slice.
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// The last window in the slice, if any.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0b0010_1101];
	/// let bv: &BitSlice = store.into();
	/// let mut windows = bv[2 .. 7].windows(3);
	/// assert_eq!(windows.next_back(), Some(&bv[4 .. 7]));
	/// assert_eq!(windows.next_back(), Some(&bv[3 .. 6]));
	/// assert_eq!(windows.next_back(), Some(&bv[2 .. 5]));
	/// assert!(windows.next_back().is_none());
	/// ```
	fn next_back(&mut self) -> Option<Self::Item> {
		if self.inner.is_empty() || self.width > self.inner.len() {
			self.inner = BitSlice::empty();
			return None;
		}
		let len = self.inner.len();
		let out = &self.inner[len - self.width ..];
		self.inner = &self.inner[.. len - 1];
		Some(out)
	}
}

/// Mark that the iterator has an exact size.
impl<'a, C, T> ExactSizeIterator for Windows<'a, C, T>
where C: Cursor, T: 'a + Bits {}

/// Mark that the iterator will not resume after halting.
impl<'a, C, T> FusedIterator for Windows<'a, C, T>
where C: Cursor, T: 'a + Bits {}

impl<'a, C, T> Iterator for Windows<'a, C, T>
where C: Cursor, T: 'a + Bits {
	type Item = &'a BitSlice<C, T>;

	/// Advances the iterator by one, returning the first window in it (if any).
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Returns
	///
	/// The leading window in the iterator, if any.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x80];
	/// let bv: &BitSlice = store.into();
	/// let bv = &bv[.. 2];
	/// let mut iter = bv.iter();
	/// assert!(iter.next().unwrap());
	/// assert!(!iter.next().unwrap());
	/// assert!(iter.next().is_none());
	/// ```
	fn next(&mut self) -> Option<Self::Item> {
		if self.width > self.inner.len() {
			self.inner = BitSlice::empty();
			None
		}
		else {
			let out = &self.inner[.. self.width];
			self.inner = &self.inner[1 ..];
			Some(out)
		}
	}

	/// Hints at the number of windows remaining in the iterator.
	///
	/// Because the exact size is always known, this always produces
	/// `(len, Some(len))`.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// - `usize`: The minimum windows remaining.
	/// - `Option<usize>`: The maximum windows remaining.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bv: &BitSlice = store.into();
	/// let bv = &bv[.. 2];
	/// let mut iter = bv.iter();
	/// assert_eq!(iter.size_hint(), (2, Some(2)));
	/// iter.next();
	/// assert_eq!(iter.size_hint(), (1, Some(1)));
	/// iter.next();
	/// assert_eq!(iter.size_hint(), (0, Some(0)));
	/// ```
	fn size_hint(&self) -> (usize, Option<usize>) {
		let len = self.inner.len();
		if self.width > len {
			(0, Some(0))
		}
		else {
			let len = len - self.width + 1;
			(len, Some(len))
		}
	}

	/// Counts how many windows are live in the iterator, consuming it.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// The number of windows remaining in the iterator.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bv: &BitSlice = store.into();
	/// assert_eq!(bv.iter().count(), 8);
	/// ```
	fn count(self) -> usize {
		self.len()
	}

	/// Advances the iterator by `n` windows, starting from zero.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `n`: The number of windows to skip, before producing the next bit after
	///   skips. If this overshoots the iterator’s remaining length, then the
	///   iterator is marked empty before returning `None`.
	///
	/// # Returns
	///
	/// If `n` does not overshoot the iterator’s bounds, this produces the `n`th
	/// bit after advancing the iterator to it, discarding the intermediate
	/// windows.
	///
	/// If `n` does overshoot the iterator’s bounds, this empties the iterator
	/// and returns `None`.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[2];
	/// let bv: &BitSlice = store.into();
	/// let mut iter = bv.iter();
	/// assert!(iter.nth(6).unwrap());
	/// assert!(!iter.nth(0).unwrap());
	/// assert!(iter.nth(0).is_none());
	/// ```
	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		let (end, ovf) = n.overflowing_add(self.width);
		if end > self.inner.len() || ovf {
			self.inner = BitSlice::empty();
			return None;
		}
		let out = &self.inner[n .. end];
		self.inner = &self.inner[n + 1 ..];
		Some(out)
	}

	/// Consumes the iterator, returning only the final window.
	///
	/// # Parameters
	///
	/// - `self`
	///
	/// # Returns
	///
	/// The last window in the iterator slice, if any.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::*;
	///
	/// let store: &[u8] = &[0x4B];
	/// let bv: &BitSlice = store.into();
	/// assert_eq!(bv.windows(3).last(), Some(&bv[5 ..]));
	/// ```
	fn last(mut self) -> Option<Self::Item> {
		self.next_back()
	}
}
