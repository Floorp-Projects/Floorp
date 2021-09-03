=========
multidict
=========

.. image:: https://github.com/aio-libs/multidict/workflows/CI/badge.svg
   :target: https://github.com/aio-libs/multidict/actions?query=workflow%3ACI
   :alt: GitHub status for master branch

.. image:: https://codecov.io/gh/aio-libs/multidict/branch/master/graph/badge.svg
   :target: https://codecov.io/gh/aio-libs/multidict
   :alt: Coverage metrics

.. image:: https://img.shields.io/pypi/v/multidict.svg
   :target: https://pypi.org/project/multidict
   :alt: PyPI

.. image:: https://readthedocs.org/projects/multidict/badge/?version=latest
   :target: http://multidict.readthedocs.org/en/latest/?badge=latest
   :alt: Documentationb

.. image:: https://img.shields.io/pypi/pyversions/multidict.svg
   :target: https://pypi.org/project/multidict
   :alt: Python versions

.. image:: https://badges.gitter.im/Join%20Chat.svg
   :target: https://gitter.im/aio-libs/Lobby
   :alt: Chat on Gitter

Multidict is dict-like collection of *key-value pairs* where key
might be occurred more than once in the container.

Introduction
------------

*HTTP Headers* and *URL query string* require specific data structure:
*multidict*. It behaves mostly like a regular ``dict`` but it may have
several *values* for the same *key* and *preserves insertion ordering*.

The *key* is ``str`` (or ``istr`` for case-insensitive dictionaries).

``multidict`` has four multidict classes:
``MultiDict``, ``MultiDictProxy``, ``CIMultiDict``
and ``CIMultiDictProxy``.

Immutable proxies (``MultiDictProxy`` and
``CIMultiDictProxy``) provide a dynamic view for the
proxied multidict, the view reflects underlying collection changes. They
implement the ``collections.abc.Mapping`` interface.

Regular mutable (``MultiDict`` and ``CIMultiDict``) classes
implement ``collections.abc.MutableMapping`` and allows to change
their own content.


*Case insensitive* (``CIMultiDict`` and
``CIMultiDictProxy``) ones assume the *keys* are case
insensitive, e.g.::

   >>> dct = CIMultiDict(key='val')
   >>> 'Key' in dct
   True
   >>> dct['Key']
   'val'

*Keys* should be ``str`` or ``istr`` instances.

The library has optional C Extensions for sake of speed.


License
-------

Apache 2

Library Installation
--------------------

.. code-block:: bash

   $ pip install multidict

The library is Python 3 only!

PyPI contains binary wheels for Linux, Windows and MacOS.  If you want to install
``multidict`` on another operation system (or *Alpine Linux* inside a Docker) the
Tarball will be used to compile the library from sources.  It requires C compiler and
Python headers installed.

To skip the compilation please use `MULTIDICT_NO_EXTENSIONS` environment variable,
e.g.:

.. code-block:: bash

   $ MULTIDICT_NO_EXTENSIONS=1 pip install multidict

Please note, Pure Python (uncompiled) version is about 20-50 times slower depending on
the usage scenario!!!



Changelog
---------
See `RTD page <http://multidict.readthedocs.org/en/latest/changes.html>`_.
