.. Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
.. For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

.. _config:

=======================
Configuration reference
=======================

Coverage.py options can be specified in a configuration file.  This makes it
easier to re-run coverage.py with consistent settings, and also allows for
specification of options that are otherwise only available in the
:ref:`API <api>`.

Configuration files also make it easier to get coverage testing of spawned
sub-processes.  See :ref:`subprocess` for more details.

The default name for configuration files is ``.coveragerc``, in the same
directory coverage.py is being run in.  Most of the settings in the
configuration file are tied to your source code and how it should be measured,
so it should be stored with your source, and checked into source control,
rather than put in your home directory.

A different location for the configuration file can be specified with the
``--rcfile=FILE`` command line option or with the ``COVERAGE_RCFILE``
environment variable.

Coverage.py will read settings from other usual configuration files if no other
configuration file is used.  It will automatically read from "setup.cfg" or
"tox.ini" if they exist.  In this case, the section names have "coverage:"
prefixed, so the ``[run]`` options described below will be found in the
``[coverage:run]`` section of the file. If coverage.py is installed with the
``toml`` extra (``pip install coverage[toml]``), it will automatically read
from "pyproject.toml". Configuration must be within the ``[tool.coverage]``
section, for example, ``[tool.coverage.run]``.


Syntax
------

A coverage.py configuration file is in classic .ini file format: sections are
introduced by a ``[section]`` header, and contain ``name = value`` entries.
Lines beginning with ``#`` or ``;`` are ignored as comments.

Strings don't need quotes. Multi-valued strings can be created by indenting
values on multiple lines.

Boolean values can be specified as ``on``, ``off``, ``true``, ``false``, ``1``,
or ``0`` and are case-insensitive.

Environment variables can be substituted in by using dollar signs: ``$WORD``
or ``${WORD}`` will be replaced with the value of ``WORD`` in the environment.
A dollar sign can be inserted with ``$$``.  Special forms can be used to
control what happens if the variable isn't defined in the environment:

- If you want to raise an error if an environment variable is undefined, use a
  question mark suffix: ``${WORD?}``.

- If you want to provide a default for missing variables, use a dash with a
  default value: ``${WORD-default value}``.

- Otherwise, missing environment variables will result in empty strings with no
  error.

Many sections and values correspond roughly to commands and options in
the :ref:`command-line interface <cmd>`.

Here's a sample configuration file::

    # .coveragerc to control coverage.py
    [run]
    branch = True

    [report]
    # Regexes for lines to exclude from consideration
    exclude_lines =
        # Have to re-enable the standard pragma
        pragma: no cover

        # Don't complain about missing debug-only code:
        def __repr__
        if self\.debug

        # Don't complain if tests don't hit defensive assertion code:
        raise AssertionError
        raise NotImplementedError

        # Don't complain if non-runnable code isn't run:
        if 0:
        if __name__ == .__main__.:

    ignore_errors = True

    [html]
    directory = coverage_html_report


.. _config_run:

[run]
-----

These values are generally used when running product code, though some apply
to more than one command.

.. _config_run_branch:

``branch`` (boolean, default False): whether to measure
:ref:`branch coverage <branch>` in addition to statement coverage.

.. _config_run_command_line:

``command_line`` (string): the command-line to run your program.  This will be
used if you run ``coverage run`` with no further arguments.  Coverage.py
options cannot be specified here, other than ``-m`` to indicate the module to
run.

.. versionadded:: 5.0

.. _config_run_concurrency:

``concurrency`` (multi-string, default "thread"): the name concurrency
libraries in use by the product code.  If your program uses `multiprocessing`_,
`gevent`_, `greenlet`_, or `eventlet`_, you must name that library in this
option, or coverage.py will produce very wrong results.

.. _multiprocessing: https://docs.python.org/3/library/multiprocessing.html
.. _greenlet: https://greenlet.readthedocs.io/
.. _gevent: http://www.gevent.org/
.. _eventlet: http://eventlet.net/

Before version 4.2, this option only accepted a single string.

.. versionadded:: 4.0

.. _config_run_context:

``context`` (string): the static context to record for this coverage run. See
:ref:`contexts` for more information

.. versionadded:: 5.0

.. _config_run_cover_pylib:

``cover_pylib`` (boolean, default False): whether to measure the Python
standard library.

.. _config_run_data_file:

``data_file`` (string, default ".coverage"): the name of the data file to use
for storing or reporting coverage. This value can include a path to another
directory.

.. _config_run_disable_warnings:

``disable_warnings`` (multi-string): a list of warnings to disable.  Warnings
that can be disabled include a short string at the end, the name of the
warning. See :ref:`cmd_warnings` for specific warnings.

.. _config_run_debug:

``debug`` (multi-string): a list of debug options.  See :ref:`the run
--debug option <cmd_run_debug>` for details.

.. _config_run_dynamic_context:

``dynamic_context`` (string): the name of a strategy for setting the dynamic
context during execution.  See :ref:`dynamic_contexts` for details.

.. _config_run_include:

``include`` (multi-string): a list of file name patterns, the files to include
in measurement or reporting.  Ignored if ``source`` is set.  See :ref:`source`
for details.

.. _config_run_note:

``note`` (string): this is now obsolete.

.. _config_run_omit:

``omit`` (multi-string): a list of file name patterns, the files to leave out
of measurement or reporting.  See :ref:`source` for details.

.. _config_run_parallel:

``parallel`` (boolean, default False): append the machine name, process
id and random number to the data file name to simplify collecting data from
many processes.  See :ref:`cmd_combining` for more information.

.. _config_run_plugins:

``plugins`` (multi-string): a list of plugin package names. See :ref:`plugins`
for more information.

.. _config_run_relative_files:

``relative_files`` (boolean, default False): *Experimental*: store relative
file paths in the data file.  This makes it easier to measure code in one (or
multiple) environments, and then report in another. See :ref:`cmd_combining`
for details.

.. versionadded:: 5.0

.. _config_run_source:

``source`` (multi-string): a list of packages or directories, the source to
measure during execution.  If set, ``include`` is ignored. See :ref:`source`
for details.

.. _config_run_timid:

``timid`` (boolean, default False): use a simpler but slower trace method.
This uses PyTracer instead of CTracer, and is only needed in very unusual
circumstances.  Try this if you get seemingly impossible results.


.. _config_paths:

[paths]
-------

The entries in this section are lists of file paths that should be considered
equivalent when combining data from different machines::

    [paths]
    source =
        src/
        /jenkins/build/*/src
        c:\myproj\src

The names of the entries ("source" in this example) are ignored, you may choose
any name that you like.  The value is a list of strings.  When combining data
with the ``combine`` command, two file paths will be combined if they start
with paths from the same list.

The first value must be an actual file path on the machine where the reporting
will happen, so that source code can be found.  The other values can be file
patterns to match against the paths of collected data, or they can be absolute
or relative file paths on the current machine.

In this example, data collected for "/jenkins/build/1234/src/module.py" will be
combined with data for "c:\\myproj\\src\\module.py", and will be reported
against the source file found at "src/module.py".

If you specify more than one list of paths, they will be considered in order.
The first list that has a match will be used.

See :ref:`cmd_combining` for more information.


.. _config_report:

[report]
--------

Values common to many kinds of reporting.

.. _config_report_exclude_lines:

``exclude_lines`` (multi-string): a list of regular expressions.  Any line of
your source code that matches one of these regexes is excluded from being
reported as missing.  More details are in :ref:`excluding`.  If you use this
option, you are replacing all the exclude regexes, so you'll need to also
supply the "pragma: no cover" regex if you still want to use it.

.. _config_report_fail_under:

``fail_under`` (float): a target coverage percentage. If the total coverage
measurement is under this value, then exit with a status code of 2.  If you
specify a non-integral value, you must also set ``[report] precision`` properly
to make use of the decimal places.  A setting of 100 will fail any value under
100, regardless of the number of decimal places of precision.

.. _config_report_ignore_errors:

``ignore_errors`` (boolean, default False): ignore source code that can't be
found, emitting a warning instead of an exception.

.. _config_report_include:

``include`` (multi-string): a list of file name patterns, the files to include
in reporting.  See :ref:`source` for details.

.. _config_report_omit:

``omit`` (multi-string): a list of file name patterns, the files to leave out
of reporting.  See :ref:`source` for details.

.. _config_report_partial_branches:

``partial_branches`` (multi-string): a list of regular expressions.  Any line
of code that matches one of these regexes is excused from being reported as
a partial branch.  More details are in :ref:`branch`.  If you use this option,
you are replacing all the partial branch regexes so you'll need to also
supply the "pragma: no branch" regex if you still want to use it.

.. _config_report_precision:

``precision`` (integer): the number of digits after the decimal point to
display for reported coverage percentages.  The default is 0, displaying for
example "87%".  A value of 2 will display percentages like "87.32%".  This
setting also affects the interpretation of the ``fail_under`` setting.

.. _config_report_show_missing:

``show_missing`` (boolean, default False): when running a summary report, show
missing lines.  See :ref:`cmd_summary` for more information.

.. _config_report_skip_covered:

``skip_covered`` (boolean, default False): Don't include files in the report
that are 100% covered files. See :ref:`cmd_summary` for more information.

.. _config_report_skip_empty:

``skip_empty`` (boolean, default False): Don't include empty files (those that
have 0 statements) in the report. See :ref:`cmd_summary` for more information.

.. _config_report_sort:

``sort`` (string, default "Name"): Sort the text report by the named column.
Allowed values are "Name", "Stmts", "Miss", "Branch", "BrPart", or "Cover".


.. _config_html:

[html]
------

Values particular to HTML reporting.  The values in the ``[report]`` section
also apply to HTML output, where appropriate.

.. _config_html_directory:

``directory`` (string, default "htmlcov"): where to write the HTML report
files.

.. _config_html_show_context:

``show_contexts`` (boolean): should the HTML report include an indication on
each line of which contexts executed the line.  See :ref:`dynamic_contexts` for
details.

.. _config_html_extra_css:

``extra_css`` (string): the path to a file of CSS to apply to the HTML report.
The file will be copied into the HTML output directory.  Don't name it
"style.css".  This CSS is in addition to the CSS normally used, though you can
overwrite as many of the rules as you like.

.. _config_html_title:

``title`` (string, default "Coverage report"): the title to use for the report.
Note this is text, not HTML.


.. _config_xml:

[xml]
-----

Values particular to XML reporting.  The values in the ``[report]`` section
also apply to XML output, where appropriate.

.. _config_xml_output:

``output`` (string, default "coverage.xml"): where to write the XML report.

.. _config_xml_package_depth:

``package_depth`` (integer, default 99): controls which directories are
identified as packages in the report.  Directories deeper than this depth are
not reported as packages.  The default is that all directories are reported as
packages.


.. _config_json:

[json]
------

Values particular to JSON reporting.  The values in the ``[report]`` section
also apply to JSON output, where appropriate.

.. versionadded:: 5.0

.. _config_json_output:

``output`` (string, default "coverage.json"): where to write the JSON file.

.. _config_json_pretty_print:

``pretty_print`` (boolean, default false): controls if the JSON is outputted
with whitespace formatted for human consumption (True) or for minimum file size
(False).

.. _config_json_show_contexts:

``show_contexts`` (boolean, default false): should the JSON report include an
indication of which contexts executed each line.  See :ref:`dynamic_contexts`
for details.
