/*! Raw Pointer Representation

This module defines the binary representation of the handle to a `BitSlice`
region. This structure is crate-internal, and defines the methods required to
store a `BitSlice` pointer in memory and retrieve values from it suitable for
work.
!*/

use crate::{
	BitIdx,
	BitSlice,
	Bits,
	Cursor,
};
use core::{
	convert::{
		AsMut,
		AsRef,
		From,
	},
	default::Default,
	fmt::{
		self,
		Debug,
		Formatter,
	},
	marker::PhantomData,
	mem,
	ptr::NonNull,
	slice,
};


/// Width in bits of a pointer on the target machine.
const PTR_BITS: usize = mem::size_of::<*const u8>() * 8;

/// Width in bits of a processor word on the target machine.
const USZ_BITS: usize = mem::size_of::<usize>() * 8;

/** In-memory representation of `&BitSlice` handles.

# Layout

This structure is a more complex version of the `*const T`/`usize` tuple that
Rust uses to represent slices throughout the language. It breaks the pointer and
counter fundamentals into sub-field components. Rust does not have bitfield
syntax, so the below description of the element layout is in C++.

```cpp
template<typename T>
struct BitPtr<T> {
  size_t ptr_head : __builtin_ctzll(alignof(T));
  size_t ptr_data : sizeof(T*) * 8
                  - __builtin_ctzll(alignof(T));

  size_t len_head : 3;
  size_t len_tail : 3
                  + __builtin_ctzll(alignof(T));
  size_t len_elts : sizeof(size_t) * 8
                  - 6
                  - __builtin_ctzll(alignof(T));
};
```

This means that the `BitPtr<T>` structure has four *logical* fields, stored in
five segments across the two *structural* fields of the type. The widths and
placements of each segment are functions of the size of `*const T` and `usize`,
and the alignment of `T`.

# Fields

This section describes the purpose, meaning, and layout of the four logical
fields.

## Data Pointer

Aligned pointers to `T` always have low bits available for use to refine the
address of a `T` to the address of a `u8`. It is stored in the high bits of the
`ptr` field, running from MSb down to (inclusive)
`core::mem::align_of::<T>().trailing_zeros()`.

## Element Counter

The memory representation stores counters that run from
`1 ... (Self::MAX_ELTS)`, where the bit pattern is `n - 1` when `n` is the true
number of elements in the slice’s domain. It is stored in the high bits of the
`len` field, running from `MSb` down to (inclusive)
`core::mem::align_of::<T>().trailing_zeros() + 6`.

## Head Bit Counter

For any fundamental type `T`, `core::mem::align_of::<T>().trailing_zeros() + 3`
bits are required to count the bit positions inside it.

|Type |Alignment|Trailing Zeros|Count Bits|
|:----|--------:|-------------:|---------:|
|`u8` |        1|             0|         3|
|`u16`|        2|             1|         4|
|`u32`|        4|             2|         5|
|`u64`|        8|             3|         6|

The head bit counter is split such that its bottom three bits are stored in the
low bits of the `len` field and the remaining high bits are stored in the low
bits of `ptr`.

The counter is a value in the range `0 .. (1 << Count)` that serves as a cursor
into the zeroth storage element to find the first live bit.

## Tail Bit Counter

This counter is the same bit width as the head bit counter. It is stored
contiguously in the middle section of the `len` field, running from (exclusive)
`core::mem::align_of::<T>().trailing_zeros() + 6` down to (inclusive) `3`. The
value in it is a cursor to the next bit *after* the last live bit of the slice.

The tail bit counter and the element counter operate together; when the tail bit
counter is `0`, then the element counter is also incremented to cover the next
element *after* the last live element in the slice domain.

# Edge Cases

The following value sets are edge cases of valid `BitPtr` structures.

## Empty Slice

The empty slice is canonically represented by a wholly zeroed slot:

- `data`: `core::ptr::null::<T>()`
- `elts`: `0usize`
- `head`: `0u8`
- `tail`: `0u8`
- `ptr`: `core::ptr::null::<u8>()`
- `len`: `0usize`

All `BitPtr` values whose `data` pointer is `null` represents the empty slice,
regardless of other field contents, but the normalized form zeros all other
fields also.

## Allocated, Uninhabited, Slice

An allocated, owned, region of memory that is uninhabited. This is functionally
the empty slice, but it must retain its pointer information. All other fields in
the slot are zeroed.

- `data`: (any valid `*const T`)
- `elts`: `0usize`
- `head`: `0u8`
- `tail`: `0u8`
- `ptr`: (any valid `*const u8`)
- `len`: `0usize`

## Maximum Elements, Maximum Tail

This, unfortunately, cannot be represented. The largest domain that can be
represented has `elts` and `tail` of `!0`, which leaves the last bit in the
element unavailable.

# Type Parameters

- `T: Bits` is the storage type over which the pointer governs.

# Safety

A `BitPtr` must never be constructed such that the element addressed by
`self.pointer().offset(self.elements())` causes an addition overflow. This will
be checked in `new()`.

A `BitPtr` must never be constructed such that the tail bit is lower in memory
than the head bit. This will be checked in `new()`.

# Undefined Behavior

Using values of this type directly as pointers or counters will result in
undefined behavior. The pointer value will be invalid for the type, and both the
pointer and length values will be invalid for the memory model and allocation
regime.
**/
#[repr(C)]
#[derive(Clone, Copy, Eq, Hash, PartialEq, PartialOrd, Ord)]
pub struct BitPtr<T>
where T: Bits {
	_ty: PhantomData<T>,
	/// Pointer to the first storage element of the slice.
	///
	/// This will always be a pointer to one byte, regardless of the storage
	/// type of the `BitSlice` or the type parameter of `Self`. It is a
	/// combination of a correctly typed and aligned pointer to `T`, and the
	/// index of a byte within that element.
	///
	/// It is not necessarily the address of the byte with the first live bit.
	/// The location of the first live bit within the first element is governed
	/// by the [`Cursor`] type of the `BitSlice` using this structure.
	///
	/// [`Cursor`]: ../trait.Cursor.html
	ptr: NonNull<u8>,
	/// Three-element bitfield structure, holding length and place information.
	///
	/// This stores the element count in its highest bits, the tail [`BitIdx`]
	/// cursor in the middle segment, and the low three bits of the head
	/// `BitIdx` in the lowest three bits.
	///
	/// [`BitIdx`]: ../struct.BitIdx.html
	len: usize,
}

impl<T> BitPtr<T>
where T: Bits {
	/// The number of high bits in `self.ptr` that are actually the address of
	/// the zeroth `T`.
	pub const PTR_DATA_BITS: usize = PTR_BITS - Self::PTR_HEAD_BITS;
	/// Marks the bits of `self.ptr` that are the `data` section.
	pub const PTR_DATA_MASK: usize = !0 & !Self::PTR_HEAD_MASK;

	/// The number of low bits in `self.ptr` that are the high bits of the head
	/// `BitIdx` cursor.
	pub const PTR_HEAD_BITS: usize = T::BITS as usize - Self::LEN_HEAD_BITS;
	/// Marks the bits of `self.ptr` that are the `head` section.
	pub const PTR_HEAD_MASK: usize = T::MASK as usize >> Self::LEN_HEAD_BITS;

	/// The number of low bits in `self.len` that are the low bits of the head
	/// `BitIdx` cursor.
	///
	/// This is always `3`, until Rust tries to target a machine whose bytes are
	/// not eight bits wide.
	pub const LEN_HEAD_BITS: usize = 3;
	/// Marks the bits of `self.len` that are the `head` section.
	pub const LEN_HEAD_MASK: usize = 7;

	/// The number of middle bits in `self.len` that are the tail `BitIdx`
	/// cursor.
	pub const LEN_TAIL_BITS: usize = T::BITS as usize;
	/// Marks the bits of `self.len` that are the `tail` section.
	pub const LEN_TAIL_MASK: usize = (T::MASK as usize) << Self::LEN_HEAD_BITS;

	/// The number of high bits in `self.len` that are used to count `T`
	/// elements in the slice.
	pub const LEN_DATA_BITS: usize = USZ_BITS - Self::LEN_INDX_BITS;
	/// Marks the bits of `self.len` that are the `data` section.
	pub const LEN_DATA_MASK: usize = !0 & !Self::LEN_INDX_MASK;

	/// The number of bits occupied by the `tail` `BitIdx` and the low 3 bits of
	/// `head`.
	pub const LEN_INDX_BITS: usize = Self::LEN_TAIL_BITS + Self::LEN_HEAD_BITS;
	/// Marks the bits of `self.len` that are either `tail` or `head`.
	pub const LEN_INDX_MASK: usize = Self::LEN_TAIL_MASK | Self::LEN_HEAD_MASK;

	/// The maximum number of elements that can be stored in a `BitPtr` domain.
	pub const MAX_ELTS: usize = 1 << Self::LEN_DATA_BITS;

	/// The maximum number of bits that can be stored in a `BitPtr` domain.
	pub const MAX_BITS: usize = !0 >> Self::LEN_HEAD_BITS;

	/// Produces an empty-slice representation.
	///
	/// This has no live bits, and has a dangling pointer. It is useful as a
	/// default value (and is the function used by `Default`) to indicate
	/// arbitrary null slices.
	///
	/// # Returns
	///
	/// An uninhabited, uninhabitable, empty slice.
	///
	/// # Safety
	///
	/// The `BitPtr` returned by this function must never be dereferenced.
	pub fn empty() -> Self {
		Self {
			_ty: PhantomData,
			ptr: NonNull::dangling(),
			len: 0,
		}
	}

	/// Produces an uninhabited slice from a bare pointer.
	///
	/// # Parameters
	///
	/// - `ptr`: A pointer to `T`.
	///
	/// # Returns
	///
	/// If `ptr` is null, then this returns the empty slice; otherwise, the
	/// returned slice is uninhabited and points to the given address.
	///
	/// # Panics
	///
	/// This function panics if the given pointer is not well aligned to its
	/// type.
	///
	/// # Safety
	///
	/// The provided pointer must be either null, or valid in the caller’s
	/// memory model and allocation regime.
	pub fn uninhabited(ptr: *const T) -> Self {
		//  Check that the pointer is properly aligned for the storage type.
		//  Null pointers are always well aligned.
		assert!(
			(ptr as usize).trailing_zeros() as usize >= Self::PTR_HEAD_BITS,
			"BitPtr domain pointers must be well aligned",
		);
		Self {
			_ty: PhantomData,
			ptr: NonNull::new(ptr as *mut u8).unwrap_or_else(NonNull::dangling),
			len: 0,
		}
	}

	/// Creates a new `BitPtr` from its components.
	///
	/// # Parameters
	///
	/// - `data`: A well-aligned pointer to a storage element. If this is null,
	///   then the empty-slice representation is returned, regardless of other
	///   parameter values.
	/// - `elts`: A number of storage elements in the domain of the new
	///   `BitPtr`. This number must be in `0 .. Self::MAX_ELTS`.
	/// - `head`: The bit index of the first live bit in the domain. This must
	///   be in the domain `0 .. T::SIZE`.
	/// - `tail`: The bit index of the first dead bit after the domain. This
	///   must be:
	///   - equal to `head` when `elts` is `1`, to create an empty slice.
	///   - in `head + 1 ..= T::SIZE` when `elts` is `1` to create a
	///     single-element slice.
	///   - in `1 ..= T::SIZE` when `elts` is greater than `1`.
	///   - in `1 .. T::SIZE` when `elts` is `Self::MAX_ELTS - 1`.
	///
	/// # Returns
	///
	/// If `data` is null, then the empty slice is returned.
	///
	/// If either of the following conditions are true, then the uninhabited
	/// slice is returned:
	///
	/// - `elts` is `0`,
	/// - `elts` is `1` **and** `head` is equal to `tail`.
	///
	/// Otherwise, a `BitPtr` structure representing the given domain is
	/// returned.
	///
	/// # Type Parameters
	///
	/// - `Head: Into<BitIdx>`: A type which can be used as a `BitIdx`.
	/// - `Tail: Into<BitIdx>`: A type which can be used as a `BitIdx`.
	///
	/// # Panics
	///
	/// This function happily panics at the slightest whiff of impropriety.
	///
	/// - If the `data` pointer is not aligned to at least the type `T`,
	/// - If the `elts` counter is not within the countable elements domain,
	///   `0 .. Self::MAX_ELTS`,
	/// - If the `data` pointer is so high in the address space that addressing
	///   the last element would cause the pointer to wrap,
	/// - If `head` or `tail` are too large for indexing bits within `T`,
	/// - If `tail` is not correctly placed relative to `head`.
	///
	/// # Safety
	///
	/// The `data` pointer and `elts` counter must describe a correctly aligned,
	/// validly allocated, region of memory. The caller is responsible for
	/// ensuring that the slice of memory that the new `BitPtr` will govern is
	/// all governable.
	pub fn new<Head: Into<BitIdx>, Tail: Into<BitIdx>>(
		data: *const T,
		elts: usize,
		head: Head,
		tail: Tail,
	) -> Self {
		let (head, tail) = (head.into(), tail.into());
		//  null pointers, and pointers to empty regions, are run through the
		//  uninhabited constructor instead
		if data.is_null() || elts == 0 || (elts == 1 && head == tail) {
			return Self::uninhabited(data);
		}

		//  Check that the pointer is properly aligned for the storage type.
		assert!(
			(data as usize).trailing_zeros() as usize >= Self::PTR_HEAD_BITS,
			"BitPtr domain pointers must be well aligned",
		);

		//  Check that the slice domain is below the ceiling.
		assert!(
			elts < Self::MAX_ELTS,
			"BitPtr domain regions must have at most {} elements",
			Self::MAX_ELTS - 1,
		);

		//  Check that the pointer is not so high in the address space that the
		//  slice domain wraps.
		if data.wrapping_offset(elts as isize) < data {
			panic!("BitPtr slices MUST NOT wrap around the address space");
		}

		//  Check that the head cursor index is within the storage element.
		assert!(
			head.is_valid::<T>(),
			"BitPtr head cursors must be in the domain 0 .. {}",
			T::SIZE,
		);

		//  Check that the tail cursor index is in the appropriate domain.
		assert!(
			BitIdx::from(*tail - 1).is_valid::<T>(),
			"BitPtr tail cursors must be in the domain 1 ..= {}",
			T::SIZE,
		);

		//  For single-element slices, check that the tail cursor is after the
		//  head cursor (single-element, head == tail, is checked above).
		if elts == 1 {
			assert!(
				tail > head,
				"BitPtr domains with one element must have the tail cursor \
				beyond the head cursor",
			);
		}
		else if elts == Self::MAX_ELTS - 1 {
			assert!(
				tail.is_valid::<T>(),
				"BitPtr domains with maximum elements must have the tail \
				cursor in 1 .. {}",
				T::SIZE,
			);
		}

		//  All invariants satisfied; build the fields
		let ptr_data = data as usize & Self::PTR_DATA_MASK;
		let ptr_head = *head as usize >> Self::LEN_HEAD_BITS;

		let len_elts = elts << Self::LEN_INDX_BITS;
		//  Store tail. Note that this wraps T::SIZE to 0. This must be
		//  reconstructed during retrieval.
		let len_tail
			= ((*tail as usize) << Self::LEN_HEAD_BITS)
			& Self::LEN_TAIL_MASK;
		let len_head = *head as usize & Self::LEN_HEAD_MASK;

		Self {
			_ty: PhantomData,
			ptr: unsafe {
				NonNull::new_unchecked((ptr_data | ptr_head) as *mut u8)
			},
			len: len_elts | len_tail | len_head,
		}
	}

	/// Extracts the pointer to the first storage element.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// The `*const T` address of the first storage element in the slice domain.
	///
	/// # Safety
	///
	/// This pointer must be valid in the user’s memory model and allocation
	/// regime.
	pub fn pointer(&self) -> *const T {
		(self.ptr.as_ptr() as usize & Self::PTR_DATA_MASK) as *const T
	}

	/// Produces the count of all elements in the slice domain.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// The number of `T` elements in the slice domain.
	///
	/// # Safety
	///
	/// This size must be valid in the user’s memory model and allocation
	/// regime.
	pub fn elements(&self) -> usize {
		self.len >> Self::LEN_INDX_BITS
	}

	/// Extracts the element cursor of the head bit.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// A `BitIdx` that is the index of the first live bit in the first element.
	/// This will be in the domain `0 .. T::SIZE`.
	pub fn head(&self) -> BitIdx {
		((((self.ptr.as_ptr() as usize & Self::PTR_HEAD_MASK) << 3)
		| (self.len & Self::LEN_HEAD_MASK)) as u8).into()
	}

	/// Extracts the element cursor of the first dead bit *after* the tail bit.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// A `BitIdx` that is the index of the first dead bit after the last live
	/// bit in the last element. This will be in the domain `1 ..= T::SIZE`.
	pub fn tail(&self) -> BitIdx {
		let bits = (self.len & Self::LEN_TAIL_MASK) >> Self::LEN_HEAD_BITS;
		if bits == 0 { T::SIZE } else { bits as u8 }.into()
	}

	/// Decomposes the pointer into raw components.
	///
	/// The values returned from this can be immediately passed into `::new` in
	/// order to rebuild the pointer.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// - `*const T`: A well aligned pointer to the first element of the slice.
	/// - `usize`: The number of elements in the slice.
	/// - `head`: The index of the first live bit in the first element.
	/// - `tail`: The index of the first dead bit in the last element.
	pub fn raw_parts(&self) -> (*const T, usize, BitIdx, BitIdx) {
		(self.pointer(), self.elements(), self.head(), self.tail())
	}

	/// Checks if the pointer represents the empty slice.
	///
	/// The empty slice has a dangling `data` pointer and zeroed `elts`, `head`,
	/// and `tail` elements.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// Whether the slice is empty or inhabited.
	pub fn is_empty(&self) -> bool {
		self.len >> Self::LEN_INDX_BITS == 0
	}

	/// Checks if the pointer represents the full slice.
	///
	/// The full slice is marked by `0` values for `elts` and `tail`, when
	/// `data` is not null. The full slice does not need `head` to be `0`.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// Whether the slice is fully extended or not.
	pub fn is_full(&self) -> bool {
		//  Self must be:
		//  - not empty
		//  - `!0` in `elts` and `tail`
		!self.is_empty()
		&& ((self.len | Self::LEN_HEAD_MASK) == !0)
	}

	///  Counts how many bits are in the domain of a `BitPtr` slice.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// A count of the live bits in the slice.
	pub fn bits(&self) -> usize {
		if self.is_empty() {
			return 0;
		}
		let (_, elts, head, tail) = self.raw_parts();
		if elts == 1 {
			return *tail as usize - *head as usize;
		}
		//  The number of bits in a domain is calculated by decrementing `elts`,
		//  multiplying it by the number of bits per element, then subtracting
		//  `head` (which is the number of dead bits in the front of the first
		//  element), and adding `tail` (which is the number of live bits in the
		//  front of the last element).
		((elts - 1) << T::BITS)
			.saturating_add(*tail as usize)
			.saturating_sub(*head as usize)
	}

	/// Produces the head element, if and only if it is partially live.
	///
	/// If the head element is completely live, this returns `None`, because the
	/// head element is returned in `body_elts()`.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// `Some(&T)` if the slice has at least one element, and the first element
	/// has at least one bit dead.
	///
	/// `None` if the slice is empty, or if the first element is completely
	/// live.
	pub fn head_elt(&self) -> Option<&T> {
		if !self.is_empty() && *self.head() > 0 {
			return Some(&self.as_ref()[0]);
		}
		None
	}

	/// Produces the slice of middle elements that are all fully live.
	///
	/// This may produce the empty slice, if the `BitPtr` slice domain has zero,
	/// one, or two elements, and the outer elements are only partially live.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// A slice of fully live storage elements.
	pub fn body_elts(&self) -> &[T] {
		let w: u8 = 1 << Self::LEN_TAIL_BITS;
		let (_, e, h, t) = self.raw_parts();
		match (e, *h, *t) {
			//  Empty slice
			(0, _, _)           => &             [          ],
			//  Single-element slice, with cursors at the far edges
			(1, 0, t) if t == w => &self.as_ref()[0 .. e - 0],
			//  Single-element slice, with partial cursors
			(1, _, _)           => &             [          ],
			//  Multiple-element slice, with cursors at the far edges
			(_, 0, t) if t == w => &self.as_ref()[0 .. e - 0],
			//  Multiple-element slice, with full head and partial tail
			(_, 0, _)           => &self.as_ref()[0 .. e - 1],
			//  Multiple-element slice, with partial tail and full head
			(_, _, t) if t == w => &self.as_ref()[1 .. e - 0],
			//  Multiple-element slice, with partial cursors
			(_, _, _)           => &self.as_ref()[1 .. e - 1],
		}
	}

	/// Produces the tail element, if and only if it is partially live.
	///
	/// If the tail element is completely live, this returns `None`, because the
	/// tail element is returned in `body_elts()`.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// `Some(&T)` if the slice has at least one element, and the last element
	/// has at least one bit dead.
	///
	/// `None` if the slice is empty, or if the last element is completely live.
	pub fn tail_elt(&self) -> Option<&T> {
		if !self.is_empty() && *self.tail() < T::SIZE {
			return Some(&self.as_ref()[self.elements() - 1]);
		}
		None
	}

	pub fn set_head<Head: Into<BitIdx>>(&mut self, head: Head) {
		if self.is_empty() {
			return;
		}
		let head = head.into();
		assert!(
			head.is_valid::<T>(),
			"Head indices must be in the domain 0 .. {}",
			T::SIZE,
		);
		if self.elements() == 1 {
			assert!(
				head <= self.tail(),
				"Single-element slices must have head below tail",
			);
		}
		self.ptr = unsafe {
			let ptr = self.ptr.as_ptr() as usize;
			NonNull::new_unchecked(
				((ptr & !Self::PTR_HEAD_MASK)
				| ((*head as usize >> Self::LEN_HEAD_BITS) & Self::PTR_HEAD_MASK)
				) as *mut u8
			)
		};
		self.len &= !Self::LEN_HEAD_MASK;
		self.len |= *head as usize & Self::LEN_HEAD_MASK;
	}

	/// Moves the `head` cursor upwards by one.
	///
	/// If `head` is at the back edge of the first element, then it will be set
	/// to the front edge of the second element, and the pointer will be moved
	/// upwards.
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Safety
	///
	/// This method is unsafe when `self` is directly, solely, managing owned
	/// memory. It mutates the pointer and element count, so if this pointer is
	/// solely responsible for owned memory, its conception of the allocation
	/// will differ from the allocator’s.
	pub unsafe fn incr_head(&mut self) {
		let (data, elts, head, tail) = self.raw_parts();
		let (new_head, wrap) = head.incr::<T>();
		if wrap {
			*self = Self::new(data.offset(1), elts - 1, new_head, tail);
		}
		else {
			*self = Self::new(data, elts, new_head, tail);
		}
	}

	/// Moves the `head` cursor downwards by one.
	///
	/// If `head` is at the front edge of the first element, then it will be set
	/// to the back edge of the zeroth element, and the pointer will be moved
	/// downwards.
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Safety
	///
	/// This function is unsafe when `self` is directly, solely, managing owned
	/// memory. It mutates the pointer and element count, so if this pointer is
	/// solely responsible for owned memory, its conception of the allocation
	/// will differ from the allocator’s.
	pub unsafe fn decr_head(&mut self) {
		let (data, elts, head, tail) = self.raw_parts();
		let (new_head, wrap) = head.decr::<T>();
		if wrap {
			*self = Self::new(data.offset(-1), elts + 1, new_head, tail);
		}
		else {
			*self = Self::new(data, elts, new_head, tail);
		}
	}

	pub fn set_tail<Tail: Into<BitIdx>>(&mut self, tail: Tail) {
		if self.is_empty() {
			return;
		}
		let tail = tail.into();
		assert!(
			BitIdx::from(*tail - 1).is_valid::<T>(),
			"Tail indices must be in the domain 1 ..= {}",
			T::SIZE,
		);
		if self.elements() == 1 {
			assert!(
				tail >= self.head(),
				"Single-element slices must have tail above head",
			);
		}
		self.len &= !Self::LEN_TAIL_MASK;
		self.len |= *tail as usize
	}

	/// Moves the `tail` cursor upwards by one.
	///
	/// If `tail` is at the back edge of the last element, then it will be set
	/// to the front edge of the next element beyond, and the element count will
	/// be increased.
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Safety
	///
	/// This function is unsafe when `self` is directly, solely, managing owned
	/// memory. It mutates the element count, so if this pointer is solely
	/// responsible for owned memory, its conception of the allocation will
	/// differ from the allocator’s.
	pub unsafe fn incr_tail(&mut self) {
		let (data, elts, head, tail) = self.raw_parts();
		let decr = BitIdx::from(*tail - 1);
		let (mut new_tail, wrap) = decr.incr::<T>();
		new_tail = BitIdx::from(*new_tail + 1);
		*self = Self::new(data, elts + wrap as usize, head, new_tail);
	}

	/// Moves the `tail` cursor downwards by one.
	///
	/// If `tail` is at the front edge of the back element, then it will be set
	/// to the back edge of the next element forward, and the element count will
	/// be decreased.
	///
	/// # Parameters
	///
	/// - `&mut self`
	///
	/// # Safety
	///
	/// This function is unsafe when `self` is directly, solely, managing owned
	/// memory. It mutates the element count, so if this pointer is solely
	/// responsible for owned memory, its conception of the allocation will
	/// differ from the allocator’s.
	pub unsafe fn decr_tail(&mut self) {
		let (data, elts, head, tail) = self.raw_parts();
		let decr = BitIdx::from(*tail - 1);
		let (mut new_tail, wrap) = decr.decr::<T>();
		new_tail = BitIdx::from(*new_tail + 1);
		*self = Self::new(data, elts - wrap as usize, head, new_tail);
	}
}

/// Gets write access to all elements in the underlying storage, including the
/// partial head and tail elements.
///
/// # Safety
///
/// This is *unsafe* to use except from known mutable `BitSlice` structures.
/// Mutability is not encoded in the `BitPtr` type system at this time, and thus
/// is not enforced by the compiler yet.
impl<T> AsMut<[T]> for BitPtr<T>
where T: Bits {
	fn as_mut(&mut self) -> &mut [T] {
		let ptr = self.pointer() as *mut T;
		let len = self.elements();
		unsafe { slice::from_raw_parts_mut(ptr, len) }
	}
}

/// Gets read access to all elements in the underlying storage, including the
/// partial head and tail elements.
impl<T> AsRef<[T]> for BitPtr<T>
where T: Bits {
	fn as_ref(&self) -> &[T] {
		unsafe { slice::from_raw_parts(self.pointer(), self.elements()) }
	}
}

/// Constructs from an immutable `BitSlice` reference handle.
impl<'a, C, T> From<&'a BitSlice<C, T>> for BitPtr<T>
where C: Cursor, T: 'a + Bits {
	fn from(src: &'a BitSlice<C, T>) -> Self {
		let src: &[()] = unsafe {
			mem::transmute::<&'a BitSlice<C, T>, &[()]>(src)
		};
		let (ptr, len) = match (src.as_ptr() as usize, src.len()) {
			(_, 0) => (NonNull::dangling(), 0),
			(0, _) => unreachable!(
				"Slices cannot have a length when they begin at address 0"
			),
			(p, l) => (unsafe { NonNull::new_unchecked(p as *mut u8) }, l),
		};
		Self { ptr, len, _ty: PhantomData }
	}
}

/// Constructs from a mutable `BitSlice` reference handle.
impl<'a, C, T> From<&'a mut BitSlice<C, T>> for BitPtr<T>
where C: Cursor, T: 'a + Bits {
	fn from(src: &'a mut BitSlice<C, T>) -> Self {
		let src: &[()] = unsafe {
			mem::transmute::<&'a mut BitSlice<C, T>, &[()]>(src)
		};
		let (ptr, len) = match (src.as_ptr() as usize, src.len()) {
			(_, 0) => (NonNull::dangling(), 0),
			(0, _) => unreachable!(
				"Slices cannot have a length when they begin at address 0"
			),
			(p, l) => (unsafe { NonNull::new_unchecked(p as *mut u8) }, l),
		};
		Self { ptr, len, _ty: PhantomData }
	}
}

/// Produces the empty-slice representation.
impl<T> Default for BitPtr<T>
where T: Bits {
	/// Produces an empty-slice representation.
	///
	/// The empty slice has no size or cursors, and its pointer is the alignment
	/// of the type. The non-null pointer allows this structure to be null-value
	/// optimized.
	fn default() -> Self {
		Self::empty()
	}
}

/// Prints the `BitPtr` data structure for debugging.
impl<T> Debug for BitPtr<T>
where T: Bits {
	fn fmt(&self, f: &mut Formatter) -> fmt::Result {
		struct HexPtr<T: Bits>(*const T);
		impl<T: Bits> Debug for HexPtr<T> {
			fn fmt(&self, f: &mut Formatter) -> fmt::Result {
				f.write_fmt(format_args!("0x{:0>1$X}", self.0 as usize, PTR_BITS >> 2))
			}
		}
		struct HexAddr(usize);
		impl Debug for HexAddr {
			fn fmt(&self, f: &mut Formatter) -> fmt::Result {
				f.write_fmt(format_args!("{:#X}", self.0))
			}
		}
		struct BinAddr<T: Bits>(BitIdx, PhantomData<T>);
		impl<T: Bits>  Debug for BinAddr<T> {
			fn fmt(&self, f: &mut Formatter) -> fmt::Result {
				f.write_fmt(format_args!("0b{:0>1$b}", *self.0, T::BITS as usize))
			}
		}
		write!(f, "BitPtr<{}>", T::TYPENAME)?;
		f.debug_struct("")
			.field("data", &HexPtr::<T>(self.pointer()))
			.field("elts", &HexAddr(self.elements()))
			.field("head", &BinAddr::<T>(self.head(), PhantomData))
			.field("tail", &BinAddr::<T>(self.tail(), PhantomData))
			.finish()
	}
}

#[cfg(test)]
mod tests {
	use super::*;

	#[test]
	fn associated_consts_u8() {
		assert_eq!(BitPtr::<u8>::PTR_DATA_BITS, PTR_BITS);
		assert_eq!(BitPtr::<u8>::PTR_HEAD_BITS, 0);
		assert_eq!(BitPtr::<u8>::LEN_DATA_BITS, USZ_BITS - 6);
		assert_eq!(BitPtr::<u8>::LEN_TAIL_BITS, 3);

		assert_eq!(BitPtr::<u8>::PTR_DATA_MASK, !0);
		assert_eq!(BitPtr::<u8>::PTR_HEAD_MASK, 0);
		assert_eq!(BitPtr::<u8>::LEN_DATA_MASK, !0 << 6);
		assert_eq!(BitPtr::<u8>::LEN_TAIL_MASK, 7 << 3);
		assert_eq!(BitPtr::<u8>::LEN_INDX_MASK, 63);
	}

	#[test]
	fn associated_consts_u16() {
		assert_eq!(BitPtr::<u16>::PTR_DATA_BITS, PTR_BITS - 1);
		assert_eq!(BitPtr::<u16>::PTR_HEAD_BITS, 1);
		assert_eq!(BitPtr::<u16>::LEN_DATA_BITS, USZ_BITS - 7);
		assert_eq!(BitPtr::<u16>::LEN_TAIL_BITS, 4);

		assert_eq!(BitPtr::<u16>::PTR_DATA_MASK, !0 << 1);
		assert_eq!(BitPtr::<u16>::PTR_HEAD_MASK, 1);
		assert_eq!(BitPtr::<u16>::LEN_DATA_MASK, !0 << 7);
		assert_eq!(BitPtr::<u16>::LEN_TAIL_MASK, 15 << 3);
		assert_eq!(BitPtr::<u16>::LEN_INDX_MASK, 127);
	}

	#[test]
	fn associated_consts_u32() {
		assert_eq!(BitPtr::<u32>::PTR_DATA_BITS, PTR_BITS - 2);
		assert_eq!(BitPtr::<u32>::PTR_HEAD_BITS, 2);
		assert_eq!(BitPtr::<u32>::LEN_DATA_BITS, USZ_BITS - 8);
		assert_eq!(BitPtr::<u32>::LEN_TAIL_BITS, 5);

		assert_eq!(BitPtr::<u32>::PTR_DATA_MASK, !0 << 2);
		assert_eq!(BitPtr::<u32>::PTR_HEAD_MASK, 3);
		assert_eq!(BitPtr::<u32>::LEN_DATA_MASK, !0 << 8);
		assert_eq!(BitPtr::<u32>::LEN_TAIL_MASK, 31 << 3);
		assert_eq!(BitPtr::<u32>::LEN_INDX_MASK, 255);
	}

	#[test]
	fn associated_consts_u64() {
		assert_eq!(BitPtr::<u64>::PTR_DATA_BITS, PTR_BITS - 3);
		assert_eq!(BitPtr::<u64>::PTR_HEAD_BITS, 3);
		assert_eq!(BitPtr::<u64>::LEN_DATA_BITS, USZ_BITS - 9);
		assert_eq!(BitPtr::<u64>::LEN_TAIL_BITS, 6);

		assert_eq!(BitPtr::<u64>::PTR_DATA_MASK, !0 << 3);
		assert_eq!(BitPtr::<u64>::PTR_HEAD_MASK, 7);
		assert_eq!(BitPtr::<u64>::LEN_DATA_MASK, !0 << 9);
		assert_eq!(BitPtr::<u64>::LEN_TAIL_MASK, 63 << 3);
		assert_eq!(BitPtr::<u64>::LEN_INDX_MASK, 511);
	}

	#[test]
	fn ctors() {
		let data: [u32; 4] = [0x756c6153, 0x2c6e6f74, 0x6e6f6d20, 0x00216f64];
		let bp = BitPtr::<u32>::new(&data as *const u32, 4, 0, 32);
		assert_eq!(bp.pointer(), &data as *const u32);
		assert_eq!(bp.elements(), 4);
		assert_eq!(*bp.head(), 0);
		assert_eq!(*bp.tail(), 32);
	}

	#[test]
	fn empty() {
		let data = [0u8; 4];
		//  anything with 0 elements is unconditionally empty
		assert!(BitPtr::<u8>::new(&data as *const u8, 0, 2, 4).is_empty());
	}

	#[test]
	fn full() {
		let elt_ct = BitPtr::<u64>::MAX_ELTS - 1;
		//  maximum elements, maximum bits
		let bp = BitPtr::<u64>::new(8 as *const u64, elt_ct, 0, 63);
		assert!(bp.is_full());

		//  one bit fewer
		let bp = BitPtr::<u64>::new(8 as *const u64, elt_ct, 0, 62);
		assert!(!bp.is_full());
		assert_eq!(*bp.tail(), 62.into());

		//  one element fewer
		let bp = BitPtr::<u64>::new(8 as *const u64, elt_ct - 1, 0, 64);
		assert!(!bp.is_full());
	}

	#[test]
	#[should_panic]
	fn overfull() {
		BitPtr::<u64>::new(8 as *const u64, BitPtr::<u64>::MAX_ELTS - 1, 0, 64);
	}
}
