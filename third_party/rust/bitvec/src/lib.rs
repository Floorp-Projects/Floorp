/*! `bitvec` – `[bool]` in overdrive.

This crate provides views into slices of bits that are truly `[u1]`. Each bit in
the data segment is used, unlike `[bool]` which ignores seven bits out of every
byte.

`bitvec`’s data structures provide strong guarantees about, and fine-grained
control of, the bit-level representation of a sequence of memory. The user is
empowered to choose the fundamental type underlying the store – `u8`, `u16`,
`u32`, or `u64` – and the order in which each primitive is traversed –
big-endian, from the most significant bit to the least, or little-endian, from
the least significant bit to the most.

This level of control is not necessary for most use cases where users just want
to put bits in a sequence, but it is critically important for users making
packets that leave main memory and hit some external device like a peripheral
controller or a network socket. In order to provide convencienc to users for
whom the storage details do not matter, `bitvec` types default to using
big-endian bit order on `u8`. This means that the bits you would write down on
paper match up with the bits as they are stored in memory.

For example, the bit sequence `[0, 1, 1, 0, 1, 0, 0, 1]` inserted into `bitvec`
structures with no extra type specification will produce the `<BigEndian, u8>`
variant, so the bits in memory are `0b01101001`. With little-endian bit order,
the memory value would be `0b10010110` (reversed order!).

In addition to providing compact, efficient, and powerful storage and
manipulation of bits in memory, the `bitvec` structures are capable of acting as
a queue, set, or stream of bits. They implement the bit-wise operators for
Boolean arithmetic, arithmetic operators for 2’s-complement numeric arithmetic,
read indexing, bit shifts, and access to the underlying storage fundamental
elements as a slice.

(Write indexing is impossible in Rust semantics.)
!*/

#![cfg_attr(not(feature = "std"), no_std)]
#![cfg_attr(all(feature = "alloc", not(feature = "std")), feature(alloc))]

#[cfg(all(feature = "alloc", not(feature = "std")))]
extern crate alloc;

#[cfg(feature = "std")]
extern crate core;

#[macro_use]
mod macros;

mod bits;
mod cursor;
mod pointer;
mod slice;

#[cfg(feature = "alloc")]
mod boxed;

#[cfg(feature = "alloc")]
mod vec;

use crate::{
	bits::BitIdx,
	pointer::BitPtr,
};

pub use crate::{
	bits::Bits,
	cursor::{
		Cursor,
		BigEndian,
		LittleEndian,
	},
	slice::BitSlice,
};

#[cfg(feature = "alloc")]
pub use crate::{
	boxed::BitBox,
	vec::BitVec,
};

/// Expose crate internals for use in doctests and external tests.
#[cfg(feature = "testing")]
pub mod testing {
	pub use crate::{
		bits::*,
		macros::*,
		pointer::*,
		slice::*,
		vec::*,
	};
}
