=================================
Using third-party Python packages
=================================

Mach and its associated commands have a variety of 3rd-party Python dependencies. Many of these
are vendored in ``third_party/python``, while others are installed at runtime via ``pip``.
Dependencies with "native code" are handled on a machine-by-machine basis.

The dependencies of Mach itself can be found at ``build/mach_virtualenv_packages.txt``. Mach commands
may have additional dependencies which are specified at ``build/<site>_virtualenv_packages.txt``.

For example, the following Mach command would have its 3rd-party dependencies declared at
``build/foo_virtualenv_packages.txt``.

.. code:: python

    @Command(
        "foo-it",
        virtualenv_name="foo",
    )
    # ...
    def foo_it_command():
        command_context.activate_virtualenv()
        import specific_dependency

The format of ``<site>_virtualenv_requirements.txt`` files are documented further in the
:py:class:`~mach.requirements.MachEnvRequirements` class.

Adding a Python package
=======================

There's two ways of using 3rd-party Python dependencies:

* :ref:`pip install the packages <python-pip-install>`. Python dependencies with native code must
  be installed using ``pip``. This is the recommended technique for adding new Python dependencies.
* :ref:`Vendor the source of the Python package in-tree <python-vendor>`. Dependencies of the Mach
  core logic or of building Firefox itself must be vendored.

.. note::

    If encountering an ``ImportError``, even after following either of the above techniques,
    then the issue could be that the package is being imported too soon.
    Move the import to after ``.activate()``/``.activate_virtualenv()`` to resolve the issue.

    This will be fixed by `bug 1717104 <https://bugzilla.mozilla.org/show_bug.cgi?id=1717104>`__.

.. _python-pip-install:

``pip install`` the package
~~~~~~~~~~~~~~~~~~~~~~~~~~~

To add a ``pip install``-d package dependency, add it to your site's
``build/<site>_virtualenv_packages.txt`` manifest file:

.. code::

    ...
    pypi:new-package==<version>
    ...

.. note::

    Some tasks are not permitted to use external resources, and for those we can
    publish packages to an internal PyPI mirror.
    See `how to upload to internal PyPI <https://wiki.mozilla.org/ReleaseEngineering/How_To/Upload_to_internal_Pypi>`_
    for more details.

.. _python-vendor:

Vendoring Python packages
~~~~~~~~~~~~~~~~~~~~~~~~~

To vendor a Python package, add it to ``third_party/python/requirements.in``
and then run ``mach vendor python``. This will update the tree of pinned
dependencies in ``third_party/python/requirements.txt`` and download them all
into the ``third_party/python`` directory.

Next, add that package and any new transitive dependencies (you'll see them added in
``third_party/python/requirements.txt``) to the associated site's dependency manifest in
``build/<site>_virtualenv_packages.txt``:

.. code::

    ...
    vendored:third_party/python/new-package
    vendored:third_party/python/new-package-dependency-foo
    vendored:third_party/python/new-package-dependency-bar
    ...

.. note::

    The following policy applies to **ALL** vendored packages:

    * Vendored PyPI libraries **MUST NOT** be modified
    * Vendored libraries **SHOULD** be released copies of libraries available on
      PyPI.

      * When considering manually vendoring a package, discuss the situation with
        the ``#build`` team to ensure that other, more maintainable options are exhausted.

    * ``mach vendor python`` **MUST** be run on Linux with Python 3.6.
      This restriction will be lifted when
      `bug 1659593 <https://bugzilla.mozilla.org/show_bug.cgi?id=1659593>`_
      is resolved.

.. note::

    We require that it is possible to build Firefox using only a checkout of the source,
    without depending on a package index. This ensures that building Firefox is
    deterministic and dependable, avoids packages from changing out from under us,
    and means we’re not affected when 3rd party services are offline. We don't want a
    DoS against PyPI or a random package maintainer removing an old tarball to delay
    a Firefox chemspill. Therefore, packages required by Mach core logic or for building
    Firefox itself must be vendored.

Package compatibility
=====================

Mach requires that all commands' package requirements be compatible with those of Mach itself.
(This is because functions and state created by Mach are still usable from within the commands, and
they may still need access to their associated 3rd-party modules).

However, it is OK for Mach commands to have package requirements which are incompatible with each
other. This allows the flexibility for some Mach commands to depend on modern dependencies while
other, more mature commands may still only be compatible with a much older version.

.. note::

    Only one version of a package may be vendored at any given time. If two Mach commands need to
    have conflicting packages, then at least one of them must ``pip install`` the package instead
    of vendoring.

    If a Mach command's dependency conflicts with a vendored package, and that vendored package
    isn't needed by Mach itself, then that vendored dependency should be moved from
    ``mach_virtualenv_packages.txt`` to its associated environment.
