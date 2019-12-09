Development
===========

Contributing
------------

Refer to the `pip development`_ documentation - it applies equally to
virtualenv, except that virtualenv issues should be filed on the `virtualenv
repo`_ at GitHub.

Virtualenv's release schedule is tied to pip's -- each time there's a new pip
release, there will be a new virtualenv release that bundles the new version of
pip.

Files in the ``virtualenv_embedded/`` subdirectory are embedded into
``virtualenv.py`` itself as base64-encoded strings (in order to support
single-file use of ``virtualenv.py`` without installing it). If your patch
changes any file in ``virtualenv_embedded/``, run ``tox -e embed`` to update
the embedded version of that file in ``virtualenv.py``; commit that and submit
it as part of your patch / pull request. The tox run will report failure
when changes are embedded, as a flag for CI.

The codebase should be linted before a pull request is merged by running
``tox -e fix_lint``. The tox run will report failure when any linting
revisions are required, as a flag for CI.

.. _pip development: https://pip.pypa.io/en/latest/development/
.. _virtualenv repo: https://github.com/pypa/virtualenv/

Running the tests
-----------------

The easy way to run tests (handles test dependencies automatically, works with the ``sdist`` too)::

    $ tox

    Note you need to first install tox separately by using::

   $ python -m pip --user install -U tox

Run ``python -m tox -av`` for a list of all supported Python environments or just run the
tests in all of the available ones by running just ``tox``.

Status and License
------------------

``virtualenv`` is a successor to `workingenv
<http://cheeseshop.python.org/pypi/workingenv.py>`_, and an extension
of `virtual-python
<http://peak.telecommunity.com/DevCenter/EasyInstall#creating-a-virtual-python>`_.

It was written by Ian Bicking, sponsored by the `Open Planning
Project <http://openplans.org>`_ and is now maintained by a
`group of developers <https://github.com/pypa/virtualenv/raw/master/AUTHORS.txt>`_.
It is licensed under an
`MIT-style permissive license <https://github.com/pypa/virtualenv/raw/master/LICENSE.txt>`_.
