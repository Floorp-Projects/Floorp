
Either
======

The enum ``Either`` with variants ``Left`` and ``Right`` and trait
implementations including Iterator, Read, Write.

Either has methods that are similar to Option and Result.

Includes convenience macros ``try_left!()`` and ``try_right!()`` to use for
short-circuiting logic.

Please read the `API documentation here`__

__ https://docs.rs/either/

|build_status|_ |crates|_

.. |build_status| image:: https://travis-ci.org/bluss/either.svg?branch=master
.. _build_status: https://travis-ci.org/bluss/either

.. |crates| image:: http://meritbadge.herokuapp.com/either
.. _crates: https://crates.io/crates/either

How to use with cargo::

    [dependencies]
    either = "1.0"


Recent Changes
--------------

- 1.1.0

  - Add methods ``left_and_then``, ``right_and_then`` by @rampantmonkey
  - Include license files in the repository and released crate

- 1.0.3

  - Add crate categories

- 1.0.2

  - Forward more ``Iterator`` methods
  - Implement ``Extend`` for ``Either<L, R>`` if ``L, R`` do.

- 1.0.1

  - Fix ``Iterator`` impl for ``Either`` to forward ``.fold()``.

- 1.0.0

  - Add default crate feature ``use_std`` so that you can opt out of linking to
    std.

- 0.1.7

  - Add methods ``.map_left()``, ``.map_right()`` and ``.either()``.
  - Add more documentation

- 0.1.3

  - Implement Display, Error

- 0.1.2

  - Add macros ``try_left!`` and ``try_right!``.

- 0.1.1

  - Implement Deref, DerefMut

- 0.1.0

  - Initial release
  - Support Iterator, Read, Write

License
-------

Dual-licensed to be compatible with the Rust project.

Licensed under the Apache License, Version 2.0
http://www.apache.org/licenses/LICENSE-2.0 or the MIT license
http://opensource.org/licenses/MIT, at your
option. This file may not be copied, modified, or distributed
except according to those terms.
