.. Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
.. For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

.. _howitworks:

=====================
How coverage.py works
=====================

For advanced use of coverage.py, or just because you are curious, it helps to
understand what's happening behind the scenes.

Coverage.py works in three phases:

* **Execution**: Coverage.py runs your code, and monitors it to see what lines
  were executed.

* **Analysis**: Coverage.py examines your code to determine what lines could
  have run.

* **Reporting**: Coverage.py combines the results of execution and analysis to
  produce a coverage number and an indication of missing execution.

The execution phase is handled by the ``coverage run`` command.  The analysis
and reporting phases are handled by the reporting commands like ``coverage
report`` or ``coverage html``.

As a short-hand, I say that coverage.py measures what lines were executed. But
it collects more information than that.  It can measure what branches were
taken, and if you have contexts enabled, for each line or branch, it will also
measure what contexts they were executed in.

Let's look at each phase in more detail.


Execution
---------

At the heart of the execution phase is a trace function.  This is a function
that the Python interpreter invokes for each line executed in a program.
Coverage.py implements a trace function that records each file and line number
as it is executed.

For more details of trace functions, see the Python docs for `sys.settrace`_,
or if you are really brave, `How C trace functions really work`_.

Executing a function for every line in your program can make execution very
slow.  Coverage.py's trace function is implemented in C to reduce that
overhead. It also takes care to not trace code that you aren't interested in.

When measuring branch coverage, the same trace function is used, but instead of
recording line numbers, coverage.py records pairs of line numbers.  Each
invocation of the trace function remembers the line number, then the next
invocation records the pair `(prev, this)` to indicate that execution
transitioned from the previous line to this line.  Internally, these are called
arcs.

As the data is being collected, coverage.py writes the data to a file, usually
named ``.coverage``.  This is a :ref:`SQLite database <dbschema>` containing
all of the measured data.

.. _sys.settrace: https://docs.python.org/3/library/sys.html#sys.settrace
.. _How C trace functions really work: https://nedbatchelder.com/text/trace-function.html


Plugins
.......

Of course coverage.py mostly measures execution of Python files.  But it can
also be used to analyze other kinds of execution.  :ref:`File tracer plugins
<file_tracer_plugins>` provide support for non-Python files.  For example,
Django HTML templates result in Python code being executed somewhere, but as a
developer, you want that execution mapped back to your .html template file.

During execution, each new Python file encountered is provided to the plugins
to consider. A plugin can claim the file and then convert the runtime Python
execution into source-level data to be recorded.


Dynamic contexts
................

When using :ref:`dynamic contexts <dynamic_contexts>`, there is a current
dynamic context that changes over the course of execution.  It starts as empty.
While it is empty, every time a new function is entered, a check is made to see
if the dynamic context should change.  While a non-empty dynamic context is
current, the check is skipped until the function that started the context
returns.


Analysis
--------

After your program has been executed and the line numbers recorded, coverage.py
needs to determine what lines could have been executed.  Luckily, compiled
Python files (.pyc files) have a table of line numbers in them.  Coverage.py
reads this table to get the set of executable lines, with a little more source
analysis to leave out things like docstrings.

The data file is read to get the set of lines that were executed.  The
difference between the executable lines and the executed lines are the lines
that were not executed.

The same principle applies for branch measurement, though the process for
determining possible branches is more involved.  Coverage.py uses the abstract
syntax tree of the Python source file to determine the set of possible
branches.


Reporting
---------

Once we have the set of executed lines and missing lines, reporting is just a
matter of formatting that information in a useful way.  Each reporting method
(text, HTML, JSON, annotated source, XML) has a different output format, but
the process is the same: write out the information in the particular format,
possibly including the source code itself.
