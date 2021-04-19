v3.4.1
======

Refresh packaging.

v3.4.0
======

#68 and bpo-42090: ``Path.joinpath`` now takes arbitrary
positional arguments and no longer accepts ``add`` as a
keyword argument.

v3.3.2
======

Updated project metadata including badges.

v3.3.1
======

bpo-42043: Add tests capturing subclassing requirements.

v3.3.0
======

#9: ``Path`` objects now expose a ``.filename`` attribute
and rely on that to resolve ``.name`` and ``.parent`` when
the ``Path`` object is at the root of the zipfile.

v3.2.0
======

#57 and bpo-40564: Mutate the passed ZipFile object
type instead of making a copy. Prevents issues when
both the local copy and the caller's copy attempt to
close the same file handle.

#56 and bpo-41035: ``Path._next`` now honors
subclasses.

#55: ``Path.is_file()`` now returns False for non-existent names.

v3.1.0
======

#47: ``.open`` now raises ``FileNotFoundError`` and
``IsADirectoryError`` when appropriate.

v3.0.0
======

#44: Merge with v1.2.0.

v1.2.0
======

#44: ``zipp.Path.open()`` now supports a compatible signature
as ``pathlib.Path.open()``, accepting text (default) or binary
modes and soliciting keyword parameters passed through to
``io.TextIOWrapper`` (encoding, newline, etc). The stream is
opened in text-mode by default now. ``open`` no
longer accepts ``pwd`` as a positional argument and does not
accept the ``force_zip64`` parameter at all. This change is
a backward-incompatible change for that single function.

v2.2.1
======

#43: Merge with v1.1.1.

v1.1.1
======

#43: Restored performance of implicit dir computation.

v2.2.0
======

#36: Rebuild package with minimum Python version declared both
in package metadata and in the python tag.

v2.1.0
======

#32: Merge with v1.1.0.

v1.1.0
======

#32: For read-only zip files, complexity of ``.exists`` and
``joinpath`` is now constant time instead of ``O(n)``, preventing
quadratic time in common use-cases and rendering large
zip files unusable for Path. Big thanks to Benjy Weinberger
for the bug report and contributed fix (#33).

v2.0.1
======

#30: Corrected version inference (from jaraco/skeleton#12).

v2.0.0
======

Require Python 3.6 or later.

v1.0.0
======

Re-release of 0.6 to correspond with release as found in
Python 3.8.

v0.6.0
======

#12: When adding implicit dirs, ensure that ancestral directories
are added and that duplicates are excluded.

The library now relies on
`more_itertools <https://pypi.org/project/more_itertools>`_.

v0.5.2
======

#7: Parent of a directory now actually returns the parent.

v0.5.1
======

Declared package as backport.

v0.5.0
======

Add ``.joinpath()`` method and ``.parent`` property.

Now a backport release of the ``zipfile.Path`` class.

v0.4.0
======

#4: Add support for zip files with implied directories.

v0.3.3
======

#3: Fix issue where ``.name`` on a directory was empty.

v0.3.2
======

#2: Fix TypeError on Python 2.7 when classic division is used.

v0.3.1
======

#1: Fix TypeError on Python 3.5 when joining to a path-like object.

v0.3.0
======

Add support for constructing a ``zipp.Path`` from any path-like
object.

``zipp.Path`` is now a new-style class on Python 2.7.

v0.2.1
======

Fix issue with ``__str__``.

v0.2.0
======

Drop reliance on future-fstrings.

v0.1.0
======

Initial release with basic functionality.
