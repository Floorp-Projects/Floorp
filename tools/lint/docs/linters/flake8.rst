Flake8
======

`Flake8`_ is a popular lint wrapper for python. Under the hood, it runs three other tools and
combines their results:

* `pep8`_ for checking style
* `pyflakes`_ for checking syntax
* `mccabe`_ for checking complexity


Run Locally
-----------

The mozlint integration of flake8 can be run using mach:

.. parsed-literal::

    $ mach lint --linter flake8 <file paths>

Alternatively, omit the ``--linter flake8`` and run all configured linters, which will include
flake8.


Configuration
-------------

Only directories explicitly whitelisted will have flake8 run against them. To enable flake8 linting
in a source directory, it must be added to the ``include`` directive in ```tools/lint/flake8.lint``.
If you wish to exclude a subdirectory of an included one, you can add it to the ``exclude``
directive.

The default configuration file lives in ``topsrcdir/.flake8``. The default configuration can be
overriden for a given subdirectory by creating a new ``.flake8`` file in the subdirectory. Be warned
that ``.flake8`` files cannot inherit from one another, so all configuration you wish to keep must
be re-defined.

.. warning::

    Only ``.flake8`` files that live in a directory that is explicitly included in the ``include``
    directive will be considered. See `bug 1277851`_ for more details.

For an overview of the supported configuration, see `flake8's documentation`_.

.. _Flake8: https://flake8.readthedocs.io/en/latest/
.. _pep8: http://pep8.readthedocs.io/en/latest/
.. _pyflakes: https://github.com/pyflakes/pyflakes
.. _mccabe: https://github.com/pycqa/mccabe
.. _bug 1277851: https://bugzilla.mozilla.org/show_bug.cgi?id=1277851
.. _flake8's documentation: https://flake8.readthedocs.io/en/latest/config.html
