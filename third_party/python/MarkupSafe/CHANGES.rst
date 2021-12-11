Version 1.1.1
-------------

Released 2019-02-23

-   Fix segfault when ``__html__`` method raises an exception when using
    the C speedups. The exception is now propagated correctly. (`#109`_)

.. _#109: https://github.com/pallets/markupsafe/pull/109


Version 1.1.0
-------------

Released 2018-11-05

-   Drop support for Python 2.6 and 3.3.
-   Build wheels for Linux, Mac, and Windows, allowing systems without
    a compiler to take advantage of the C extension speedups. (`#104`_)
-   Use newer CPython API on Python 3, resulting in a 1.5x speedup.
    (`#64`_)
-   ``escape`` wraps ``__html__`` result in ``Markup``, consistent with
    documented behavior. (`#69`_)

.. _#64: https://github.com/pallets/markupsafe/pull/64
.. _#69: https://github.com/pallets/markupsafe/pull/69
.. _#104: https://github.com/pallets/markupsafe/pull/104


Version 1.0
-----------

Released 2017-03-07

-   Fixed custom types not invoking ``__unicode__`` when used with
    ``format()``.
-   Added ``__version__`` module attribute.
-   Improve unescape code to leave lone ampersands alone.


Version 0.18
------------

Released 2013-05-22

-   Fixed ``__mul__`` and string splitting on Python 3.


Version 0.17
------------

Released 2013-05-21

-   Fixed a bug with broken interpolation on tuples.


Version 0.16
------------

Released 2013-05-20

-   Improved Python 3 Support and removed 2to3.
-   Removed support for Python 3.2 and 2.5.


Version 0.15
------------

Released 2011-07-20

-   Fixed a typo that caused the library to fail to install on pypy and
    jython.


Version 0.14
------------

Released 2011-07-20

-   Release fix for 0.13.


Version 0.13
------------

Released 2011-07-20

-   Do not attempt to compile extension for PyPy or Jython.
-   Work around some 64bit Windows issues.


Version 0.12
------------

Released 2011-02-17

-   Improved PyPy compatibility.
