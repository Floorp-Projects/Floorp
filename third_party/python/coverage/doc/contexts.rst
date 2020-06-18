.. Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
.. For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

.. _contexts:

====================
Measurement contexts
====================

.. versionadded:: 5.0

Coverage.py measures whether code was run, but it can also record the context
in which it was run.  This can provide more information to help you understand
the behavior of your tests.

There are two kinds of context: static and dynamic.  Static contexts are fixed
for an entire run, and are set explicitly with an option.  Dynamic contexts
change over the course of a single run.

.. _static_contexts:

Static contexts
---------------

A static context is set by an option when you run coverage.py.  The value is
fixed for the duration of a run.  They can be any text you like, for example,
"python3" or "with_numpy".  The context is recorded with the data.

When you :ref:`combine multiple data files <cmd_combining>` together, they can
have differing contexts.  All of the information is retained, so that the
different contexts are correctly recorded in the combined file.

A static context is specified with the ``--context=CONTEXT`` option to
:ref:`the coverage run command <cmd_run>`, or the ``[run] context`` setting in
the configuration file.


.. _dynamic_contexts:

Dynamic contexts
----------------

Dynamic contexts are found during execution.  They are most commonly used to
answer the question "what test ran this line?," but have been generalized to
allow any kind of context tracking.  As execution proceeds, the dynamic context
changes to record the context of execution.  Separate data is recorded for each
context, so that it can be analyzed later.

There are three ways to enable dynamic contexts:

* you can set the ``[run] dynamic_context`` option in your .coveragerc file, or

* you can enable a :ref:`dynamic context switcher <dynamic_context_plugins>`
  plugin, or

* another tool (such as a test runner) can call the
  :meth:`.Coverage.switch_context` method to set the context explicitly.
  The pytest plugin `pytest-cov`_ has a ``--cov-context`` option that uses this
  to set the dynamic context for each test.

.. _pytest-cov: https://pypi.org/project/pytest-cov/

The ``[run] dynamic_context`` setting has only one option now.  Set it to
``test_function`` to start a new dynamic context for every test function::

    [run]
    dynamic_context = test_function

Each test function you run will be considered a separate dynamic context, and
coverage data will be segregated for each.  A test function is any function
whose name starts with "test".

If you have both a static context and a dynamic context, they are joined with a
pipe symbol to be recorded as a single string.

Initially, when your program starts running, the dynamic context is an empty
string.  Any code measured before a dynamic context is set will be recorded in
this empty context.  For example, if you are recording test names as contexts,
then the code run by the test runner before (and between) tests will be in the
empty context.

Dynamic contexts can be explicitly disabled by setting ``dynamic_context`` to
``none``.

.. _context_reporting:

Context reporting
-----------------

The ``coverage report`` and ``coverage html`` commands both accept
``--contexts`` option, a comma-separated list of regular expressions.  The
report will be limited to the contexts that match one of those patterns.

The ``coverage html`` command also has ``--show-contexts``.  If set, the HTML
report will include an annotation on each covered line indicating the number of
contexts that executed the line.  Clicking the annotation displays a list of
the contexts.


Raw data
--------

For more advanced reporting or analysis, the .coverage data file is a SQLite
database. See :ref:`dbschema` for details.
