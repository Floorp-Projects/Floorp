# Changelog

All notable changes will be documented in this file.

## 0.11.0

This contains the last (planned) compiler version upgrade, to `1.33.0`, and the
last major feature add before `1.0`: Serde-powered de/serialization.

Deserialization is not possible without access to an allocator, so it is behind
a feature gate, `serdes`, which depends on the `alloc` feature.

`BitSlice`, `BitBox`, and `BitVec` all support serialization, and `BitBox` and
`BitVec` support deserialization

### Added

- `serdes` feature to serialize `BitSlice`, `BitBox`, and `BitVec`, and
  deserialize `BitBox` and `BitVec`.
- `change_cursor<D>` method on `BitSlice`, `BitBox`, and `BitVec`, which enable
  changing the element traversal order on a data set without modifying that
  data. This is useful for working with slices that have their cursor type
  erased, such as crossing serialization or foreign-language boundaries.
- Internal domain models for the memory regions governed by `BitPtr`. These
  models provide improved logical support for manipulating bit sequences with as
  little inefficiency as possible.
- `BitPtr::bare_parts` and `BitPtr::region_data` internal APIs for accessing
  components of the pointer structure.
- Clippy is now part of the development routine.

### Changed

- The internal `Bits` trait uses a `const fn` stabilized in `1.33.0` in order to
  compute type information, rather than requiring explicit statements in the
  implementations.

## 0.10.1

Bugfix for [Issue #7](https://github.com/myrrlyn/bitvec/issues/7).
`BitSlice::count_ones` and `BitSlice::count_zeros` counted the total number of
bits present in a slice, not the number of bits set or unset, when operating
inside a single element.

The small case used `.map().count()`, but the large case correctly used
`.map().filter().count()`. The missing `.filter()` call, to remove unset or set
bits from the counting, was the cause of the bug.

Thanks to GitHub user [@geq1t](https://github.com/geq1t) for the report!

## 0.10.0

This version was a complete rewrite of the entire crate. The minimum compiler
version has been upgraded to `1.31.0`. The crate is written against the Rust
2018 edition of the language. It will be a `1.0` release after polishing.

### Added

- `BitPtr` custom pointer representation. This is the most important component
  of the rewrite, and what enabled the expanded feature set and API surface.
  This structure allows `BitSlice` and `BitVec` to have head cursors at any bit,
  not just at the front edge of an element. This allows the crate to support
  arbitrary range slicing and slice splitting, and in turn greatly expand the
  usability of the slice and vector types.

  The `BitPtr` type is wholly crate-internal, and renders the `&BitSlice` and
  `BitVec` handle types ***wholly incompatible*** with standard Rust slice and
  vector handles. With great power comes great responsibility to never, ever,
  interchange these types through any means except the provided translation API.

- Range indexing and more powerful iteration. Bit-precision addressing allows
  arbitrary subslices and enables more of the slice API from `core`.

### Changed

- Almost everything has been rewritten. The git diff for this version is
  horrifying.

- Formatting traits better leverage the builtin printing structures available
  from `core::fmt`, and are made available on `no_std`.

### Removed

- `u64` is only usable as the storage type on 64-bit systems; it has 32-bit
  alignment on 32-bit systems and as such is unusable there.

## 0.9.0

### Changed

- The trait `Endian` has been renamed to `Cursor`, and all type variables
  `E: Endian` have been renamed to `C: Cursor`.

- The `Bits` trait is no longer bound by `Default`.

## 0.8.0

### Added

- `std` and `alloc` features, which can be disabled for use in `#![no_std]`
  libraries. This was implemented by Robert Habermeier, `rphmeier@gmail.com`.

  Note that the `BitSlice` tests and all the examples are disabled when the
  `alloc` feature is not present. They will function normally when `alloc` is
  present but `std` is not.

### Changed

- Compute `Bits::WIDTH` as `size_of::<Self>() * 8` instead of `1 << Bits::BITS`.

## 0.7.0

### Added

- `examples/readme.rs` tracks the contents of the example code in `README.md`.
  It will continue to do so until the `external_doc` feature stabilizes so that
  the contents of the README can be included in the module documentation of
  `src/lib.rs`.

- Officially use the Rust community code of conduct.

- README sections describe why a user might want this library, and what makes it
  different than `bit-vec`.

### Changed

- Update minimum Rust version to `1.30.0`.

  Internally, this permits use of `std` rather than `::std`. This compiler
  edition does not change *intra-crate* macro usage. Clients at `1.30.0` and
  above no longer need `#[macro_use]` above `extern crate bitvec;`, and are able
  to import the `bitvec!` macro directly with `use bitvec::bitvec;` or
  `use bitvec::prelude::*`.

  Implementation note: References to literals stabilized at *some* point between
  `1.20.0` and `1.30.0`, so the static bool items used for indexing are no
  longer needed.

- Include numeric arithmetic as well as set arithmetic in the README.

## 0.6.0

### Changed

- Update minimum Rust version to `1.25.0` in order to use nested imports.
- Fix logic in `Endian::prev`, and re-enabled edge tests.
- Pluralize `BitSlice::count_one()` and `BitSlice::count_zero()` function names.
- Fix documentation and comments.
- Consolidate implementation of `bitvec!` to not use any other macros.

## 0.5.0

### Added

- `BitVec` and `BitSlice` implement `Hash`.

- `BitVec` fully implements addition, negation, and subtraction.

- `BitSlice` implements in-place addition and negation.
  - `impl AddAssign for BitSlice`
  - `impl Neg for &mut BitSlice`

  This distinction is required in order to match the expectations of the
  arithmetic traits and the realities of immovable `BitSlice`.

- `BitSlice` offers `.all()`, `.any()`, `.not_all()`, `.not_any()`, and
  `.some()` methods to perform n-ary Boolean logic.
  - `.all()` tests if all bits are set high
  - `.any()` tests if any bits are set high (includes `.all()`)
  - `.not_all()` tests if any bits are set low (includes `.not_all()`)
  - `.not_any()` tests if all bits are set low
  - `.some()` tests if any bits are high and any are low (excludes `.all()` and
    `.not_all()`)

- `BitSlice` can count how many bits are set high or low with `.count_one()` and
  `.count_zero()`.

## 0.4.0

### Added

`BitSlice::for_each` provides mutable iteration over a slice. It yields each
successive `(index: usize, bit: bool)` pair to a closure, and stores the return
value of that closure at the yielded index.

`BitVec` now implements `Eq` and `Ord` against other `BitVec`s. It is impossible
at this time to make `BitVec` generic over anything that is `Borrow<BitSlice>`,
which would allow comparisons over different ownership types. The declaration

```rust
impl<A, B, C, D, E> PartialEq<C> for BitVec<A, B>
where A: Endian,
    B: Bits,
    C: Borrow<BitSlice<D, E>>,
    D: Endian,
    E: Bits,
{
    fn eq(&self, rhs: E) { … }
}
```

is impossible to write, so `BitVec == BitSlice` will be rejected.

As with many other traits on `BitVec`, the implementations are just a thin
wrapper over the corresponding `BitSlice` implementations.

### Changed

Refine the API documentation. Rust guidelines recommend imperative rather than
descriptive summaries for function documentation, which largely meant stripping
the trailing -s from the first verb in each function document.

I also moved the example code from the trait-level documentation to the
function-level documentation, so that it would show up an `type::func` in the
`rustdoc` output rather than just `type`. This makes it much clearer what is
being tested.

### Removed

`BitVec` methods `iter` and `raw_len` moved to `BitSlice` in `0.3.0` but were
not removed in that release.

The remaining debugging `eprintln!` calls have been stripped.

## 0.3.0

Split `BitVec` off into `BitSlice` wherever possible.

### Added

- The `BitSlice` type is the `[T]` to `BitVec`'s `Vec<T>`. `BitVec` now `Deref`s
  to it, and has offloaded all the work that does not require managing allocated
  memory.
- Almost all of the public API on both types has documentation and example code.

### Changed

- The implementations of left- ard right- shift are now faster.
- `BitVec` can `Borrow` and `Deref` down to `BitSlice`, and offloads as much
  work as possible to it.
- `Clone` is more intelligent.

## 0.2.0

Improved the `bitvec!` macro.

### Changed

- `bitvec!` takes more syntaxes to better match `vec!`, and has better
  runtime performance. The increased static memory used by `bitvec!` should be
  more than counterbalanced by the vastly better generated code.

## 0.1.0

Initial implementation and release.

### Added

- `Endian` and `Bits` traits
- `BitVec` type with basic `Vec` idioms and parallel trait implementations
- `bitvec!` generator macro
