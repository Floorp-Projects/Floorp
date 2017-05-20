
odds
====

Odds and ends — collection miscellania. Extra functionality related to slices,
strings and other things.

Please read the `API documentation here`__

__ https://docs.rs/odds/

|build_status|_ |crates|_

.. |build_status| image:: https://travis-ci.org/bluss/odds.svg
.. _build_status: https://travis-ci.org/bluss/odds

.. |crates| image:: http://meritbadge.herokuapp.com/odds
.. _crates: https://crates.io/crates/odds

Recent Changes
--------------

- 0.2.25

  - Add ``UnalignedIter``
  - Add ``SliceCopyIter``
  - ``CharStr`` now implements more traits.

- 0.2.24
  
  - Add ``CharStr``

- 0.2.23

  - Add ``RevSlice``, a reversed view of a slice
  - Add ``encode_utf8`` for encoding chars

- 0.2.22

  - Improve slice's ``.find()`` and ``.rfind()`` and related methods
    by explicitly unrolling their search loop.

- 0.2.21

  - Add ``slice::rotate_left`` to cyclically rotate elements in a slice.

- 0.2.20

  - Add ``SliceFindSplit`` with ``.find_split, .rfind_split, .find_split_mut,``
    ``.rfind_split_mut``.
  - Add ``VecFindRemove`` with ``.find_remove(), .rfind_remove()``.

- 0.2.19

  - Add trait ``SliceFind`` with methods ``.find(&T), .rfind(&T)`` for
    slices.
  - Add function ``vec(iterable) -> Vec``
  - Add prelude module

- 0.2.18

  - Correct ``split_aligned_for<T>`` to use the trait bound.

- 0.2.17

  - Add ``split_aligned_for<T>`` function that splits a byte slice into
    head and tail slices and a main slice that is a well aligned block
    of type ``&[T]``. Where ``T`` is a pod type like for example ``u64``.
  - Add ``Stride, StrideMut`` that moved here from itertools
  - Add ``mend_slices`` iterator extension that moved here from itertools

- 0.2.16

  - Add ``fix`` function that makes it much easier to use the ``Fix`` combinator.
    Type inference works much better for the closure this way.

- 0.2.15

  - Add ``std::slice::shared_prefix`` to efficiently compute the shared
    prefix of two byte slices
  - Add str extension methods ``.char_chunks(n)`` and ``char_windows(n)``
    that are iterators that do the char-wise equivalent of slice's chunks and windows
    iterators.

- 0.2.14

  - Fix ``get_slice`` to check ``start <= end`` as well.

- 0.2.13

  - Add extension trait ``StrSlice`` with method ``get_slice`` that is a slicing
    method that returns an optional slice.

- 0.2.12

  - Add default feature "std". odds uses ``no_std`` if you opt out of this
    feature.

- 0.2.11

  - Add type parameter to ``IndexRange`` (that defaults to ``usize``,
    so that it's non-breaking).
  - Drop dep on ``unreachable`` (provided in a simpler implementation locally).

- 0.2.10

  - Fix feature flags when using cargo feature ``unstable``

- 0.2.9

  - Add ``slice_unchecked_mut``
  - Add ``ref_slice``, ``ref_slice_mut``

- 0.2.8

  - Add `VecExt::retain_mut`

- 0.2.7

  - `inline(always)` on `debug_assert_unreachable`

- 0.2.6

  - Add lifetime bounds for Fix for well-formedness (Rust RFC 1214)
  - Add `StrExt::is_acceptable_index`

- 0.2.5
  
  - Add `StringExt::insert_str` and `VecExt::splice`

- 0.2.4

  - Add `odds::string::StrExt`, extensions to `&str`.

- 0.2.3

  - Add default for Fix so that ``Fix<T> == Fix<T, T>``

- 0.2.2

  - Add ptr_eq, ref_eq

- 0.2.1

  - Add slice_unchecked

- 0.2.0

  - Removed **Void**, see ``void`` crate instead.

License
-------

Dual-licensed to be compatible with the Rust project.

Licensed under the Apache License, Version 2.0
http://www.apache.org/licenses/LICENSE-2.0 or the MIT license
http://opensource.org/licenses/MIT, at your
option. This file may not be copied, modified, or distributed
except according to those terms.


