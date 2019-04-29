/*! Bit Management

The `Bits` trait defines constants and free functions suitable for managing bit
storage of a fundamental, and is the constraint for the storage type of the data
structures of the rest of the crate.
!*/

use crate::Cursor;
use core::{
	cmp::Eq,
	convert::From,
	fmt::{
		self,
		Binary,
		Debug,
		Display,
		Formatter,
		LowerHex,
		UpperHex,
	},
	mem::size_of,
	ops::{
		BitAnd,
		BitAndAssign,
		BitOrAssign,
		Deref,
		DerefMut,
		Not,
		Shl,
		ShlAssign,
		Shr,
		ShrAssign,
	},
};

/** Generalizes over the fundamental types for use in `bitvec` data structures.

This trait must only be implemented on unsigned integer primitives with full
alignment. It cannot be implemented on `u128` on any architecture, or on `u64`
on 32-bit systems.

The `Sealed` supertrait ensures that this can only be implemented locally, and
will never be implemented by downstream crates on new types.
**/
pub trait Bits:
	//  Forbid external implementation
	Sealed
	+ Binary
	//  Element-wise binary manipulation
	+ BitAnd<Self, Output=Self>
	+ BitAndAssign<Self>
	+ BitOrAssign<Self>
	//  Permit indexing into a generic array
	+ Copy
	+ Debug
	+ Display
	//  Permit testing a value against 1 in `get()`.
	+ Eq
	//  Rust treats numeric literals in code as vaguely typed and does not make
	//  them concrete until long after trait expansion, so this enables building
	//  a concrete Self value from a numeric literal.
	+ From<u8>
	//  Permit extending into a `u64`.
	+ Into<u64>
	+ LowerHex
	+ Not<Output=Self>
	+ Shl<u8, Output=Self>
	+ ShlAssign<u8>
	+ Shr<u8, Output=Self>
	+ ShrAssign<u8>
	//  Allow direct access to a concrete implementor type.
	+ Sized
	+ UpperHex
{
	/// The size, in bits, of this type.
	const SIZE: u8 = size_of::<Self>() as u8 * 8;

	/// The number of bits required to index the type. This is always
	/// log<sub>2</sub> of the type’s bit size.
	///
	/// Incidentally, this can be computed as `Self::SIZE.trailing_zeros()` once
	/// that becomes a valid constexpr.
	const BITS: u8; // = Self::SIZE.trailing_zeros() as u8;

	/// The bitmask to turn an arbitrary `usize` into a bit index. Bit indices
	/// are always stored in the lowest bits of an index value.
	const MASK: u8 = Self::SIZE - 1;

	/// Name of the implementing type.
	const TYPENAME: &'static str;

	/// Sets a specific bit in an element to a given value.
	///
	/// # Parameters
	///
	/// - `place`: A bit index in the element, from `0` at `LSb` to `Self::MASK`
	///   at `MSb`. The bit under this index will be set according to `value`.
	/// - `value`: A Boolean value, which sets the bit on `true` and unsets it
	///   on `false`.
	///
	/// # Type Parameters
	///
	/// - `C: Cursor`: A `Cursor` implementation to translate the index into a
	///   position.
	///
	/// # Panics
	///
	/// This function panics if `place` is not less than `T::SIZE`, in order
	/// to avoid index out of range errors.
	///
	/// # Examples
	///
	/// This example sets and unsets bits in a byte.
	///
	/// ```rust
	/// use bitvec::{Bits, LittleEndian};
	/// let mut elt: u8 = 0;
	/// elt.set::<LittleEndian>(0.into(), true);
	/// assert_eq!(elt, 0b0000_0001);
	/// elt.set::<LittleEndian>(4.into(), true);
	/// assert_eq!(elt, 0b0001_0001);
	/// elt.set::<LittleEndian>(0.into(), false);
	/// assert_eq!(elt, 0b0001_0000);
	/// ```
	///
	/// This example overruns the index, and panics.
	///
	/// ```rust,should_panic
	/// use bitvec::{Bits, LittleEndian};
	/// let mut elt: u8 = 0;
	/// elt.set::<LittleEndian>(8.into(), true);
	/// ```
	fn set<C: Cursor>(&mut self, place: BitIdx, value: bool) {
		let place: BitPos = C::at::<Self>(place);
		assert!(
			*place < Self::SIZE,
			"Index out of range: {} overflows {}",
			*place,
			Self::SIZE,
		);
		//  Blank the selected bit
		*self &= !(Self::from(1u8) << *place);
		//  Set the selected bit
		*self |= Self::from(value as u8) << *place;
	}

	/// Gets a specific bit in an element.
	///
	/// # Parameters
	///
	/// - `place`: A bit index in the element, from `0` at `LSb` to `Self::MASK`
	///   at `MSb`. The bit under this index will be retrieved as a `bool`.
	///
	/// # Returns
	///
	/// The value of the bit under `place`, as a `bool`.
	///
	/// # Type Parameters
	///
	/// - `C: Cursor`: A `Cursor` implementation to translate the index into a
	///   position.
	///
	/// # Panics
	///
	/// This function panics if `place` is not less than `T::SIZE`, in order
	/// to avoid index out of range errors.
	///
	/// # Examples
	///
	/// This example gets two bits from a byte.
	///
	/// ```rust
	/// use bitvec::{Bits, LittleEndian};
	/// let elt: u8 = 0b0000_0100;
	/// assert!(!elt.get::<LittleEndian>(1.into()));
	/// assert!(elt.get::<LittleEndian>(2.into()));
	/// assert!(!elt.get::<LittleEndian>(3.into()));
	/// ```
	///
	/// This example overruns the index, and panics.
	///
	/// ```rust,should_panic
	/// use bitvec::{Bits, LittleEndian};
	/// 0u8.get::<LittleEndian>(8.into());
	/// ```
	fn get<C: Cursor>(&self, place: BitIdx) -> bool {
		let place: BitPos = C::at::<Self>(place);
		assert!(
			*place < Self::SIZE,
			"Index out of range: {} overflows {}",
			*place,
			Self::SIZE,
		);
		//  Shift down so the targeted bit is in LSb, then blank all other bits.
		(*self >> *place) & Self::from(1) == Self::from(1)
	}

	/// Counts how many bits in `self` are set to `1`.
	///
	/// This zero-extends `self` to `u64`, and uses the [`u64::count_ones`]
	/// inherent method.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// The number of bits in `self` set to `1`. This is a `usize` instead of a
	/// `u32` in order to ease arithmetic throughout the crate.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::Bits;
	/// assert_eq!(Bits::count_ones(&0u8), 0);
	/// assert_eq!(Bits::count_ones(&128u8), 1);
	/// assert_eq!(Bits::count_ones(&192u8), 2);
	/// assert_eq!(Bits::count_ones(&224u8), 3);
	/// assert_eq!(Bits::count_ones(&240u8), 4);
	/// assert_eq!(Bits::count_ones(&248u8), 5);
	/// assert_eq!(Bits::count_ones(&252u8), 6);
	/// assert_eq!(Bits::count_ones(&254u8), 7);
	/// assert_eq!(Bits::count_ones(&255u8), 8);
	/// ```
	///
	/// [`u64::count_ones`]: https://doc.rust-lang.org/stable/std/primitive.u64.html#method.count_ones
	#[inline(always)]
	fn count_ones(&self) -> usize {
		u64::count_ones((*self).into()) as usize
	}

	/// Counts how many bits in `self` are set to `0`.
	///
	/// This inverts `self`, so all `0` bits are `1` and all `1` bits are `0`,
	/// then zero-extends `self` to `u64` and uses the [`u64::count_ones`]
	/// inherent method.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// The number of bits in `self` set to `0`. This is a `usize` instead of a
	/// `u32` in order to ease arithmetic throughout the crate.
	///
	/// # Examples
	///
	/// ```rust
	/// use bitvec::Bits;
	/// assert_eq!(Bits::count_zeros(&0u8), 8);
	/// assert_eq!(Bits::count_zeros(&1u8), 7);
	/// assert_eq!(Bits::count_zeros(&3u8), 6);
	/// assert_eq!(Bits::count_zeros(&7u8), 5);
	/// assert_eq!(Bits::count_zeros(&15u8), 4);
	/// assert_eq!(Bits::count_zeros(&31u8), 3);
	/// assert_eq!(Bits::count_zeros(&63u8), 2);
	/// assert_eq!(Bits::count_zeros(&127u8), 1);
	/// assert_eq!(Bits::count_zeros(&255u8), 0);
	/// ```
	///
	/// [`u64::count_ones`]: https://doc.rust-lang.org/stable/std/primitive.u64.html#method.count_ones
	#[inline(always)]
	fn count_zeros(&self) -> usize {
		u64::count_ones((!*self).into()) as usize
	}
}

/** Newtype indicating a semantic index into an element.

This type is consumed by [`Cursor`] implementors, which use it to produce a
concrete bit position inside an element.

`BitIdx` is a semantic counter which has a defined, constant, and predictable
ordering. Values of `BitIdx` refer strictly to abstract ordering, and not to the
actual position in an element, so `BitIdx(0)` is the first bit in an element,
but is not required to be the electrical `LSb`, `MSb`, or any other.

[`Cursor`]: ../trait.Cursor.html
**/
#[derive(Clone, Copy, Debug, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct BitIdx(pub(crate) u8);

impl BitIdx {
	/// Checks if the index is valid for a type.
	///
	/// # Parameters
	///
	/// - `self`: The index to validate.
	///
	/// # Returns
	///
	/// Whether the index is valid for the storage type in question.
	///
	/// # Type Parameters
	///
	/// - `T: Bits`: The storage type used to determine index validity.
	pub fn is_valid<T: Bits>(self) -> bool {
		*self < T::SIZE
	}

	/// Increments a cursor to the next value, wrapping if needed.
	///
	/// # Parameters
	///
	/// - `self`: The original cursor.
	///
	/// # Returns
	///
	/// - `Self`: An incremented cursor.
	/// - `bool`: Marks whether the increment crossed an element boundary.
	///
	/// # Type Parameters
	///
	/// - `T: Bits`: The storage type for which the increment will be
	///   calculated.
	///
	/// # Panics
	///
	/// This method panics if `self` is not less than `T::SIZE`, in order to
	/// avoid index out of range errors.
	///
	/// # Examples
	///
	/// This example increments inside an element.
	///
	/// ```rust
	/// # #[cfg(feature = "testing")] {
	/// use bitvec::testing::BitIdx;
	/// assert_eq!(BitIdx::from(3).incr::<u8>(), (4.into(), false));
	/// # }
	/// ```
	///
	/// This example increments at the high edge, and wraps to the next element.
	///
	/// ```rust
	/// # #[cfg(feature = "testing")] {
	/// use bitvec::testing::BitIdx;
	/// assert_eq!(BitIdx::from(7).incr::<u8>(), (0.into(), true));
	/// # }
	/// ```
	pub fn incr<T: Bits>(self) -> (Self, bool) {
		assert!(
			*self < T::SIZE,
			"Index out of range: {} overflows {}",
			*self,
			T::SIZE,
		);
		let next = (*self).wrapping_add(1) & T::MASK;
		(next.into(), next == 0)
	}

	/// Decrements a cursor to the previous value, wrapping if needed.
	///
	/// # Parameters
	///
	/// - `self`: The original cursor.
	///
	/// # Returns
	///
	/// - `Self`: A decremented cursor.
	/// - `bool`: Marks whether the decrement crossed an element boundary.
	///
	/// # Type Parameters
	///
	/// - `T: Bits`: The storage type for which the decrement will be
	///   calculated.
	///
	/// # Panics
	///
	/// This method panics if `self` is not less than `T::SIZE`, in order to
	/// avoid index out of range errors.
	///
	/// # Examples
	///
	/// This example decrements inside an element.
	///
	/// ```rust
	/// # #[cfg(feature = "testing")] {
	/// use bitvec::testing::BitIdx;
	/// assert_eq!(BitIdx::from(5).decr::<u8>(), (4.into(), false));
	/// # }
	/// ```
	///
	/// This example decrements at the low edge, and wraps to the previous
	/// element.
	///
	/// ```rust
	/// # #[cfg(feature = "testing")] {
	/// use bitvec::testing::BitIdx;
	/// assert_eq!(BitIdx::from(0).decr::<u8>(), (7.into(), true));
	/// # }
	pub fn decr<T: Bits>(self) -> (Self, bool) {
		assert!(
			*self < T::SIZE,
			"Index out of range: {} overflows {}",
			*self,
			T::SIZE,
		);
		let (prev, wrap) = (*self).overflowing_sub(1);
		((prev & T::MASK).into(), wrap)
	}

	/// Finds the destination bit a certain distance away from a starting bit.
	///
	/// This produces the number of elements to move, and then the bit index of
	/// the destination bit in the destination element.
	///
	/// # Parameters
	///
	/// - `self`: The bit index in an element of the starting position. This
	///   must be in the domain `0 .. T::SIZE`.
	/// - `by`: The number of bits by which to move. Negative values move
	///   downwards in memory: towards `LSb`, then starting again at `MSb` of
	///   the prior element in memory (decreasing address). Positive values move
	///   upwards in memory: towards `MSb`, then starting again at `LSb` of the
	///   subsequent element in memory (increasing address).
	///
	/// # Returns
	///
	/// - `isize`: The number of elements by which to change the caller’s
	///   element cursor. This value can be passed directly into [`ptr::offset`]
	/// - `BitIdx`: The bit index of the destination bit in the newly selected
	///   element. This will always be in the domain `0 .. T::SIZE`. This
	///   value can be passed directly into [`Cursor`] functions to compute the
	///   correct place in the element.
	///
	/// # Type Parameters
	///
	/// - `T: Bits`: The storage type with which the offset will be calculated.
	///
	/// # Panics
	///
	/// This function panics if `from` is not less than `T::SIZE`, in order
	/// to avoid index out of range errors.
	///
	/// # Safety
	///
	/// `by` must not be large enough to cause the returned `isize` value to,
	/// when applied to [`ptr::offset`], produce a reference out of bounds of
	/// the original allocation. This method has no means of checking this
	/// requirement.
	///
	/// # Examples
	///
	/// This example calculates offsets within the same element.
	///
	/// ```rust
	/// # #[cfg(feature = "testing")] {
	/// use bitvec::testing::BitIdx;
	/// assert_eq!(BitIdx::from(1).offset::<u32>(4isize), (0, 5.into()));
	/// assert_eq!(BitIdx::from(6).offset::<u32>(-3isize), (0, 3.into()));
	/// # }
	/// ```
	///
	/// This example calculates offsets that cross into other elements. It uses
	/// `u32`, so the bit index domain is `0 ..= 31`.
	///
	/// `7 - 18`, modulo 32, wraps down from 0 to 31 and continues decreasing.
	/// `23 + 68`, modulo 32, wraps up from 31 to 0 and continues increasing.
	///
	/// ```rust
	/// # #[cfg(feature = "testing")] {
	/// use bitvec::testing::BitIdx;
	/// assert_eq!(BitIdx::from(7).offset::<u32>(-18isize), (-1, 21.into()));
	/// assert_eq!(BitIdx::from(23).offset::<u32>(68isize), (2, 27.into()));
	/// # }
	/// ```
	///
	/// [`Cursor`]: ../trait.Cursor.html
	/// [`ptr::offset`]: https://doc.rust-lang.org/stable/std/primitive.pointer.html#method.offset
	pub fn offset<T: Bits>(self, by: isize) -> (isize, Self) {
		assert!(
			*self < T::SIZE,
			"Index out of range: {} overflows {}",
			*self,
			T::SIZE,
		);
		//  If the `isize` addition does not overflow, then the sum can be used
		//  directly.
		if let (far, false) = by.overflowing_add(*self as isize) {
			//  If `far` is in the domain `0 .. T::SIZE`, then the offset did
			//  not depart the element.
			if far >= 0 && far < T::SIZE as isize {
				(0, (far as u8).into())
			}
			//  If `far` is negative, then the offset leaves the initial element
			//  going down. If `far` is not less than `T::SIZE`, then the
			//  offset leaves the initial element going up.
			else {
				//  `Shr` on `isize` sign-extends
				(
					far >> T::BITS,
					((far & (T::MASK as isize)) as u8).into(),
				)
			}
		}
		//  If the `isize` addition overflows, then the `by` offset is positive.
		//  Add as `usize` and use that. This is guaranteed not to overflow,
		//  because `isize -> usize` doubles the domain, but `self` is limited
		//  to `0 .. T::SIZE`.
		else {
			let far = *self as usize + by as usize;
			//  This addition will always result in a `usize` whose lowest
			//  `T::BITS` bits are the bit index in the destination element,
			//  and the rest of the high bits (shifted down) are the number of
			//  elements by which to advance.
			(
				(far >> T::BITS) as isize,
				((far & (T::MASK as usize)) as u8).into(),
			)
		}
	}

	/// Computes the size of a span from `self` for `len` bits.
	///
	/// # Parameters
	///
	/// - `self`
	/// - `len`: The number of bits to include in the span.
	///
	/// # Returns
	///
	/// - `usize`: The number of elements `T` included in the span. This will
	///   be in the domain `1 .. usize::max_value()`.
	/// - `BitIdx`: The index of the first bit *after* the span. This will be in
	///   the domain `1 ..= T::SIZE`.
	///
	/// # Type Parameters
	///
	/// - `T: Bits`: The type of the elements for which this span is computed.
	///
	/// # Examples
	///
	/// ```rust
	/// # #[cfg(feature = "testing")] {
	/// use bitvec::testing::{BitIdx, Bits};
	///
	/// let h: BitIdx = 0.into();
	/// assert_eq!(BitIdx::from(0).span::<u8>(8), (1, 8.into()))
	/// # }
	/// ```
	pub fn span<T: Bits>(self, len: usize) -> (usize, BitIdx) {
		//  Number of bits in the head *element*. Domain 32 .. 0.
		let bits_in_head = (T::SIZE - *self) as usize;
		//  If there are n bits live between the head cursor (which marks the
		//  address of the first live bit) and the back edge of the element,
		//  then when len is <= n, the span covers one element.
		//  When len == n, the tail will be T::SIZE, which is valid for a tail.
		//  TODO(myrrlyn): Separate BitIdx into Head and Tail types, which have
		//  their proper range enforcements.
		if len <= bits_in_head {
			(1, (*self + len as u8).into())
		}
		//  If there are more bits in the span than n, then subtract n from len
		//  and use the difference to count elements and bits.
		else {
			//  1 ..
			let bits_after_head = len - bits_in_head;
			//  Count the number of wholly filled elements
			let whole_elts = bits_after_head >> T::BITS;
			//  Count the number of bits in the *next* element. If this is zero,
			//  become T::SIZE; if it is nonzero, add one more to elts. elts
			//  must have one added to it by default to account for the head
			//  element.
			let tail_bits = bits_after_head as u8 & T::MASK;
			if tail_bits == 0 {
				(whole_elts + 1, T::SIZE.into())
			}
			else {
				(whole_elts + 2, tail_bits.into())
			}
		}
	}
}

/// Wraps a `u8` as a `BitIdx`.
impl From<u8> for BitIdx {
	fn from(src: u8) -> Self {
		BitIdx(src)
	}
}

/// Unwraps a `BitIdx` to a `u8`.
impl Into<u8> for BitIdx {
	fn into(self) -> u8 {
		self.0
	}
}

impl Display for BitIdx {
	fn fmt(&self, f: &mut Formatter) -> fmt::Result {
		write!(f, "BitIdx({})", self.0)
	}
}

impl Deref for BitIdx {
	type Target = u8;
	fn deref(&self) -> &Self::Target { &self.0 } }

impl DerefMut for BitIdx {
	fn deref_mut(&mut self) -> &mut Self::Target { &mut self.0 }
}

/** Newtype indicating a concrete index into an element.

This type is produced by [`Cursor`] implementors, and denotes a concrete bit in
an element rather than a semantic bit.

`Cursor` implementors translate `BitIdx` values, which are semantic places, into
`BitPos` values, which are concrete electrical positions.

[`Cursor`]: ../trait.Cursor.html
**/
#[derive(Clone, Copy, Debug, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct BitPos(u8);

impl BitPos {
	/// Checks if the position is valid for a type.
	///
	/// # Parameters
	///
	/// - `self`: The position to validate.
	///
	/// # Returns
	///
	/// Whether the position is valid for the storage type in question.
	///
	/// # Type Parameters
	///
	/// - `T: Bits`: The storage type used to determine position validity.
	pub fn is_valid<T: Bits>(self) -> bool {
		*self < T::SIZE
	}
}

/// Wraps a `u8` as a `BitPos`.
impl From<u8> for BitPos {
	fn from(src: u8) -> Self {
		BitPos(src)
	}
}

/// Unwraps a `BitPos` to a `u8`.
impl Into<u8> for BitPos {
	fn into(self) -> u8 {
		self.0
	}
}

impl Display for BitPos {
	fn fmt(&self, f: &mut Formatter) -> fmt::Result {
		write!(f, "BitPos({})", self.0)
	}
}

impl Deref for BitPos {
	type Target = u8;
	fn deref(&self) -> &Self::Target { &self.0 }
}

impl DerefMut for BitPos {
	fn deref_mut(&mut self) -> &mut Self::Target { &mut self.0 }
}

impl Bits for u8  { const BITS: u8 = 3; const TYPENAME: &'static str = "u8";  }
impl Bits for u16 { const BITS: u8 = 4; const TYPENAME: &'static str = "u16"; }
impl Bits for u32 { const BITS: u8 = 5; const TYPENAME: &'static str = "u32"; }

#[cfg(target_pointer_width = "64")]
impl Bits for u64 { const BITS: u8 = 6; const TYPENAME: &'static str = "u64"; }

/// Marker trait to seal `Bits` against downstream implementation.
///
/// This trait is public in the module, so that other modules in the crate can
/// use it, but so long as it is not exported by the crate root and this module
/// is private, this trait effectively forbids downstream implementation of the
/// `Bits` trait.
#[doc(hidden)]
pub trait Sealed {}

impl Sealed for u8 {}
impl Sealed for u16 {}
impl Sealed for u32 {}

#[cfg(target_pointer_width = "64")]
impl Sealed for u64 {}

#[cfg(test)]
mod tests {
	use super::*;

	#[test]
	fn jump_far_up() {
		//  isize::max_value() is 0x7f...ff, so the result bit will be one less
		//  than the start bit.
		for n in 1 .. 8 {
			let (elt, bit) = BitIdx::from(n).offset::<u8>(isize::max_value());
			assert_eq!(elt, (isize::max_value() >> u8::BITS) + 1);
			assert_eq!(*bit, n - 1);
		}
		let (elt, bit) = BitIdx::from(0).offset::<u8>(isize::max_value());
		assert_eq!(elt, isize::max_value() >> u8::BITS);
		assert_eq!(*bit, 7);
	}

	#[test]
	fn jump_far_down() {
		//  isize::min_value() is 0x80...00, so the result bit will be equal to
		//  the start bit
		for n in 0 .. 8 {
			let (elt, bit) = BitIdx::from(n).offset::<u8>(isize::min_value());
			assert_eq!(elt, isize::min_value() >> u8::BITS);
			assert_eq!(*bit, n);
		}
	}

	#[test]
	#[should_panic]
	fn offset_out_of_bound() {
		BitIdx::from(64).offset::<u64>(isize::max_value());
	}
}
