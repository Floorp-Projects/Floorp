=====
jsmin
=====

JavaScript minifier.

Usage
=====

.. code:: python

 from jsmin import jsmin
 with open('myfile.js') as js_file:
     minified = jsmin(js_file.read())

You can run it as a commandline tool also::

  python -m jsmin myfile.js

As yet, ``jsmin`` makes no attempt to be compatible with
`ECMAScript 6 / ES.next / Harmony <http://wiki.ecmascript.org/doku.php?id=harmony:specification_drafts>`_.
If you're using it on Harmony code, though, you might find the ``quote_chars``
parameter useful:

.. code:: python

 from jsmin import jsmin
 with open('myfile.js') as js_file:
     minified = jsmin(js_file.read(), quote_chars="'\"`")


Where to get it
===============

* install the package `from pypi <https://pypi.python.org/pypi/jsmin/>`_
* get the latest release `from the stable branch on bitbucket <https://bitbucket.org/dcs/jsmin/branch/stable>`_
* get the development version `from the default branch on bitbucket <https://bitbucket.org/dcs/jsmin/branch/default>`_

Contributing
============

`Issues <https://bitbucket.org/dcs/jsmin/issues>`_ and `Pull requests <https://bitbucket.org/dcs/jsmin/pull-requests>`_
will be gratefully received on Bitbucket. Pull requests on github are great too, but the issue tracker lives on
bitbucket.

If possible, please make separate pull requests for tests and for code: tests will be committed on the stable branch
(which tracks the latest released version) while code will go to default by, erm, default.

Unless you request otherwise, your Bitbucket identity will be added to the contributor's list below; if you prefer a
different name feel free to add it in your pull request instead. (If you prefer not to be mentioned you'll have to let
the maintainer know somehow.)

Build/test status
=================

Both default and stable branches are tested with Travis: https://travis-ci.org/tikitu/jsmin

Stable (latest released version plus any new tests) is tested against CPython 2.6, 2.7, 3.2, and 3.3.
Currently:

.. image:: https://travis-ci.org/tikitu/jsmin.png?branch=ghstable

If stable is failing that means there's a new test that fails on *the latest released version on pypi*, with no fix yet
released.

Default (development version, might be ahead of latest released version) is tested against CPython 2.6, 2.7, 3.2, and
3.3. Currently:

.. image:: https://travis-ci.org/tikitu/jsmin.png?branch=master

If default is failing don't use it, but as long as stable is passing the pypi release should be ok.

Contributors (chronological commit order)
=========================================

* `Dave St.Germain <https://bitbucket.org/dcs>`_ (original author)
* `Hans weltar <https://bitbucket.org/hansweltar>`_
* `Tikitu de Jager <mailto:tikitu+jsmin@logophile.org>`_ (current maintainer)
* https://bitbucket.org/rennat
* `Nick Alexander <https://bitbucket.org/ncalexan>`_
