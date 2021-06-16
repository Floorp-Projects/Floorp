The `old pathlib <https://bitbucket.org/pitrou/pathlib>`_
module on bitbucket is in bugfix-only mode.
The goal of pathlib2 is to provide a backport of
`standard pathlib <http://docs.python.org/dev/library/pathlib.html>`_
module which tracks the standard library module,
so all the newest features of the standard pathlib can be
used also on older Python versions.

Download
--------

Standalone releases are available on PyPI:
http://pypi.python.org/pypi/pathlib2/

Development
-----------

The main development takes place in the Python standard library: see
the `Python developer's guide <http://docs.python.org/devguide/>`_.
In particular, new features should be submitted to the
`Python bug tracker <http://bugs.python.org/>`_.

Issues that occur in this backport, but that do not occur not in the
standard Python pathlib module can be submitted on
the `pathlib2 bug tracker <https://github.com/mcmtroffaes/pathlib2/issues>`_.

Documentation
-------------

Refer to the
`standard pathlib <http://docs.python.org/dev/library/pathlib.html>`_
documentation.

.. |travis| image:: https://travis-ci.org/mcmtroffaes/pathlib2.png?branch=develop
    :target: https://travis-ci.org/mcmtroffaes/pathlib2
    :alt: travis-ci

.. |appveyor| image:: https://ci.appveyor.com/api/projects/status/baddx3rpet2wyi2c?svg=true
    :target: https://ci.appveyor.com/project/mcmtroffaes/pathlib2
    :alt: appveyor

.. |codecov| image:: https://codecov.io/gh/mcmtroffaes/pathlib2/branch/develop/graph/badge.svg
    :target: https://codecov.io/gh/mcmtroffaes/pathlib2
    :alt: codecov



