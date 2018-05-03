History
-------

Version 2.1.0
^^^^^^^^^^^^^

- Sync with upstream pathlib from CPython: gethomedir, home,
  expanduser.

Version 2.0.1
^^^^^^^^^^^^^

- Fix TypeError exceptions in write_bytes and write_text (contributed
  by Emanuele Gaifas, see pull request #2).

Version 2.0
^^^^^^^^^^^

- Sync with upstream pathlib from CPython: read_text, write_text,
  read_bytes, write_bytes, __enter__, __exit__, samefile.
- Use travis and appveyor for continuous integration.
- Fixed some bugs in test code.

Version 1.0.1
^^^^^^^^^^^^^

- Pull request #4: Python 2.6 compatibility by eevee.

Version 1.0
^^^^^^^^^^^

This version brings ``pathlib`` up to date with the official Python 3.4
release, and also fixes a couple of 2.7-specific issues.

- Python issue #20765: Add missing documentation for PurePath.with_name()
  and PurePath.with_suffix().
- Fix test_mkdir_parents when the working directory has additional bits
  set (such as the setgid or sticky bits).
- Python issue #20111: pathlib.Path.with_suffix() now sanity checks the
  given suffix.
- Python issue #19918: Fix PurePath.relative_to() under Windows.
- Python issue #19921: When Path.mkdir() is called with parents=True, any
  missing parent is created with the default permissions, ignoring the mode
  argument (mimicking the POSIX "mkdir -p" command).
- Python issue #19887: Improve the Path.resolve() algorithm to support
  certain symlink chains.
- Make pathlib usable under Python 2.7 with unicode pathnames (only pure
  ASCII, though).
- Issue #21: fix TypeError under Python 2.7 when using new division.
- Add tox support for easier testing.

Version 0.97
^^^^^^^^^^^^

This version brings ``pathlib`` up to date with the final API specified
in :pep:`428`.  The changes are too long to list here, it is recommended
to read the `documentation <https://pathlib.readthedocs.org/>`_.

.. warning::
   The API in this version is partially incompatible with pathlib 0.8 and
   earlier.  Be sure to check your code for possible breakage!

Version 0.8
^^^^^^^^^^^

- Add PurePath.name and PurePath.anchor.
- Add Path.owner and Path.group.
- Add Path.replace().
- Add Path.as_uri().
- Issue #10: when creating a file with Path.open(), don't set the executable
  bit.
- Issue #11: fix comparisons with non-Path objects.

Version 0.7
^^^^^^^^^^^

- Add '**' (recursive) patterns to Path.glob().
- Fix openat() support after the API refactoring in Python 3.3 beta1.
- Add a *target_is_directory* argument to Path.symlink_to()

Version 0.6
^^^^^^^^^^^

- Add Path.is_file() and Path.is_symlink()
- Add Path.glob() and Path.rglob()
- Add PurePath.match()

Version 0.5
^^^^^^^^^^^

- Add Path.mkdir().
- Add Python 2.7 compatibility by Michele Lacchia.
- Make parent() raise ValueError when the level is greater than the path
  length.
