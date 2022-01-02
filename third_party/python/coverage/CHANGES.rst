.. Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
.. For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

==============================
Change history for coverage.py
==============================

These changes are listed in decreasing version number order. Note this can be
different from a strict chronological order when there are two branches in
development at the same time, such as 4.5.x and 5.0.

This list is detailed and covers changes in each pre-release version.  If you
want to know what's different in 5.0 since 4.5.x, see :ref:`whatsnew5x`.


    .. When updating the "Unreleased" header to a specific version, use this
    .. format.  Don't forget the jump target:
    ..
    ..  .. _changes_981:
    ..
    ..  Version 9.8.1 --- 2027-07-27
    ..  ----------------------------


.. _changes_51:

Version 5.1 --- 2020-04-12
--------------------------

- The JSON report now includes counts of covered and missing branches. Thanks,
  Salvatore Zagaria.

- On Python 3.8, try-finally-return reported wrong branch coverage with
  decorated async functions (`issue 946`_).  This is now fixed. Thanks, Kjell
  Braden.

- The :meth:`~coverage.Coverage.get_option` and
  :meth:`~coverage.Coverage.set_option` methods can now manipulate the
  ``[paths]`` configuration setting.  Thanks to Bernát Gábor for the fix for
  `issue 967`_.

.. _issue 946: https://github.com/nedbat/coveragepy/issues/946
.. _issue 967: https://github.com/nedbat/coveragepy/issues/967


.. _changes_504:

Version 5.0.4 --- 2020-03-16
----------------------------

- If using the ``[run] relative_files`` setting, the XML report will use
  relative files in the ``<source>`` elements indicating the location of source
  code.  Closes `issue 948`_.

- The textual summary report could report missing lines with negative line
  numbers on PyPy3 7.1 (`issue 943`_).  This is now fixed.

- Windows wheels for Python 3.8 were incorrectly built, but are now fixed.
  (`issue 949`_)

- Updated Python 3.9 support to 3.9a4.

- HTML reports couldn't be sorted if localStorage wasn't available. This is now
  fixed: sorting works even though the sorting setting isn't retained. (`issue
  944`_ and `pull request 945`_). Thanks, Abdeali Kothari.

.. _issue 943: https://github.com/nedbat/coveragepy/issues/943
.. _issue 944: https://github.com/nedbat/coveragepy/issues/944
.. _pull request 945: https://github.com/nedbat/coveragepy/pull/945
.. _issue 948: https://github.com/nedbat/coveragepy/issues/948
.. _issue 949: https://github.com/nedbat/coveragepy/issues/949


.. _changes_503:

Version 5.0.3 --- 2020-01-12
----------------------------

- A performance improvement in 5.0.2 didn't work for test suites that changed
  directory before combining data, causing "Couldn't use data file: no such
  table: meta" errors (`issue 916`_).  This is now fixed.

- Coverage could fail to run your program with some form of "ModuleNotFound" or
  "ImportError" trying to import from the current directory. This would happen
  if coverage had been packaged into a zip file (for example, on Windows), or
  was found indirectly (for example, by pyenv-virtualenv).  A number of
  different scenarios were described in `issue 862`_ which is now fixed.  Huge
  thanks to Agbonze O. Jeremiah for reporting it, and Alexander Waters and
  George-Cristian Bîrzan for protracted debugging sessions.

- Added the "premain" debug option.

- Added SQLite compile-time options to the "debug sys" output.

.. _issue 862: https://github.com/nedbat/coveragepy/issues/862
.. _issue 916: https://github.com/nedbat/coveragepy/issues/916


.. _changes_502:

Version 5.0.2 --- 2020-01-05
----------------------------

- Programs that used multiprocessing and changed directories would fail under
  coverage.  This is now fixed (`issue 890`_).  A side effect is that debug
  information about the config files read now shows absolute paths to the
  files.

- When running programs as modules (``coverage run -m``) with ``--source``,
  some measured modules were imported before coverage starts.  This resulted in
  unwanted warnings ("Already imported a file that will be measured") and a
  reduction in coverage totals (`issue 909`_).  This is now fixed.

- If no data was collected, an exception about "No data to report" could happen
  instead of a 0% report being created (`issue 884`_).  This is now fixed.

- The handling of source files with non-encodable file names has changed.
  Previously, if a file name could not be encoded as UTF-8, an error occurred,
  as described in `issue 891`_.  Now, those files will not be measured, since
  their data would not be recordable.

- A new warning ("dynamic-conflict") is issued if two mechanisms are trying to
  change the dynamic context.  Closes `issue 901`_.

- ``coverage run --debug=sys`` would fail with an AttributeError. This is now
  fixed (`issue 907`_).

.. _issue 884: https://github.com/nedbat/coveragepy/issues/884
.. _issue 890: https://github.com/nedbat/coveragepy/issues/890
.. _issue 891: https://github.com/nedbat/coveragepy/issues/891
.. _issue 901: https://github.com/nedbat/coveragepy/issues/901
.. _issue 907: https://github.com/nedbat/coveragepy/issues/907
.. _issue 909: https://github.com/nedbat/coveragepy/issues/909


.. _changes_501:

Version 5.0.1 --- 2019-12-22
----------------------------

- If a 4.x data file is the cause of a "file is not a database" error, then use
  a more specific error message, "Looks like a coverage 4.x data file, are you
  mixing versions of coverage?"  Helps diagnose the problems described in
  `issue 886`_.

- Measurement contexts and relative file names didn't work together, as
  reported in `issue 899`_ and `issue 900`_.  This is now fixed, thanks to
  David Szotten.

- When using ``coverage run --concurrency=multiprocessing``, all data files
  should be named with parallel-ready suffixes.  5.0 mistakenly named the main
  process' file with no suffix when using ``--append``.  This is now fixed,
  closing `issue 880`_.

- Fixed a problem on Windows when the current directory is changed to a
  different drive (`issue 895`_).  Thanks, Olivier Grisel.

- Updated Python 3.9 support to 3.9a2.

.. _issue 880: https://github.com/nedbat/coveragepy/issues/880
.. _issue 886: https://github.com/nedbat/coveragepy/issues/886
.. _issue 895: https://github.com/nedbat/coveragepy/issues/895
.. _issue 899: https://github.com/nedbat/coveragepy/issues/899
.. _issue 900: https://github.com/nedbat/coveragepy/issues/900


.. _changes_50:

Version 5.0 --- 2019-12-14
--------------------------

Nothing new beyond 5.0b2.


.. _changes_50b2:

Version 5.0b2 --- 2019-12-08
----------------------------

- An experimental ``[run] relative_files`` setting tells coverage to store
  relative file names in the data file. This makes it easier to run tests in
  one (or many) environments, and then report in another.  It has not had much
  real-world testing, so it may change in incompatible ways in the future.

- When constructing a :class:`coverage.Coverage` object, `data_file` can be
  specified as None to prevent writing any data file at all.  In previous
  versions, an explicit `data_file=None` argument would use the default of
  ".coverage". Fixes `issue 871`_.

- Python files run with ``-m`` now have ``__spec__`` defined properly.  This
  fixes `issue 745`_ (about not being able to run unittest tests that spawn
  subprocesses), and `issue 838`_, which described the problem directly.

- The ``[paths]`` configuration section is now ordered. If you specify more
  than one list of patterns, the first one that matches will be used.  Fixes
  `issue 649`_.

- The :func:`.coverage.numbits.register_sqlite_functions` function now also
  registers `numbits_to_nums` for use in SQLite queries.  Thanks, Simon
  Willison.

- Python 3.9a1 is supported.

- Coverage.py has a mascot: :ref:`Sleepy Snake <sleepy>`.

.. _issue 649: https://github.com/nedbat/coveragepy/issues/649
.. _issue 745: https://github.com/nedbat/coveragepy/issues/745
.. _issue 838: https://github.com/nedbat/coveragepy/issues/838
.. _issue 871: https://github.com/nedbat/coveragepy/issues/871


.. _changes_50b1:

Version 5.0b1 --- 2019-11-11
----------------------------

- The HTML and textual reports now have a ``--skip-empty`` option that skips
  files with no statements, notably ``__init__.py`` files.  Thanks, Reya B.

- Configuration can now be read from `TOML`_ files.  This requires installing
  coverage.py with the ``[toml]`` extra.  The standard "pyproject.toml" file
  will be read automatically if no other configuration file is found, with
  settings in the ``[tool.coverage.]`` namespace.  Thanks to Frazer McLean for
  implementation and persistence.  Finishes `issue 664`_.

- The ``[run] note`` setting has been deprecated. Using it will result in a
  warning, and the note will not be written to the data file.  The
  corresponding :class:`.CoverageData` methods have been removed.

- The HTML report has been reimplemented (no more table around the source
  code). This allowed for a better presentation of the context information,
  hopefully resolving `issue 855`_.

- Added sqlite3 module version information to ``coverage debug sys`` output.

- Asking the HTML report to show contexts (``[html] show_contexts=True`` or
  ``coverage html --show-contexts``) will issue a warning if there were no
  contexts measured (`issue 851`_).

.. _TOML: https://github.com/toml-lang/toml#readme
.. _issue 664: https://github.com/nedbat/coveragepy/issues/664
.. _issue 851: https://github.com/nedbat/coveragepy/issues/851
.. _issue 855: https://github.com/nedbat/coveragepy/issues/855


.. _changes_50a8:

Version 5.0a8 --- 2019-10-02
----------------------------

- The :class:`.CoverageData` API has changed how queries are limited to
  specific contexts.  Now you use :meth:`.CoverageData.set_query_context` to
  set a single exact-match string, or :meth:`.CoverageData.set_query_contexts`
  to set a list of regular expressions to match contexts.  This changes the
  command-line ``--contexts`` option to use regular expressions instead of
  filename-style wildcards.


.. _changes_50a7:

Version 5.0a7 --- 2019-09-21
----------------------------

- Data can now be "reported" in JSON format, for programmatic use, as requested
  in `issue 720`_.  The new ``coverage json`` command writes raw and summarized
  data to a JSON file.  Thanks, Matt Bachmann.

- Dynamic contexts are now supported in the Python tracer, which is important
  for PyPy users.  Closes `issue 846`_.

- The compact line number representation introduced in 5.0a6 is called a
  "numbits."  The :mod:`coverage.numbits` module provides functions for working
  with them.

- The reporting methods used to permanently apply their arguments to the
  configuration of the Coverage object.  Now they no longer do.  The arguments
  affect the operation of the method, but do not persist.

- A class named "test_something" no longer confuses the ``test_function``
  dynamic context setting.  Fixes `issue 829`_.

- Fixed an unusual tokenizing issue with backslashes in comments.  Fixes
  `issue 822`_.

- ``debug=plugin`` didn't properly support configuration or dynamic context
  plugins, but now it does, closing `issue 834`_.

.. _issue 720: https://github.com/nedbat/coveragepy/issues/720
.. _issue 822: https://github.com/nedbat/coveragepy/issues/822
.. _issue 834: https://github.com/nedbat/coveragepy/issues/834
.. _issue 829: https://github.com/nedbat/coveragepy/issues/829
.. _issue 846: https://github.com/nedbat/coveragepy/issues/846


.. _changes_50a6:

Version 5.0a6 --- 2019-07-16
----------------------------

- Reporting on contexts. Big thanks to Stephan Richter and Albertas Agejevas
  for the contribution.

  - The ``--contexts`` option is available on the ``report`` and ``html``
    commands.  It's a comma-separated list of shell-style wildcards, selecting
    the contexts to report on.  Only contexts matching one of the wildcards
    will be included in the report.

  - The ``--show-contexts`` option for the ``html`` command adds context
    information to each covered line.  Hovering over the "ctx" marker at the
    end of the line reveals a list of the contexts that covered the line.

- Database changes:

  - Line numbers are now stored in a much more compact way.  For each file and
    context, a single binary string is stored with a bit per line number.  This
    greatly improves memory use, but makes ad-hoc use difficult.

  - Dynamic contexts with no data are no longer written to the database.

  - SQLite data storage is now faster.  There's no longer a reason to keep the
    JSON data file code, so it has been removed.

- Changes to the :class:`.CoverageData` interface:

  - The new :meth:`.CoverageData.dumps` method serializes the data to a string,
    and a corresponding :meth:`.CoverageData.loads` method reconstitutes this
    data.  The format of the data string is subject to change at any time, and
    so should only be used between two installations of the same version of
    coverage.py.

  - The :meth:`CoverageData constructor<.CoverageData.__init__>` has a new
    argument, `no_disk` (default: False).  Setting it to True prevents writing
    any data to the disk.  This is useful for transient data objects.

- Added the classmethod :meth:`.Coverage.current` to get the latest started
  Coverage instance.

- Multiprocessing support in Python 3.8 was broken, but is now fixed.  Closes
  `issue 828`_.

- Error handling during reporting has changed slightly.  All reporting methods
  now behave the same.  The ``--ignore-errors`` option keeps errors from
  stopping the reporting, but files that couldn't parse as Python will always
  be reported as warnings.  As with other warnings, you can suppress them with
  the ``[run] disable_warnings`` configuration setting.

- Coverage.py no longer fails if the user program deletes its current
  directory. Fixes `issue 806`_.  Thanks, Dan Hemberger.

- The scrollbar markers in the HTML report now accurately show the highlighted
  lines, regardless of what categories of line are highlighted.

- The hack to accommodate ShiningPanda_ looking for an obsolete internal data
  file has been removed, since ShiningPanda 0.22 fixed it four years ago.

- The deprecated `Reporter.file_reporters` property has been removed.

.. _ShiningPanda: https://wiki.jenkins.io/display/JENKINS/ShiningPanda+Plugin
.. _issue 806: https://github.com/nedbat/coveragepy/pull/806
.. _issue 828: https://github.com/nedbat/coveragepy/issues/828


.. _changes_50a5:

Version 5.0a5 --- 2019-05-07
----------------------------

- Drop support for Python 3.4

- Dynamic contexts can now be set two new ways, both thanks to Justas
  Sadzevičius.

  - A plugin can implement a ``dynamic_context`` method to check frames for
    whether a new context should be started.  See
    :ref:`dynamic_context_plugins` for more details.

  - Another tool (such as a test runner) can use the new
    :meth:`.Coverage.switch_context` method to explicitly change the context.

- The ``dynamic_context = test_function`` setting now works with Python 2
  old-style classes, though it only reports the method name, not the class it
  was defined on.  Closes `issue 797`_.

- ``fail_under`` values more than 100 are reported as errors.  Thanks to Mike
  Fiedler for closing `issue 746`_.

- The "missing" values in the text output are now sorted by line number, so
  that missing branches are reported near the other lines they affect. The
  values used to show all missing lines, and then all missing branches.

- Access to the SQLite database used for data storage is now thread-safe.
  Thanks, Stephan Richter. This closes `issue 702`_.

- Combining data stored in SQLite is now about twice as fast, fixing `issue
  761`_.  Thanks, Stephan Richter.

- The ``filename`` attribute on :class:`.CoverageData` objects has been made
  private.  You can use the ``data_filename`` method to get the actual file
  name being used to store data, and the ``base_filename`` method to get the
  original filename before parallelizing suffixes were added.  This is part of
  fixing `issue 708`_.

- Line numbers in the HTML report now align properly with source lines, even
  when Chrome's minimum font size is set, fixing `issue 748`_.  Thanks Wen Ye.

.. _issue 702: https://github.com/nedbat/coveragepy/issues/702
.. _issue 708: https://github.com/nedbat/coveragepy/issues/708
.. _issue 746: https://github.com/nedbat/coveragepy/issues/746
.. _issue 748: https://github.com/nedbat/coveragepy/issues/748
.. _issue 761: https://github.com/nedbat/coveragepy/issues/761
.. _issue 797: https://github.com/nedbat/coveragepy/issues/797


.. _changes_50a4:

Version 5.0a4 --- 2018-11-25
----------------------------

- You can specify the command line to run your program with the ``[run]
  command_line`` configuration setting, as requested in `issue 695`_.

- Coverage will create directories as needed for the data file if they don't
  exist, closing `issue 721`_.

- The ``coverage run`` command has always adjusted the first entry in sys.path,
  to properly emulate how Python runs your program.  Now this adjustment is
  skipped if sys.path[0] is already different than Python's default.  This
  fixes `issue 715`_.

- Improvements to context support:

  - The "no such table: meta" error is fixed.: `issue 716`_.

  - Combining data files is now much faster.

- Python 3.8 (as of today!) passes all tests.

.. _issue 695: https://github.com/nedbat/coveragepy/issues/695
.. _issue 715: https://github.com/nedbat/coveragepy/issues/715
.. _issue 716: https://github.com/nedbat/coveragepy/issues/716
.. _issue 721: https://github.com/nedbat/coveragepy/issues/721


.. _changes_50a3:

Version 5.0a3 --- 2018-10-06
----------------------------

- Context support: static contexts let you specify a label for a coverage run,
  which is recorded in the data, and retained when you combine files.  See
  :ref:`contexts` for more information.

- Dynamic contexts: specifying ``[run] dynamic_context = test_function`` in the
  config file will record the test function name as a dynamic context during
  execution.  This is the core of "Who Tests What" (`issue 170`_).  Things to
  note:

  - There is no reporting support yet.  Use SQLite to query the .coverage file
    for information.  Ideas are welcome about how reporting could be extended
    to use this data.

  - There's a noticeable slow-down before any test is run.

  - Data files will now be roughly N times larger, where N is the number of
    tests you have.  Combining data files is therefore also N times slower.

  - No other values for ``dynamic_context`` are recognized yet.  Let me know
    what else would be useful.  I'd like to use a pytest plugin to get better
    information directly from pytest, for example.

.. _issue 170: https://github.com/nedbat/coveragepy/issues/170

- Environment variable substitution in configuration files now supports two
  syntaxes for controlling the behavior of undefined variables: if ``VARNAME``
  is not defined, ``${VARNAME?}`` will raise an error, and ``${VARNAME-default
  value}`` will use "default value".

- Partial support for Python 3.8, which has not yet released an alpha. Fixes
  `issue 707`_ and `issue 714`_.

.. _issue 707: https://github.com/nedbat/coveragepy/issues/707
.. _issue 714: https://github.com/nedbat/coveragepy/issues/714


.. _changes_50a2:

Version 5.0a2 --- 2018-09-03
----------------------------

- Coverage's data storage has changed.  In version 4.x, .coverage files were
  basically JSON.  Now, they are SQLite databases.  This means the data file
  can be created earlier than it used to.  A large amount of code was
  refactored to support this change.

  - Because the data file is created differently than previous releases, you
    may need ``parallel=true`` where you didn't before.

  - The old data format is still available (for now) by setting the environment
    variable COVERAGE_STORAGE=json. Please tell me if you think you need to
    keep the JSON format.

  - The database schema is guaranteed to change in the future, to support new
    features.  I'm looking for opinions about making the schema part of the
    public API to coverage.py or not.

- Development moved from `Bitbucket`_ to `GitHub`_.

- HTML files no longer have trailing and extra whitespace.

- The sort order in the HTML report is stored in local storage rather than
  cookies, closing `issue 611`_.  Thanks, Federico Bond.

- pickle2json, for converting v3 data files to v4 data files, has been removed.

.. _Bitbucket: https://bitbucket.org/ned/coveragepy
.. _GitHub: https://github.com/nedbat/coveragepy

.. _issue 611: https://github.com/nedbat/coveragepy/issues/611


.. _changes_50a1:

Version 5.0a1 --- 2018-06-05
----------------------------

- Coverage.py no longer supports Python 2.6 or 3.3.

- The location of the configuration file can now be specified with a
  ``COVERAGE_RCFILE`` environment variable, as requested in `issue 650`_.

- Namespace packages are supported on Python 3.7, where they used to cause
  TypeErrors about path being None. Fixes `issue 700`_.

- A new warning (``already-imported``) is issued if measurable files have
  already been imported before coverage.py started measurement.  See
  :ref:`cmd_warnings` for more information.

- Running coverage many times for small runs in a single process should be
  faster, closing `issue 625`_.  Thanks, David MacIver.

- Large HTML report pages load faster.  Thanks, Pankaj Pandey.

.. _issue 625: https://bitbucket.org/ned/coveragepy/issues/625/lstat-dominates-in-the-case-of-small
.. _issue 650: https://bitbucket.org/ned/coveragepy/issues/650/allow-setting-configuration-file-location
.. _issue 700: https://github.com/nedbat/coveragepy/issues/700


.. _changes_454:

Version 4.5.4 --- 2019-07-29
----------------------------

- Multiprocessing support in Python 3.8 was broken, but is now fixed.  Closes
  `issue 828`_.

.. _issue 828: https://github.com/nedbat/coveragepy/issues/828


.. _changes_453:

Version 4.5.3 --- 2019-03-09
----------------------------

- Only packaging metadata changes.


.. _changes_452:

Version 4.5.2 --- 2018-11-12
----------------------------

- Namespace packages are supported on Python 3.7, where they used to cause
  TypeErrors about path being None. Fixes `issue 700`_.

- Python 3.8 (as of today!) passes all tests.  Fixes `issue 707`_ and
  `issue 714`_.

- Development moved from `Bitbucket`_ to `GitHub`_.

.. _issue 700: https://github.com/nedbat/coveragepy/issues/700
.. _issue 707: https://github.com/nedbat/coveragepy/issues/707
.. _issue 714: https://github.com/nedbat/coveragepy/issues/714

.. _Bitbucket: https://bitbucket.org/ned/coveragepy
.. _GitHub: https://github.com/nedbat/coveragepy


.. _changes_451:

Version 4.5.1 --- 2018-02-10
----------------------------

- Now that 4.5 properly separated the ``[run] omit`` and ``[report] omit``
  settings, an old bug has become apparent.  If you specified a package name
  for ``[run] source``, then omit patterns weren't matched inside that package.
  This bug (`issue 638`_) is now fixed.

- On Python 3.7, reporting about a decorated function with no body other than a
  docstring would crash coverage.py with an IndexError (`issue 640`_).  This is
  now fixed.

- Configurer plugins are now reported in the output of ``--debug=sys``.

.. _issue 638: https://bitbucket.org/ned/coveragepy/issues/638/run-omit-is-ignored-since-45
.. _issue 640: https://bitbucket.org/ned/coveragepy/issues/640/indexerror-reporting-on-an-empty-decorated


.. _changes_45:

Version 4.5 --- 2018-02-03
--------------------------

- A new kind of plugin is supported: configurers are invoked at start-up to
  allow more complex configuration than the .coveragerc file can easily do.
  See :ref:`api_plugin` for details.  This solves the complex configuration
  problem described in `issue 563`_.

- The ``fail_under`` option can now be a float.  Note that you must specify the
  ``[report] precision`` configuration option for the fractional part to be
  used.  Thanks to Lars Hupfeldt Nielsen for help with the implementation.
  Fixes `issue 631`_.

- The ``include`` and ``omit`` options can be specified for both the ``[run]``
  and ``[report]`` phases of execution.  4.4.2 introduced some incorrect
  interactions between those phases, where the options for one were confused
  for the other.  This is now corrected, fixing `issue 621`_ and `issue 622`_.
  Thanks to Daniel Hahler for seeing more clearly than I could.

- The ``coverage combine`` command used to always overwrite the data file, even
  when no data had been read from apparently combinable files.  Now, an error
  is raised if we thought there were files to combine, but in fact none of them
  could be used.  Fixes `issue 629`_.

- The ``coverage combine`` command could get confused about path separators
  when combining data collected on Windows with data collected on Linux, as
  described in `issue 618`_.  This is now fixed: the result path always uses
  the path separator specified in the ``[paths]`` result.

- On Windows, the HTML report could fail when source trees are deeply nested,
  due to attempting to create HTML filenames longer than the 250-character
  maximum.  Now filenames will never get much larger than 200 characters,
  fixing `issue 627`_.  Thanks to Alex Sandro for helping with the fix.

.. _issue 563: https://bitbucket.org/ned/coveragepy/issues/563/platform-specific-configuration
.. _issue 618: https://bitbucket.org/ned/coveragepy/issues/618/problem-when-combining-windows-generated
.. _issue 621: https://bitbucket.org/ned/coveragepy/issues/621/include-ignored-warning-when-using
.. _issue 622: https://bitbucket.org/ned/coveragepy/issues/622/report-omit-overwrites-run-omit
.. _issue 627: https://bitbucket.org/ned/coveragepy/issues/627/failure-generating-html-reports-when-the
.. _issue 629: https://bitbucket.org/ned/coveragepy/issues/629/multiple-use-of-combine-leads-to-empty
.. _issue 631: https://bitbucket.org/ned/coveragepy/issues/631/precise-coverage-percentage-value


.. _changes_442:

Version 4.4.2 --- 2017-11-05
----------------------------

- Support for Python 3.7.  In some cases, class and module docstrings are no
  longer counted in statement totals, which could slightly change your total
  results.

- Specifying both ``--source`` and ``--include`` no longer silently ignores the
  include setting, instead it displays a warning. Thanks, Loïc Dachary.  Closes
  `issue 265`_ and `issue 101`_.

- Fixed a race condition when saving data and multiple threads are tracing
  (`issue 581`_). It could produce a "dictionary changed size during iteration"
  RuntimeError.  I believe this mostly but not entirely fixes the race
  condition.  A true fix would likely be too expensive.  Thanks, Peter Baughman
  for the debugging, and Olivier Grisel for the fix with tests.

- Configuration values which are file paths will now apply tilde-expansion,
  closing `issue 589`_.

- Now secondary config files like tox.ini and setup.cfg can be specified
  explicitly, and prefixed sections like `[coverage:run]` will be read. Fixes
  `issue 588`_.

- Be more flexible about the command name displayed by help, fixing
  `issue 600`_. Thanks, Ben Finney.

.. _issue 101: https://bitbucket.org/ned/coveragepy/issues/101/settings-under-report-affect-running
.. _issue 581: https://bitbucket.org/ned/coveragepy/issues/581/race-condition-when-saving-data-under
.. _issue 588: https://bitbucket.org/ned/coveragepy/issues/588/using-rcfile-path-to-toxini-uses-run
.. _issue 589: https://bitbucket.org/ned/coveragepy/issues/589/allow-expansion-in-coveragerc
.. _issue 600: https://bitbucket.org/ned/coveragepy/issues/600/get-program-name-from-command-line-when


.. _changes_441:

Version 4.4.1 --- 2017-05-14
----------------------------

- No code changes: just corrected packaging for Python 2.7 Linux wheels.


.. _changes_44:

Version 4.4 --- 2017-05-07
--------------------------

- Reports could produce the wrong file names for packages, reporting ``pkg.py``
  instead of the correct ``pkg/__init__.py``.  This is now fixed.  Thanks, Dirk
  Thomas.

- XML reports could produce ``<source>`` and ``<class>`` lines that together
  didn't specify a valid source file path.  This is now fixed. (`issue 526`_)

- Namespace packages are no longer warned as having no code. (`issue 572`_)

- Code that uses ``sys.settrace(sys.gettrace())`` in a file that wasn't being
  coverage-measured would prevent correct coverage measurement in following
  code. An example of this was running doctests programmatically. This is now
  fixed. (`issue 575`_)

- Errors printed by the ``coverage`` command now go to stderr instead of
  stdout.

- Running ``coverage xml`` in a directory named with non-ASCII characters would
  fail under Python 2. This is now fixed. (`issue 573`_)

.. _issue 526: https://bitbucket.org/ned/coveragepy/issues/526/generated-xml-invalid-paths-for-cobertura
.. _issue 572: https://bitbucket.org/ned/coveragepy/issues/572/no-python-source-warning-for-namespace
.. _issue 573: https://bitbucket.org/ned/coveragepy/issues/573/cant-generate-xml-report-if-some-source
.. _issue 575: https://bitbucket.org/ned/coveragepy/issues/575/running-doctest-prevents-complete-coverage


Version 4.4b1 --- 2017-04-04
----------------------------

- Some warnings can now be individually disabled.  Warnings that can be
  disabled have a short name appended.  The ``[run] disable_warnings`` setting
  takes a list of these warning names to disable. Closes both `issue 96`_ and
  `issue 355`_.

- The XML report now includes attributes from version 4 of the Cobertura XML
  format, fixing `issue 570`_.

- In previous versions, calling a method that used collected data would prevent
  further collection.  For example, `save()`, `report()`, `html_report()`, and
  others would all stop collection.  An explicit `start()` was needed to get it
  going again.  This is no longer true.  Now you can use the collected data and
  also continue measurement. Both `issue 79`_ and `issue 448`_ described this
  problem, and have been fixed.

- Plugins can now find unexecuted files if they choose, by implementing the
  `find_executable_files` method.  Thanks, Emil Madsen.

- Minimal IronPython support. You should be able to run IronPython programs
  under ``coverage run``, though you will still have to do the reporting phase
  with CPython.

- Coverage.py has long had a special hack to support CPython's need to measure
  the coverage of the standard library tests. This code was not installed by
  kitted versions of coverage.py.  Now it is.

.. _issue 79: https://bitbucket.org/ned/coveragepy/issues/79/save-prevents-harvesting-on-stop
.. _issue 96: https://bitbucket.org/ned/coveragepy/issues/96/unhelpful-warnings-produced-when-using
.. _issue 355: https://bitbucket.org/ned/coveragepy/issues/355/warnings-should-be-suppressable
.. _issue 448: https://bitbucket.org/ned/coveragepy/issues/448/save-and-html_report-prevent-further
.. _issue 570: https://bitbucket.org/ned/coveragepy/issues/570/cobertura-coverage-04dtd-support


.. _changes_434:

Version 4.3.4 --- 2017-01-17
----------------------------

- Fixing 2.6 in version 4.3.3 broke other things, because the too-tricky
  exception wasn't properly derived from Exception, described in `issue 556`_.
  A newb mistake; it hasn't been a good few days.

.. _issue 556: https://bitbucket.org/ned/coveragepy/issues/556/43-fails-if-there-are-html-files-in-the


.. _changes_433:

Version 4.3.3 --- 2017-01-17
----------------------------

- Python 2.6 support was broken due to a testing exception imported for the
  benefit of the coverage.py test suite.  Properly conditionalizing it fixed
  `issue 554`_ so that Python 2.6 works again.

.. _issue 554: https://bitbucket.org/ned/coveragepy/issues/554/traceback-on-python-26-starting-with-432


.. _changes_432:

Version 4.3.2 --- 2017-01-16
----------------------------

- Using the ``--skip-covered`` option on an HTML report with 100% coverage
  would cause a "No data to report" error, as reported in `issue 549`_. This is
  now fixed; thanks, Loïc Dachary.

- If-statements can be optimized away during compilation, for example, `if 0:`
  or `if __debug__:`.  Coverage.py had problems properly understanding these
  statements which existed in the source, but not in the compiled bytecode.
  This problem, reported in `issue 522`_, is now fixed.

- If you specified ``--source`` as a directory, then coverage.py would look for
  importable Python files in that directory, and could identify ones that had
  never been executed at all.  But if you specified it as a package name, that
  detection wasn't performed.  Now it is, closing `issue 426`_. Thanks to Loïc
  Dachary for the fix.

- If you started and stopped coverage measurement thousands of times in your
  process, you could crash Python with a "Fatal Python error: deallocating
  None" error.  This is now fixed.  Thanks to Alex Groce for the bug report.

- On PyPy, measuring coverage in subprocesses could produce a warning: "Trace
  function changed, measurement is likely wrong: None".  This was spurious, and
  has been suppressed.

- Previously, coverage.py couldn't start on Jython, due to that implementation
  missing the multiprocessing module (`issue 551`_). This problem has now been
  fixed. Also, `issue 322`_ about not being able to invoke coverage
  conveniently, seems much better: ``jython -m coverage run myprog.py`` works
  properly.

- Let's say you ran the HTML report over and over again in the same output
  directory, with ``--skip-covered``. And imagine due to your heroic
  test-writing efforts, a file just achieved the goal of 100% coverage. With
  coverage.py 4.3, the old HTML file with the less-than-100% coverage would be
  left behind.  This file is now properly deleted.

.. _issue 322: https://bitbucket.org/ned/coveragepy/issues/322/cannot-use-coverage-with-jython
.. _issue 426: https://bitbucket.org/ned/coveragepy/issues/426/difference-between-coverage-results-with
.. _issue 522: https://bitbucket.org/ned/coveragepy/issues/522/incorrect-branch-reporting
.. _issue 549: https://bitbucket.org/ned/coveragepy/issues/549/skip-covered-with-100-coverage-throws-a-no
.. _issue 551: https://bitbucket.org/ned/coveragepy/issues/551/coveragepy-cannot-be-imported-in-jython27


.. _changes_431:

Version 4.3.1 --- 2016-12-28
----------------------------

- Some environments couldn't install 4.3, as described in `issue 540`_. This is
  now fixed.

- The check for conflicting ``--source`` and ``--include`` was too simple in a
  few different ways, breaking a few perfectly reasonable use cases, described
  in `issue 541`_.  The check has been reverted while we re-think the fix for
  `issue 265`_.

.. _issue 540: https://bitbucket.org/ned/coveragepy/issues/540/cant-install-coverage-v43-into-under
.. _issue 541: https://bitbucket.org/ned/coveragepy/issues/541/coverage-43-breaks-nosetest-with-coverage


.. _changes_43:

Version 4.3 --- 2016-12-27
--------------------------

Special thanks to **Loïc Dachary**, who took an extraordinary interest in
coverage.py and contributed a number of improvements in this release.

- Subprocesses that are measured with `automatic subprocess measurement`_ used
  to read in any pre-existing data file.  This meant data would be incorrectly
  carried forward from run to run.  Now those files are not read, so each
  subprocess only writes its own data. Fixes `issue 510`_.

- The ``coverage combine`` command will now fail if there are no data files to
  combine. The combine changes in 4.2 meant that multiple combines could lose
  data, leaving you with an empty .coverage data file. Fixes
  `issue 525`_, `issue 412`_, `issue 516`_, and probably `issue 511`_.

- Coverage.py wouldn't execute `sys.excepthook`_ when an exception happened in
  your program.  Now it does, thanks to Andrew Hoos.  Closes `issue 535`_.

- Branch coverage fixes:

  - Branch coverage could misunderstand a finally clause on a try block that
    never continued on to the following statement, as described in `issue
    493`_.  This is now fixed. Thanks to Joe Doherty for the report and Loïc
    Dachary for the fix.

  - A while loop with a constant condition (while True) and a continue
    statement would be mis-analyzed, as described in `issue 496`_. This is now
    fixed, thanks to a bug report by Eli Skeggs and a fix by Loïc Dachary.

  - While loops with constant conditions that were never executed could result
    in a non-zero coverage report.  Artem Dayneko reported this in `issue
    502`_, and Loïc Dachary provided the fix.

- The HTML report now supports a ``--skip-covered`` option like the other
  reporting commands.  Thanks, Loïc Dachary for the implementation, closing
  `issue 433`_.

- Options can now be read from a tox.ini file, if any. Like setup.cfg, sections
  are prefixed with "coverage:", so ``[run]`` options will be read from the
  ``[coverage:run]`` section of tox.ini. Implements part of `issue 519`_.
  Thanks, Stephen Finucane.

- Specifying both ``--source`` and ``--include`` no longer silently ignores the
  include setting, instead it fails with a message. Thanks, Nathan Land and
  Loïc Dachary. Closes `issue 265`_.

- The ``Coverage.combine`` method has a new parameter, ``strict=False``, to
  support failing if there are no data files to combine.

- When forking subprocesses, the coverage data files would have the same random
  number appended to the file name. This didn't cause problems, because the
  file names had the process id also, making collisions (nearly) impossible.
  But it was disconcerting.  This is now fixed.

- The text report now properly sizes headers when skipping some files, fixing
  `issue 524`_. Thanks, Anthony Sottile and Loïc Dachary.

- Coverage.py can now search .pex files for source, just as it can .zip and
  .egg.  Thanks, Peter Ebden.

- Data files are now about 15% smaller.

- Improvements in the ``[run] debug`` setting:

  - The "dataio" debug setting now also logs when data files are deleted during
    combining or erasing.

  - A new debug option, "multiproc", for logging the behavior of
    ``concurrency=multiprocessing``.

  - If you used the debug options "config" and "callers" together, you'd get a
    call stack printed for every line in the multi-line config output. This is
    now fixed.

- Fixed an unusual bug involving multiple coding declarations affecting code
  containing code in multi-line strings: `issue 529`_.

- Coverage.py will no longer be misled into thinking that a plain file is a
  package when interpreting ``--source`` options.  Thanks, Cosimo Lupo.

- If you try to run a non-Python file with coverage.py, you will now get a more
  useful error message. `Issue 514`_.

- The default pragma regex changed slightly, but this will only matter to you
  if you are deranged and use mixed-case pragmas.

- Deal properly with non-ASCII file names in an ASCII-only world, `issue 533`_.

- Programs that set Unicode configuration values could cause UnicodeErrors when
  generating HTML reports.  Pytest-cov is one example.  This is now fixed.

- Prevented deprecation warnings from configparser that happened in some
  circumstances, closing `issue 530`_.

- Corrected the name of the jquery.ba-throttle-debounce.js library. Thanks,
  Ben Finney.  Closes `issue 505`_.

- Testing against PyPy 5.6 and PyPy3 5.5.

- Switched to pytest from nose for running the coverage.py tests.

- Renamed AUTHORS.txt to CONTRIBUTORS.txt, since there are other ways to
  contribute than by writing code. Also put the count of contributors into the
  author string in setup.py, though this might be too cute.

.. _sys.excepthook: https://docs.python.org/3/library/sys.html#sys.excepthook
.. _issue 265: https://bitbucket.org/ned/coveragepy/issues/265/when-using-source-include-is-silently
.. _issue 412: https://bitbucket.org/ned/coveragepy/issues/412/coverage-combine-should-error-if-no
.. _issue 433: https://bitbucket.org/ned/coveragepy/issues/433/coverage-html-does-not-suport-skip-covered
.. _issue 493: https://bitbucket.org/ned/coveragepy/issues/493/confusing-branching-failure
.. _issue 496: https://bitbucket.org/ned/coveragepy/issues/496/incorrect-coverage-with-branching-and
.. _issue 502: https://bitbucket.org/ned/coveragepy/issues/502/incorrect-coverage-report-with-cover
.. _issue 505: https://bitbucket.org/ned/coveragepy/issues/505/use-canonical-filename-for-debounce
.. _issue 514: https://bitbucket.org/ned/coveragepy/issues/514/path-to-problem-file-not-reported-when
.. _issue 510: https://bitbucket.org/ned/coveragepy/issues/510/erase-still-needed-in-42
.. _issue 511: https://bitbucket.org/ned/coveragepy/issues/511/version-42-coverage-combine-empties
.. _issue 516: https://bitbucket.org/ned/coveragepy/issues/516/running-coverage-combine-twice-deletes-all
.. _issue 519: https://bitbucket.org/ned/coveragepy/issues/519/coverage-run-sections-in-toxini-or-as
.. _issue 524: https://bitbucket.org/ned/coveragepy/issues/524/coverage-report-with-skip-covered-column
.. _issue 525: https://bitbucket.org/ned/coveragepy/issues/525/coverage-combine-when-not-in-parallel-mode
.. _issue 529: https://bitbucket.org/ned/coveragepy/issues/529/encoding-marker-may-only-appear-on-the
.. _issue 530: https://bitbucket.org/ned/coveragepy/issues/530/deprecationwarning-you-passed-a-bytestring
.. _issue 533: https://bitbucket.org/ned/coveragepy/issues/533/exception-on-unencodable-file-name
.. _issue 535: https://bitbucket.org/ned/coveragepy/issues/535/sysexcepthook-is-not-called


.. _changes_42:

Version 4.2 --- 2016-07-26
--------------------------

- Since ``concurrency=multiprocessing`` uses subprocesses, options specified on
  the coverage.py command line will not be communicated down to them.  Only
  options in the configuration file will apply to the subprocesses.
  Previously, the options didn't apply to the subprocesses, but there was no
  indication.  Now it is an error to use ``--concurrency=multiprocessing`` and
  other run-affecting options on the command line.  This prevents
  failures like those reported in `issue 495`_.

- Filtering the HTML report is now faster, thanks to Ville Skyttä.

.. _issue 495: https://bitbucket.org/ned/coveragepy/issues/495/branch-and-concurrency-are-conflicting


Version 4.2b1 --- 2016-07-04
----------------------------

Work from the PyCon 2016 Sprints!

- BACKWARD INCOMPATIBILITY: the ``coverage combine`` command now ignores an
  existing ``.coverage`` data file.  It used to include that file in its
  combining.  This caused confusing results, and extra tox "clean" steps.  If
  you want the old behavior, use the new ``coverage combine --append`` option.

- The ``concurrency`` option can now take multiple values, to support programs
  using multiprocessing and another library such as eventlet.  This is only
  possible in the configuration file, not from the command line. The
  configuration file is the only way for sub-processes to all run with the same
  options.  Fixes `issue 484`_.  Thanks to Josh Williams for prototyping.

- Using a ``concurrency`` setting of ``multiprocessing`` now implies
  ``--parallel`` so that the main program is measured similarly to the
  sub-processes.

- When using `automatic subprocess measurement`_, running coverage commands
  would create spurious data files.  This is now fixed, thanks to diagnosis and
  testing by Dan Riti.  Closes `issue 492`_.

- A new configuration option, ``report:sort``, controls what column of the
  text report is used to sort the rows.  Thanks to Dan Wandschneider, this
  closes `issue 199`_.

- The HTML report has a more-visible indicator for which column is being
  sorted.  Closes `issue 298`_, thanks to Josh Williams.

- If the HTML report cannot find the source for a file, the message now
  suggests using the ``-i`` flag to allow the report to continue. Closes
  `issue 231`_, thanks, Nathan Land.

- When reports are ignoring errors, there's now a warning if a file cannot be
  parsed, rather than being silently ignored.  Closes `issue 396`_. Thanks,
  Matthew Boehm.

- A new option for ``coverage debug`` is available: ``coverage debug config``
  shows the current configuration.  Closes `issue 454`_, thanks to Matthew
  Boehm.

- Running coverage as a module (``python -m coverage``) no longer shows the
  program name as ``__main__.py``.  Fixes `issue 478`_.  Thanks, Scott Belden.

- The `test_helpers` module has been moved into a separate pip-installable
  package: `unittest-mixins`_.

.. _automatic subprocess measurement: https://coverage.readthedocs.io/en/latest/subprocess.html
.. _issue 199: https://bitbucket.org/ned/coveragepy/issues/199/add-a-way-to-sort-the-text-report
.. _issue 231: https://bitbucket.org/ned/coveragepy/issues/231/various-default-behavior-in-report-phase
.. _issue 298: https://bitbucket.org/ned/coveragepy/issues/298/show-in-html-report-that-the-columns-are
.. _issue 396: https://bitbucket.org/ned/coveragepy/issues/396/coverage-xml-shouldnt-bail-out-on-parse
.. _issue 454: https://bitbucket.org/ned/coveragepy/issues/454/coverage-debug-config-should-be
.. _issue 478: https://bitbucket.org/ned/coveragepy/issues/478/help-shows-silly-program-name-when-running
.. _issue 484: https://bitbucket.org/ned/coveragepy/issues/484/multiprocessing-greenlet-concurrency
.. _issue 492: https://bitbucket.org/ned/coveragepy/issues/492/subprocess-coverage-strange-detection-of
.. _unittest-mixins: https://pypi.org/project/unittest-mixins/


.. _changes_41:

Version 4.1 --- 2016-05-21
--------------------------

- The internal attribute `Reporter.file_reporters` was removed in 4.1b3.  It
  should have come has no surprise that there were third-party tools out there
  using that attribute.  It has been restored, but with a deprecation warning.


Version 4.1b3 --- 2016-05-10
----------------------------

- When running your program, execution can jump from an ``except X:`` line to
  some other line when an exception other than ``X`` happens.  This jump is no
  longer considered a branch when measuring branch coverage.

- When measuring branch coverage, ``yield`` statements that were never resumed
  were incorrectly marked as missing, as reported in `issue 440`_.  This is now
  fixed.

- During branch coverage of single-line callables like lambdas and generator
  expressions, coverage.py can now distinguish between them never being called,
  or being called but not completed.  Fixes `issue 90`_, `issue 460`_ and
  `issue 475`_.

- The HTML report now has a map of the file along the rightmost edge of the
  page, giving an overview of where the missed lines are.  Thanks, Dmitry
  Shishov.

- The HTML report now uses different monospaced fonts, favoring Consolas over
  Courier.  Along the way, `issue 472`_ about not properly handling one-space
  indents was fixed.  The index page also has slightly different styling, to
  try to make the clickable detail pages more apparent.

- Missing branches reported with ``coverage report -m`` will now say ``->exit``
  for missed branches to the exit of a function, rather than a negative number.
  Fixes `issue 469`_.

- ``coverage --help`` and ``coverage --version`` now mention which tracer is
  installed, to help diagnose problems. The docs mention which features need
  the C extension. (`issue 479`_)

- Officially support PyPy 5.1, which required no changes, just updates to the
  docs.

- The `Coverage.report` function had two parameters with non-None defaults,
  which have been changed.  `show_missing` used to default to True, but now
  defaults to None.  If you had been calling `Coverage.report` without
  specifying `show_missing`, you'll need to explicitly set it to True to keep
  the same behavior.  `skip_covered` used to default to False. It is now None,
  which doesn't change the behavior.  This fixes `issue 485`_.

- It's never been possible to pass a namespace module to one of the analysis
  functions, but now at least we raise a more specific error message, rather
  than getting confused. (`issue 456`_)

- The `coverage.process_startup` function now returns the `Coverage` instance
  it creates, as suggested in `issue 481`_.

- Make a small tweak to how we compare threads, to avoid buggy custom
  comparison code in thread classes. (`issue 245`_)

.. _issue 90: https://bitbucket.org/ned/coveragepy/issues/90/lambda-expression-confuses-branch
.. _issue 245: https://bitbucket.org/ned/coveragepy/issues/245/change-solution-for-issue-164
.. _issue 440: https://bitbucket.org/ned/coveragepy/issues/440/yielded-twisted-failure-marked-as-missed
.. _issue 456: https://bitbucket.org/ned/coveragepy/issues/456/coverage-breaks-with-implicit-namespaces
.. _issue 460: https://bitbucket.org/ned/coveragepy/issues/460/confusing-html-report-for-certain-partial
.. _issue 469: https://bitbucket.org/ned/coveragepy/issues/469/strange-1-line-number-in-branch-coverage
.. _issue 472: https://bitbucket.org/ned/coveragepy/issues/472/html-report-indents-incorrectly-for-one
.. _issue 475: https://bitbucket.org/ned/coveragepy/issues/475/generator-expression-is-marked-as-not
.. _issue 479: https://bitbucket.org/ned/coveragepy/issues/479/clarify-the-need-for-the-c-extension
.. _issue 481: https://bitbucket.org/ned/coveragepy/issues/481/asyncioprocesspoolexecutor-tracing-not
.. _issue 485: https://bitbucket.org/ned/coveragepy/issues/485/coveragereport-ignores-show_missing-and


Version 4.1b2 --- 2016-01-23
----------------------------

- Problems with the new branch measurement in 4.1 beta 1 were fixed:

  - Class docstrings were considered executable.  Now they no longer are.

  - ``yield from`` and ``await`` were considered returns from functions, since
    they could transfer control to the caller.  This produced unhelpful
    "missing branch" reports in a number of circumstances.  Now they no longer
    are considered returns.

  - In unusual situations, a missing branch to a negative number was reported.
    This has been fixed, closing `issue 466`_.

- The XML report now produces correct package names for modules found in
  directories specified with ``source=``.  Fixes `issue 465`_.

- ``coverage report`` won't produce trailing whitespace.

.. _issue 465: https://bitbucket.org/ned/coveragepy/issues/465/coveragexml-produces-package-names-with-an
.. _issue 466: https://bitbucket.org/ned/coveragepy/issues/466/impossible-missed-branch-to-a-negative


Version 4.1b1 --- 2016-01-10
----------------------------

- Branch analysis has been rewritten: it used to be based on bytecode, but now
  uses AST analysis.  This has changed a number of things:

  - More code paths are now considered runnable, especially in
    ``try``/``except`` structures.  This may mean that coverage.py will
    identify more code paths as uncovered.  This could either raise or lower
    your overall coverage number.

  - Python 3.5's ``async`` and ``await`` keywords are properly supported,
    fixing `issue 434`_.

  - Some long-standing branch coverage bugs were fixed:

    - `issue 129`_: functions with only a docstring for a body would
      incorrectly report a missing branch on the ``def`` line.

    - `issue 212`_: code in an ``except`` block could be incorrectly marked as
      a missing branch.

    - `issue 146`_: context managers (``with`` statements) in a loop or ``try``
      block could confuse the branch measurement, reporting incorrect partial
      branches.

    - `issue 422`_: in Python 3.5, an actual partial branch could be marked as
      complete.

- Pragmas to disable coverage measurement can now be used on decorator lines,
  and they will apply to the entire function or class being decorated.  This
  implements the feature requested in `issue 131`_.

- Multiprocessing support is now available on Windows.  Thanks, Rodrigue
  Cloutier.

- Files with two encoding declarations are properly supported, fixing
  `issue 453`_. Thanks, Max Linke.

- Non-ascii characters in regexes in the configuration file worked in 3.7, but
  stopped working in 4.0.  Now they work again, closing `issue 455`_.

- Form-feed characters would prevent accurate determination of the beginning of
  statements in the rest of the file.  This is now fixed, closing `issue 461`_.

.. _issue 129: https://bitbucket.org/ned/coveragepy/issues/129/misleading-branch-coverage-of-empty
.. _issue 131: https://bitbucket.org/ned/coveragepy/issues/131/pragma-on-a-decorator-line-should-affect
.. _issue 146: https://bitbucket.org/ned/coveragepy/issues/146/context-managers-confuse-branch-coverage
.. _issue 212: https://bitbucket.org/ned/coveragepy/issues/212/coverage-erroneously-reports-partial
.. _issue 422: https://bitbucket.org/ned/coveragepy/issues/422/python35-partial-branch-marked-as-fully
.. _issue 434: https://bitbucket.org/ned/coveragepy/issues/434/indexerror-in-python-35
.. _issue 453: https://bitbucket.org/ned/coveragepy/issues/453/source-code-encoding-can-only-be-specified
.. _issue 455: https://bitbucket.org/ned/coveragepy/issues/455/unusual-exclusions-stopped-working-in
.. _issue 461: https://bitbucket.org/ned/coveragepy/issues/461/multiline-asserts-need-too-many-pragma


.. _changes_403:

Version 4.0.3 --- 2015-11-24
----------------------------

- Fixed a mysterious problem that manifested in different ways: sometimes
  hanging the process (`issue 420`_), sometimes making database connections
  fail (`issue 445`_).

- The XML report now has correct ``<source>`` elements when using a
  ``--source=`` option somewhere besides the current directory.  This fixes
  `issue 439`_. Thanks, Arcadiy Ivanov.

- Fixed an unusual edge case of detecting source encodings, described in
  `issue 443`_.

- Help messages that mention the command to use now properly use the actual
  command name, which might be different than "coverage".  Thanks to Ben
  Finney, this closes `issue 438`_.

.. _issue 420: https://bitbucket.org/ned/coveragepy/issues/420/coverage-40-hangs-indefinitely-on-python27
.. _issue 438: https://bitbucket.org/ned/coveragepy/issues/438/parameterise-coverage-command-name
.. _issue 439: https://bitbucket.org/ned/coveragepy/issues/439/incorrect-cobertura-file-sources-generated
.. _issue 443: https://bitbucket.org/ned/coveragepy/issues/443/coverage-gets-confused-when-encoding
.. _issue 445: https://bitbucket.org/ned/coveragepy/issues/445/django-app-cannot-connect-to-cassandra


.. _changes_402:

Version 4.0.2 --- 2015-11-04
----------------------------

- More work on supporting unusually encoded source. Fixed `issue 431`_.

- Files or directories with non-ASCII characters are now handled properly,
  fixing `issue 432`_.

- Setting a trace function with sys.settrace was broken by a change in 4.0.1,
  as reported in `issue 436`_.  This is now fixed.

- Officially support PyPy 4.0, which required no changes, just updates to the
  docs.

.. _issue 431: https://bitbucket.org/ned/coveragepy/issues/431/couldnt-parse-python-file-with-cp1252
.. _issue 432: https://bitbucket.org/ned/coveragepy/issues/432/path-with-unicode-characters-various
.. _issue 436: https://bitbucket.org/ned/coveragepy/issues/436/disabled-coverage-ctracer-may-rise-from


.. _changes_401:

Version 4.0.1 --- 2015-10-13
----------------------------

- When combining data files, unreadable files will now generate a warning
  instead of failing the command.  This is more in line with the older
  coverage.py v3.7.1 behavior, which silently ignored unreadable files.
  Prompted by `issue 418`_.

- The --skip-covered option would skip reporting on 100% covered files, but
  also skipped them when calculating total coverage.  This was wrong, it should
  only remove lines from the report, not change the final answer.  This is now
  fixed, closing `issue 423`_.

- In 4.0, the data file recorded a summary of the system on which it was run.
  Combined data files would keep all of those summaries.  This could lead to
  enormous data files consisting of mostly repetitive useless information. That
  summary is now gone, fixing `issue 415`_.  If you want summary information,
  get in touch, and we'll figure out a better way to do it.

- Test suites that mocked os.path.exists would experience strange failures, due
  to coverage.py using their mock inadvertently.  This is now fixed, closing
  `issue 416`_.

- Importing a ``__init__`` module explicitly would lead to an error:
  ``AttributeError: 'module' object has no attribute '__path__'``, as reported
  in `issue 410`_.  This is now fixed.

- Code that uses ``sys.settrace(sys.gettrace())`` used to incur a more than 2x
  speed penalty.  Now there's no penalty at all. Fixes `issue 397`_.

- Pyexpat C code will no longer be recorded as a source file, fixing
  `issue 419`_.

- The source kit now contains all of the files needed to have a complete source
  tree, re-fixing `issue 137`_ and closing `issue 281`_.

.. _issue 281: https://bitbucket.org/ned/coveragepy/issues/281/supply-scripts-for-testing-in-the
.. _issue 397: https://bitbucket.org/ned/coveragepy/issues/397/stopping-and-resuming-coverage-with
.. _issue 410: https://bitbucket.org/ned/coveragepy/issues/410/attributeerror-module-object-has-no
.. _issue 415: https://bitbucket.org/ned/coveragepy/issues/415/repeated-coveragedataupdates-cause
.. _issue 416: https://bitbucket.org/ned/coveragepy/issues/416/mocking-ospathexists-causes-failures
.. _issue 418: https://bitbucket.org/ned/coveragepy/issues/418/json-parse-error
.. _issue 419: https://bitbucket.org/ned/coveragepy/issues/419/nosource-no-source-for-code-path-to-c
.. _issue 423: https://bitbucket.org/ned/coveragepy/issues/423/skip_covered-changes-reported-total


.. _changes_40:

Version 4.0 --- 2015-09-20
--------------------------

No changes from 4.0b3


Version 4.0b3 --- 2015-09-07
----------------------------

- Reporting on an unmeasured file would fail with a traceback.  This is now
  fixed, closing `issue 403`_.

- The Jenkins ShiningPanda_ plugin looks for an obsolete file name to find the
  HTML reports to publish, so it was failing under coverage.py 4.0.  Now we
  create that file if we are running under Jenkins, to keep things working
  smoothly. `issue 404`_.

- Kits used to include tests and docs, but didn't install them anywhere, or
  provide all of the supporting tools to make them useful.  Kits no longer
  include tests and docs.  If you were using them from the older packages, get
  in touch and help me understand how.

.. _issue 403: https://bitbucket.org/ned/coveragepy/issues/403/hasherupdate-fails-with-typeerror-nonetype
.. _issue 404: https://bitbucket.org/ned/coveragepy/issues/404/shiningpanda-jenkins-plugin-cant-find-html


Version 4.0b2 --- 2015-08-22
----------------------------

- 4.0b1 broke ``--append`` creating new data files.  This is now fixed, closing
  `issue 392`_.

- ``py.test --cov`` can write empty data, then touch files due to ``--source``,
  which made coverage.py mistakenly force the data file to record lines instead
  of arcs.  This would lead to a "Can't combine line data with arc data" error
  message.  This is now fixed, and changed some method names in the
  CoverageData interface.  Fixes `issue 399`_.

- `CoverageData.read_fileobj` and `CoverageData.write_fileobj` replace the
  `.read` and `.write` methods, and are now properly inverses of each other.

- When using ``report --skip-covered``, a message will now be included in the
  report output indicating how many files were skipped, and if all files are
  skipped, coverage.py won't accidentally scold you for having no data to
  report.  Thanks, Krystian Kichewko.

- A new conversion utility has been added:  ``python -m coverage.pickle2json``
  will convert v3.x pickle data files to v4.x JSON data files.  Thanks,
  Alexander Todorov.  Closes `issue 395`_.

- A new version identifier is available, `coverage.version_info`, a plain tuple
  of values similar to `sys.version_info`_.

.. _issue 392: https://bitbucket.org/ned/coveragepy/issues/392/run-append-doesnt-create-coverage-file
.. _issue 395: https://bitbucket.org/ned/coveragepy/issues/395/rfe-read-pickled-files-as-well-for
.. _issue 399: https://bitbucket.org/ned/coveragepy/issues/399/coverageexception-cant-combine-line-data
.. _sys.version_info: https://docs.python.org/3/library/sys.html#sys.version_info


Version 4.0b1 --- 2015-08-02
----------------------------

- Coverage.py is now licensed under the Apache 2.0 license.  See NOTICE.txt for
  details.  Closes `issue 313`_.

- The data storage has been completely revamped.  The data file is now
  JSON-based instead of a pickle, closing `issue 236`_.  The `CoverageData`
  class is now a public supported documented API to the data file.

- A new configuration option, ``[run] note``, lets you set a note that will be
  stored in the `runs` section of the data file.  You can use this to annotate
  the data file with any information you like.

- Unrecognized configuration options will now print an error message and stop
  coverage.py.  This should help prevent configuration mistakes from passing
  silently.  Finishes `issue 386`_.

- In parallel mode, ``coverage erase`` will now delete all of the data files,
  fixing `issue 262`_.

- Coverage.py now accepts a directory name for ``coverage run`` and will run a
  ``__main__.py`` found there, just like Python will.  Fixes `issue 252`_.
  Thanks, Dmitry Trofimov.

- The XML report now includes a ``missing-branches`` attribute.  Thanks, Steve
  Peak.  This is not a part of the Cobertura DTD, so the XML report no longer
  references the DTD.

- Missing branches in the HTML report now have a bit more information in the
  right-hand annotations.  Hopefully this will make their meaning clearer.

- All the reporting functions now behave the same if no data had been
  collected, exiting with a status code of 1.  Fixed ``fail_under`` to be
  applied even when the report is empty.  Thanks, Ionel Cristian Mărieș.

- Plugins are now initialized differently.  Instead of looking for a class
  called ``Plugin``, coverage.py looks for a function called ``coverage_init``.

- A file-tracing plugin can now ask to have built-in Python reporting by
  returning `"python"` from its `file_reporter()` method.

- Code that was executed with `exec` would be mis-attributed to the file that
  called it.  This is now fixed, closing `issue 380`_.

- The ability to use item access on `Coverage.config` (introduced in 4.0a2) has
  been changed to a more explicit `Coverage.get_option` and
  `Coverage.set_option` API.

- The ``Coverage.use_cache`` method is no longer supported.

- The private method ``Coverage._harvest_data`` is now called
  ``Coverage.get_data``, and returns the ``CoverageData`` containing the
  collected data.

- The project is consistently referred to as "coverage.py" throughout the code
  and the documentation, closing `issue 275`_.

- Combining data files with an explicit configuration file was broken in 4.0a6,
  but now works again, closing `issue 385`_.

- ``coverage combine`` now accepts files as well as directories.

- The speed is back to 3.7.1 levels, after having slowed down due to plugin
  support, finishing up `issue 387`_.

.. _issue 236: https://bitbucket.org/ned/coveragepy/issues/236/pickles-are-bad-and-you-should-feel-bad
.. _issue 252: https://bitbucket.org/ned/coveragepy/issues/252/coverage-wont-run-a-program-with
.. _issue 262: https://bitbucket.org/ned/coveragepy/issues/262/when-parallel-true-erase-should-erase-all
.. _issue 275: https://bitbucket.org/ned/coveragepy/issues/275/refer-consistently-to-project-as-coverage
.. _issue 313: https://bitbucket.org/ned/coveragepy/issues/313/add-license-file-containing-2-3-or-4
.. _issue 380: https://bitbucket.org/ned/coveragepy/issues/380/code-executed-by-exec-excluded-from
.. _issue 385: https://bitbucket.org/ned/coveragepy/issues/385/coverage-combine-doesnt-work-with-rcfile
.. _issue 386: https://bitbucket.org/ned/coveragepy/issues/386/error-on-unrecognised-configuration
.. _issue 387: https://bitbucket.org/ned/coveragepy/issues/387/performance-degradation-from-371-to-40

.. 40 issues closed in 4.0 below here


Version 4.0a6 --- 2015-06-21
----------------------------

- Python 3.5b2 and PyPy 2.6.0 are supported.

- The original module-level function interface to coverage.py is no longer
  supported.  You must now create a ``coverage.Coverage`` object, and use
  methods on it.

- The ``coverage combine`` command now accepts any number of directories as
  arguments, and will combine all the data files from those directories.  This
  means you don't have to copy the files to one directory before combining.
  Thanks, Christine Lytwynec.  Finishes `issue 354`_.

- Branch coverage couldn't properly handle certain extremely long files. This
  is now fixed (`issue 359`_).

- Branch coverage didn't understand yield statements properly.  Mickie Betz
  persisted in pursuing this despite Ned's pessimism.  Fixes `issue 308`_ and
  `issue 324`_.

- The COVERAGE_DEBUG environment variable can be used to set the
  ``[run] debug`` configuration option to control what internal operations are
  logged.

- HTML reports were truncated at formfeed characters.  This is now fixed
  (`issue 360`_).  It's always fun when the problem is due to a `bug in the
  Python standard library <http://bugs.python.org/issue19035>`_.

- Files with incorrect encoding declaration comments are no longer ignored by
  the reporting commands, fixing `issue 351`_.

- HTML reports now include a timestamp in the footer, closing `issue 299`_.
  Thanks, Conrad Ho.

- HTML reports now begrudgingly use double-quotes rather than single quotes,
  because there are "software engineers" out there writing tools that read HTML
  and somehow have no idea that single quotes exist.  Capitulates to the absurd
  `issue 361`_.  Thanks, Jon Chappell.

- The ``coverage annotate`` command now handles non-ASCII characters properly,
  closing `issue 363`_.  Thanks, Leonardo Pistone.

- Drive letters on Windows were not normalized correctly, now they are. Thanks,
  Ionel Cristian Mărieș.

- Plugin support had some bugs fixed, closing `issue 374`_ and `issue 375`_.
  Thanks, Stefan Behnel.

.. _issue 299: https://bitbucket.org/ned/coveragepy/issues/299/inserted-created-on-yyyy-mm-dd-hh-mm-in
.. _issue 308: https://bitbucket.org/ned/coveragepy/issues/308/yield-lambda-branch-coverage
.. _issue 324: https://bitbucket.org/ned/coveragepy/issues/324/yield-in-loop-confuses-branch-coverage
.. _issue 351: https://bitbucket.org/ned/coveragepy/issues/351/files-with-incorrect-encoding-are-ignored
.. _issue 354: https://bitbucket.org/ned/coveragepy/issues/354/coverage-combine-should-take-a-list-of
.. _issue 359: https://bitbucket.org/ned/coveragepy/issues/359/xml-report-chunk-error
.. _issue 360: https://bitbucket.org/ned/coveragepy/issues/360/html-reports-get-confused-by-l-in-the-code
.. _issue 361: https://bitbucket.org/ned/coveragepy/issues/361/use-double-quotes-in-html-output-to
.. _issue 363: https://bitbucket.org/ned/coveragepy/issues/363/annotate-command-hits-unicode-happy-fun
.. _issue 374: https://bitbucket.org/ned/coveragepy/issues/374/c-tracer-lookups-fail-in
.. _issue 375: https://bitbucket.org/ned/coveragepy/issues/375/ctracer_handle_return-reads-byte-code


Version 4.0a5 --- 2015-02-16
----------------------------

- Plugin support is now implemented in the C tracer instead of the Python
  tracer. This greatly improves the speed of tracing projects using plugins.

- Coverage.py now always adds the current directory to sys.path, so that
  plugins can import files in the current directory (`issue 358`_).

- If the `config_file` argument to the Coverage constructor is specified as
  ".coveragerc", it is treated as if it were True.  This means setup.cfg is
  also examined, and a missing file is not considered an error (`issue 357`_).

- Wildly experimental: support for measuring processes started by the
  multiprocessing module.  To use, set ``--concurrency=multiprocessing``,
  either on the command line or in the .coveragerc file (`issue 117`_). Thanks,
  Eduardo Schettino.  Currently, this does not work on Windows.

- A new warning is possible, if a desired file isn't measured because it was
  imported before coverage.py was started (`issue 353`_).

- The `coverage.process_startup` function now will start coverage measurement
  only once, no matter how many times it is called.  This fixes problems due
  to unusual virtualenv configurations (`issue 340`_).

- Added 3.5.0a1 to the list of supported CPython versions.

.. _issue 117: https://bitbucket.org/ned/coveragepy/issues/117/enable-coverage-measurement-of-code-run-by
.. _issue 340: https://bitbucket.org/ned/coveragepy/issues/340/keyerror-subpy
.. _issue 353: https://bitbucket.org/ned/coveragepy/issues/353/40a3-introduces-an-unexpected-third-case
.. _issue 357: https://bitbucket.org/ned/coveragepy/issues/357/behavior-changed-when-coveragerc-is
.. _issue 358: https://bitbucket.org/ned/coveragepy/issues/358/all-coverage-commands-should-adjust


Version 4.0a4 --- 2015-01-25
----------------------------

- Plugins can now provide sys_info for debugging output.

- Started plugins documentation.

- Prepared to move the docs to readthedocs.org.


Version 4.0a3 --- 2015-01-20
----------------------------

- Reports now use file names with extensions.  Previously, a report would
  describe a/b/c.py as "a/b/c".  Now it is shown as "a/b/c.py".  This allows
  for better support of non-Python files, and also fixed `issue 69`_.

- The XML report now reports each directory as a package again.  This was a bad
  regression, I apologize.  This was reported in `issue 235`_, which is now
  fixed.

- A new configuration option for the XML report: ``[xml] package_depth``
  controls which directories are identified as packages in the report.
  Directories deeper than this depth are not reported as packages.
  The default is that all directories are reported as packages.
  Thanks, Lex Berezhny.

- When looking for the source for a frame, check if the file exists. On
  Windows, .pyw files are no longer recorded as .py files. Along the way, this
  fixed `issue 290`_.

- Empty files are now reported as 100% covered in the XML report, not 0%
  covered (`issue 345`_).

- Regexes in the configuration file are now compiled as soon as they are read,
  to provide error messages earlier (`issue 349`_).

.. _issue 69: https://bitbucket.org/ned/coveragepy/issues/69/coverage-html-overwrite-files-that-doesnt
.. _issue 235: https://bitbucket.org/ned/coveragepy/issues/235/package-name-is-missing-in-xml-report
.. _issue 290: https://bitbucket.org/ned/coveragepy/issues/290/running-programmatically-with-pyw-files
.. _issue 345: https://bitbucket.org/ned/coveragepy/issues/345/xml-reports-line-rate-0-for-empty-files
.. _issue 349: https://bitbucket.org/ned/coveragepy/issues/349/bad-regex-in-config-should-get-an-earlier


Version 4.0a2 --- 2015-01-14
----------------------------

- Officially support PyPy 2.4, and PyPy3 2.4.  Drop support for
  CPython 3.2 and older versions of PyPy.  The code won't work on CPython 3.2.
  It will probably still work on older versions of PyPy, but I'm not testing
  against them.

- Plugins!

- The original command line switches (`-x` to run a program, etc) are no
  longer supported.

- A new option: `coverage report --skip-covered` will reduce the number of
  files reported by skipping files with 100% coverage.  Thanks, Krystian
  Kichewko.  This means that empty `__init__.py` files will be skipped, since
  they are 100% covered, closing `issue 315`_.

- You can now specify the ``--fail-under`` option in the ``.coveragerc`` file
  as the ``[report] fail_under`` option.  This closes `issue 314`_.

- The ``COVERAGE_OPTIONS`` environment variable is no longer supported.  It was
  a hack for ``--timid`` before configuration files were available.

- The HTML report now has filtering.  Type text into the Filter box on the
  index page, and only modules with that text in the name will be shown.
  Thanks, Danny Allen.

- The textual report and the HTML report used to report partial branches
  differently for no good reason.  Now the text report's "missing branches"
  column is a "partial branches" column so that both reports show the same
  numbers.  This closes `issue 342`_.

- If you specify a ``--rcfile`` that cannot be read, you will get an error
  message.  Fixes `issue 343`_.

- The ``--debug`` switch can now be used on any command.

- You can now programmatically adjust the configuration of coverage.py by
  setting items on `Coverage.config` after construction.

- A module run with ``-m`` can be used as the argument to ``--source``, fixing
  `issue 328`_.  Thanks, Buck Evan.

- The regex for matching exclusion pragmas has been fixed to allow more kinds
  of whitespace, fixing `issue 334`_.

- Made some PyPy-specific tweaks to improve speed under PyPy.  Thanks, Alex
  Gaynor.

- In some cases, with a source file missing a final newline, coverage.py would
  count statements incorrectly.  This is now fixed, closing `issue 293`_.

- The status.dat file that HTML reports use to avoid re-creating files that
  haven't changed is now a JSON file instead of a pickle file.  This obviates
  `issue 287`_ and `issue 237`_.

.. _issue 237: https://bitbucket.org/ned/coveragepy/issues/237/htmlcov-with-corrupt-statusdat
.. _issue 287: https://bitbucket.org/ned/coveragepy/issues/287/htmlpy-doesnt-specify-pickle-protocol
.. _issue 293: https://bitbucket.org/ned/coveragepy/issues/293/number-of-statement-detection-wrong-if-no
.. _issue 314: https://bitbucket.org/ned/coveragepy/issues/314/fail_under-param-not-working-in-coveragerc
.. _issue 315: https://bitbucket.org/ned/coveragepy/issues/315/option-to-omit-empty-files-eg-__init__py
.. _issue 328: https://bitbucket.org/ned/coveragepy/issues/328/misbehavior-in-run-source
.. _issue 334: https://bitbucket.org/ned/coveragepy/issues/334/pragma-not-recognized-if-tab-character
.. _issue 342: https://bitbucket.org/ned/coveragepy/issues/342/console-and-html-coverage-reports-differ
.. _issue 343: https://bitbucket.org/ned/coveragepy/issues/343/an-explicitly-named-non-existent-config


Version 4.0a1 --- 2014-09-27
----------------------------

- Python versions supported are now CPython 2.6, 2.7, 3.2, 3.3, and 3.4, and
  PyPy 2.2.

- Gevent, eventlet, and greenlet are now supported, closing `issue 149`_.
  The ``concurrency`` setting specifies the concurrency library in use.  Huge
  thanks to Peter Portante for initial implementation, and to Joe Jevnik for
  the final insight that completed the work.

- Options are now also read from a setup.cfg file, if any.  Sections are
  prefixed with "coverage:", so the ``[run]`` options will be read from the
  ``[coverage:run]`` section of setup.cfg.  Finishes `issue 304`_.

- The ``report -m`` command can now show missing branches when reporting on
  branch coverage.  Thanks, Steve Leonard. Closes `issue 230`_.

- The XML report now contains a <source> element, fixing `issue 94`_.  Thanks
  Stan Hu.

- The class defined in the coverage module is now called ``Coverage`` instead
  of ``coverage``, though the old name still works, for backward compatibility.

- The ``fail-under`` value is now rounded the same as reported results,
  preventing paradoxical results, fixing `issue 284`_.

- The XML report will now create the output directory if need be, fixing
  `issue 285`_.  Thanks, Chris Rose.

- HTML reports no longer raise UnicodeDecodeError if a Python file has
  undecodable characters, fixing `issue 303`_ and `issue 331`_.

- The annotate command will now annotate all files, not just ones relative to
  the current directory, fixing `issue 57`_.

- The coverage module no longer causes deprecation warnings on Python 3.4 by
  importing the imp module, fixing `issue 305`_.

- Encoding declarations in source files are only considered if they are truly
  comments.  Thanks, Anthony Sottile.

.. _issue 57: https://bitbucket.org/ned/coveragepy/issues/57/annotate-command-fails-to-annotate-many
.. _issue 94: https://bitbucket.org/ned/coveragepy/issues/94/coverage-xml-doesnt-produce-sources
.. _issue 149: https://bitbucket.org/ned/coveragepy/issues/149/coverage-gevent-looks-broken
.. _issue 230: https://bitbucket.org/ned/coveragepy/issues/230/show-line-no-for-missing-branches-in
.. _issue 284: https://bitbucket.org/ned/coveragepy/issues/284/fail-under-should-show-more-precision
.. _issue 285: https://bitbucket.org/ned/coveragepy/issues/285/xml-report-fails-if-output-file-directory
.. _issue 303: https://bitbucket.org/ned/coveragepy/issues/303/unicodedecodeerror
.. _issue 304: https://bitbucket.org/ned/coveragepy/issues/304/attempt-to-get-configuration-from-setupcfg
.. _issue 305: https://bitbucket.org/ned/coveragepy/issues/305/pendingdeprecationwarning-the-imp-module
.. _issue 331: https://bitbucket.org/ned/coveragepy/issues/331/failure-of-encoding-detection-on-python2


.. _changes_371:

Version 3.7.1 --- 2013-12-13
----------------------------

- Improved the speed of HTML report generation by about 20%.

- Fixed the mechanism for finding OS-installed static files for the HTML report
  so that it will actually find OS-installed static files.


.. _changes_37:

Version 3.7 --- 2013-10-06
--------------------------

- Added the ``--debug`` switch to ``coverage run``.  It accepts a list of
  options indicating the type of internal activity to log to stderr.

- Improved the branch coverage facility, fixing `issue 92`_ and `issue 175`_.

- Running code with ``coverage run -m`` now behaves more like Python does,
  setting sys.path properly, which fixes `issue 207`_ and `issue 242`_.

- Coverage.py can now run .pyc files directly, closing `issue 264`_.

- Coverage.py properly supports .pyw files, fixing `issue 261`_.

- Omitting files within a tree specified with the ``source`` option would
  cause them to be incorrectly marked as unexecuted, as described in
  `issue 218`_.  This is now fixed.

- When specifying paths to alias together during data combining, you can now
  specify relative paths, fixing `issue 267`_.

- Most file paths can now be specified with username expansion (``~/src``, or
  ``~build/src``, for example), and with environment variable expansion
  (``build/$BUILDNUM/src``).

- Trying to create an XML report with no files to report on, would cause a
  ZeroDivideError, but no longer does, fixing `issue 250`_.

- When running a threaded program under the Python tracer, coverage.py no
  longer issues a spurious warning about the trace function changing: "Trace
  function changed, measurement is likely wrong: None."  This fixes `issue
  164`_.

- Static files necessary for HTML reports are found in system-installed places,
  to ease OS-level packaging of coverage.py.  Closes `issue 259`_.

- Source files with encoding declarations, but a blank first line, were not
  decoded properly.  Now they are.  Thanks, Roger Hu.

- The source kit now includes the ``__main__.py`` file in the root coverage
  directory, fixing `issue 255`_.

.. _issue 92: https://bitbucket.org/ned/coveragepy/issues/92/finally-clauses-arent-treated-properly-in
.. _issue 164: https://bitbucket.org/ned/coveragepy/issues/164/trace-function-changed-warning-when-using
.. _issue 175: https://bitbucket.org/ned/coveragepy/issues/175/branch-coverage-gets-confused-in-certain
.. _issue 207: https://bitbucket.org/ned/coveragepy/issues/207/run-m-cannot-find-module-or-package-in
.. _issue 242: https://bitbucket.org/ned/coveragepy/issues/242/running-a-two-level-package-doesnt-work
.. _issue 218: https://bitbucket.org/ned/coveragepy/issues/218/run-command-does-not-respect-the-omit-flag
.. _issue 250: https://bitbucket.org/ned/coveragepy/issues/250/uncaught-zerodivisionerror-when-generating
.. _issue 255: https://bitbucket.org/ned/coveragepy/issues/255/directory-level-__main__py-not-included-in
.. _issue 259: https://bitbucket.org/ned/coveragepy/issues/259/allow-use-of-system-installed-third-party
.. _issue 261: https://bitbucket.org/ned/coveragepy/issues/261/pyw-files-arent-reported-properly
.. _issue 264: https://bitbucket.org/ned/coveragepy/issues/264/coverage-wont-run-pyc-files
.. _issue 267: https://bitbucket.org/ned/coveragepy/issues/267/relative-path-aliases-dont-work


.. _changes_36:

Version 3.6 --- 2013-01-05
--------------------------

- Added a page to the docs about troublesome situations, closing `issue 226`_,
  and added some info to the TODO file, closing `issue 227`_.

.. _issue 226: https://bitbucket.org/ned/coveragepy/issues/226/make-readme-section-to-describe-when
.. _issue 227: https://bitbucket.org/ned/coveragepy/issues/227/update-todo


Version 3.6b3 --- 2012-12-29
----------------------------

- Beta 2 broke the nose plugin. It's fixed again, closing `issue 224`_.

.. _issue 224: https://bitbucket.org/ned/coveragepy/issues/224/36b2-breaks-nosexcover


Version 3.6b2 --- 2012-12-23
----------------------------

- Coverage.py runs on Python 2.3 and 2.4 again. It was broken in 3.6b1.

- The C extension is optionally compiled using a different more widely-used
  technique, taking another stab at fixing `issue 80`_ once and for all.

- Combining data files would create entries for phantom files if used with
  ``source`` and path aliases.  It no longer does.

- ``debug sys`` now shows the configuration file path that was read.

- If an oddly-behaved package claims that code came from an empty-string
  file name, coverage.py no longer associates it with the directory name,
  fixing `issue 221`_.

.. _issue 221: https://bitbucket.org/ned/coveragepy/issues/221/coveragepy-incompatible-with-pyratemp


Version 3.6b1 --- 2012-11-28
----------------------------

- Wildcards in ``include=`` and ``omit=`` arguments were not handled properly
  in reporting functions, though they were when running.  Now they are handled
  uniformly, closing `issue 143`_ and `issue 163`_.  **NOTE**: it is possible
  that your configurations may now be incorrect.  If you use ``include`` or
  ``omit`` during reporting, whether on the command line, through the API, or
  in a configuration file, please check carefully that you were not relying on
  the old broken behavior.

- The **report**, **html**, and **xml** commands now accept a ``--fail-under``
  switch that indicates in the exit status whether the coverage percentage was
  less than a particular value.  Closes `issue 139`_.

- The reporting functions coverage.report(), coverage.html_report(), and
  coverage.xml_report() now all return a float, the total percentage covered
  measurement.

- The HTML report's title can now be set in the configuration file, with the
  ``--title`` switch on the command line, or via the API.

- Configuration files now support substitution of environment variables, using
  syntax like ``${WORD}``.  Closes `issue 97`_.

- Embarrassingly, the ``[xml] output=`` setting in the .coveragerc file simply
  didn't work.  Now it does.

- The XML report now consistently uses file names for the file name attribute,
  rather than sometimes using module names.  Fixes `issue 67`_.
  Thanks, Marcus Cobden.

- Coverage percentage metrics are now computed slightly differently under
  branch coverage.  This means that completely unexecuted files will now
  correctly have 0% coverage, fixing `issue 156`_.  This also means that your
  total coverage numbers will generally now be lower if you are measuring
  branch coverage.

- When installing, now in addition to creating a "coverage" command, two new
  aliases are also installed.  A "coverage2" or "coverage3" command will be
  created, depending on whether you are installing in Python 2.x or 3.x.
  A "coverage-X.Y" command will also be created corresponding to your specific
  version of Python.  Closes `issue 111`_.

- The coverage.py installer no longer tries to bootstrap setuptools or
  Distribute.  You must have one of them installed first, as `issue 202`_
  recommended.

- The coverage.py kit now includes docs (closing `issue 137`_) and tests.

- On Windows, files are now reported in their correct case, fixing `issue 89`_
  and `issue 203`_.

- If a file is missing during reporting, the path shown in the error message
  is now correct, rather than an incorrect path in the current directory.
  Fixes `issue 60`_.

- Running an HTML report in Python 3 in the same directory as an old Python 2
  HTML report would fail with a UnicodeDecodeError. This issue (`issue 193`_)
  is now fixed.

- Fixed yet another error trying to parse non-Python files as Python, this
  time an IndentationError, closing `issue 82`_ for the fourth time...

- If `coverage xml` fails because there is no data to report, it used to
  create a zero-length XML file.  Now it doesn't, fixing `issue 210`_.

- Jython files now work with the ``--source`` option, fixing `issue 100`_.

- Running coverage.py under a debugger is unlikely to work, but it shouldn't
  fail with "TypeError: 'NoneType' object is not iterable".  Fixes `issue
  201`_.

- On some Linux distributions, when installed with the OS package manager,
  coverage.py would report its own code as part of the results.  Now it won't,
  fixing `issue 214`_, though this will take some time to be repackaged by the
  operating systems.

- Docstrings for the legacy singleton methods are more helpful.  Thanks Marius
  Gedminas.  Closes `issue 205`_.

- The pydoc tool can now show documentation for the class `coverage.coverage`.
  Closes `issue 206`_.

- Added a page to the docs about contributing to coverage.py, closing
  `issue 171`_.

- When coverage.py ended unsuccessfully, it may have reported odd errors like
  ``'NoneType' object has no attribute 'isabs'``.  It no longer does,
  so kiss `issue 153`_ goodbye.

.. _issue 60: https://bitbucket.org/ned/coveragepy/issues/60/incorrect-path-to-orphaned-pyc-files
.. _issue 67: https://bitbucket.org/ned/coveragepy/issues/67/xml-report-filenames-may-be-generated
.. _issue 89: https://bitbucket.org/ned/coveragepy/issues/89/on-windows-all-packages-are-reported-in
.. _issue 97: https://bitbucket.org/ned/coveragepy/issues/97/allow-environment-variables-to-be
.. _issue 100: https://bitbucket.org/ned/coveragepy/issues/100/source-directive-doesnt-work-for-packages
.. _issue 111: https://bitbucket.org/ned/coveragepy/issues/111/when-installing-coverage-with-pip-not
.. _issue 137: https://bitbucket.org/ned/coveragepy/issues/137/provide-docs-with-source-distribution
.. _issue 139: https://bitbucket.org/ned/coveragepy/issues/139/easy-check-for-a-certain-coverage-in-tests
.. _issue 143: https://bitbucket.org/ned/coveragepy/issues/143/omit-doesnt-seem-to-work-in-coverage
.. _issue 153: https://bitbucket.org/ned/coveragepy/issues/153/non-existent-filename-triggers
.. _issue 156: https://bitbucket.org/ned/coveragepy/issues/156/a-completely-unexecuted-file-shows-14
.. _issue 163: https://bitbucket.org/ned/coveragepy/issues/163/problem-with-include-and-omit-filename
.. _issue 171: https://bitbucket.org/ned/coveragepy/issues/171/how-to-contribute-and-run-tests
.. _issue 193: https://bitbucket.org/ned/coveragepy/issues/193/unicodedecodeerror-on-htmlpy
.. _issue 201: https://bitbucket.org/ned/coveragepy/issues/201/coverage-using-django-14-with-pydb-on
.. _issue 202: https://bitbucket.org/ned/coveragepy/issues/202/get-rid-of-ez_setuppy-and
.. _issue 203: https://bitbucket.org/ned/coveragepy/issues/203/duplicate-filenames-reported-when-filename
.. _issue 205: https://bitbucket.org/ned/coveragepy/issues/205/make-pydoc-coverage-more-friendly
.. _issue 206: https://bitbucket.org/ned/coveragepy/issues/206/pydoc-coveragecoverage-fails-with-an-error
.. _issue 210: https://bitbucket.org/ned/coveragepy/issues/210/if-theres-no-coverage-data-coverage-xml
.. _issue 214: https://bitbucket.org/ned/coveragepy/issues/214/coveragepy-measures-itself-on-precise


.. _changes_353:

Version 3.5.3 --- 2012-09-29
----------------------------

- Line numbers in the HTML report line up better with the source lines, fixing
  `issue 197`_, thanks Marius Gedminas.

- When specifying a directory as the source= option, the directory itself no
  longer needs to have a ``__init__.py`` file, though its sub-directories do,
  to be considered as source files.

- Files encoded as UTF-8 with a BOM are now properly handled, fixing
  `issue 179`_.  Thanks, Pablo Carballo.

- Fixed more cases of non-Python files being reported as Python source, and
  then not being able to parse them as Python.  Closes `issue 82`_ (again).
  Thanks, Julian Berman.

- Fixed memory leaks under Python 3, thanks, Brett Cannon. Closes `issue 147`_.

- Optimized .pyo files may not have been handled correctly, `issue 195`_.
  Thanks, Marius Gedminas.

- Certain unusually named file paths could have been mangled during reporting,
  `issue 194`_.  Thanks, Marius Gedminas.

- Try to do a better job of the impossible task of detecting when we can't
  build the C extension, fixing `issue 183`_.

- Testing is now done with `tox`_, thanks, Marc Abramowitz.

.. _issue 147: https://bitbucket.org/ned/coveragepy/issues/147/massive-memory-usage-by-ctracer
.. _issue 179: https://bitbucket.org/ned/coveragepy/issues/179/htmlreporter-fails-when-source-file-is
.. _issue 183: https://bitbucket.org/ned/coveragepy/issues/183/install-fails-for-python-23
.. _issue 194: https://bitbucket.org/ned/coveragepy/issues/194/filelocatorrelative_filename-could-mangle
.. _issue 195: https://bitbucket.org/ned/coveragepy/issues/195/pyo-file-handling-in-codeunit
.. _issue 197: https://bitbucket.org/ned/coveragepy/issues/197/line-numbers-in-html-report-do-not-align
.. _tox: https://tox.readthedocs.io/


.. _changes_352:

Version 3.5.2 --- 2012-05-04
----------------------------

No changes since 3.5.2.b1


Version 3.5.2b1 --- 2012-04-29
------------------------------

- The HTML report has slightly tweaked controls: the buttons at the top of
  the page are color-coded to the source lines they affect.

- Custom CSS can be applied to the HTML report by specifying a CSS file as
  the ``extra_css`` configuration value in the ``[html]`` section.

- Source files with custom encodings declared in a comment at the top are now
  properly handled during reporting on Python 2.  Python 3 always handled them
  properly.  This fixes `issue 157`_.

- Backup files left behind by editors are no longer collected by the source=
  option, fixing `issue 168`_.

- If a file doesn't parse properly as Python, we don't report it as an error
  if the file name seems like maybe it wasn't meant to be Python.  This is a
  pragmatic fix for `issue 82`_.

- The ``-m`` switch on ``coverage report``, which includes missing line numbers
  in the summary report, can now be specified as ``show_missing`` in the
  config file.  Closes `issue 173`_.

- When running a module with ``coverage run -m <modulename>``, certain details
  of the execution environment weren't the same as for
  ``python -m <modulename>``.  This had the unfortunate side-effect of making
  ``coverage run -m unittest discover`` not work if you had tests in a
  directory named "test".  This fixes `issue 155`_ and `issue 142`_.

- Now the exit status of your product code is properly used as the process
  status when running ``python -m coverage run ...``.  Thanks, JT Olds.

- When installing into pypy, we no longer attempt (and fail) to compile
  the C tracer function, closing `issue 166`_.

.. _issue 142: https://bitbucket.org/ned/coveragepy/issues/142/executing-python-file-syspath-is-replaced
.. _issue 155: https://bitbucket.org/ned/coveragepy/issues/155/cant-use-coverage-run-m-unittest-discover
.. _issue 157: https://bitbucket.org/ned/coveragepy/issues/157/chokes-on-source-files-with-non-utf-8
.. _issue 166: https://bitbucket.org/ned/coveragepy/issues/166/dont-try-to-compile-c-extension-on-pypy
.. _issue 168: https://bitbucket.org/ned/coveragepy/issues/168/dont-be-alarmed-by-emacs-droppings
.. _issue 173: https://bitbucket.org/ned/coveragepy/issues/173/theres-no-way-to-specify-show-missing-in


.. _changes_351:

Version 3.5.1 --- 2011-09-23
----------------------------

- The ``[paths]`` feature unfortunately didn't work in real world situations
  where you wanted to, you know, report on the combined data.  Now all paths
  stored in the combined file are canonicalized properly.


Version 3.5.1b1 --- 2011-08-28
------------------------------

- When combining data files from parallel runs, you can now instruct
  coverage.py about which directories are equivalent on different machines.  A
  ``[paths]`` section in the configuration file lists paths that are to be
  considered equivalent.  Finishes `issue 17`_.

- for-else constructs are understood better, and don't cause erroneous partial
  branch warnings.  Fixes `issue 122`_.

- Branch coverage for ``with`` statements is improved, fixing `issue 128`_.

- The number of partial branches reported on the HTML summary page was
  different than the number reported on the individual file pages.  This is
  now fixed.

- An explicit include directive to measure files in the Python installation
  wouldn't work because of the standard library exclusion.  Now the include
  directive takes precedence, and the files will be measured.  Fixes
  `issue 138`_.

- The HTML report now handles Unicode characters in Python source files
  properly.  This fixes `issue 124`_ and `issue 144`_. Thanks, Devin
  Jeanpierre.

- In order to help the core developers measure the test coverage of the
  standard library, Brandon Rhodes devised an aggressive hack to trick Python
  into running some coverage.py code before anything else in the process.
  See the coverage/fullcoverage directory if you are interested.

.. _issue 17: https://bitbucket.org/ned/coveragepy/issues/17/support-combining-coverage-data-from
.. _issue 122: https://bitbucket.org/ned/coveragepy/issues/122/for-else-always-reports-missing-branch
.. _issue 124: https://bitbucket.org/ned/coveragepy/issues/124/no-arbitrary-unicode-in-html-reports-in
.. _issue 128: https://bitbucket.org/ned/coveragepy/issues/128/branch-coverage-of-with-statement-in-27
.. _issue 138: https://bitbucket.org/ned/coveragepy/issues/138/include-should-take-precedence-over-is
.. _issue 144: https://bitbucket.org/ned/coveragepy/issues/144/failure-generating-html-output-for


.. _changes_35:

Version 3.5 --- 2011-06-29
--------------------------

- The HTML report hotkeys now behave slightly differently when the current
  chunk isn't visible at all:  a chunk on the screen will be selected,
  instead of the old behavior of jumping to the literal next chunk.
  The hotkeys now work in Google Chrome.  Thanks, Guido van Rossum.


Version 3.5b1 --- 2011-06-05
----------------------------

- The HTML report now has hotkeys.  Try ``n``, ``s``, ``m``, ``x``, ``b``,
  ``p``, and ``c`` on the overview page to change the column sorting.
  On a file page, ``r``, ``m``, ``x``, and ``p`` toggle the run, missing,
  excluded, and partial line markings.  You can navigate the highlighted
  sections of code by using the ``j`` and ``k`` keys for next and previous.
  The ``1`` (one) key jumps to the first highlighted section in the file,
  and ``0`` (zero) scrolls to the top of the file.

- The ``--omit`` and ``--include`` switches now interpret their values more
  usefully.  If the value starts with a wildcard character, it is used as-is.
  If it does not, it is interpreted relative to the current directory.
  Closes `issue 121`_.

- Partial branch warnings can now be pragma'd away.  The configuration option
  ``partial_branches`` is a list of regular expressions.  Lines matching any of
  those expressions will never be marked as a partial branch.  In addition,
  there's a built-in list of regular expressions marking statements which
  should never be marked as partial.  This list includes ``while True:``,
  ``while 1:``, ``if 1:``, and ``if 0:``.

- The ``coverage()`` constructor accepts single strings for the ``omit=`` and
  ``include=`` arguments, adapting to a common error in programmatic use.

- Modules can now be run directly using ``coverage run -m modulename``, to
  mirror Python's ``-m`` flag.  Closes `issue 95`_, thanks, Brandon Rhodes.

- ``coverage run`` didn't emulate Python accurately in one small detail: the
  current directory inserted into ``sys.path`` was relative rather than
  absolute. This is now fixed.

- HTML reporting is now incremental: a record is kept of the data that
  produced the HTML reports, and only files whose data has changed will
  be generated.  This should make most HTML reporting faster.

- Pathological code execution could disable the trace function behind our
  backs, leading to incorrect code measurement.  Now if this happens,
  coverage.py will issue a warning, at least alerting you to the problem.
  Closes `issue 93`_.  Thanks to Marius Gedminas for the idea.

- The C-based trace function now behaves properly when saved and restored
  with ``sys.gettrace()`` and ``sys.settrace()``.  This fixes `issue 125`_
  and `issue 123`_.  Thanks, Devin Jeanpierre.

- Source files are now opened with Python 3.2's ``tokenize.open()`` where
  possible, to get the best handling of Python source files with encodings.
  Closes `issue 107`_, thanks, Brett Cannon.

- Syntax errors in supposed Python files can now be ignored during reporting
  with the ``-i`` switch just like other source errors.  Closes `issue 115`_.

- Installation from source now succeeds on machines without a C compiler,
  closing `issue 80`_.

- Coverage.py can now be run directly from a working tree by specifying
  the directory name to python:  ``python coverage_py_working_dir run ...``.
  Thanks, Brett Cannon.

- A little bit of Jython support: `coverage run` can now measure Jython
  execution by adapting when $py.class files are traced. Thanks, Adi Roiban.
  Jython still doesn't provide the Python libraries needed to make
  coverage reporting work, unfortunately.

- Internally, files are now closed explicitly, fixing `issue 104`_.  Thanks,
  Brett Cannon.

.. _issue 80: https://bitbucket.org/ned/coveragepy/issues/80/is-there-a-duck-typing-way-to-know-we-cant
.. _issue 93: https://bitbucket.org/ned/coveragepy/issues/93/copying-a-mock-object-breaks-coverage
.. _issue 95: https://bitbucket.org/ned/coveragepy/issues/95/run-subcommand-should-take-a-module-name
.. _issue 104: https://bitbucket.org/ned/coveragepy/issues/104/explicitly-close-files
.. _issue 107: https://bitbucket.org/ned/coveragepy/issues/107/codeparser-not-opening-source-files-with
.. _issue 115: https://bitbucket.org/ned/coveragepy/issues/115/fail-gracefully-when-reporting-on-file
.. _issue 121: https://bitbucket.org/ned/coveragepy/issues/121/filename-patterns-are-applied-stupidly
.. _issue 123: https://bitbucket.org/ned/coveragepy/issues/123/pyeval_settrace-used-in-way-that-breaks
.. _issue 125: https://bitbucket.org/ned/coveragepy/issues/125/coverage-removes-decoratortoolss-tracing


.. _changes_34:

Version 3.4 --- 2010-09-19
--------------------------

- The XML report is now sorted by package name, fixing `issue 88`_.

- Programs that exited with ``sys.exit()`` with no argument weren't handled
  properly, producing a coverage.py stack trace.  That is now fixed.

.. _issue 88: https://bitbucket.org/ned/coveragepy/issues/88/xml-report-lists-packages-in-random-order


Version 3.4b2 --- 2010-09-06
----------------------------

- Completely unexecuted files can now be included in coverage results, reported
  as 0% covered.  This only happens if the --source option is specified, since
  coverage.py needs guidance about where to look for source files.

- The XML report output now properly includes a percentage for branch coverage,
  fixing `issue 65`_ and `issue 81`_.

- Coverage percentages are now displayed uniformly across reporting methods.
  Previously, different reports could round percentages differently.  Also,
  percentages are only reported as 0% or 100% if they are truly 0 or 100, and
  are rounded otherwise.  Fixes `issue 41`_ and `issue 70`_.

- The precision of reported coverage percentages can be set with the
  ``[report] precision`` config file setting.  Completes `issue 16`_.

- Threads derived from ``threading.Thread`` with an overridden `run` method
  would report no coverage for the `run` method.  This is now fixed, closing
  `issue 85`_.

.. _issue 16: https://bitbucket.org/ned/coveragepy/issues/16/allow-configuration-of-accuracy-of-percentage-totals
.. _issue 41: https://bitbucket.org/ned/coveragepy/issues/41/report-says-100-when-it-isnt-quite-there
.. _issue 65: https://bitbucket.org/ned/coveragepy/issues/65/branch-option-not-reported-in-cobertura
.. _issue 70: https://bitbucket.org/ned/coveragepy/issues/70/text-report-and-html-report-disagree-on-coverage
.. _issue 81: https://bitbucket.org/ned/coveragepy/issues/81/xml-report-does-not-have-condition-coverage-attribute-for-lines-with-a
.. _issue 85: https://bitbucket.org/ned/coveragepy/issues/85/threadrun-isnt-measured


Version 3.4b1 --- 2010-08-21
----------------------------

- BACKWARD INCOMPATIBILITY: the ``--omit`` and ``--include`` switches now take
  file patterns rather than file prefixes, closing `issue 34`_ and `issue 36`_.

- BACKWARD INCOMPATIBILITY: the `omit_prefixes` argument is gone throughout
  coverage.py, replaced with `omit`, a list of file name patterns suitable for
  `fnmatch`.  A parallel argument `include` controls what files are included.

- The run command now has a ``--source`` switch, a list of directories or
  module names.  If provided, coverage.py will only measure execution in those
  source files.

- Various warnings are printed to stderr for problems encountered during data
  measurement: if a ``--source`` module has no Python source to measure, or is
  never encountered at all, or if no data is collected.

- The reporting commands (report, annotate, html, and xml) now have an
  ``--include`` switch to restrict reporting to modules matching those file
  patterns, similar to the existing ``--omit`` switch. Thanks, Zooko.

- The run command now supports ``--include`` and ``--omit`` to control what
  modules it measures. This can speed execution and reduce the amount of data
  during reporting. Thanks Zooko.

- Since coverage.py 3.1, using the Python trace function has been slower than
  it needs to be.  A cache of tracing decisions was broken, but has now been
  fixed.

- Python 2.7 and 3.2 have introduced new opcodes that are now supported.

- Python files with no statements, for example, empty ``__init__.py`` files,
  are now reported as having zero statements instead of one.  Fixes `issue 1`_.

- Reports now have a column of missed line counts rather than executed line
  counts, since developers should focus on reducing the missed lines to zero,
  rather than increasing the executed lines to varying targets.  Once
  suggested, this seemed blindingly obvious.

- Line numbers in HTML source pages are clickable, linking directly to that
  line, which is highlighted on arrival.  Added a link back to the index page
  at the bottom of each HTML page.

- Programs that call ``os.fork`` will properly collect data from both the child
  and parent processes.  Use ``coverage run -p`` to get two data files that can
  be combined with ``coverage combine``.  Fixes `issue 56`_.

- Coverage.py is now runnable as a module: ``python -m coverage``.  Thanks,
  Brett Cannon.

- When measuring code running in a virtualenv, most of the system library was
  being measured when it shouldn't have been.  This is now fixed.

- Doctest text files are no longer recorded in the coverage data, since they
  can't be reported anyway.  Fixes `issue 52`_ and `issue 61`_.

- Jinja HTML templates compile into Python code using the HTML file name,
  which confused coverage.py.  Now these files are no longer traced, fixing
  `issue 82`_.

- Source files can have more than one dot in them (foo.test.py), and will be
  treated properly while reporting.  Fixes `issue 46`_.

- Source files with DOS line endings are now properly tokenized for syntax
  coloring on non-DOS machines.  Fixes `issue 53`_.

- Unusual code structure that confused exits from methods with exits from
  classes is now properly analyzed.  See `issue 62`_.

- Asking for an HTML report with no files now shows a nice error message rather
  than a cryptic failure ('int' object is unsubscriptable). Fixes `issue 59`_.

.. _issue 1:  https://bitbucket.org/ned/coveragepy/issues/1/empty-__init__py-files-are-reported-as-1-executable
.. _issue 34: https://bitbucket.org/ned/coveragepy/issues/34/enhanced-omit-globbing-handling
.. _issue 36: https://bitbucket.org/ned/coveragepy/issues/36/provide-regex-style-omit
.. _issue 46: https://bitbucket.org/ned/coveragepy/issues/46
.. _issue 53: https://bitbucket.org/ned/coveragepy/issues/53
.. _issue 52: https://bitbucket.org/ned/coveragepy/issues/52/doctesttestfile-confuses-source-detection
.. _issue 56: https://bitbucket.org/ned/coveragepy/issues/56
.. _issue 61: https://bitbucket.org/ned/coveragepy/issues/61/annotate-i-doesnt-work
.. _issue 62: https://bitbucket.org/ned/coveragepy/issues/62
.. _issue 59: https://bitbucket.org/ned/coveragepy/issues/59/html-report-fails-with-int-object-is
.. _issue 82: https://bitbucket.org/ned/coveragepy/issues/82/tokenerror-when-generating-html-report


.. _changes_331:

Version 3.3.1 --- 2010-03-06
----------------------------

- Using `parallel=True` in .coveragerc file prevented reporting, but now does
  not, fixing `issue 49`_.

- When running your code with "coverage run", if you call `sys.exit()`,
  coverage.py will exit with that status code, fixing `issue 50`_.

.. _issue 49: https://bitbucket.org/ned/coveragepy/issues/49
.. _issue 50: https://bitbucket.org/ned/coveragepy/issues/50


.. _changes_33:

Version 3.3 --- 2010-02-24
--------------------------

- Settings are now read from a .coveragerc file.  A specific file can be
  specified on the command line with --rcfile=FILE.  The name of the file can
  be programmatically set with the `config_file` argument to the coverage()
  constructor, or reading a config file can be disabled with
  `config_file=False`.

- Fixed a problem with nested loops having their branch possibilities
  mischaracterized: `issue 39`_.

- Added coverage.process_start to enable coverage measurement when Python
  starts.

- Parallel data file names now have a random number appended to them in
  addition to the machine name and process id.

- Parallel data files combined with "coverage combine" are deleted after
  they're combined, to clean up unneeded files.  Fixes `issue 40`_.

- Exceptions thrown from product code run with "coverage run" are now displayed
  without internal coverage.py frames, so the output is the same as when the
  code is run without coverage.py.

- The `data_suffix` argument to the coverage constructor is now appended with
  an added dot rather than simply appended, so that .coveragerc files will not
  be confused for data files.

- Python source files that don't end with a newline can now be executed, fixing
  `issue 47`_.

- Added an AUTHORS.txt file.

.. _issue 39: https://bitbucket.org/ned/coveragepy/issues/39
.. _issue 40: https://bitbucket.org/ned/coveragepy/issues/40
.. _issue 47: https://bitbucket.org/ned/coveragepy/issues/47


.. _changes_32:

Version 3.2 --- 2009-12-05
--------------------------

- Added a ``--version`` option on the command line.


Version 3.2b4 --- 2009-12-01
----------------------------

- Branch coverage improvements:

  - The XML report now includes branch information.

- Click-to-sort HTML report columns are now persisted in a cookie.  Viewing
  a report will sort it first the way you last had a coverage report sorted.
  Thanks, `Chris Adams`_.

- On Python 3.x, setuptools has been replaced by `Distribute`_.

.. _Distribute: https://pypi.org/project/distribute/


Version 3.2b3 --- 2009-11-23
----------------------------

- Fixed a memory leak in the C tracer that was introduced in 3.2b1.

- Branch coverage improvements:

  - Branches to excluded code are ignored.

- The table of contents in the HTML report is now sortable: click the headers
  on any column.  Thanks, `Chris Adams`_.

.. _Chris Adams: http://chris.improbable.org


Version 3.2b2 --- 2009-11-19
----------------------------

- Branch coverage improvements:

  - Classes are no longer incorrectly marked as branches: `issue 32`_.

  - "except" clauses with types are no longer incorrectly marked as branches:
    `issue 35`_.

- Fixed some problems syntax coloring sources with line continuations and
  source with tabs: `issue 30`_ and `issue 31`_.

- The --omit option now works much better than before, fixing `issue 14`_ and
  `issue 33`_.  Thanks, Danek Duvall.

.. _issue 14: https://bitbucket.org/ned/coveragepy/issues/14
.. _issue 30: https://bitbucket.org/ned/coveragepy/issues/30
.. _issue 31: https://bitbucket.org/ned/coveragepy/issues/31
.. _issue 32: https://bitbucket.org/ned/coveragepy/issues/32
.. _issue 33: https://bitbucket.org/ned/coveragepy/issues/33
.. _issue 35: https://bitbucket.org/ned/coveragepy/issues/35


Version 3.2b1 --- 2009-11-10
----------------------------

- Branch coverage!

- XML reporting has file paths that let Cobertura find the source code.

- The tracer code has changed, it's a few percent faster.

- Some exceptions reported by the command line interface have been cleaned up
  so that tracebacks inside coverage.py aren't shown.  Fixes `issue 23`_.

.. _issue 23: https://bitbucket.org/ned/coveragepy/issues/23


.. _changes_31:

Version 3.1 --- 2009-10-04
--------------------------

- Source code can now be read from eggs.  Thanks, Ross Lawley.  Fixes
  `issue 25`_.

.. _issue 25: https://bitbucket.org/ned/coveragepy/issues/25


Version 3.1b1 --- 2009-09-27
----------------------------

- Python 3.1 is now supported.

- Coverage.py has a new command line syntax with sub-commands.  This expands
  the possibilities for adding features and options in the future.  The old
  syntax is still supported.  Try "coverage help" to see the new commands.
  Thanks to Ben Finney for early help.

- Added an experimental "coverage xml" command for producing coverage reports
  in a Cobertura-compatible XML format.  Thanks, Bill Hart.

- Added the --timid option to enable a simpler slower trace function that works
  for DecoratorTools projects, including TurboGears.  Fixed `issue 12`_ and
  `issue 13`_.

- HTML reports show modules from other directories.  Fixed `issue 11`_.

- HTML reports now display syntax-colored Python source.

- Programs that change directory will still write .coverage files in the
  directory where execution started.  Fixed `issue 24`_.

- Added a "coverage debug" command for getting diagnostic information about the
  coverage.py installation.

.. _issue 11: https://bitbucket.org/ned/coveragepy/issues/11
.. _issue 12: https://bitbucket.org/ned/coveragepy/issues/12
.. _issue 13: https://bitbucket.org/ned/coveragepy/issues/13
.. _issue 24: https://bitbucket.org/ned/coveragepy/issues/24


.. _changes_301:

Version 3.0.1 --- 2009-07-07
----------------------------

- Removed the recursion limit in the tracer function.  Previously, code that
  ran more than 500 frames deep would crash. Fixed `issue 9`_.

- Fixed a bizarre problem involving pyexpat, whereby lines following XML parser
  invocations could be overlooked.  Fixed `issue 10`_.

- On Python 2.3, coverage.py could mis-measure code with exceptions being
  raised.  This is now fixed.

- The coverage.py code itself will now not be measured by coverage.py, and no
  coverage.py modules will be mentioned in the nose --with-cover plug-in.
  Fixed `issue 8`_.

- When running source files, coverage.py now opens them in universal newline
  mode just like Python does.  This lets it run Windows files on Mac, for
  example.

.. _issue 9: https://bitbucket.org/ned/coveragepy/issues/9
.. _issue 10: https://bitbucket.org/ned/coveragepy/issues/10
.. _issue 8: https://bitbucket.org/ned/coveragepy/issues/8


.. _changes_30:

Version 3.0 --- 2009-06-13
--------------------------

- Fixed the way the Python library was ignored.  Too much code was being
  excluded the old way.

- Tabs are now properly converted in HTML reports.  Previously indentation was
  lost.  Fixed `issue 6`_.

- Nested modules now get a proper flat_rootname.  Thanks, Christian Heimes.

.. _issue 6: https://bitbucket.org/ned/coveragepy/issues/6


Version 3.0b3 --- 2009-05-16
----------------------------

- Added parameters to coverage.__init__ for options that had been set on the
  coverage object itself.

- Added clear_exclude() and get_exclude_list() methods for programmatic
  manipulation of the exclude regexes.

- Added coverage.load() to read previously-saved data from the data file.

- Improved the finding of code files.  For example, .pyc files that have been
  installed after compiling are now located correctly.  Thanks, Detlev
  Offenbach.

- When using the object API (that is, constructing a coverage() object), data
  is no longer saved automatically on process exit.  You can re-enable it with
  the auto_data=True parameter on the coverage() constructor. The module-level
  interface still uses automatic saving.


Version 3.0b --- 2009-04-30
---------------------------

HTML reporting, and continued refactoring.

- HTML reports and annotation of source files: use the new -b (browser) switch.
  Thanks to George Song for code, inspiration and guidance.

- Code in the Python standard library is not measured by default.  If you need
  to measure standard library code, use the -L command-line switch during
  execution, or the cover_pylib=True argument to the coverage() constructor.

- Source annotation into a directory (-a -d) behaves differently.  The
  annotated files are named with their hierarchy flattened so that same-named
  files from different directories no longer collide.  Also, only files in the
  current tree are included.

- coverage.annotate_file is no longer available.

- Programs executed with -x now behave more as they should, for example,
  __file__ has the correct value.

- .coverage data files have a new pickle-based format designed for better
  extensibility.

- Removed the undocumented cache_file argument to coverage.usecache().


Version 3.0b1 --- 2009-03-07
----------------------------

Major overhaul.

- Coverage.py is now a package rather than a module.  Functionality has been
  split into classes.

- The trace function is implemented in C for speed.  Coverage.py runs are now
  much faster.  Thanks to David Christian for productive micro-sprints and
  other encouragement.

- Executable lines are identified by reading the line number tables in the
  compiled code, removing a great deal of complicated analysis code.

- Precisely which lines are considered executable has changed in some cases.
  Therefore, your coverage stats may also change slightly.

- The singleton coverage object is only created if the module-level functions
  are used.  This maintains the old interface while allowing better
  programmatic use of coverage.py.

- The minimum supported Python version is 2.3.


Version 2.85 --- 2008-09-14
---------------------------

- Add support for finding source files in eggs. Don't check for
  morf's being instances of ModuleType, instead use duck typing so that
  pseudo-modules can participate. Thanks, Imri Goldberg.

- Use os.realpath as part of the fixing of file names so that symlinks won't
  confuse things. Thanks, Patrick Mezard.


Version 2.80 --- 2008-05-25
---------------------------

- Open files in rU mode to avoid line ending craziness. Thanks, Edward Loper.


Version 2.78 --- 2007-09-30
---------------------------

- Don't try to predict whether a file is Python source based on the extension.
  Extension-less files are often Pythons scripts. Instead, simply parse the
  file and catch the syntax errors. Hat tip to Ben Finney.


Version 2.77 --- 2007-07-29
---------------------------

- Better packaging.


Version 2.76 --- 2007-07-23
---------------------------

- Now Python 2.5 is *really* fully supported: the body of the new with
  statement is counted as executable.


Version 2.75 --- 2007-07-22
---------------------------

- Python 2.5 now fully supported. The method of dealing with multi-line
  statements is now less sensitive to the exact line that Python reports during
  execution. Pass statements are handled specially so that their disappearance
  during execution won't throw off the measurement.


Version 2.7 --- 2007-07-21
--------------------------

- "#pragma: nocover" is excluded by default.

- Properly ignore docstrings and other constant expressions that appear in the
  middle of a function, a problem reported by Tim Leslie.

- coverage.erase() shouldn't clobber the exclude regex. Change how parallel
  mode is invoked, and fix erase() so that it erases the cache when called
  programmatically.

- In reports, ignore code executed from strings, since we can't do anything
  useful with it anyway.

- Better file handling on Linux, thanks Guillaume Chazarain.

- Better shell support on Windows, thanks Noel O'Boyle.

- Python 2.2 support maintained, thanks Catherine Proulx.

- Minor changes to avoid lint warnings.


Version 2.6 --- 2006-08-23
--------------------------

- Applied Joseph Tate's patch for function decorators.

- Applied Sigve Tjora and Mark van der Wal's fixes for argument handling.

- Applied Geoff Bache's parallel mode patch.

- Refactorings to improve testability. Fixes to command-line logic for parallel
  mode and collect.


Version 2.5 --- 2005-12-04
--------------------------

- Call threading.settrace so that all threads are measured. Thanks Martin
  Fuzzey.

- Add a file argument to report so that reports can be captured to a different
  destination.

- Coverage.py can now measure itself.

- Adapted Greg Rogers' patch for using relative file names, and sorting and
  omitting files to report on.


Version 2.2 --- 2004-12-31
--------------------------

- Allow for keyword arguments in the module global functions. Thanks, Allen.


Version 2.1 --- 2004-12-14
--------------------------

- Return 'analysis' to its original behavior and add 'analysis2'. Add a global
  for 'annotate', and factor it, adding 'annotate_file'.


Version 2.0 --- 2004-12-12
--------------------------

Significant code changes.

- Finding executable statements has been rewritten so that docstrings and
  other quirks of Python execution aren't mistakenly identified as missing
  lines.

- Lines can be excluded from consideration, even entire suites of lines.

- The file system cache of covered lines can be disabled programmatically.

- Modernized the code.


Earlier History
---------------

2001-12-04 GDR Created.

2001-12-06 GDR Added command-line interface and source code annotation.

2001-12-09 GDR Moved design and interface to separate documents.

2001-12-10 GDR Open cache file as binary on Windows. Allow simultaneous -e and
-x, or -a and -r.

2001-12-12 GDR Added command-line help. Cache analysis so that it only needs to
be done once when you specify -a and -r.

2001-12-13 GDR Improved speed while recording. Portable between Python 1.5.2
and 2.1.1.

2002-01-03 GDR Module-level functions work correctly.

2002-01-07 GDR Update sys.path when running a file with the -x option, so that
it matches the value the program would get if it were run on its own.
