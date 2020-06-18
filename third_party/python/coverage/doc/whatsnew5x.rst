.. Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
.. For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

.. _whatsnew5x:

====================
Major changes in 5.0
====================

This is an overview of the changes in 5.0 since the last version of 4.5.x. This
is not a complete list of all changes. See the :ref:`complete change history
<changes>` for all the details.


Open Questions
--------------

- How to support SQL access to data?  The database schema has to be convenient
  and efficient for coverage.py's execution, which would naturally make it an
  internal implementation detail.  But the coverage data is now more complex,
  and SQL access could be a powerful way to access it, pointing toward a public
  guaranteed schema.  What's the right balance?


Backward Incompatibilities
--------------------------

- Python 2.6, 3.3 and 3.4 are no longer supported.

- The :class:`.CoverageData` interface is still public, but has changed.

- The data file is now created earlier than it used to be.  In some
  circumstances, you may need to use ``parallel=true`` to avoid multiple
  processes overwriting each others' data.

- When constructing a :class:`coverage.Coverage` object, `data_file` can be
  specified as None to prevent writing any data file at all.  In previous
  versions, an explicit `data_file=None` argument would use the default of
  ".coverage". Fixes :github:`871`.

- The ``[run] note`` setting has been deprecated. Using it will result in a
  warning, and the note will not be written to the data file.  The
  corresponding :class:`.CoverageData` methods have been removed.

- The deprecated `Reporter.file_reporters` property has been removed.

- The reporting methods used to permanently apply their arguments to the
  configuration of the Coverage object.  Now they no longer do.  The arguments
  affect the operation of the method, but do not persist.

- Many internal attributes and functions were changed. These were not part of
  the public supported API. If your code used them, it might now stop working.


New Features
------------

- Coverage.py can now record the context in which each line was executed. The
  contexts are stored in the data file and can be used to drill down into why a
  particular line was run.  Static contexts let you specify a label for an
  entire coverage run, for example to separate coverage for different operating
  systems or versions of Python.  Dynamic contexts can change during a single
  measurement run.  This can be used to record the names of the tests that
  executed each line.  See :ref:`contexts` for full information.

- Coverage's data storage has changed.  In version 4.x, .coverage files were
  basically JSON.  Now, they are SQLite databases.  The database schema is
  documented (:ref:`dbschema`), but might still be in flux.

- Data can now be "reported" in JSON format, for programmatic use, as requested
  in :github:`720`.  The new ``coverage json`` command writes raw and
  summarized data to a JSON file.  Thanks, Matt Bachmann.

- Configuration can now be read from `TOML`_ files.  This requires installing
  coverage.py with the ``[toml]`` extra.  The standard "pyproject.toml" file
  will be read automatically if no other configuration file is found, with
  settings in the ``[tool.coverage.]`` namespace.  Thanks to Frazer McLean for
  implementation and persistence.  Finishes :github:`664`.

- The HTML and textual reports now have a ``--skip-empty`` option that skips
  files with no statements, notably ``__init__.py`` files.  Thanks, Reya B.

- You can specify the command line to run your program with the ``[run]
  command_line`` configuration setting, as requested in :github:`695`.

- An experimental ``[run] relative_files`` setting tells coverage to store
  relative file names in the data file. This makes it easier to run tests in
  one (or many) environments, and then report in another.  It has not had much
  real-world testing, so it may change in incompatible ways in the future.

- Environment variable substitution in configuration files now supports two
  syntaxes for controlling the behavior of undefined variables: if ``VARNAME``
  is not defined, ``${VARNAME?}`` will raise an error, and ``${VARNAME-default
  value}`` will use "default value".

- The location of the configuration file can now be specified with a
  ``COVERAGE_RCFILE`` environment variable, as requested in `issue 650`_.

- A new warning (``already-imported``) is issued if measurable files have
  already been imported before coverage.py started measurement.  See
  :ref:`cmd_warnings` for more information.

- Error handling during reporting has changed slightly.  All reporting methods
  now behave the same.  The ``--ignore-errors`` option keeps errors from
  stopping the reporting, but files that couldn't parse as Python will always
  be reported as warnings.  As with other warnings, you can suppress them with
  the ``[run] disable_warnings`` configuration setting.

- Added the classmethod :meth:`.Coverage.current` to get the latest started
  Coverage instance.


.. _TOML: https://github.com/toml-lang/toml#readme
.. _issue 650: https://bitbucket.org/ned/coveragepy/issues/650/allow-setting-configuration-file-location


Bugs Fixed
----------

- The ``coverage run`` command has always adjusted the first entry in sys.path,
  to properly emulate how Python runs your program.  Now this adjustment is
  skipped if sys.path[0] is already different than Python's default.  This
  fixes :github:`715`.

- Python files run with ``-m`` now have ``__spec__`` defined properly.  This
  fixes :github:`745` (about not being able to run unittest tests that spawn
  subprocesses), and :github:`838`, which described the problem directly.

- Coverage will create directories as needed for the data file if they don't
  exist, closing :github:`721`.

- ``fail_under`` values more than 100 are reported as errors.  Thanks to Mike
  Fiedler for closing :github:`746`.

- The "missing" values in the text output are now sorted by line number, so
  that missing branches are reported near the other lines they affect. The
  values used to show all missing lines, and then all missing branches.

- Coverage.py no longer fails if the user program deletes its current
  directory. Fixes :github:`806`.  Thanks, Dan Hemberger.
