Development
===========

Contributing
------------

Refer to the `pip development`_ documentation - it applies equally to
virtualenv, except that virtualenv issues should filed on the `virtualenv
repo`_ at GitHub.

Virtualenv's release schedule is tied to pip's -- each time there's a new pip
release, there will be a new virtualenv release that bundles the new version of
pip.

Files in the `virtualenv_embedded/` subdirectory are embedded into
`virtualenv.py` itself as base64-encoded strings (in order to support
single-file use of `virtualenv.py` without installing it). If your patch
changes any file in `virtualenv_embedded/`, run `bin/rebuild-script.py` to
update the embedded version of that file in `virtualenv.py`; commit that and
submit it as part of your patch / pull request.

.. _pip development: https://pip.pypa.io/en/latest/development/
.. _virtualenv repo: https://github.com/pypa/virtualenv/

Running the tests
-----------------

Virtualenv's test suite is small and not yet at all comprehensive, but we aim
to grow it.

The easy way to run tests (handles test dependencies automatically)::

    $ python setup.py test

If you want to run only a selection of the tests, you'll need to run them
directly with pytest instead. Create a virtualenv, and install required
packages::

    $ pip install pytest mock

Run pytest::

    $ pytest

Or select just a single test file to run::

    $ pytest tests/test_virtualenv

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
