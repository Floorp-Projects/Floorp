.. Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
.. For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

.. _subprocess:

=======================
Measuring sub-processes
=======================

Complex test suites may spawn sub-processes to run tests, either to run them in
parallel, or because sub-process behavior is an important part of the system
under test. Measuring coverage in those sub-processes can be tricky because you
have to modify the code spawning the process to invoke coverage.py.

There's an easier way to do it: coverage.py includes a function,
:func:`coverage.process_startup` designed to be invoked when Python starts.  It
examines the ``COVERAGE_PROCESS_START`` environment variable, and if it is set,
begins coverage measurement. The environment variable's value will be used as
the name of the :ref:`configuration file <config>` to use.

.. note::
    The subprocess only sees options in the configuration file.  Options set on
    the command line will not be used in the subprocesses.

.. note::
    If you have subprocesses because you are using :mod:`multiprocessing
    <python:multiprocessing>`, the ``--concurrency=multiprocessing``
    command-line option should take care of everything for you.  See
    :ref:`cmd_run` for details.

When using this technique, be sure to set the parallel option to true so that
multiple coverage.py runs will each write their data to a distinct file.


Configuring Python for sub-process coverage
-------------------------------------------

Measuring coverage in sub-processes is a little tricky.  When you spawn a
sub-process, you are invoking Python to run your program.  Usually, to get
coverage measurement, you have to use coverage.py to run your program.  Your
sub-process won't be using coverage.py, so we have to convince Python to use
coverage.py even when not explicitly invoked.

To do that, we'll configure Python to run a little coverage.py code when it
starts.  That code will look for an environment variable that tells it to start
coverage measurement at the start of the process.

To arrange all this, you have to do two things: set a value for the
``COVERAGE_PROCESS_START`` environment variable, and then configure Python to
invoke :func:`coverage.process_startup` when Python processes start.

How you set ``COVERAGE_PROCESS_START`` depends on the details of how you create
sub-processes.  As long as the environment variable is visible in your
sub-process, it will work.

You can configure your Python installation to invoke the ``process_startup``
function in two ways:

#. Create or append to sitecustomize.py to add these lines::

    import coverage
    coverage.process_startup()

#. Create a .pth file in your Python installation containing::

    import coverage; coverage.process_startup()

The sitecustomize.py technique is cleaner, but may involve modifying an
existing sitecustomize.py, since there can be only one.  If there is no
sitecustomize.py already, you can create it in any directory on the Python
path.

The .pth technique seems like a hack, but works, and is documented behavior.
On the plus side, you can create the file with any name you like so you don't
have to coordinate with other .pth files.  On the minus side, you have to
create the file in a system-defined directory, so you may need privileges to
write it.

Note that if you use one of these techniques, you must undo them if you
uninstall coverage.py, since you will be trying to import it during Python
start-up.  Be sure to remove the change when you uninstall coverage.py, or use
a more defensive approach to importing it.


Signal handlers and atexit
--------------------------

.. hmm, this isn't specifically about subprocesses, is there a better place
    where we could talk about this?

To successfully write a coverage data file, the Python sub-process under
analysis must shut down cleanly and have a chance for coverage.py to run the
``atexit`` handler it registers.

For example if you send SIGTERM to end the sub-process, but your sub-process
has never registered any SIGTERM handler, then a coverage file won't be
written.  See the `atexit`_ docs for details of when the handler isn't run.

.. _atexit: https://docs.python.org/3/library/atexit.html
