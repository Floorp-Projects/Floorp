.. Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
.. For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

.. _excluding:

===============================
Excluding code from coverage.py
===============================

You may have code in your project that you know won't be executed, and you want
to tell coverage.py to ignore it.  For example, you may have debugging-only
code that won't be executed during your unit tests. You can tell coverage.py to
exclude this code during reporting so that it doesn't clutter your reports with
noise about code that you don't need to hear about.

Coverage.py will look for comments marking clauses for exclusion.  In this
code, the "if debug" clause is excluded from reporting::

    a = my_function1()
    if debug:   # pragma: no cover
        msg = "blah blah"
        log_message(msg, a)
    b = my_function2()

Any line with a comment of "pragma: no cover" is excluded.  If that line
introduces a clause, for example, an if clause, or a function or class
definition, then the entire clause is also excluded.  Here the __repr__
function is not reported as missing::

    class MyObject(object):
        def __init__(self):
            blah1()
            blah2()

        def __repr__(self): # pragma: no cover
            return "<MyObject>"

Excluded code is executed as usual, and its execution is recorded in the
coverage data as usual. When producing reports though, coverage.py excludes it
from the list of missing code.


Branch coverage
---------------

When measuring :ref:`branch coverage <branch>`, a conditional will not be
counted as a branch if one of its choices is excluded::

    def only_one_choice(x):
        if x:
            blah1()
            blah2()
        else:       # pragma: no cover
            # x is always true.
            blah3()

Because the ``else`` clause is excluded, the ``if`` only has one possible next
line, so it isn't considered a branch at all.


Advanced exclusion
------------------

Coverage.py identifies exclusions by matching lines against a list of regular
expressions. Using :ref:`configuration files <config>` or the coverage
:ref:`API <api>`, you can add to that list. This is useful if you have
often-used constructs to exclude that can be matched with a regex. You can
exclude them all at once without littering your code with exclusion pragmas.

For example, you might decide that __repr__ functions are usually only used in
debugging code, and are uninteresting to test themselves.  You could exclude
all of them by adding a regex to the exclusion list::

    [report]
    exclude_lines = def __repr__

For example, here's a list of exclusions I've used::

    [report]
    exclude_lines =
        pragma: no cover
        def __repr__
        if self.debug:
        if settings.DEBUG
        raise AssertionError
        raise NotImplementedError
        if 0:
        if __name__ == .__main__.:

Note that when using the ``exclude_lines`` option in a configuration file, you
are taking control of the entire list of regexes, so you need to re-specify the
default "pragma: no cover" match if you still want it to apply.

A similar pragma, "no branch", can be used to tailor branch coverage
measurement.  See :ref:`branch` for details.


Excluding source files
----------------------

See :ref:`source` for ways to limit what files coverage.py measures or reports
on.
