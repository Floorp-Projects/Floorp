=========
Changelog
=========

..
    You should *NOT* be adding new change log entries to this file, this
    file is managed by towncrier. You *may* edit previous change logs to
    fix problems like typo corrections or such.
    To add a new change log entry, please see
    https://pip.pypa.io/en/latest/development/#adding-a-news-entry
    we named the news folder "changes".

    WARNING: Don't drop the next directive!

.. towncrier release notes start

1.6.3 (2020-11-14)
==================

Bugfixes
--------

- No longer loose characters when decoding incorrect percent-sequences (like ``%e2%82%f8``). All non-decodable percent-sequences are now preserved.
  `#517 <https://github.com/aio-libs/yarl/issues/517>`_
- Provide x86 Windows wheels.
  `#535 <https://github.com/aio-libs/yarl/issues/535>`_


----


1.6.2 (2020-10-12)
==================


Bugfixes
--------

- Provide generated ``.c`` files in TarBall distribution.
  `#530  <https://github.com/aio-libs/multidict/issues/530>`_

1.6.1 (2020-10-12)
==================

Features
--------

- Provide wheels for ``aarch64``, ``i686``, ``ppc64le``, ``s390x`` architectures on
  Linux as well as ``x86_64``.
  `#507  <https://github.com/aio-libs/yarl/issues/507>`_
- Provide wheels for Python 3.9.
  `#526 <https://github.com/aio-libs/yarl/issues/526>`_

Bugfixes
--------

- ``human_repr()`` now always produces valid representation equivalent to the original URL (if the original URL is valid).
  `#511 <https://github.com/aio-libs/yarl/issues/511>`_
- Fixed  requoting a single percent followed by a percent-encoded character in the Cython implementation.
  `#514 <https://github.com/aio-libs/yarl/issues/514>`_
- Fix ValueError when decoding ``%`` which is not followed by two hexadecimal digits.
  `#516 <https://github.com/aio-libs/yarl/issues/516>`_
- Fix decoding ``%`` followed by a space and hexadecimal digit.
  `#520 <https://github.com/aio-libs/yarl/issues/520>`_
- Fix annotation of ``with_query()``/``update_query()`` methods for ``key=[val1, val2]`` case.
  `#528 <https://github.com/aio-libs/yarl/issues/528>`_

Removal
-------

- Drop Python 3.5 support; Python 3.6 is the minimal supported Python version.


----


1.6.0 (2020-09-23)
==================

Features
--------

- Allow for int and float subclasses in query, while still denying bool.
  `#492 <https://github.com/aio-libs/yarl/issues/492>`_


Bugfixes
--------

- Do not requote arguments in ``URL.build()``, ``with_xxx()`` and in ``/`` operator.
  `#502 <https://github.com/aio-libs/yarl/issues/502>`_
- Keep IPv6 brackets in ``origin()``.
  `#504 <https://github.com/aio-libs/yarl/issues/504>`_


----


1.5.1 (2020-08-01)
==================

Bugfixes
--------

- Fix including relocated internal ``yarl._quoting_c`` C-extension into published PyPI dists.
  `#485 <https://github.com/aio-libs/yarl/issues/485>`_


Misc
----

- `#484 <https://github.com/aio-libs/yarl/issues/484>`_


----


1.5.0 (2020-07-26)
==================

Features
--------

- Convert host to lowercase on URL building.
  `#386 <https://github.com/aio-libs/yarl/issues/386>`_
- Allow using ``mod`` operator (`%`) for updating query string (an alias for ``update_query()`` method).
  `#435 <https://github.com/aio-libs/yarl/issues/435>`_
- Allow use of sequences such as ``list`` and ``tuple`` in the values
  of a mapping such as ``dict`` to represent that a key has many values::

      url = URL("http://example.com")
      assert url.with_query({"a": [1, 2]}) == URL("http://example.com/?a=1&a=2")

  `#443 <https://github.com/aio-libs/yarl/issues/443>`_
- Support URL.build() with scheme and path (creates a relative URL).
  `#464 <https://github.com/aio-libs/yarl/issues/464>`_
- Cache slow IDNA encode/decode calls.
  `#476 <https://github.com/aio-libs/yarl/issues/476>`_
- Add ``@final`` / ``Final`` type hints
  `#477 <https://github.com/aio-libs/yarl/issues/477>`_
- Support URL authority/raw_authority properties and authority argument of ``URL.build()`` method.
  `#478 <https://github.com/aio-libs/yarl/issues/478>`_
- Hide the library implementation details, make the exposed public list very clean.
  `#483 <https://github.com/aio-libs/yarl/issues/483>`_


Bugfixes
--------

- Fix tests with newer Python (3.7.6, 3.8.1 and 3.9.0+).
  `#409 <https://github.com/aio-libs/yarl/issues/409>`_
- Fix a bug where query component, passed in a form of mapping or sequence, is unquoted in unexpected way.
  `#426 <https://github.com/aio-libs/yarl/issues/426>`_
- Hide `Query` and `QueryVariable` type aliases in `__init__.pyi`, now they are prefixed with underscore.
  `#431 <https://github.com/aio-libs/yarl/issues/431>`_
- Keep ipv6 brackets after updating port/user/password.
  `#451 <https://github.com/aio-libs/yarl/issues/451>`_


----


1.4.2 (2019-12-05)
==================

Features
--------

- Workaround for missing `str.isascii()` in Python 3.6
  `#389 <https://github.com/aio-libs/yarl/issues/389>`_


----


1.4.1 (2019-11-29)
==================

* Fix regression, make the library work on Python 3.5 and 3.6 again.

1.4.0 (2019-11-29)
==================

* Distinguish an empty password in URL from a password not provided at all (#262)

* Fixed annotations for optional parameters of ``URL.build`` (#309)

* Use None as default value of ``user`` parameter of ``URL.build`` (#309)

* Enforce building C Accelerated modules when installing from source tarball, use
  ``YARL_NO_EXTENSIONS`` environment variable for falling back to (slower) Pure Python
  implementation (#329)

* Drop Python 3.5 support

* Fix quoting of plus in path by pure python version (#339)

* Don't create a new URL if fragment is unchanged (#292)

* Included in error msg the path that produces starting slash forbidden error (#376)

* Skip slow IDNA encoding for ASCII-only strings (#387)


1.3.0 (2018-12-11)
==================

* Fix annotations for ``query`` parameter (#207)

* An incoming query sequence can have int variables (the same as for
  Mapping type) (#208)

* Add ``URL.explicit_port`` property (#218)

* Give a friendlier error when port cant be converted to int (#168)

* ``bool(URL())`` now returns ``False`` (#272)

1.2.6 (2018-06-14)
==================

* Drop Python 3.4 trove classifier (#205)

1.2.5 (2018-05-23)
==================

* Fix annotations for ``build`` (#199)

1.2.4 (2018-05-08)
==================

* Fix annotations for ``cached_property`` (#195)

1.2.3 (2018-05-03)
==================

* Accept ``str`` subclasses in ``URL`` constructor (#190)

1.2.2 (2018-05-01)
==================

* Fix build

1.2.1 (2018-04-30)
==================

* Pin minimal required Python to 3.5.3 (#189)

1.2.0 (2018-04-30)
==================

* Forbid inheritance, replace ``__init__`` with ``__new__`` (#171)

* Support PEP-561 (provide type hinting marker) (#182)

1.1.1 (2018-02-17)
==================

* Fix performance regression: don't encode enmpty netloc (#170)

1.1.0 (2018-01-21)
==================

* Make pure Python quoter consistent with Cython version (#162)

1.0.0 (2018-01-15)
==================

* Use fast path if quoted string does not need requoting (#154)

* Speed up quoting/unquoting by ``_Quoter`` and ``_Unquoter`` classes (#155)

* Drop ``yarl.quote`` and ``yarl.unquote`` public functions (#155)

* Add custom string writer, reuse static buffer if available (#157)
  Code is 50-80 times faster than Pure Python version (was 4-5 times faster)

* Don't recode IP zone (#144)

* Support ``encoded=True`` in ``yarl.URL.build()`` (#158)

* Fix updating query with multiple keys (#160)

0.18.0 (2018-01-10)
===================

* Fallback to IDNA 2003 if domain name is not IDNA 2008 compatible (#152)

0.17.0 (2017-12-30)
===================

* Use IDNA 2008 for domain name processing (#149)

0.16.0 (2017-12-07)
===================

* Fix raising ``TypeError`` by ``url.query_string()`` after
  ``url.with_query({})`` (empty mapping) (#141)

0.15.0 (2017-11-23)
===================

* Add ``raw_path_qs`` attribute (#137)

0.14.2 (2017-11-14)
===================

* Restore ``strict`` parameter as no-op in ``quote`` / ``unquote``

0.14.1 (2017-11-13)
===================

* Restore ``strict`` parameter as no-op for sake of compatibility with
  aiohttp 2.2

0.14.0 (2017-11-11)
===================

* Drop strict mode (#123)

* Fix ``"ValueError: Unallowed PCT %"`` when there's a ``"%"`` in the url (#124)

0.13.0 (2017-10-01)
===================

* Document ``encoded`` parameter (#102)

* Support relative urls like ``'?key=value'`` (#100)

* Unsafe encoding for QS fixed. Encode ``;`` char in value param (#104)

* Process passwords without user names (#95)

0.12.0 (2017-06-26)
===================

* Properly support paths without leading slash in ``URL.with_path()`` (#90)

* Enable type annotation checks

0.11.0 (2017-06-26)
===================

* Normalize path (#86)

* Clear query and fragment parts in ``.with_path()`` (#85)

0.10.3 (2017-06-13)
===================

* Prevent double URL args unquoting (#83)

0.10.2 (2017-05-05)
===================

* Unexpected hash behaviour (#75)


0.10.1 (2017-05-03)
===================

* Unexpected compare behaviour (#73)

* Do not quote or unquote + if not a query string. (#74)


0.10.0 (2017-03-14)
===================

* Added ``URL.build`` class method (#58)

* Added ``path_qs`` attribute (#42)


0.9.8 (2017-02-16)
==================

* Do not quote ``:`` in path


0.9.7 (2017-02-16)
==================

* Load from pickle without _cache (#56)

* Percent-encoded pluses in path variables become spaces (#59)


0.9.6 (2017-02-15)
==================

* Revert backward incompatible change (BaseURL)


0.9.5 (2017-02-14)
==================

* Fix BaseURL rich comparison support


0.9.4 (2017-02-14)
==================

* Use BaseURL


0.9.3 (2017-02-14)
==================

* Added BaseURL


0.9.2 (2017-02-08)
==================

* Remove debug print


0.9.1 (2017-02-07)
==================

* Do not lose tail chars (#45)


0.9.0 (2017-02-07)
==================

* Allow to quote ``%`` in non strict mode (#21)

* Incorrect parsing of query parameters with %3B (;) inside (#34)

* Fix core dumps (#41)

* tmpbuf - compiling error (#43)

* Added ``URL.update_path()`` method

* Added ``URL.update_query()`` method (#47)


0.8.1 (2016-12-03)
==================

* Fix broken aiohttp: revert back ``quote`` / ``unquote``.


0.8.0 (2016-12-03)
==================

* Support more verbose error messages in ``.with_query()`` (#24)

* Don't percent-encode ``@`` and ``:`` in path (#32)

* Don't expose ``yarl.quote`` and ``yarl.unquote``, these functions are
  part of private API

0.7.1 (2016-11-18)
==================

* Accept not only ``str`` but all classes inherited from ``str`` also (#25)

0.7.0 (2016-11-07)
==================

* Accept ``int`` as value for ``.with_query()``

0.6.0 (2016-11-07)
==================

* Explicitly use UTF8 encoding in setup.py (#20)
* Properly unquote non-UTF8 strings (#19)

0.5.3 (2016-11-02)
==================

* Don't use namedtuple fields but indexes on URL construction

0.5.2 (2016-11-02)
==================

* Inline ``_encode`` class method

0.5.1 (2016-11-02)
==================

* Make URL construction faster by removing extra classmethod calls

0.5.0 (2016-11-02)
==================

* Add cython optimization for quoting/unquoting
* Provide binary wheels

0.4.3 (2016-09-29)
==================

* Fix typing stubs

0.4.2 (2016-09-29)
==================

* Expose ``quote()`` and ``unquote()`` as public API

0.4.1 (2016-09-28)
==================

* Support empty values in query (``'/path?arg'``)

0.4.0 (2016-09-27)
==================

* Introduce ``relative()`` (#16)

0.3.2 (2016-09-27)
==================

* Typo fixes #15

0.3.1 (2016-09-26)
==================

* Support sequence of pairs as ``with_query()`` parameter

0.3.0 (2016-09-26)
==================

* Introduce ``is_default_port()``

0.2.1 (2016-09-26)
==================

* Raise ValueError for URLs like 'http://:8080/'

0.2.0 (2016-09-18)
==================

* Avoid doubling slashes when joining paths (#13)

* Appending path starting from slash is forbidden (#12)

0.1.4 (2016-09-09)
==================

* Add kwargs support for ``with_query()`` (#10)

0.1.3 (2016-09-07)
==================

* Document ``with_query()``, ``with_fragment()`` and ``origin()``

* Allow ``None`` for ``with_query()`` and ``with_fragment()``

0.1.2 (2016-09-07)
==================

* Fix links, tune docs theme.

0.1.1 (2016-09-06)
==================

* Update README, old version used obsolete API

0.1.0 (2016-09-06)
==================

* The library was deeply refactored, bytes are gone away but all
  accepted strings are encoded if needed.

0.0.1 (2016-08-30)
==================

* The first release.
