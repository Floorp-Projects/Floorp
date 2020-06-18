.. Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
.. For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

.. _trouble:

=========================
Things that cause trouble
=========================

Coverage.py works well, and I want it to properly measure any Python program,
but there are some situations it can't cope with.  This page details some known
problems, with possible courses of action, and links to coverage.py bug reports
with more information.

I would love to :ref:`hear from you <contact>` if you have information about
any of these problems, even just to explain to me why you want them to start
working properly.

If your problem isn't discussed here, you can of course search the `coverage.py
bug tracker`_ directly to see if there is some mention of it.

.. _coverage.py bug tracker: https://github.com/nedbat/coveragepy/issues


Things that don't work
----------------------

There are a number of popular modules, packages, and libraries that prevent
coverage.py from working properly:

* `execv`_, or one of its variants.  These end the current program and replace
  it with a new one.  This doesn't save the collected coverage data, so your
  program that calls execv will not be fully measured.  A patch for coverage.py
  is in `issue 43`_.

* `thread`_, in the Python standard library, is the low-level threading
  interface.  Threads created with this module will not be traced.  Use the
  higher-level `threading`_ module instead.

* `sys.settrace`_ is the Python feature that coverage.py uses to see what's
  happening in your program.  If another part of your program is using
  sys.settrace, then it will conflict with coverage.py, and it won't be
  measured properly.

.. _execv: https://docs.python.org/3/library/os.html#os.execl
.. _sys.settrace: https://docs.python.org/3/library/sys.html#sys.settrace
.. _thread: https://docs.python.org/3/library/_thread.html
.. _threading: https://docs.python.org/3/library/threading.html
.. _issue 43: https://bitbucket.org/ned/coveragepy/issues/43/coverage-measurement-fails-on-code


Still having trouble?
---------------------

If your problem isn't mentioned here, and isn't already reported in the
`coverage.py bug tracker`_, please :ref:`get in touch with me <contact>`,
we'll figure out a solution.
