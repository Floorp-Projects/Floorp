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

5.1.0 (2020-12-03)
==================

Features
--------

- Support ``GenericAliases`` (``MultiDict[str]``) for Python 3.9+
  `#553 <https://github.com/aio-libs/multidict/issues/553>`_


Bugfixes
--------

- Synchronize the declared supported Python versions in ``setup.py`` with actually supported and tested ones.
  `#552 <https://github.com/aio-libs/multidict/issues/552>`_


----


5.0.1 (2020-11-14)
==================

Bugfixes
--------

- Provide x86 Windows wheels
  `#550 <https://github.com/aio-libs/multidict/issues/550>`_


----


5.0.0 (2020-10-12)
==================

Features
--------

- Provide wheels for ``aarch64``, ``i686``, ``ppc64le``, ``s390x`` architectures on Linux
  as well as ``x86_64``.
  `#500 <https://github.com/aio-libs/multidict/issues/500>`_
- Provide wheels for Python 3.9.
  `#534 <https://github.com/aio-libs/multidict/issues/534>`_

Removal
-------

- Drop Python 3.5 support; Python 3.6 is the minimal supported Python version.

Misc
----

- `#503 <https://github.com/aio-libs/multidict/issues/503>`_


----


4.7.6 (2020-05-15)
==================

Bugfixes
--------

- Fixed an issue with some versions of the ``wheel`` dist
  failing because of being unable to detect the license file.
  `#481 <https://github.com/aio-libs/multidict/issues/481>`_


----


4.7.5 (2020-02-21)
==================

Bugfixes
--------

- Fixed creating and updating of MultiDict from a sequence of pairs and keyword
  arguments. Previously passing a list argument modified it inplace, and other sequences
  caused an error.
  `#457 <https://github.com/aio-libs/multidict/issues/457>`_
- Fixed comparing with mapping: an exception raised in the
  :py:func:`~object.__len__` method caused raising a SyntaxError.
  `#459 <https://github.com/aio-libs/multidict/issues/459>`_
- Fixed comparing with mapping: all exceptions raised in the
  :py:func:`~object.__getitem__` method were silenced.
  `#460 <https://github.com/aio-libs/multidict/issues/460>`_


----


4.7.4 (2020-01-11)
==================

Bugfixes
--------

- ``MultiDict.iter`` fix memory leak when used iterator over
  :py:mod:`multidict` instance.
  `#452 <https://github.com/aio-libs/multidict/issues/452>`_


----


4.7.3 (2019-12-30)
==================

Features
--------

- Implement ``__sizeof__`` function to correctly calculate all internal structures size.
  `#444 <https://github.com/aio-libs/multidict/issues/444>`_
- Expose ``getversion()`` function.
  `#451 <https://github.com/aio-libs/multidict/issues/451>`_


Bugfixes
--------

- Fix crashes in ``popone``/``popall`` when default is returned.
  `#450 <https://github.com/aio-libs/multidict/issues/450>`_


Improved Documentation
----------------------

- Corrected the documentation for ``MultiDict.extend()``
  `#446 <https://github.com/aio-libs/multidict/issues/446>`_


----


4.7.2 (2019-12-20)
==================

Bugfixes
--------

- Fix crashing when multidict is used pyinstaller
  `#432 <https://github.com/aio-libs/multidict/issues/432>`_
- Fix typing for :py:meth:`CIMultiDict.copy`
  `#434 <https://github.com/aio-libs/multidict/issues/434>`_
- Fix memory leak in ``MultiDict.copy()``
  `#443 <https://github.com/aio-libs/multidict/issues/443>`_


----


4.7.1 (2019-12-12)
==================

Bugfixes
--------

- :py:meth:`CIMultiDictProxy.copy` return object type
  :py:class:`multidict._multidict.CIMultiDict`
  `#427 <https://github.com/aio-libs/multidict/issues/427>`_
- Make :py:class:`CIMultiDict` subclassable again
  `#416 <https://github.com/aio-libs/multidict/issues/416>`_
- Fix regression, multidict can be constructed from arbitrary iterable of pairs again.
  `#418 <https://github.com/aio-libs/multidict/issues/418>`_
- :py:meth:`CIMultiDict.add` may be called with keyword arguments
  `#421 <https://github.com/aio-libs/multidict/issues/421>`_


Improved Documentation
----------------------

- Mention ``MULTIDICT_NO_EXTENSIONS`` environment variable in docs.
  `#393 <https://github.com/aio-libs/multidict/issues/393>`_
- Document the fact that ``istr`` preserves the casing of argument untouched but uses internal lower-cased copy for keys comparison.
  `#419 <https://github.com/aio-libs/multidict/issues/419>`_


----


4.7.0 (2019-12-10)
==================

Features
--------

- Replace Cython optimization with pure C
  `#249 <https://github.com/aio-libs/multidict/issues/249>`_
- Implement ``__length_hint__()`` for iterators
  `#310 <https://github.com/aio-libs/multidict/issues/310>`_
- Support the MultiDict[str] generic specialization in the runtime.
  `#392 <https://github.com/aio-libs/multidict/issues/392>`_
- Embed pair_list_t structure into MultiDict Python object
  `#395 <https://github.com/aio-libs/multidict/issues/395>`_
- Embed multidict pairs for small dictionaries to amortize the memory usage.
  `#396 <https://github.com/aio-libs/multidict/issues/396>`_
- Support weak references to C Extension classes.
  `#399 <https://github.com/aio-libs/multidict/issues/399>`_
- Add docstrings to provided classes.
  `#400 <https://github.com/aio-libs/multidict/issues/400>`_
- Merge ``multidict._istr`` back with ``multidict._multidict``.
  `#409 <https://github.com/aio-libs/multidict/issues/409>`_


Bugfixes
--------

- Explicitly call ``tp_free`` slot on deallocation.
  `#407 <https://github.com/aio-libs/multidict/issues/407>`_
- Return class from __class_getitem__ to simplify subclassing
  `#413 <https://github.com/aio-libs/multidict/issues/413>`_


----


4.6.1 (2019-11-21)
====================

Bugfixes
--------

- Fix PyPI link for GitHub Issues badge.
  `#391 <https://github.com/aio-libs/aiohttp/issues/391>`_

4.6.0 (2019-11-20)
====================

Bugfixes
--------

- Fix GC object tracking.
  `#314 <https://github.com/aio-libs/aiohttp/issues/314>`_
- Preserve the case of `istr` strings.
  `#374 <https://github.com/aio-libs/aiohttp/issues/374>`_
- Generate binary wheels for Python 3.8.
