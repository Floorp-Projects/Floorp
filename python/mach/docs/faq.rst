.. _mach_faq:

==========================
Frequently Asked Questions
==========================

How do I report bugs?
---------------------

Bugs against the ``mach`` core can be filed in Bugzilla in the `Firefox
Build System::Mach
Core <https://bugzilla.mozilla.org/enter_bug.cgi?product=Firefox%20Build%20System&component=Mach%20Core>`__ component.

.. note::

   Most ``mach`` bugs are bugs in individual commands, not bugs in the core
   ``mach`` code. Bugs for individual commands should be filed against the
   component that command is related to. For example, bugs in the
   *build* command should be filed against *Firefox Build System ::
   General*. Bugs against testing commands should be filed somewhere in
   the *Testing* product.

How do I debug a command failing with a Python exception?
---------------------------------------------------------

You can run a command and break into ``pdb``, the Python debugger,
when the command is invoked with:

.. code-block:: shell

   ./mach --debug-command FAILING-COMMAND ARGS ...

How do I debug ``mach`` itself?
-------------------------------

If you are editing the mach code, or other Python modules you can
open the terminal and start debugging with pdb with the following:

.. code-block:: shell

        python3 -m pdb ./mach <command>

How do I debug ``pytest`` tests?
--------------------------------

First, before debugging, run ``./mach python-test`` once to ensure that
the testing virtualenv is up-to-date:

.. code-block:: shell

    ./mach python-test path/to/test.py

Then, using the testing virtualenv, debug the test file:

.. code-block:: shell

    <objdir>/_virtualenvs/python-test/bin/python -m pdb path/to/test.py

How do I profile a slow command?
--------------------------------

To diagnose bottlenecks, you can collect a performance profile:

.. code-block:: shell

    ./mach --profile-command SLOW-COMMAND ARGS ...

Then, you can visualize ``mach_profile_SLOW-COMMAND.cProfile`` using
`snakeviz <https://jiffyclub.github.io/snakeviz/>`__:

.. code-block:: shell

    # If you don't have snakeviz installed yet:
    python3 -m pip install snakeviz
    python3 -m snakeviz mach_profile_SLOW-COMMAND.cProfile

How do I profile ``mach`` itself?
---------------------------------

Since ``--profile-command`` only profiles commands, you'll need to invoke ``cProfile``
directly to profile ``mach`` itself:

.. code-block:: shell

    python3 -m cProfile -o mach.cProfile ./mach ...
    python3 -m snakeviz mach.cProfile

Is ``mach`` a build system?
---------------------------

No. ``mach`` is just a generic command dispatching tool that happens to have
a few commands that interact with the real build system. Historically,
``mach`` *was* born to become a better interface to the build system.
However, its potential beyond just build system interaction was quickly
realized and ``mach`` grew to fit those needs.

How do I add features to ``mach``?
----------------------------------
If you would like to add a new feature to ``mach`` that cannot be implemented as
a ``mach`` command, the first step is to file a bug in the
``Firefox Build System :: Mach Core`` component.

Should I implement X as a ``mach`` command?
-------------------------------------------

There are no hard or fast rules. Generally speaking, if you have some
piece of functionality or action that is useful to multiple people
(especially if it results in productivity wins), then you should
consider implementing a ``mach`` command for it.

Some other cases where you should consider implementing something as a
``mach`` command:

-  When your tool is a random script in the tree. Random scripts are
   hard to find and may not conform to coding conventions or best
   practices. ``Mach`` provides a framework in which your tool can live that
   will put it in a better position to succeed than if it were on its
   own.
-  When the alternative is a ``make`` target. The build team generally does
   not like one-off ``make`` targets that aren't part of building (read:
   compiling) the tree. This includes things related to testing and
   packaging. These weigh down ``Makefiles`` and add to the burden of
   maintaining the build system. Instead, you are encouraged to
   implement ancillary functionality in Python. If you do implement something
   in Python, hooking it up to ``mach`` is often trivial.

How do I use 3rd-party Python packages in my ``mach`` command?
--------------------------------------------------------------

See :ref:`Using third-party Python packages`.

How does ``mach`` fit into the modules system?
----------------------------------------------

Mozilla operates with a `modules governance
system <https://www.mozilla.org/about/governance/policies/module-ownership/>`__ where
there are different components with different owners. There is not
currently a ``mach`` module. There may or may never be one; currently ``mach``
is owned by the build team.

Even if a ``mach`` module were established, ``mach`` command modules would
likely never belong to it. Instead, ``mach`` command modules are owned by the
team/module that owns the system they interact with. In other words, ``mach``
is not a power play to consolidate authority for tooling. Instead, it aims to
expose that tooling through a common, shared interface.


Who do I contact for help or to report issues?
----------------------------------------------

You can ask questions in
`#build <https://chat.mozilla.org/#/room/#build:mozilla.org>`__.
