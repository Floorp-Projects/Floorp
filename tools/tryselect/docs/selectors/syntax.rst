Syntax Selector
===============

.. warning::

    Try syntax is antiquated and hard to understand. If you aren't already
    familiar with try syntax, you might want to use the ``fuzzy`` selector
    instead.

Try syntax is a command line string that goes into the commit message. Using
``mach try syntax`` will automatically create a temporary commit with your
chosen syntax and then delete it again after pushing to try.

Try syntax can contain all kinds of different options parsed by various
places in various repos, but the majority are parsed by `try_option_syntax.py`_.
The most common arguments include:

    * ``-b/--build`` - One of ``d``, ``o`` or ``do``. This is the build type,
      either opt, debug or both (required).
    * ``-p/--platform`` - The platforms you want to build and/or run tests on
      (required).
    * ``-u/--unittests`` - The test tasks you want to run (optional).
    * ``-t/--talos`` - The talos tasks you want to run (optional).

Here are some examples:

.. code-block:: shell

    $ mach try syntax -b do -p linux,macosx64 -u mochitest-e10s-1,crashtest -t none
    $ mach try syntax -b d -p win64 -u all
    $ mach try syntax -b o -p linux64

Unfortunately, knowing the magic strings that make it work can be a bit of a
guessing game. If you are unsure of what string will get your task to run, try
using :doc:`mach try fuzzy <fuzzy>` instead.

While using ``mach try syntax -b do -p all -u all -t all`` will work, heavy use
of ``all`` is discouraged as it consumes a lot of unnecessary resources (some of
which are hardware constrained).

..  _try_option_syntax.py: https://searchfox.org/mozilla-central/source/taskcluster/taskgraph/try_option_syntax.py
