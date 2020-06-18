.. Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
.. For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

.. _faq:

==================
FAQ and other help
==================


Frequently asked questions
--------------------------

Q: Why do unexecutable lines show up as executed?
.................................................

Usually this is because you've updated your code and run coverage.py on it
again without erasing the old data.  Coverage.py records line numbers executed,
so the old data may have recorded a line number which has since moved, causing
coverage.py to claim a line has been executed which cannot be.

If old data is persisting, you can use an explicit ``coverage erase`` command
to clean out the old data.


Q: Why do the bodies of functions show as executed, but the def lines do not?
.............................................................................

This happens because coverage.py is started after the functions are defined.
The definition lines are executed without coverage measurement, then
coverage.py is started, then the function is called.  This means the body is
measured, but the definition of the function itself is not.

The same thing can happen with the bodies of classes.

To fix this, start coverage.py earlier.  If you use the :ref:`command line
<cmd>` to run your program with coverage.py, then your entire program will be
monitored.  If you are using the :ref:`API <api>`, you need to call
coverage.start() before importing the modules that define your functions.


Q: My decorator lines are marked as covered, but the "def" line is not.  Why?
.............................................................................

Different versions of Python report execution on different lines.  Coverage.py
adapts its behavior to the version of Python being used.  In Python 3.7 and
earlier, a decorated function definition only reported the decorator as
executed. In Python 3.8 and later, both the decorator and the "def" are
reported.  If you collect execution data on Python 3.7, and then run coverage
reports on Python 3.8, there will be a discrepancy.


Q: How is the total percentage calculated?
..........................................

Coverage.py counts the total number of possible executions. This is the number
of executable statements minus the number of excluded statements.  It then
counts the number of those possibilities that were actually executed.  The
total percentage is the actual executions divided by the possible executions.

As an example, a coverage report with 1514 statements and 901 missed
statements would calculate a total percentage of (1514-901)/1514, or 40.49%.

:ref:`Branch coverage <branch>` extends the calculation to include the total
number of possible branch exits, and the number of those taken.  In this case
the specific numbers shown in coverage reports don't calculate out to the
percentage shown, because the number of missing branch exits isn't reported
explicitly.  A branch line that wasn't executed at all is counted once as a
missing statement in the report, instead of as two missing branches.  Reports
show the number of partial branches, which is the lines that were executed but
did not execute all of their exits.


Q: Coverage.py is much slower than I remember, what's going on?
...............................................................

Make sure you are using the C trace function.  Coverage.py provides two
implementations of the trace function.  The C implementation runs much faster.
To see what you are running, use ``coverage debug sys``.  The output contains
details of the environment, including a line that says either
``tracer: CTracer`` or ``tracer: PyTracer``.  If it says ``PyTracer`` then you
are using the slow Python implementation.

Try re-installing coverage.py to see what happened and if you get the CTracer
as you should.


Q: Isn't coverage testing the best thing ever?
..............................................

It's good, but `it isn't perfect`__.

__ https://nedbatchelder.com/blog/200710/flaws_in_coverage_measurement.html


..  Other resources
    ---------------

    There are a number of projects that help integrate coverage.py into other
    systems:

    - `trialcoverage`_ is a plug-in for Twisted trial.

    .. _trialcoverage: https://pypi.org/project/trialcoverage/

    - `pytest-coverage`_

    .. _pytest-coverage: https://pypi.org/project/pytest-coverage/

    - `django-coverage`_ for use with Django.

    .. _django-coverage: https://pypi.org/project/django-coverage/


Q: Where can I get more help with coverage.py?
..............................................

You can discuss coverage.py or get help using it on the `Testing In Python`_
mailing list.

.. _Testing In Python: http://lists.idyll.org/listinfo/testing-in-python

Bug reports are gladly accepted at the `GitHub issue tracker`_.

.. _GitHub issue tracker: https://github.com/nedbat/coveragepy/issues

`I can be reached`__ in a number of ways, I'm happy to answer questions about
using coverage.py.

__  https://nedbatchelder.com/site/aboutned.html


History
-------

Coverage.py was originally written by `Gareth Rees`_.
Since 2004, `Ned Batchelder`_ has extended and maintained it with the help of
`many others`_.  The :ref:`change history <changes>` has all the details.

.. _Gareth Rees:    http://garethrees.org/
.. _Ned Batchelder: https://nedbatchelder.com
.. _many others:    https://github.com/nedbat/coveragepy/blob/master/CONTRIBUTORS.txt
