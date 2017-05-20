
arrayvec
========

A vector with fixed capacity.  Requires Rust 1.2+.

Please read the `API documentation here`__

__ https://bluss.github.io/arrayvec

|build_status|_ |crates|_ |crates2|_

.. |build_status| image:: https://travis-ci.org/bluss/arrayvec.svg
.. _build_status: https://travis-ci.org/bluss/arrayvec

.. |crates| image:: http://meritbadge.herokuapp.com/arrayvec
.. _crates: https://crates.io/crates/arrayvec

.. |crates2| image:: http://meritbadge.herokuapp.com/nodrop
.. _crates2: https://crates.io/crates/nodrop

Recent Changes (arrayvec)
-------------------------

- 0.3.23

  - Implement ``PartialOrd, Ord`` as well as ``PartialOrd<str>`` for
    ``ArrayString``.

- 0.3.22

  - Implement ``Array`` for the 65536 size

- 0.3.21

  - Use ``encode_utf8`` from crate odds
  - Add constructor ``ArrayString::from_byte_string``

- 0.3.20

  - Simplify and speed up ``ArrayString``’s ``.push(char)``-

- 0.3.19

  - Add new crate feature ``use_generic_array`` which allows using their
    ``GenericArray`` just like a regular fixed size array for the storage
    of an ``ArrayVec``.

- 0.3.18

  - Fix bounds check in ``ArrayVec::insert``!
    It would be buggy if ``self.len() < index < self.capacity()``. Take note of
    the push out behavior specified in the docs.

- 0.3.17

  - Added crate feature ``use_union`` which forwards to the nodrop crate feature
  - Added methods ``.is_full()`` to ``ArrayVec`` and ``ArrayString``.

- 0.3.16

  - Added method ``.retain()`` to ``ArrayVec``.
  - Added methods ``.as_slice(), .as_mut_slice()`` to ``ArrayVec`` and ``.as_str()``
    to ``ArrayString``.

- 0.3.15

  - Add feature std, which you can opt out of to use ``no_std`` (requires Rust 1.6
    to opt out).
  - Implement ``Clone::clone_from`` for ArrayVec and ArrayString

- 0.3.14

  - Add ``ArrayString::from(&str)``

- 0.3.13

  - Added ``DerefMut`` impl for ``ArrayString``.
  - Added method ``.simplify()`` to drop the element for ``CapacityError``.
  - Added method ``.dispose()`` to ``ArrayVec``

- 0.3.12

  - Added ArrayString, a fixed capacity analogy of String

- 0.3.11

  - Added trait impls Default, PartialOrd, Ord, Write for ArrayVec

- 0.3.10

  - Go back to using external NoDrop, fixing a panic safety bug (issue #3)

- 0.3.8

  - Inline the non-dropping logic to remove one drop flag in the
    ArrayVec representation.

- 0.3.7

  - Added method .into_inner()
  - Added unsafe method .set_len()

Recent Changes (nodrop)
-----------------------

- 0.1.9

  - Fix issue in recent nightly where ``repr(u8)`` did not work. Use
    a better way to get rid of the enum layout optimization.

- 0.1.8
  
  - Add crate feature ``use_union`` that uses untagged unions to implement NoDrop.
    Finally we have an implementation without hacks, without a runtime flag,
    and without an actual ``Drop`` impl (which was needed to suppress drop).
    The crate feature requires nightly and is unstable.

- 0.1.7

  - Remove crate feature ``no_drop_flag``, because it doesn't compile on nightly
    anymore. Drop flags are gone anyway!

- 0.1.6

  - Add feature std, which you can opt out of to use ``no_std``.

- 0.1.5

  - Added crate feature ``use_needs_drop`` which is a nightly-only
    optimization, which skips overwriting if the inner value does not need
    drop.

Recent Changes (nodrop-union)
-----------------------

- 0.1.9

  - Add ``Copy, Clone`` implementations

- 0.1.8

  - Initial release


License
=======

Dual-licensed to be compatible with the Rust project.

Licensed under the Apache License, Version 2.0
http://www.apache.org/licenses/LICENSE-2.0 or the MIT license
http://opensource.org/licenses/MIT, at your
option. This file may not be copied, modified, or distributed
except according to those terms.


