
Itertools
=========

Extra iterator adaptors, functions and macros.

Please read the `API documentation here`__

__ https://docs.rs/itertools/

|build_status|_ |crates|_

.. |build_status| image:: https://travis-ci.org/bluss/rust-itertools.svg?branch=master
.. _build_status: https://travis-ci.org/bluss/rust-itertools

.. |crates| image:: http://meritbadge.herokuapp.com/itertools
.. _crates: https://crates.io/crates/itertools

How to use with cargo:

.. code:: toml

    [dependencies]
    itertools = "0.6.3"

How to use in your crate:

.. code:: rust

    #[macro_use] extern crate itertools;

    use itertools::Itertools;

How to contribute:

- Fix a bug or implement a new thing
- Include tests for your new feature, preferably a quickcheck test
- Make a Pull Request


Recent Changes
--------------

- 0.6.5

  - Fix bug in ``.cartesian_product()``'s fold (which only was visible for
    unfused iterators).

- 0.6.4

  - Add specific ``fold`` implementations for ``.cartesian_product()`` and
    ``cons_tuples()``, which improves their performance in fold, foreach, and
    iterator consumers derived from them.

- 0.6.3

  - Add iterator adaptor ``.positions(predicate)`` by @tmccombs

- 0.6.2

  - Add function ``process_results`` which can “lift” a function of the regular
    values of an iterator so that it can process the ``Ok`` values from an
    iterator of ``Results`` instead, by @shepmaster
  - Add iterator method ``.concat()`` which combines all iterator elements
    into a single collection using the ``Extend`` trait, by @srijs

- 0.6.1

  - Better size hint testing and subsequent size hint bugfixes by @rkarp.
    Fixes bugs in product, interleave_shortest size hints.
  - New iterator method ``.all_equal()`` by @phimuemue

- 0.6.0

  - Deprecated names were removed in favour of their replacements
  - ``.flatten()`` does not implement double ended iteration anymore
  - ``.fold_while()`` uses ``&mut self`` and returns ``FoldWhile<T>``, for
    composability (#168)
  - ``.foreach()`` and ``.fold1()`` use ``self``, like ``.fold()`` does.
  - ``.combinations(0)`` now produces a single empty vector. (#174)

- 0.5.10

  - Add itertools method ``.kmerge_by()`` (and corresponding free function)
  - Relaxed trait requirement of ``.kmerge()`` and ``.minmax()`` to PartialOrd.

- 0.5.9

  - Add multipeek method ``.reset_peek()``
  - Add categories

- 0.5.8

  - Add iterator adaptor ``.peeking_take_while()`` and its trait ``PeekingNext``.

- 0.5.7

  - Add iterator adaptor ``.with_position()``
  - Fix multipeek's performance for long peeks by using ``VecDeque``.

- 0.5.6

  - Add ``.map_results()``

- 0.5.5

  - Many more adaptors now implement ``Debug``
  - Add free function constructor ``repeat_n``. ``RepeatN::new`` is now
    deprecated.

- 0.5.4

  - Add infinite generator function ``iterate``, that takes a seed and a
    closure.

- 0.5.3

  - Special-cased ``.fold()`` for flatten and put back. ``.foreach()``
    now uses fold on the iterator, to pick up any iterator specific loop
    implementation.
  - ``.combinations(n)`` asserts up front that ``n != 0``, instead of
    running into an error on the second iterator element.

- 0.5.2

  - Add ``.tuples::<T>()`` that iterates by two, three or four elements at
    a time (where ``T`` is a tuple type).
  - Add ``.tuple_windows::<T>()`` that iterates using a window of the
    two, three or four most recent elements.
  - Add ``.next_tuple::<T>()`` method, that picks the next two, three or four
    elements in one go.
  - ``.interleave()`` now has an accurate size hint.

- 0.5.1

  - Workaround module/function name clash that made racer crash on completing
    itertools. Only internal changes needed.

- 0.5.0

  - `Release announcement <http://bluss.github.io/rust/2016/09/26/itertools-0.5.0/>`_
  - Renamed:

    - combinations is now tuple_combinations
    - combinations_n to combinations
    - group_by_lazy, chunks_lazy to group_by, chunks
    - Unfold::new to unfold()
    - RepeatCall::new to repeat_call()
    - Zip::new to multizip
    - PutBack::new, PutBackN::new to put_back, put_back_n
    - PutBack::with_value is now a builder setter, not a constructor
    - MultiPeek::new, .multipeek() to multipeek()
    - format to format_with and format_default to format
    - .into_rc() to rciter
    - ``Partition`` enum is now ``Either``

  - Module reorganization:

    - All iterator structs are under ``itertools::structs`` but also
      reexported to the top level, for backwards compatibility
    - All free functions are reexported at the root, ``itertools::free`` will
      be removed in the next version

  - Removed:

    - ZipSlices, use .zip() instead
    - .enumerate_from(), ZipTrusted, due to being unstable
    - .mend_slices(), moved to crate odds
    - Stride, StrideMut, moved to crate odds
    - linspace(), moved to crate itertools-num
    - .sort_by(), use .sorted_by()
    - .is_empty_hint(), use .size_hint()
    - .dropn(), use .dropping()
    - .map_fn(), use .map()
    - .slice(), use .take() / .skip()
    - helper traits in misc
    - ``new`` constructors on iterator structs, use Itertools trait or free
      functions instead
    - ``itertools::size_hint`` is now private

  - Behaviour changes:

    - format and format_with helpers now panic if you try to format them more
      than once.
    - ``repeat_call`` is not double ended anymore

  - New features:

    - tuple flattening iterator is constructible with ``cons_tuples``
    - itertools reexports ``Either`` from the ``either`` crate. ``Either<L, R>``
      is an iterator when ``L, R`` are.
    - ``MinMaxResult`` now implements Copy and Clone
    - tuple_combinations supports 1-4 tuples of combinations (previously just 2)

- 0.4.19

  - Add ``.minmax_by()``
  - Add ``itertools::free::cloned``
  - Add ``itertools::free::rciter``
  - Improve ``.step(n)`` slightly to take advantage of specialized Fuse better.

- 0.4.18

  - Only changes related to the "unstable" crate feature. This feature is more
    or less deprecated.

    - Use deprecated warnings when unstable is enabled. .enumerate_from() will
      be removed imminently since it's using a deprecated libstd trait.

- 0.4.17

  - Fix bug in .kmerge() that caused it to often produce the wrong order (#134)

- 0.4.16

  - Improve precision of the interleave_shortest adaptor's size hint (it is
    now computed exactly when possible).

- 0.4.15

  - Fixup on top of the workaround in 0.4.14. A function in itertools::free was
    removed by mistake and now it is added back again.

- 0.4.14

  - Workaround an upstream regression in a rust nightly build that broke
    compilation of of itertools::free::{interleave, merge}

- 0.4.13

  - Add .minmax() and .minmax_by_key(), iterator methods for finding both minimum
    and maximum in one scan.
  - Add .format_default(), a simpler version of .format() (lazy formatting
    for iterators).

- 0.4.12

  - Add .zip_eq(), an adaptor like .zip() except it ensures iterators
    of inequal length don't pass silently (instead it panics).
  - Add .fold_while(), an iterator method that is a fold that
    can short-circuit.
  - Add .partition_map(), an iterator method that can separate elements
    into two collections.

- 0.4.11

  - Add .get() for Stride{,Mut} and .get_mut() for StrideMut

- 0.4.10

  - Improve performance of .kmerge()

- 0.4.9

  - Add k-ary merge adaptor .kmerge()
  - Fix a bug in .islice() with ranges a..b where a > b.

- 0.4.8

  - Implement Clone, Debug for Linspace

- 0.4.7

  - Add function diff_with() that compares two iterators
  - Add .combinations_n(), an n-ary combinations iterator
  - Add methods PutBack::with_value and PutBack::into_parts.

- 0.4.6

  - Add method .sorted()
  - Add module ``itertools::free`` with free function variants of common
    iterator adaptors and methods.
    For example ``enumerate(iterable)``, ``rev(iterable)``, and so on.

- 0.4.5

  - Add .flatten()

- 0.4.4

  - Allow composing ZipSlices with itself

- 0.4.3

  - Write iproduct!() as a single expression; this allows temporary values
    in its arguments.

- 0.4.2

  - Add .fold_options()
  - Require Rust 1.1 or later

- 0.4.1

  - Update .dropping() to take advantage of .nth()

- 0.4.0

  - .merge(), .unique() and .dedup() now perform better due to not using
    function pointers
  - Add free functions enumerate() and rev()
  - Breaking changes:

    - Return types of .merge() and .merge_by() renamed and changed
    - Method Merge::new removed
    - .merge_by() now takes a closure that returns bool.
    - Return type of .dedup() changed
    - Return type of .mend_slices() changed
    - Return type of .unique() changed
    - Removed function times(), struct Times: use a range instead
    - Removed deprecated macro icompr!()
    - Removed deprecated FnMap and method .fn_map(): use .map_fn()
    - .interleave_shortest() is no longer guaranteed to act like fused

- 0.3.25

  - Rename .sort_by() to .sorted_by(). Old name is deprecated.
  - Fix well-formedness warnings from RFC 1214, no user visible impact

- 0.3.24

  - Improve performance of .merge()'s ordering function slightly

- 0.3.23

  - Added .chunks(), similar to (and based on) .group_by_lazy().
  - Tweak linspace to match numpy.linspace and make it double ended.

- 0.3.22

  - Added ZipSlices, a fast zip for slices

- 0.3.21

  - Remove `Debug` impl for `Format`, it will have different use later

- 0.3.20

  - Optimize .group_by_lazy()

- 0.3.19

  - Added .group_by_lazy(), a possibly nonallocating group by
  - Added .format(), a nonallocating formatting helper for iterators
  - Remove uses of RandomAccessIterator since it has been deprecated in rust.

- 0.3.17

  - Added (adopted) Unfold from rust

- 0.3.16

  - Added adaptors .unique(), .unique_by()

- 0.3.15

  - Added method .sort_by()

- 0.3.14

  - Added adaptor .while_some()

- 0.3.13

  - Added adaptor .interleave_shortest()
  - Added adaptor .pad_using()

- 0.3.11

  - Added assert_equal function

- 0.3.10

  - Bugfix .combinations() size_hint.

- 0.3.8

  - Added source RepeatCall

- 0.3.7

  - Added adaptor PutBackN
  - Added adaptor .combinations()

- 0.3.6

  - Added itertools::partition, partition a sequence in place based on a predicate.
  - Deprecate icompr!() with no replacement.

- 0.3.5

  - .map_fn() replaces deprecated .fn_map().

- 0.3.4

  - .take_while_ref() *by-ref adaptor*
  - .coalesce() *adaptor*
  - .mend_slices() *adaptor*

- 0.3.3

  - .dropping_back() *method*
  - .fold1() *method*
  - .is_empty_hint() *method*

License
-------

Dual-licensed to be compatible with the Rust project.

Licensed under the Apache License, Version 2.0
http://www.apache.org/licenses/LICENSE-2.0 or the MIT license
http://opensource.org/licenses/MIT, at your
option. This file may not be copied, modified, or distributed
except according to those terms.
