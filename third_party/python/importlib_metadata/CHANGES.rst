v3.10.1
=======

* Minor tweaks from CPython.

v3.10.0
=======

* #295: Internal refactoring to unify section parsing logic.

v3.9.1
======

* #296: Exclude 'prepare' package.
* #297: Fix ValueError when entry points contains comments.

v3.9.0
======

* Use of Mapping (dict) interfaces on ``SelectableGroups``
  is now flagged as deprecated. Instead, users are advised
  to use the select interface for future compatibility.

  Suppress the warning with this filter:
  ``ignore:SelectableGroups dict interface``.

  Or with this invocation in the Python environment:
  ``warnings.filterwarnings('ignore', 'SelectableGroups dict interface')``.

  Preferably, switch to the ``select`` interface introduced
  in 3.7.0. See the
  `entry points documentation <https://importlib-metadata.readthedocs.io/en/latest/using.html#entry-points>`_ and changelog for the 3.6
  release below for more detail.

  For some use-cases, especially those that rely on
  ``importlib.metadata`` in Python 3.8 and 3.9 or
  those relying on older ``importlib_metadata`` (especially
  on Python 3.5 and earlier),
  `backports.entry_points_selectable <https://pypi.org/project/backports.entry_points_selectable>`_
  was created to ease the transition. Please have a look
  at that project if simply relying on importlib_metadata 3.6+
  is not straightforward. Background in #298.

* #283: Entry point parsing no longer relies on ConfigParser
  and instead uses a custom, one-pass parser to load the
  config, resulting in a ~20% performance improvement when
  loading entry points.

v3.8.2
======

* #293: Re-enabled lazy evaluation of path lookup through
  a FreezableDefaultDict.

v3.8.1
======

* #293: Workaround for error in distribution search.

v3.8.0
======

* #290: Add mtime-based caching for ``FastPath`` and its
  lookups, dramatically increasing performance for repeated
  distribution lookups.

v3.7.3
======

* Docs enhancements and cleanup following review in
  `GH-24782 <https://github.com/python/cpython/pull/24782>`_.

v3.7.2
======

* Cleaned up cruft in entry_points docstring.

v3.7.1
======

* Internal refactoring to facilitate ``entry_points() -> dict``
  deprecation.

v3.7.0
======

* #131: Added ``packages_distributions`` to conveniently
  resolve a top-level package or module to its distribution(s).

v3.6.0
======

* #284: Introduces new ``EntryPoints`` object, a tuple of
  ``EntryPoint`` objects but with convenience properties for
  selecting and inspecting the results:

  - ``.select()`` accepts ``group`` or ``name`` keyword
    parameters and returns a new ``EntryPoints`` tuple
    with only those that match the selection.
  - ``.groups`` property presents all of the group names.
  - ``.names`` property presents the names of the entry points.
  - Item access (e.g. ``eps[name]``) retrieves a single
    entry point by name.

  ``entry_points`` now accepts "selection parameters",
  same as ``EntryPoint.select()``.

  ``entry_points()`` now provides a future-compatible
  ``SelectableGroups`` object that supplies the above interface
  (except item access) but remains a dict for compatibility.

  In the future, ``entry_points()`` will return an
  ``EntryPoints`` object for all entry points.

  If passing selection parameters to ``entry_points``, the
  future behavior is invoked and an ``EntryPoints`` is the
  result.

* #284: Construction of entry points using
  ``dict([EntryPoint, ...])`` is now deprecated and raises
  an appropriate DeprecationWarning and will be removed in
  a future version.

* #300: ``Distribution.entry_points`` now presents as an
  ``EntryPoints`` object and access by index is no longer
  allowed. If access by index is required, cast to a sequence
  first.

v3.5.0
======

* #280: ``entry_points`` now only returns entry points for
  unique distributions (by name).

v3.4.0
======

* #10: Project now declares itself as being typed.
* #272: Additional performance enhancements to distribution
  discovery.
* #111: For PyPA projects, add test ensuring that
  ``MetadataPathFinder._search_paths`` honors the needed
  interface. Method is still private.

v3.3.0
======

* #265: ``EntryPoint`` objects now expose a ``.dist`` object
  referencing the ``Distribution`` when constructed from a
  Distribution.

v3.2.0
======

* The object returned by ``metadata()`` now has a
  formally-defined protocol called ``PackageMetadata``
  with declared support for the ``.get_all()`` method.
  Fixes #126.

v3.1.1
======

v2.1.1
======

* #261: Restored compatibility for package discovery for
  metadata without version in the name and for legacy
  eggs.

v3.1.0
======

* Merge with 2.1.0.

v2.1.0
======

* #253: When querying for package metadata, the lookup
  now honors
  `package normalization rules <https://packaging.python.org/specifications/recording-installed-packages/>`_.

v3.0.0
======

* Require Python 3.6 or later.

v2.0.0
======

* ``importlib_metadata`` no longer presents a
  ``__version__`` attribute. Consumers wishing to
  resolve the version of the package should query it
  directly with
  ``importlib_metadata.version('importlib-metadata')``.
  Closes #71.

v1.7.0
======

* ``PathNotFoundError`` now has a custom ``__str__``
  mentioning "package metadata" being missing to help
  guide users to the cause when the package is installed
  but no metadata is present. Closes #124.

v1.6.1
======

* Added ``Distribution._local()`` as a provisional
  demonstration of how to load metadata for a local
  package. Implicitly requires that
  `pep517 <https://pypi.org/project/pep517>`_ is
  installed. Ref #42.
* Ensure inputs to FastPath are Unicode. Closes #121.
* Tests now rely on ``importlib.resources.files`` (and
  backport) instead of the older ``path`` function.
* Support any iterable from ``find_distributions``.
  Closes #122.

v1.6.0
======

* Added ``module`` and ``attr`` attributes to ``EntryPoint``

v1.5.2
======

* Fix redundant entries from ``FastPath.zip_children``.
  Closes #117.

v1.5.1
======

* Improve reliability and consistency of compatibility
  imports for contextlib and pathlib when running tests.
  Closes #116.

v1.5.0
======

* Additional performance optimizations in FastPath now
  saves an additional 20% on a typical call.
* Correct for issue where PyOxidizer finder has no
  ``__module__`` attribute. Closes #110.

v1.4.0
======

* Through careful optimization, ``distribution()`` is
  3-4x faster. Thanks to Antony Lee for the
  contribution. Closes #95.

* When searching through ``sys.path``, if any error
  occurs attempting to list a path entry, that entry
  is skipped, making the system much more lenient
  to errors. Closes #94.

v1.3.0
======

* Improve custom finders documentation. Closes #105.

v1.2.0
======

* Once again, drop support for Python 3.4. Ref #104.

v1.1.3
======

* Restored support for Python 3.4 due to improper version
  compatibility declarations in the v1.1.0 and v1.1.1
  releases. Closes #104.

v1.1.2
======

* Repaired project metadata to correctly declare the
  ``python_requires`` directive. Closes #103.

v1.1.1
======

* Fixed ``repr(EntryPoint)`` on PyPy 3 also. Closes #102.

v1.1.0
======

* Dropped support for Python 3.4.
* EntryPoints are now pickleable. Closes #96.
* Fixed ``repr(EntryPoint)`` on PyPy 2. Closes #97.

v1.0.0
======

* Project adopts semver for versioning.

* Removed compatibility shim introduced in 0.23.

* For better compatibility with the stdlib implementation and to
  avoid the same distributions being discovered by the stdlib and
  backport implementations, the backport now disables the
  stdlib DistributionFinder during initialization (import time).
  Closes #91 and closes #100.

0.23
====

* Added a compatibility shim to prevent failures on beta releases
  of Python before the signature changed to accept the
  "context" parameter on find_distributions. This workaround
  will have a limited lifespan, not to extend beyond release of
  Python 3.8 final.

0.22
====

* Renamed ``package`` parameter to ``distribution_name``
  as `recommended <https://bugs.python.org/issue34632#msg349423>`_
  in the following functions: ``distribution``, ``metadata``,
  ``version``, ``files``, and ``requires``. This
  backward-incompatible change is expected to have little impact
  as these functions are assumed to be primarily used with
  positional parameters.

0.21
====

* ``importlib.metadata`` now exposes the ``DistributionFinder``
  metaclass and references it in the docs for extending the
  search algorithm.
* Add ``Distribution.at`` for constructing a Distribution object
  from a known metadata directory on the file system. Closes #80.
* Distribution finders now receive a context object that
  supplies ``.path`` and ``.name`` properties. This change
  introduces a fundamental backward incompatibility for
  any projects implementing a ``find_distributions`` method
  on a ``MetaPathFinder``. This new layer of abstraction
  allows this context to be supplied directly or constructed
  on demand and opens the opportunity for a
  ``find_distributions`` method to solicit additional
  context from the caller. Closes #85.

0.20
====

* Clarify in the docs that calls to ``.files`` could return
  ``None`` when the metadata is not present. Closes #69.
* Return all requirements and not just the first for dist-info
  packages. Closes #67.

0.19
====

* Restrain over-eager egg metadata resolution.
* Add support for entry points with colons in the name. Closes #75.

0.18
====

* Parse entry points case sensitively.  Closes #68
* Add a version constraint on the backport configparser package.  Closes #66

0.17
====

* Fix a permission problem in the tests on Windows.

0.16
====

* Don't crash if there exists an EGG-INFO directory on sys.path.

0.15
====

* Fix documentation.

0.14
====

* Removed ``local_distribution`` function from the API.
  **This backward-incompatible change removes this
  behavior summarily**. Projects should remove their
  reliance on this behavior. A replacement behavior is
  under review in the `pep517 project
  <https://github.com/pypa/pep517>`_. Closes #42.

0.13
====

* Update docstrings to match PEP 8. Closes #63.
* Merged modules into one module. Closes #62.

0.12
====

* Add support for eggs.  !65; Closes #19.

0.11
====

* Support generic zip files (not just wheels).  Closes #59
* Support zip files with multiple distributions in them.  Closes #60
* Fully expose the public API in ``importlib_metadata.__all__``.

0.10
====

* The ``Distribution`` ABC is now officially part of the public API.
  Closes #37.
* Fixed support for older single file egg-info formats.  Closes #43.
* Fixed a testing bug when ``$CWD`` has spaces in the path.  Closes #50.
* Add Python 3.8 to the ``tox`` testing matrix.

0.9
===

* Fixed issue where entry points without an attribute would raise an
  Exception.  Closes #40.
* Removed unused ``name`` parameter from ``entry_points()``. Closes #44.
* ``DistributionFinder`` classes must now be instantiated before
  being placed on ``sys.meta_path``.

0.8
===

* This library can now discover/enumerate all installed packages. **This
  backward-incompatible change alters the protocol finders must
  implement to support distribution package discovery.** Closes #24.
* The signature of ``find_distributions()`` on custom installer finders
  should now accept two parameters, ``name`` and ``path`` and
  these parameters must supply defaults.
* The ``entry_points()`` method no longer accepts a package name
  but instead returns all entry points in a dictionary keyed by the
  ``EntryPoint.group``. The ``resolve`` method has been removed. Instead,
  call ``EntryPoint.load()``, which has the same semantics as
  ``pkg_resources`` and ``entrypoints``.  **This is a backward incompatible
  change.**
* Metadata is now always returned as Unicode text regardless of
  Python version. Closes #29.
* This library can now discover metadata for a 'local' package (found
  in the current-working directory). Closes #27.
* Added ``files()`` function for resolving files from a distribution.
* Added a new ``requires()`` function, which returns the requirements
  for a package suitable for parsing by
  ``packaging.requirements.Requirement``. Closes #18.
* The top-level ``read_text()`` function has been removed.  Use
  ``PackagePath.read_text()`` on instances returned by the ``files()``
  function.  **This is a backward incompatible change.**
* Release dates are now automatically injected into the changelog
  based on SCM tags.

0.7
===

* Fixed issue where packages with dashes in their names would
  not be discovered. Closes #21.
* Distribution lookup is now case-insensitive. Closes #20.
* Wheel distributions can no longer be discovered by their module
  name. Like Path distributions, they must be indicated by their
  distribution package name.

0.6
===

* Removed ``importlib_metadata.distribution`` function. Now
  the public interface is primarily the utility functions exposed
  in ``importlib_metadata.__all__``. Closes #14.
* Added two new utility functions ``read_text`` and
  ``metadata``.

0.5
===

* Updated README and removed details about Distribution
  class, now considered private. Closes #15.
* Added test suite support for Python 3.4+.
* Fixed SyntaxErrors on Python 3.4 and 3.5. !12
* Fixed errors on Windows joining Path elements. !15

0.4
===

* Housekeeping.

0.3
===

* Added usage documentation.  Closes #8
* Add support for getting metadata from wheels on ``sys.path``.  Closes #9

0.2
===

* Added ``importlib_metadata.entry_points()``.  Closes #1
* Added ``importlib_metadata.resolve()``.  Closes #12
* Add support for Python 2.7.  Closes #4

0.1
===

* Initial release.


..
   Local Variables:
   mode: change-log-mode
   indent-tabs-mode: nil
   sentence-end-double-space: t
   fill-column: 78
   coding: utf-8
   End:
