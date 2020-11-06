# Thunderdome Changelog

## Unreleased Changes

## 0.3.0 (2020-10-16)
* Added `Arena::invalidate` for invalidating indices on-demand, as a faster remove-followed-by-reinsert.
* Added `Index::to_bits` and `Index::from_bits` for converting indices to a form convenient for passing outside of Rust.
* Added `Arena::clear` for conveniently clearing the whole arena.
* Change the semantics of `Arena::drain` to drop any remaining uniterated items when the `Drain` iterator is dropped, clearing the `Arena`.

## 0.2.1 (2020-10-01)
* Added `Default` implementation for `Arena`.
* Added `IntoIterator` implementation for `Arena` ([#1](https://github.com/LPGhatguy/thunderdome/issues/1))
* Added `Arena::iter` and `Arena::iter_mut` ([#2](https://github.com/LPGhatguy/thunderdome/issues/2))

## 0.2.0 (2020-09-03)
* Bumped MSRV to 1.34.1.
* Reduced size of `Index` by limiting `Arena` to 2^32 elements and 2^32 generations per slot.
	* These limits should not be hit in practice, but will consistently trigger panics.
* Changed generation counter to wrap instead of panic on overflow.
	* Collisions where an index using the same slot and a colliding generation on [1, 2^32] should be incredibly unlikely.

## 0.1.1 (2020-09-02)
* Added `Arena::with_capacity` for preallocating space.
* Added `Arena::len`, `Arena::capacity`, and `Arena::is_empty`.
* Improved panic-on-wrap guarantees, especially around unsafe code.
* Simplified and documented implementation.

## 0.1.0 (2020-09-02)
* Initial release
* Pretty much completely untested
* You probably shouldn't use this version