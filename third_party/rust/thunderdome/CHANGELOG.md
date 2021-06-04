# Thunderdome Changelog

## Unreleased Changes

## [0.4.1][0.4.1] (2021-02-24)
* Implemented `IntoIterator` for `&Arena` and `&mut Arena`. ([#18][issue-18])
* Added `Arena::get2_mut` for getting two mutable references of different slots at once. ([#22][issue-22])

[issue-18]: https://github.com/LPGhatguy/thunderdome/pull/18
[issue-22]: https://github.com/LPGhatguy/thunderdome/pull/22
[0.4.1]: https://github.com/LPGhatguy/thunderdome/releases/tag/v0.4.1

## [0.4.0][0.4.0] (2020-11-17)
* Added `Index::slot` for extracting the slot portion of an index.
* Added `Arena::contains_slot` for checking whether a slot is occupied in a given `Arena` and resolving its `Index` if so.
* Added `Arena::get_by_slot` and `Arena::get_by_slot_mut` for retrieving an entry by its slot, ignoring generation.
* Added `Arena::remove_by_slot` for removing an entry by its slot, ignoring generation.
* Added `Arena::contains` for checking whether an `Index` is valid for a given `Arena`.
* Added `Arena::retain` for conveniently removing entries which do not satisfy a given predicate.
* Fix `Arena::iter_mut` to return mutable references.

[0.4.0]: https://github.com/LPGhatguy/thunderdome/releases/tag/v0.4.0

## [0.3.0][0.3.0] (2020-10-16)
* Added `Arena::invalidate` for invalidating indices on-demand, as a faster remove-followed-by-reinsert.
* Added `Index::to_bits` and `Index::from_bits` for converting indices to a form convenient for passing outside of Rust.
* Added `Arena::clear` for conveniently clearing the whole arena.
* Change the semantics of `Arena::drain` to drop any remaining uniterated items when the `Drain` iterator is dropped, clearing the `Arena`.

[0.3.0]: https://github.com/LPGhatguy/thunderdome/releases/tag/v0.3.0

## [0.2.1][0.2.1] (2020-10-01)
* Added `Default` implementation for `Arena`.
* Added `IntoIterator` implementation for `Arena` ([#1](https://github.com/LPGhatguy/thunderdome/issues/1))
* Added `Arena::iter` and `Arena::iter_mut` ([#2](https://github.com/LPGhatguy/thunderdome/issues/2))

[0.2.1]: https://github.com/LPGhatguy/thunderdome/releases/tag/v0.2.1

## [0.2.0][0.2.0] (2020-09-03)
* Bumped MSRV to 1.34.1.
* Reduced size of `Index` by limiting `Arena` to 2^32 elements and 2^32 generations per slot.
	* These limits should not be hit in practice, but will consistently trigger panics.
* Changed generation counter to wrap instead of panic on overflow.
	* Collisions where an index using the same slot and a colliding generation on [1, 2^32] should be incredibly unlikely.

[0.2.0]: https://github.com/LPGhatguy/thunderdome/releases/tag/v0.2.0

## [0.1.1][0.1.1] (2020-09-02)
* Added `Arena::with_capacity` for preallocating space.
* Added `Arena::len`, `Arena::capacity`, and `Arena::is_empty`.
* Improved panic-on-wrap guarantees, especially around unsafe code.
* Simplified and documented implementation.

[0.1.1]: https://github.com/LPGhatguy/thunderdome/releases/tag/v0.1.1

## [0.1.0][0.1.0] (2020-09-02)
* Initial release

[0.1.0]: https://github.com/LPGhatguy/thunderdome/releases/tag/v0.1.0
