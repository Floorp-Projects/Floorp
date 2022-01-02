
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
    either = "1.6"


Recent Changes
--------------

- 1.6.1

  - Add new methods ``.expect_left()``, ``.unwrap_left()``,
    and equivalents on the right, by @spenserblack (#51)

- 1.6.0

  - Add new modules ``serde_untagged`` and ``serde_untagged_optional`` to customize
    how ``Either`` fields are serialized in other types, by @MikailBag (#49)

- 1.5.3

  - Add new method ``.map()`` for ``Either<T, T>`` by @nvzqz (#40).

- 1.5.2

  - Add new methods ``.left_or()``, ``.left_or_default()``, ``.left_or_else()``,
    and equivalents on the right, by @DCjanus (#36)

- 1.5.1

  - Add ``AsRef`` and ``AsMut`` implementations for common unsized types:
    ``str``, ``[T]``, ``CStr``, ``OsStr``, and ``Path``, by @mexus (#29)

- 1.5.0

  - Add new methods ``.factor_first()``, ``.factor_second()`` and ``.into_inner()``
    by @mathstuf (#19)

- 1.4.0

  - Add inherent method ``.into_iter()`` by @cuviper (#12)

- 1.3.0

  - Add opt-in serde support by @hcpl

- 1.2.0

  - Add method ``.either_with()`` by @Twey (#13)

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
