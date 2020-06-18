.. Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
.. For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

.. _contributing:

===========================
Contributing to coverage.py
===========================

.. highlight:: console

I welcome contributions to coverage.py.  Over the years, dozens of people have
provided patches of various sizes to add features or fix bugs.  This page
should have all the information you need to make a contribution.

One source of history or ideas are the `bug reports`_ against coverage.py.
There you can find ideas for requested features, or the remains of rejected
ideas.

.. _bug reports: https://github.com/nedbat/coveragepy/issues


Before you begin
----------------

If you have an idea for coverage.py, run it by me before you begin writing
code.  This way, I can get you going in the right direction, or point you to
previous work in the area.  Things are not always as straightforward as they
seem, and having the benefit of lessons learned by those before you can save
you frustration.


Getting the code
----------------

The coverage.py code is hosted on a GitHub repository at
https://github.com/nedbat/coveragepy.  To get a working environment, follow
these steps:

#.  (Optional, but recommended) Create a Python 3.6 virtualenv to work in,
    and activate it.

.. like this:
 mkvirtualenv -p /usr/local/pythonz/pythons/CPython-2.7.11/bin/python coverage

#.  Clone the repository::

        $ git clone https://github.com/nedbat/coveragepy
        $ cd coveragepy

#.  Install the requirements::

        $ pip install -r requirements/dev.pip

#.  Install a number of versions of Python.  Coverage.py supports a wide range
    of Python versions.  The more you can test with, the more easily your code
    can be used as-is.  If you only have one version, that's OK too, but may
    mean more work integrating your contribution.


Running the tests
-----------------

The tests are written mostly as standard unittest-style tests, and are run with
pytest running under `tox`_::

    $ tox
    py27 develop-inst-noop: /Users/ned/coverage/trunk
    py27 installed: DEPRECATION: Python 2.7 will reach the end of its life on January 1st, 2020. Please upgrade your Python as Python 2.7 won't be maintained after that date. A future version of pip will drop support for Python 2.7.,apipkg==1.5,atomicwrites==1.3.0,attrs==19.1.0,-e git+git@github.com:nedbat/coveragepy.git@40ecd174f148219ebd343e1a5bfdf8931f3785e4#egg=coverage,covtestegg1==0.0.0,decorator==4.4.0,dnspython==1.16.0,enum34==1.1.6,eventlet==0.24.1,execnet==1.6.0,flaky==3.5.3,funcsigs==1.0.2,future==0.17.1,gevent==1.2.2,greenlet==0.4.15,mock==3.0.5,monotonic==1.5,more-itertools==5.0.0,pathlib2==2.3.3,pluggy==0.11.0,py==1.8.0,PyContracts==1.8.12,pyparsing==2.4.0,pytest==4.5.0,pytest-forked==1.0.2,pytest-xdist==1.28.0,scandir==1.10.0,six==1.12.0,unittest-mixins==1.6,wcwidth==0.1.7
    py27 run-test-pre: PYTHONHASHSEED='1375790451'
    py27 run-test: commands[0] | python setup.py --quiet clean develop
    warning: no previously-included files matching '*.py[co]' found anywhere in distribution
    py27 run-test: commands[1] | python igor.py zip_mods install_egg remove_extension
    py27 run-test: commands[2] | python igor.py test_with_tracer py
    === CPython 2.7.14 with Python tracer (.tox/py27/bin/python) ===
    bringing up nodes...
    ............................s.....s............................................................................s.....s.................................s.s.........s...................... [ 21%]
    .............................................................s.........................................s.................................................................................. [ 43%]
    ..............................s...................................ss..ss.s...................ss.....s.........................................s........................................... [ 64%]
    ..............................ssssssss.ss..ssss............ssss.ssssss....................................................................s........................................s...... [ 86%]
    ...s........s............................................................................................................                                                                  [100%]
    818 passed, 47 skipped in 66.39 seconds
    py27 run-test: commands[3] | python setup.py --quiet build_ext --inplace
    py27 run-test: commands[4] | python igor.py test_with_tracer c
    === CPython 2.7.14 with C tracer (.tox/py27/bin/python) ===
    bringing up nodes...
    ...........................s.....s.............................................................................s.....s..................................ss.......s......................... [ 21%]
    ................................................................................s...........ss..s..............ss......s............................................s..................... [ 43%]
    ....................................................................................s...................s...................s............................................................. [ 64%]
    .................................s.............................s.......................................................s......................................s....................s...... [ 86%]
    .........s..............................................................................................................                                                                   [100%]
    841 passed, 24 skipped in 63.95 seconds
    py36 develop-inst-noop: /Users/ned/coverage/trunk
    py36 installed: apipkg==1.5,atomicwrites==1.3.0,attrs==19.1.0,-e git+git@github.com:nedbat/coveragepy.git@40ecd174f148219ebd343e1a5bfdf8931f3785e4#egg=coverage,covtestegg1==0.0.0,decorator==4.4.0,dnspython==1.16.0,eventlet==0.24.1,execnet==1.6.0,flaky==3.5.3,future==0.17.1,gevent==1.2.2,greenlet==0.4.15,mock==3.0.5,monotonic==1.5,more-itertools==7.0.0,pluggy==0.11.0,py==1.8.0,PyContracts==1.8.12,pyparsing==2.4.0,pytest==4.5.0,pytest-forked==1.0.2,pytest-xdist==1.28.0,six==1.12.0,unittest-mixins==1.6,wcwidth==0.1.7
    py36 run-test-pre: PYTHONHASHSEED='1375790451'
    py36 run-test: commands[0] | python setup.py --quiet clean develop
    warning: no previously-included files matching '*.py[co]' found anywhere in distribution
    py36 run-test: commands[1] | python igor.py zip_mods install_egg remove_extension
    py36 run-test: commands[2] | python igor.py test_with_tracer py
    === CPython 3.6.7 with Python tracer (.tox/py36/bin/python) ===
    bringing up nodes...
    .......................................................................................................................................................................................... [ 21%]
    ..............................................................ss.......s...............s.............ss.s...............ss.....s.......................................................... [ 43%]
    ...............................................s...................................................................................s...s.........s........................................ [ 64%]
    .................sssssss.ssss..................................................sssssssssssss..........................s........s.......................................................... [ 86%]
    ......s............s.....................................................................................................                                                                  [100%]
    823 passed, 42 skipped in 59.05 seconds
    py36 run-test: commands[3] | python setup.py --quiet build_ext --inplace
    py36 run-test: commands[4] | python igor.py test_with_tracer c
    === CPython 3.6.7 with C tracer (.tox/py36/bin/python) ===
    bringing up nodes...
    .......................................................................................................................................................................................... [ 21%]
    ...........................................s..s..........................................................................s.......................................s.........s.............. [ 42%]
    ...............................s......s.s.s.................ss......s...........................................................................................................s......... [ 64%]
    ...........................................................s...s..............................................................................................................s........s.. [ 86%]
    ........................s................................................................................................                                                                  [100%]
    847 passed, 18 skipped in 60.53 seconds
    ____________________________________________________________________________________________ summary _____________________________________________________________________________________________
      py27: commands succeeded
      py36: commands succeeded
      congratulations :)

Tox runs the complete test suite twice for each version of Python you have
installed.  The first run uses the Python implementation of the trace function,
the second uses the C implementation.

To limit tox to just a few versions of Python, use the ``-e`` switch::

    $ tox -e py27,py37

To run just a few tests, you can use `pytest test selectors`_::

    $ tox tests/test_misc.py
    $ tox tests/test_misc.py::HasherTest
    $ tox tests/test_misc.py::HasherTest::test_string_hashing

These command run the tests in one file, one class, and just one test,
respectively.

You can also affect the test runs with environment variables. Define any of
these as 1 to use them:

- COVERAGE_NO_PYTRACER: disables the Python tracer if you only want to run the
  CTracer tests.

- COVERAGE_NO_CTRACER: disables the C tracer if you only want to run the
  PyTracer tests.

- COVERAGE_AST_DUMP: will dump the AST tree as it is being used during code
  parsing.

- COVERAGE_KEEP_TMP: keeps the temporary directories in which tests are run.
  This makes debugging tests easier. The temporary directories are at
  ``$TMPDIR/coverage_test/*``, and are named for the test that made them.


Of course, run all the tests on every version of Python you have, before
submitting a change.

.. _pytest test selectors: http://doc.pytest.org/en/latest/usage.html#specifying-tests-selecting-tests


Lint, etc
---------

I try to keep the coverage.py as clean as possible.  I use pylint to alert me
to possible problems::

    $ make lint
    pylint coverage setup.py tests
    python -m tabnanny coverage setup.py tests
    python igor.py check_eol

The source is pylint-clean, even if it's because there are pragmas quieting
some warnings.  Please try to keep it that way, but don't let pylint warnings
keep you from sending patches.  I can clean them up.

Lines should be kept to a 100-character maximum length.  I recommend an
`editorconfig.org`_ plugin for your editor of choice.

Other style questions are best answered by looking at the existing code.
Formatting of docstrings, comments, long lines, and so on, should match the
code that already exists.


Coverage testing coverage.py
----------------------------

Coverage.py can measure itself, but it's complicated.  The process has been
packaged up to make it easier::

    $ make metacov metahtml

Then look at htmlcov/index.html.  Note that due to the recursive nature of
coverage.py measuring itself, there are some parts of the code that will never
appear as covered, even though they are executed.


Contributing
------------

When you are ready to contribute a change, any way you can get it to me is
probably fine.  A pull request on GitHub is great, but a simple diff or
patch works too.


.. _editorconfig.org: http://editorconfig.org
.. _tox: https://tox.readthedocs.io/
