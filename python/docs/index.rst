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

Using a Python package index
============================

If the Python package is not used in the building of Firefox then it can be
installed from a package index. Some tasks are not permitted to use external
resources, and for those we can publish packages to an internal PyPI mirror.
See `how to upload to internal PyPI <https://wiki.mozilla.org/ReleaseEngineering/How_To/Upload_to_internal_Pypi>`_
for more details. If you are not restricted, you can install packages from PyPI
or another package index.

All packages installed from a package index **MUST** specify hashes to ensure
compatibiliy and protect against remote tampering. Hash-checking mode can be
forced on when using ``pip`` be specifying the ``--require-hashes``
command-line option. See `hash-checking mode <https://pip.pypa.io/en/stable/reference/pip_install/#hash-checking-mode>`_ for
more details.

Note that when using a Python package index there is a risk that the service
could be unavailable, or packages may be updated or even pulled without notice.
These issues are less likely with our internal PyPI mirror, but still possible.
If this is undesirable, then consider vendoring the package.
