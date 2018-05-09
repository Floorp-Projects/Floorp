=================================
Using third-party Python packages
=================================

When using third-party Python packages, there are two options:

#. Install/use a vendored version of the package.
#. Install the package from a package index, such as PyPI or our internal
   mirror.

Vendoring Python packages
=========================

If the Python package is to be used in the building of Firefox itself, then we
**MUST** use a vendored version. This ensures that to build Firefox we only
require a checkout of the source, and do not depend on a package index. This
ensures that building Firefox is deterministic and dependable, avoids packages
from changing out from under us, and means we’re not affected when 3rd party
services are offline. We don't want a DoS against PyPI or a random package
maintainer removing an old tarball to delay a Firefox chemspill.

Where possible, the following policy applies to **ALL** vendored packages:

* Vendored libraries **SHOULD NOT** be modified except as required to
  successfully vendor them.
* Vendored libraries **SHOULD** be released copies of libraries available on
  PyPI.


Adding a Python package
~~~~~~~~~~~~~~~~~~~~~~~

To vendor a Python package, run ``mach vendor python [PACKAGE]``, where
``[PACKAGE]`` is one or more package names along with a version number in the
format ``pytest==3.5.1``. The package will be installed, transient dependencies
will be determined, and a ``requirements.txt`` file will be generated with the
full list of dependencies. The requirements file is then used with ``pip`` to
download and extract the source distributions of all packages into the
``third_party/python`` directory.

If you're familiar with ``Pipfile`` you can also directly modify this in the in
the top source directory and then run ``mach vendor python`` for your changes
to take effect. This allows advanced options such as specifying alternative
package indexes (see below), and
`PEP 508 specifiers <https://www.python.org/dev/peps/pep-0508/>`_.

Note that the `specification <https://github.com/pypa/pipfile>`_ for
``Pipfile`` and ``Pipfile.lock`` is still in active development. More
information can be found in the
`Pipenv documentation <https://docs.pipenv.org/>`_, which is the reference
implementation we're using.

What if the package isn't on PyPI?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If the package is available on another Python package index, then you can add
these details to ``Pipfile`` by
`specifying package indexes <https://docs.pipenv.org/advanced/#specifying-package-indexes>`_.
If the package isn't available on any Python package index, then you can
manually copy the source distribution into the ``third_party/python`` directory.

Using a Python package index
============================

If the Python package is not used in the building of Firefox then it can be
installed from a package index. Some tasks are not permitted to use external
resources, and for those we can publish packages to an internal PyPI mirror.
See `how to upload to internal PyPI <https://wiki.mozilla.org/ReleaseEngineering/How_To/Upload_to_internal_Pypi>`_
for more details. If you are not restricted, you can install packages from PyPI
or another package index.

All packages installed from a package index **MUST** specify hashes to ensure
compatibility and protect against remote tampering. Hash-checking mode can be
forced on when using ``pip`` be specifying the ``--require-hashes``
command-line option. See `hash-checking mode <https://pip.pypa.io/en/stable/reference/pip_install/#hash-checking-mode>`_ for
more details.

Note that when using a Python package index there is a risk that the service
could be unavailable, or packages may be updated or even pulled without notice.
These issues are less likely with our internal PyPI mirror, but still possible.
If this is undesirable, then consider vendoring the package.
