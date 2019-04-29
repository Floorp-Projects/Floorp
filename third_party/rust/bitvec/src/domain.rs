/*! Data Model for Bit Sequence Domains

The domains governed by `BitSlice` and owned-variant handles have different
representative states depending on the span of governed elements and live bits.

This module provides representations of the domain states for ease of use by
handle operations.
!*/

use crate::{
	bits::{
		BitIdx,
		Bits,
	},
	pointer::BitPtr,
};

/// Variant markers for the kinds of domains.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum BitDomainKind {
	/// Zero elements
	Empty,
	/// Single element, partial on both edges
	Minor,
	/// Multiple elements, partial on both edges
	Major,
	/// Partial on head only
	PartialHead,
	/// Partial on tail only
	PartialTail,
	/// Fully spans elements
	Spanning,
}

impl BitDomainKind {
	/// Tests if the variant is `Empty`.
	pub fn is_empty(self)        -> bool { self == BitDomainKind::Empty       }
	/// Tests if the variant is `Minor`.
	pub fn is_minor(self)        -> bool { self == BitDomainKind::Minor       }
	/// Tests if the variant is `Major`.
	pub fn is_major(self)        -> bool { self == BitDomainKind::Major       }
	/// Tests if the variant is `PartialHead`.
	pub fn is_partial_head(self) -> bool { self == BitDomainKind::PartialHead }
	/// Tests if the variant is `PartialTail`.
	pub fn is_partial_tail(self) -> bool { self == BitDomainKind::PartialTail }
	/// Tests if the variant is `Spanning`.
	pub fn is_spanning(self)     -> bool { self == BitDomainKind::Spanning    }
}

impl<T> From<BitPtr<T>> for BitDomainKind
where T: Bits {
	fn from(bitptr: BitPtr<T>) -> Self {
		let (e, h, t) = bitptr.region_data();
		let w = T::SIZE;

		match (e, *h, *t) {
			//  Empty
			(0, _, _)             => BitDomainKind::Empty,
			//  Reaches both edges, for any number of elements
			(_, 0, tc) if tc == w => BitDomainKind::Spanning,
			//  Reaches only the tail edge, for any number of elements
			(_, _, tc) if tc == w => BitDomainKind::PartialHead,
			//  Reaches only the head edge, for any number of elements
			(_, 0, _ )            => BitDomainKind::PartialTail,
			//  Reaches neither edge, for one element
			(1, _, _)             => BitDomainKind::Minor,
			//  Reaches neither edge, for multiple elements
			(_, _, _ )            => BitDomainKind::Major,
		}
	}
}

/// Representations of the state of the bit domain in its containing elements.
///
/// # Lifetimes
///
/// - `'a`: Lifetime of the containing storage
///
/// # Type Parameters
///
/// - `T: Bits` The type of the elements the domain inhabits.
#[derive(Clone, Debug)]
pub enum BitDomain<'a, T>
where T: 'a + Bits {
	/// Empty domain.
	Empty,
	/// Single element domain which does not reach either edge.
	///
	/// # Members
	///
	/// - `.0`: index of the first live domain bit in the element
	/// - `.1`: reference to the element contatining the domain
	/// - `.2`: index of the first dead bit after the domain
	///
	/// # Invariants
	///
	/// - `.0` must satisfy `BitIdx::is_valid::<T>`
	/// - `.1` must satisfy `BitIdx::is_valid_tail::<T>`
	///
	/// # Behavior
	///
	/// This variant is produced when the domain is contained entirely inside
	/// one element, and does not reach to either edge.
	Minor(BitIdx, &'a T, BitIdx),
	/// Multpile element domain which does not reach the edge of its edge
	/// elements.
	///
	/// # Members
	///
	/// - `.0`: index of the first live domain bit in the first element
	/// - `.1`: reference to the partial head edge element
	/// - `.2`: slice handle to the fully-live elements in the interior. This
	///   may be empty.
	/// - `.3`: reference to the partial tail edge element
	/// - `.4`: index of the first dead bit after the domain
	///
	/// # Behavior
	///
	/// This variant is produced when the domain uses at least two elements, and
	/// reaches neither the head edge of the head element nor the tail edge of
	/// the tail element.
	Major(BitIdx, &'a T, &'a [T], &'a T, BitIdx),
	/// Domain with a partial head cursor and fully extended tail cursor.
	///
	/// # Members
	///
	/// - `.0`: index of the first live bit in the head element
	/// - `.1`: reference to the partial head element
	/// - `.2`: reference to the full elements of the domain. This may be empty.
	///
	/// # Behavior
	///
	/// This variant is produced when the domain’s head cursor is past `0`, and
	/// its tail cursor is exactly `T::SIZE`.
	PartialHead(BitIdx, &'a T, &'a [T]),
	/// Domain with a fully extended head cursor and partial tail cursor.
	///
	/// # Members
	///
	/// - `.0`: reference to the full elements of the domain. This may be empty.
	/// - `.1`: reference to the partial tail element
	/// - `.2`: index of the first dead bit after the live bits in the tail
	///
	/// # Behavior
	///
	/// This variant is produced when the domain’s head cursor is exactly `0`,
	/// and its tail cursor is less than `T::SIZE`.
	PartialTail(&'a [T], &'a T, BitIdx),
	/// Domain which fully spans its containing elements.
	///
	/// # Members
	///
	/// - `.0`: slice handle to the elements containing the domain
	///
	/// # Behavior
	///
	/// This variant is produced when the all elements in the domain are fully
	/// populated.
	Spanning(&'a [T]),
}

impl<'a, T> From<BitPtr<T>> for BitDomain<'a, T>
where T: 'a + Bits {
	fn from(bitptr: BitPtr<T>) -> Self {
		use BitDomainKind as Bdk;
		let (e, h, t) = bitptr.region_data();
		let data = bitptr.as_slice();

		match bitptr.domain_kind() {
			Bdk::Empty => BitDomain::Empty,
			Bdk::Minor => BitDomain::Minor(h, &data[0], t),
			Bdk::Major => BitDomain::Major(
				h, &data[0], &data[1 .. e - 1], &data[e - 1], t
			),
			Bdk::PartialHead => BitDomain::PartialHead(
				h, &data[0], &data[1 ..]
			),
			Bdk::PartialTail => BitDomain::PartialTail(
				&data[.. e - 1], &data[e - 1], t
			),
			Bdk::Spanning => BitDomain::Spanning(data),
		}
	}
}

/// Representations of the state of the bit domain in its containing elements.
///
/// # Lifetimes
///
/// - `'a`: Lifetime of the containing storage
///
/// # Type Parameters
///
/// - `T: Bits` The type of the elements the domain inhabits.
#[derive(Debug)]
pub enum BitDomainMut<'a, T>
where T: 'a + Bits {
	/// Empty domain.
	Empty,
	/// Single element domain which does not reach either edge.
	///
	/// # Members
	///
	/// - `.0`: index of the first live domain bit in the element
	/// - `.1`: mutable reference to the element contatining the domain
	/// - `.2`: index of the first dead bit after the domain
	///
	/// # Invariants
	///
	/// - `.0` must satisfy `BitIdx::is_valid::<T>`
	/// - `.1` must satisfy `BitIdx::is_valid_tail::<T>`
	///
	/// # Behavior
	///
	/// This variant is produced when the domain is contained entirely inside
	/// one element, and does not reach to either edge.
	Minor(BitIdx, &'a mut T, BitIdx),
	/// Multpile element domain which does not reach the edge of its edge
	/// elements.
	///
	/// # Members
	///
	/// - `.0`: index of the first live domain bit in the first element
	/// - `.1`: mutable reference to the partial head edge element
	/// - `.2`: mutable slice handle to the fully-live elements in the interior.
	///   This may be empty.
	/// - `.3`: mutable reference to the partial tail edge element
	/// - `.4`: index of the first dead bit after the domain
	///
	/// # Behavior
	///
	/// This variant is produced when the domain uses at least two elements, and
	/// reaches neither the head edge of the head element nor the tail edge of
	/// the tail element.
	Major(BitIdx, &'a mut T, &'a mut [T], &'a mut T, BitIdx),
	/// Domain with a partial head cursor and fully extended tail cursor.
	///
	/// # Members
	///
	/// - `.0`: index of the first live bit in the head element
	/// - `.1`: mutable reference to the partial head element
	/// - `.2`: mutable reference to the full elements of the domain. This may
	///   be empty.
	///
	/// # Behavior
	///
	/// This variant is produced when the domain’s head cursor is past `0`, and
	/// its tail cursor is exactly `T::SIZE`.
	PartialHead(BitIdx, &'a mut T, &'a mut [T]),
	/// Domain with a fully extended head cursor and partial tail cursor.
	///
	/// # Members
	///
	/// - `.0`: mutable reference to the full elements of the domain. This may
	///   be empty.
	/// - `.1`: mutable reference to the partial tail element
	/// - `.2`: index of the first dead bit after the live bits in the tail
	///
	/// # Behavior
	///
	/// This variant is produced when the domain’s head cursor is exactly `0`,
	/// and its tail cursor is less than `T::SIZE`.
	PartialTail(&'a mut [T], &'a mut T, BitIdx),
	/// Domain which fully spans its containing elements.
	///
	/// # Members
	///
	/// - `.0`: mutable slice handle to the elements containing the domain
	///
	/// # Behavior
	///
	/// This variant is produced when the all elements in the domain are fully
	/// populated.
	Spanning(&'a mut [T]),
}

impl<'a, T> From<BitPtr<T>> for BitDomainMut<'a, T>
where T: 'a + Bits {
	fn from(bitptr: BitPtr<T>) -> Self {
		use BitDomainKind as Bdk;
		let (h, t) = bitptr.cursors();
		let data = bitptr.as_mut_slice();

		match bitptr.domain_kind() {
			Bdk::Empty => BitDomainMut::Empty,
			Bdk::Minor => BitDomainMut::Minor(h, &mut data[0], t),
			Bdk::Major => {
				let (head, body) = data.split_first_mut()
					.expect("Major cannot fail to split head");
				let (tail, body) = body.split_last_mut()
					.expect("Major cannot fail to split tail");
				BitDomainMut::Major(h, head, body, tail, t)
			},
			Bdk::PartialHead => {
				let (head, tail) = data.split_first_mut()
					.expect("PartialHead cannot fail to split");
				BitDomainMut::PartialHead(h, head, tail)
			},
			Bdk::PartialTail => {
				let (tail, head) = data.split_last_mut()
					.expect("PartialTail cannot fail to split");
				BitDomainMut::PartialTail(head, tail, t)
			},
			Bdk::Spanning => BitDomainMut::Spanning(data),
		}
	}
}

#[cfg(all(test, feature = "testing"))]
mod tests {
	use super::*;

	#[test]
	fn minor() {
		let data: u8 = 0u8;
		let bp = BitPtr::new(&data, 1, 1, 6);

		assert!(bp.domain_kind().is_minor());
	}

	#[test]
	fn major() {
		let data: &[u16] = &[0u16, !0u16];
		let bp = BitPtr::new(&data[0], 2, 1, 12);

		assert!(bp.domain_kind().is_major());
	}

	#[test]
	fn partial_head() {
		let data: u32 = 0u32;
		let bp = BitPtr::new(&data, 1, 4, 32);

		assert!(bp.domain_kind().is_partial_head());

		let data: &[u32] = &[0u32, !0u32];
		let bp = BitPtr::new(&data[0], 2, 4, 32);

		assert!(bp.domain_kind().is_partial_head());
	}

	#[test]
	fn partial_tail() {
		let data: u64 = 0u64;
		let bp = BitPtr::new(&data, 1, 0, 60);

		assert!(bp.domain_kind().is_partial_tail());

		let data: &[u64] = &[0u64, !0u64];
		let bp = BitPtr::new(&data[0], 2, 0, 60);

		assert!(bp.domain_kind().is_partial_tail());
	}

	#[test]
	fn spanning() {
		let data: u8 = 0u8;
		let bp = BitPtr::new(&data, 1, 0, 8);

		assert!(bp.domain_kind().is_spanning());

		let data: &[u16] = &[0u16, !0u16];
		let bp = BitPtr::new(&data[0], 2, 0, 16);

		assert!(bp.domain_kind().is_spanning());
	}
}
