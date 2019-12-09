Installation
============

.. warning::

    We advise installing virtualenv-1.9 or greater. Prior to version 1.9, the
    pip included in virtualenv did not download from PyPI over SSL.

.. warning::

    When using pip to install virtualenv, we advise using pip 1.3 or greater.
    Prior to version 1.3, pip did not download from PyPI over SSL.

.. warning::

    We advise against using easy_install to install virtualenv when using
    setuptools < 0.9.7, because easy_install didn't download from PyPI over SSL
    and was broken in some subtle ways.

In Windows, run the ``pip`` provided by your Python installation to install ``virtualenv``.

::

 > pip install virtualenv

In non-Windows systems it is discouraged to run ``pip`` as root including with ``sudo``.
Generally use your system package manager if it provides a package.
This avoids conflicts in versions and file locations between the system package manager and ``pip``.
See your distribution's package manager documentation for instructions on using it to install ``virtualenv``.

Using ``pip install --user`` is less hazardous but can still cause trouble within the particular user account.
If a system package expects the system provided ``virtualenv`` and an incompatible version is installed with ``--user`` that package may have problems within that user account.
To install within your user account with ``pip`` (if you have pip 1.3 or greater installed):

::

 $ pip install --user virtualenv

Note: The specific ``bin`` path may vary per distribution but is often ``~/.local/bin`` and must be added to your ``$PATH`` if not already present.

Or to get the latest unreleased dev version:

::

 $ pip install --user https://github.com/pypa/virtualenv/tarball/master


To install version ``X.X.X`` globally from source:

::

 $ pip install --user https://github.com/pypa/virtualenv/tarball/X.X.X

To *use* locally from source:

::

 $ curl --location --output virtualenv-X.X.X.tar.gz https://github.com/pypa/virtualenv/tarball/X.X.X
 $ tar xvfz virtualenv-X.X.X.tar.gz
 $ cd pypa-virtualenv-YYYYYY
 $ python virtualenv.py myVE

.. note::

    The ``virtualenv.py`` script is *not* supported if run without the
    necessary pip/setuptools/virtualenv distributions available locally. All
    of the installation methods above include a ``virtualenv_support``
    directory alongside ``virtualenv.py`` which contains a complete set of
    pip and setuptools distributions, and so are fully supported.
