.. Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
.. For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

.. _source:

=======================
Specifying source files
=======================

When coverage.py is running your program and measuring its execution, it needs
to know what code to measure and what code not to.  Measurement imposes a speed
penalty, and the collected data must be stored in memory and then on disk.
More importantly, when reviewing your coverage reports, you don't want to be
distracted with modules that aren't your concern.

Coverage.py has a number of ways you can focus it in on the code you care
about.


.. _source_execution:

Execution
---------

When running your code, the ``coverage run`` command will by default measure
all code, unless it is part of the Python standard library.

You can specify source to measure with the ``--source`` command-line switch, or
the ``[run] source`` configuration value.  The value is a comma- or
newline-separated list of directories or package names.  If specified, only
source inside these directories or packages will be measured.  Specifying the
source option also enables coverage.py to report on unexecuted files, since it
can search the source tree for files that haven't been measured at all.  Only
importable files (ones at the root of the tree, or in directories with a
``__init__.py`` file) will be considered. Files with unusual punctuation in
their names will be skipped (they are assumed to be scratch files written by
text editors). Files that do not end with ``.py`` or ``.pyo`` or ``.pyc``
will also be skipped.

You can further fine-tune coverage.py's attention with the ``--include`` and
``--omit`` switches (or ``[run] include`` and ``[run] omit`` configuration
values). ``--include`` is a list of file name patterns. If specified, only
files matching those patterns will be measured. ``--omit`` is also a list of
file name patterns, specifying files not to measure.  If both ``include`` and
``omit`` are specified, first the set of files is reduced to only those that
match the include patterns, then any files that match the omit pattern are
removed from the set.

The ``include`` and ``omit`` file name patterns follow typical shell syntax:
``*`` matches any number of characters and ``?`` matches a single character.
Patterns that start with a wildcard character are used as-is, other patterns
are interpreted relative to the current directory::

    [run]
    omit =
        # omit anything in a .local directory anywhere
        */.local/*
        # omit everything in /usr
        /usr/*
        # omit this single file
        utils/tirefire.py

The ``source``, ``include``, and ``omit`` values all work together to determine
the source that will be measured.

If both ``source`` and ``include`` are set, the ``include`` value is ignored
and a warning is printed on the standard output.


.. _source_reporting:

Reporting
---------

Once your program is measured, you can specify the source files you want
reported.  Usually you want to see all the code that was measured, but if you
are measuring a large project, you may want to get reports for just certain
parts.

The report commands (``report``, ``html``, ``json``, ``annotate``, and ``xml``)
all take optional ``modules`` arguments, and ``--include`` and ``--omit``
switches. The ``modules`` arguments specify particular modules to report on.
The ``include`` and ``omit`` values are lists of file name patterns, just as
with the ``run`` command.

Remember that the reporting commands can only report on the data that has been
collected, so the data you're looking for may not be in the data available for
reporting.

Note that these are ways of specifying files to measure.  You can also exclude
individual source lines.  See :ref:`excluding` for details.
