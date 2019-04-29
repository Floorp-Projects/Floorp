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

/// Traverses an element from `MSbit` to `LSbit`.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct BigEndian;

/// Traverses an element from `LSbit` to `MSbit`.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct LittleEndian;

/** A cursor over an element.

# Usage

`bitvec` structures store and operate on semantic counts, not bit positions. The
`Cursor::at` function takes a semantic cursor, `BitIdx`, and produces an
electrical position, `BitPos`.
**/
pub trait Cursor {
	/// Name of the cursor type, for use in text display.
	const TYPENAME: &'static str;

	/// Translate a semantic bit index into an electrical bit position.
	///
	/// # Parameters
	///
	/// - `cursor`: The semantic bit value.
	///
	/// # Returns
	///
	/// - A concrete position. This value can be used for shifting and masking
	///   to extract a bit from an element. This must be in the domain
	///   `0 .. T::SIZE`.
	///
	/// # Type Parameters
	///
	/// - `T: Bits`: The storage type for which the position will be calculated.
	///
	/// # Invariants
	///
	/// The function **must** be *total* for the domain `.. T::SIZE`. All values
	/// in this domain are valid indices that the library will pass to it, and
	/// which this function must satisfy.
	///
	/// The function **must** be *bijective* over the domain `.. T::SIZE`. All
	/// input values in this domain must have one and only one correpsonding
	/// output, which must also be in this domain.
	///
	/// The function *may* support input in the domain `T::SIZE ..`. The library
	/// will not produce any values in this domain as input indices. The
	/// function **must not** produce output in the domain `T::SIZE ..`. It must
	/// choose between panicking, or producing an output in `.. T::SIZE`. The
	/// reduction in domain from `T::SIZE ..` to `.. T::SIZE` removes the
	/// requirement for inputs in `T::SIZE ..` to have unique outputs in
	/// `.. T::SIZE`.
	///
	/// This function **must** be *pure*. Calls which have the same input must
	/// produce the same output. This invariant is only required to be upheld
	/// for the lifetime of all data structures which use an implementor. The
	/// behavior of the function *may* be modified after all existing dependent
	/// data structures are destroyed and before any new dependent data
	/// structures are created.
	///
	/// # Non-Invariants
	///
	/// This function is *not* required to be stateless. It *may* refer to
	/// immutable global state, subject to the purity requirement on lifetimes.
	///
	/// # Safety
	///
	/// This function requires that the output be in the domain `.. T::SIZE`.
	/// Implementors must uphold this themselves. Outputs in the domain
	/// `T::SIZE ..` will induce panics elsewhere in the library.
	fn at<T: Bits>(cursor: BitIdx) -> BitPos;
}

impl Cursor for BigEndian {
	const TYPENAME: &'static str = "BigEndian";

	/// Maps a semantic count to a concrete position.
	///
	/// `BigEndian` order moves from `MSbit` first to `LSbit` last.
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
	/// `LittleEndian` order moves from `LSbit` first to `MSbit` last.
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
