.. Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
.. For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

.. _cmd:

==============================
Coverage.py command line usage
==============================

.. highlight:: console

When you install coverage.py, a command-line script simply called ``coverage``
is placed in your Python scripts directory.  To help with multi-version
installs, it will also create either a ``coverage2`` or ``coverage3`` alias,
and a ``coverage-X.Y`` alias, depending on the version of Python you're using.
For example, when installing on Python 3.7, you will be able to use
``coverage``, ``coverage3``, or ``coverage-3.7`` on the command line.

Coverage.py has a number of commands which determine the action performed:

* **run** -- Run a Python program and collect execution data.

* **report** -- Report coverage results.

* **html** -- Produce annotated HTML listings with coverage results.

* **json** -- Produce a JSON report with coverage results.

* **xml** -- Produce an XML report with coverage results.

* **annotate** -- Annotate source files with coverage results.

* **erase** -- Erase previously collected coverage data.

* **combine** -- Combine together a number of data files.

* **debug** -- Get diagnostic information.

Help is available with the **help** command, or with the ``--help`` switch on
any other command::

    $ coverage help
    $ coverage help run
    $ coverage run --help

Version information for coverage.py can be displayed with
``coverage --version``.

Any command can use a configuration file by specifying it with the
``--rcfile=FILE`` command-line switch.  Any option you can set on the command
line can also be set in the configuration file.  This can be a better way to
control coverage.py since the configuration file can be checked into source
control, and can provide options that other invocation techniques (like test
runner plugins) may not offer. See :ref:`config` for more details.


.. _cmd_run:

Execution
---------

You collect execution data by running your Python program with the **run**
command::

    $ coverage run my_program.py arg1 arg2
    blah blah ..your program's output.. blah blah

Your program runs just as if it had been invoked with the Python command line.
Arguments after your file name are passed to your program as usual in
``sys.argv``.  Rather than providing a file name, you can use the ``-m`` switch
and specify an importable module name instead, just as you can with the
Python ``-m`` switch::

    $ coverage run -m packagename.modulename arg1 arg2
    blah blah ..your program's output.. blah blah

.. note::

    In most cases, the program to use here is a test runner, not your program
    you are trying to measure. The test runner will run your tests and coverage
    will measure the coverage of your code along the way.

If you want :ref:`branch coverage <branch>` measurement, use the ``--branch``
flag.  Otherwise only statement coverage is measured.

You can specify the code to measure with the ``--source``, ``--include``, and
``--omit`` switches.  See :ref:`Specifying source files <source_execution>` for
details of their interpretation.  Remember to put options for run after "run",
but before the program invocation::

    $ coverage run --source=dir1,dir2 my_program.py arg1 arg2
    $ coverage run --source=dir1,dir2 -m packagename.modulename arg1 arg2

Coverage.py can measure multi-threaded programs by default. If you are using
more exotic concurrency, with the `multiprocessing`_, `greenlet`_, `eventlet`_,
or `gevent`_ libraries, then coverage.py will get very confused.  Use the
``--concurrency`` switch to properly measure programs using these libraries.
Give it a value of ``multiprocessing``, ``thread``, ``greenlet``, ``eventlet``,
or ``gevent``.  Values other than ``thread`` require the :ref:`C extension
<install_extension>`.

If you are using ``--concurrency=multiprocessing``, you must set other options
in the configuration file.  Options on the command line will not be passed to
the processes that multiprocessing creates.  Best practice is to use the
configuration file for all options.

.. _multiprocessing: https://docs.python.org/3/library/multiprocessing.html
.. _greenlet: https://greenlet.readthedocs.io/
.. _gevent: http://www.gevent.org/
.. _eventlet: http://eventlet.net/

If you are measuring coverage in a multi-process program, or across a number of
machines, you'll want the ``--parallel-mode`` switch to keep the data separate
during measurement.  See :ref:`cmd_combining` below.

You can specify a :ref:`static context <contexts>` for a coverage run with
``--context``.  This can be any label you want, and will be recorded with the
data.  See :ref:`contexts` for more information.

By default, coverage.py does not measure code installed with the Python
interpreter, for example, the standard library. If you want to measure that
code as well as your own, add the ``-L`` (or ``--pylib``) flag.

If your coverage results seem to be overlooking code that you know has been
executed, try running coverage.py again with the ``--timid`` flag.  This uses a
simpler but slower trace method, and might be needed in rare cases.


.. _cmd_warnings:

Warnings
--------

During execution, coverage.py may warn you about conditions it detects that
could affect the measurement process.  The possible warnings include:

* ``Couldn't parse Python file XXX (couldnt-parse)`` |br|
  During reporting, a file was thought to be Python, but it couldn't be parsed
  as Python.

* ``Trace function changed, measurement is likely wrong: XXX (trace-changed)``
  |br|
  Coverage measurement depends on a Python setting called the trace function.
  Other Python code in your product might change that function, which will
  disrupt coverage.py's measurement.  This warning indicates that has happened.
  The XXX in the message is the new trace function value, which might provide
  a clue to the cause.

* ``Module XXX has no Python source (module-not-python)`` |br|
  You asked coverage.py to measure module XXX, but once it was imported, it
  turned out not to have a corresponding .py file.  Without a .py file,
  coverage.py can't report on missing lines.

* ``Module XXX was never imported (module-not-imported)`` |br|
  You asked coverage.py to measure module XXX, but it was never imported by
  your program.

* ``No data was collected (no-data-collected)`` |br|
  Coverage.py ran your program, but didn't measure any lines as executed.
  This could be because you asked to measure only modules that never ran,
  or for other reasons.

* ``Module XXX was previously imported, but not measured
  (module-not-measured)``
  |br|
  You asked coverage.py to measure module XXX, but it had already been imported
  when coverage started.  This meant coverage.py couldn't monitor its
  execution.

* ``Already imported a file that will be measured: XXX (already-imported)``
  |br|
  File XXX had already been imported when coverage.py started measurement. Your
  setting for ``--source`` or ``--include`` indicates that you wanted to
  measure that file.  Lines will be missing from the coverage report since the
  execution during import hadn't been measured.

* ``--include is ignored because --source is set (include-ignored)`` |br|
  Both ``--include`` and ``--source`` were specified while running code.  Both
  are meant to focus measurement on a particular part of your source code, so
  ``--include`` is ignored in favor of ``--source``.

* ``Conflicting dynamic contexts (dynamic-conflict)`` |br|
  The ``[run] dynamic_context`` option is set in the configuration file, but
  something (probably a test runner plugin) is also calling the
  :meth:`.Coverage.switch_context` function to change the context. Only one of
  these mechanisms should be in use at a time.

Individual warnings can be disabled with the `disable_warnings
<config_run_disable_warnings>`_ configuration setting.  To silence "No data was
collected," add this to your .coveragerc file::

    [run]
    disable_warnings = no-data-collected


.. _cmd_datafile:

Data file
---------

Coverage.py collects execution data in a file called ".coverage".  If need be,
you can set a new file name with the COVERAGE_FILE environment variable.  This
can include a path to another directory.

By default, each run of your program starts with an empty data set. If you need
to run your program multiple times to get complete data (for example, because
you need to supply different options), you can accumulate data across runs with
the ``--append`` flag on the **run** command.

To erase the collected data, use the **erase** command::

    $ coverage erase


.. _cmd_combining:

Combining data files
--------------------

Often test suites are run under different conditions, for example, with
different versions of Python, or dependencies, or on different operating
systems.  In these cases, you can collect coverage data for each test run, and
then combine all the separate data files into one combined file for reporting.

The **combine** command reads a number of separate data files, matches the data
by source file name, and writes a combined data file with all of the data.

Coverage normally writes data to a filed named ".coverage".  The ``run
--parallel-mode`` switch (or ``[run] parallel=True`` configuration option)
tells coverage to expand the file name to include machine name, process id, and
a random number so that every data file is distinct::

    .coverage.Neds-MacBook-Pro.local.88335.316857
    .coverage.Geometer.8044.799674

You can also define a new data file name with the ``[run] data_file`` option.

Once you have created a number of these files, you can copy them all to a
single directory, and use the **combine** command to combine them into one
.coverage data file::

    $ coverage combine

You can also name directories or files on the command line::

    $ coverage combine data1.dat windows_data_files/

Coverage.py will collect the data from those places and combine them.  The
current directory isn't searched if you use command-line arguments.  If you
also want data from the current directory, name it explicitly on the command
line.

When coverage.py combines data files, it looks for files named the same as the
data file (defaulting to ".coverage"), with a dotted suffix.  Here are some
examples of data files that can be combined::

    .coverage.machine1
    .coverage.20120807T212300
    .coverage.last_good_run.ok

An existing combined data file is ignored and re-written. If you want to use
**combine** to accumulate results into the .coverage data file over a number of
runs, use the ``--append`` switch on the **combine** command.  This behavior
was the default before version 4.2.

To combine data for a source file, coverage has to find its data in each of the
data files.  Different test runs may run the same source file from different
locations. For example, different operating systems will use different paths
for the same file, or perhaps each Python version is run from a different
subdirectory.  Coverage needs to know that different file paths are actually
the same source file for reporting purposes.

You can tell coverage.py how different source locations relate with a
``[paths]`` section in your configuration file (see :ref:`config_paths`).
It might be more convenient to use the ``[run] relative_files``
setting to store relative file paths (see :ref:`relative_files
<config_run_relative_files>`).

If any of the data files can't be read, coverage.py will print a warning
indicating the file and the problem.


.. _cmd_reporting:

Reporting
---------

Coverage.py provides a few styles of reporting, with the **report**, **html**,
**annotate**, **json**, and **xml** commands.  They share a number of common
options.

The command-line arguments are module or file names to report on, if you'd like
to report on a subset of the data collected.

The ``--include`` and ``--omit`` flags specify lists of file name patterns.
They control which files to report on, and are described in more detail in
:ref:`source`.

The ``-i`` or ``--ignore-errors`` switch tells coverage.py to ignore problems
encountered trying to find source files to report on.  This can be useful if
some files are missing, or if your Python execution is tricky enough that file
names are synthesized without real source files.

If you provide a ``--fail-under`` value, the total percentage covered will be
compared to that value.  If it is less, the command will exit with a status
code of 2, indicating that the total coverage was less than your target.  This
can be used as part of a pass/fail condition, for example in a continuous
integration server.  This option isn't available for **annotate**.


.. _cmd_summary:

Coverage summary
----------------

The simplest reporting is a textual summary produced with **report**::

    $ coverage report
    Name                      Stmts   Miss  Cover
    ---------------------------------------------
    my_program.py                20      4    80%
    my_module.py                 15      2    86%
    my_other_module.py           56      6    89%
    ---------------------------------------------
    TOTAL                        91     12    87%

For each module executed, the report shows the count of executable statements,
the number of those statements missed, and the resulting coverage, expressed
as a percentage.

The ``-m`` flag also shows the line numbers of missing statements::

    $ coverage report -m
    Name                      Stmts   Miss  Cover   Missing
    -------------------------------------------------------
    my_program.py                20      4    80%   33-35, 39
    my_module.py                 15      2    86%   8, 12
    my_other_module.py           56      6    89%   17-23
    -------------------------------------------------------
    TOTAL                        91     12    87%

If you are using branch coverage, then branch statistics will be reported in
the Branch and BrPart (for Partial Branch) columns, the Missing column will
detail the missed branches::

    $ coverage report -m
    Name                      Stmts   Miss Branch BrPart  Cover   Missing
    ---------------------------------------------------------------------
    my_program.py                20      4     10      2    80%   33-35, 36->38, 39
    my_module.py                 15      2      3      0    86%   8, 12
    my_other_module.py           56      6      5      1    89%   17-23, 40->45
    ---------------------------------------------------------------------
    TOTAL                        91     12     18      3    87%

You can restrict the report to only certain files by naming them on the
command line::

    $ coverage report -m my_program.py my_other_module.py
    Name                      Stmts   Miss  Cover   Missing
    -------------------------------------------------------
    my_program.py                20      4    80%   33-35, 39
    my_other_module.py           56      6    89%   17-23
    -------------------------------------------------------
    TOTAL                        76     10    87%

The ``--skip-covered`` switch will skip any file with 100% coverage, letting
you focus on the files that still need attention.  The ``--skip-empty`` switch
will skip any file with no executable statements.

If you have :ref:`recorded contexts <contexts>`, the ``--contexts`` option lets
you choose which contexts to report on.  See :ref:`context_reporting` for
details.

Other common reporting options are described above in :ref:`cmd_reporting`.


.. _cmd_html:

HTML annotation
---------------

Coverage.py can annotate your source code for which lines were executed
and which were not.  The **html** command creates an HTML report similar to the
**report** summary, but as an HTML file.  Each module name links to the source
file decorated to show the status of each line.

Here's a `sample report`__.

__ https://nedbatchelder.com/files/sample_coverage_html/index.html

Lines are highlighted green for executed, red for missing, and gray for
excluded.  The counts at the top of the file are buttons to turn on and off
the highlighting.

A number of keyboard shortcuts are available for navigating the report.
Click the keyboard icon in the upper right to see the complete list.

The title of the report can be set with the ``title`` setting in the
``[html]`` section of the configuration file, or the ``--title`` switch on
the command line.

If you prefer a different style for your HTML report, you can provide your
own CSS file to apply, by specifying a CSS file in the ``[html]`` section of
the configuration file.  See :ref:`config_html` for details.

The ``-d`` argument specifies an output directory, defaulting to "htmlcov"::

    $ coverage html -d coverage_html

Other common reporting options are described above in :ref:`cmd_reporting`.

Generating the HTML report can be time-consuming.  Stored with the HTML report
is a data file that is used to speed up reporting the next time.  If you
generate a new report into the same directory, coverage.py will skip
generating unchanged pages, making the process faster.

The ``--skip-covered`` switch will skip any file with 100% coverage, letting
you focus on the files that still need attention.  The ``--skip-empty`` switch
will skip any file with no executable statements.

If you have :ref:`recorded contexts <contexts>`, the ``--contexts`` option lets
you choose which contexts to report on, and the ``--show-contexts`` option will
annotate lines with the contexts that ran them.  See :ref:`context_reporting`
for details.


.. _cmd_annotation:

Text annotation
---------------

The **annotate** command produces a text annotation of your source code.  With
a ``-d`` argument specifying an output directory, each Python file becomes a
text file in that directory.  Without ``-d``, the files are written into the
same directories as the original Python files.

Coverage status for each line of source is indicated with a character prefix::

    > executed
    ! missing (not executed)
    - excluded

For example::

      # A simple function, never called with x==1

    > def h(x):
          """Silly function."""
    -     if 0:   #pragma: no cover
    -         pass
    >     if x == 1:
    !         a = 1
    >     else:
    >         a = 2

Other common reporting options are described above in :ref:`cmd_reporting`.


.. _cmd_xml:

XML reporting
-------------

The **xml** command writes coverage data to a "coverage.xml" file in a format
compatible with `Cobertura`_.

.. _Cobertura: http://cobertura.github.io/cobertura/

You can specify the name of the output file with the ``-o`` switch.

Other common reporting options are described above in :ref:`cmd_reporting`.


.. _cmd_json:

JSON reporting
--------------

The **json** command writes coverage data to a "coverage.json" file.

You can specify the name of the output file with the ``-o`` switch.  The JSON
can be nicely formatted by specifying the ``--pretty-print`` switch.

Other common reporting options are described above in :ref:`cmd_reporting`.


.. _cmd_debug:

Diagnostics
-----------

The **debug** command shows internal information to help diagnose problems.
If you are reporting a bug about coverage.py, including the output of this
command can often help::

    $ coverage debug sys > please_attach_to_bug_report.txt

Three types of information are available:

* ``config``: show coverage's configuration
* ``sys``: show system configuration
* ``data``: show a summary of the collected coverage data
* ``premain``: show the call stack invoking coverage


.. _cmd_run_debug:

The ``--debug`` option is available on all commands.  It instructs coverage.py
to log internal details of its operation, to help with diagnosing problems.  It
takes a comma-separated list of options, each indicating a facet of operation
to log:

* ``callers``: annotate each debug message with a stack trace of the callers
  to that point.

* ``config``: before starting, dump all the :ref:`configuration <config>`
  values.

* ``dataio``: log when reading or writing any data file.

* ``dataop``: log when data is added to the CoverageData object.

* ``multiproc``: log the start and stop of multiprocessing processes.

* ``pid``: annotate all warnings and debug output with the process and thread
  ids.

* ``plugin``: print information about plugin operations.

* ``process``: show process creation information, and changes in the current
  directory.

* ``self``: annotate each debug message with the object printing the message.

* ``sql``: log the SQL statements used for recording data.

* ``sys``: before starting, dump all the system and environment information,
  as with :ref:`coverage debug sys <cmd_debug>`.

* ``trace``: print every decision about whether to trace a file or not. For
  files not being traced, the reason is also given.

Debug options can also be set with the ``COVERAGE_DEBUG`` environment variable,
a comma-separated list of these options.

The debug output goes to stderr, unless the ``COVERAGE_DEBUG_FILE`` environment
variable names a different file, which will be appended to.
