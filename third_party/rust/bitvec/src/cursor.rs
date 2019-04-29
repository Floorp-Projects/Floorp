/*! Bit Cursors

`bitvec` structures are parametric over any ordering of bits within an element.
The `Cursor` trait maps a cursor position (indicated by the `BitIdx` type) to an
electrical position (indicated by the `BitPos` type) within that element, and
also defines the order of traversal over an element.

The only requirement on implementors of `Cursor` is that the transform function
from cursor (`BitIdx`) to position (`BitPos`) is *total* (every integer in the
domain `0 .. T::SIZE` is used) and *unique* (each cursor maps to one and only
one position, and each position is mapped by one and only one cursor).
Contiguity is not required.

`Cursor` is a stateless trait, and implementors should be zero-sized types.
!*/

use super::bits::{
	BitIdx,
	BitPos,
	Bits,
};

/// Traverses an element from `MSb` to `LSb`.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct BigEndian;

/// Traverses an element from `LSb` to `MSb`.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct LittleEndian;

/** A cursor over an element.

# Usage

`bitvec` structures store and operate on semantic counts, not bit positions. The
`Cursor::at` function takes a semantic cursor, `BitIdx`, and produces an
electrical position, `BitPos`.
**/
pub trait Cursor {
	const TYPENAME: &'static str;

	/// Translate a semantic bit index into an electrical bit position.
	///
	/// # Parameters
	///
	/// - `cursor`: The semantic bit value. This must be in the domain
	///   `0 .. T::SIZE`.
	///
	/// # Returns
	///
	/// - A concrete position. This value can be used for shifting and masking
	///   to extract a bit from an element.
	///
	/// # Type Parameters
	///
	/// - `T: Bits`: The storage type for which the position will be calculated.
	///
	/// # Safety
	///
	/// This function requires that `cursor` be in the domain `0 .. T::SIZE`.
	/// Implementors must check this themselves.
	fn at<T: Bits>(cursor: BitIdx) -> BitPos;
}

impl Cursor for BigEndian {
	const TYPENAME: &'static str = "BigEndian";

	/// Maps a semantic count to a concrete position.
	///
	/// `BigEndian` order moves from `MSb` first to `LSb` last.
	fn at<T: Bits>(cursor: BitIdx) -> BitPos {
		assert!(
			*cursor < T::SIZE,
			"Index out of range: {} overflows {}",
			*cursor,
			T::SIZE,
		);
		(T::MASK - *cursor).into()
	}
}

impl Cursor for LittleEndian {
	const TYPENAME: &'static str = "LittleEndian";

	/// Maps a semantic count to a concrete position.
	///
	/// `LittleEndian` order moves from `LSb` first to `LSb` last.
	fn at<T: Bits>(cursor: BitIdx) -> BitPos {
		assert!(
			*cursor < T::SIZE,
			"Index out of range: {} overflows {}",
			*cursor,
			T::SIZE,
		);
		(*cursor).into()
	}
}
