Fuzzing Interface
=================

The fuzzing interface is glue code living in mozilla-central in order to
make it easier for developers and security researchers to test C/C++
code with either `libFuzzer <https://llvm.org/docs/LibFuzzer.html>`__ or
`afl-fuzz <http://lcamtuf.coredump.cx/afl/>`__.

What can be tested?
~~~~~~~~~~~~~~~~~~~

The interface can be used to test all C/C++ code that either ends up in
``libxul`` (more precisely, the gtest version of ``libxul``) **or** is
part of the JS engine.

Note that this is not the right testing approach for testing the full
browser as a whole. It is rather meant for component-based testing
(especially as some components cannot be easily separated out of the
full build).


Getting Started with libFuzzer
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


Using Existing Builds
^^^^^^^^^^^^^^^^^^^^^

If you are just trying to use an existing fuzzing target (for
reproducing a bug or testing), then you can simply download an ASan
fuzzing build from Taskcluster. The easiest way to do this is using
fuzzfetch, a tool that makes downloading and unpacking these builds very
easy.

You can install ``fuzzfetch`` from
`Github <https://github.com/MozillaSecurity/fuzzfetch>`__ or via pip.
Afterwards, you can run

::

   python -m fuzzfetch -a --fuzzing --tests gtest

to fetch the latest build. Afterwards, you can run any fuzzing target as
described in the section ":ref:`How to run your code`".
Alternatively you can make your own local build by following the steps
below.


Build Requirements
^^^^^^^^^^^^^^^^^^

You will need a Linux environment with a recent Clang (recommend at least Clang 8).


Build Flags
^^^^^^^^^^^

You need to ensure that your build is

-  an AddressSanitizer (ASan) build
   (``ac_add_options --enable-address-sanitizer``),
-  a fuzzing build (``ac_add_options --enable-fuzzing``)

If you are adding a **new** fuzzing target, you also need to ensure that
the code you are trying to test is instrumented for libFuzzer. This is
done by adding a special include into each respective ``moz.build`` file
like in this `example for
mtransport <https://searchfox.org/mozilla-central/rev/de7676288a78b70d2b9927c79493adbf294faad5/media/mtransport/moz.build#18-19>`__.

By keeping coverage limited to the parts that are actually being tested
using this tool, you not only increase the performance but also
potentially reduce the amount of noise that libFuzzer sees.


Where to put your fuzzing code
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

You can put all of your code into a subdirectory of the code that you
are trying to test and enable that directory when the build system flags
for a fuzzing build are set.

See `this
example <https://searchfox.org/mozilla-central/rev/110706c3c09d457dc70293b213d7bccb4f6f5643/media/mtransport/fuzztest/moz.build>`__
for how the ``moz.build`` in your subdirectory could look like and `this
example <https://searchfox.org/mozilla-central/rev/de7676288a78b70d2b9927c79493adbf294faad5/media/mtransport/moz.build#18-24>`__
for how your directory is enabled in the fuzzing build.


How your code should look like
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

See `this
example <https://searchfox.org/mozilla-central/source/media/mtransport/fuzztest/stun_parser_libfuzz.cpp>`__.


How to build your code
^^^^^^^^^^^^^^^^^^^^^^

After a regular ``mach build``, you need to run an additional
``mach gtest nonexistant`` to ensure that the gtests are built but no
tests are executed. You only need to rerun the gtest command for changes
to your fuzzing implementation.

.. note::

   **Note:** If you have set the below mentioned environment variables,
   for example because you are rebuilding from the same shell which you
   used for a previous run, then this command will start the fuzzing.
   For cleanliness it is recommended to interrupt the running executable
   with CTRL+C at that point and restart just the firefox binary as
   described below.


How to run your code
^^^^^^^^^^^^^^^^^^^^

You need to set the following environment variable to enable running the
fuzzing code inside Firefox instead of the regular browser.

-  ``FUZZER=name``

Where ``name`` is the name of your fuzzing module that you specified
when calling the ``MOZ_FUZZING_INTERFACE_RAW`` macro. For the example
above, this would be “StunParser”.

Now when you invoke the firefox binary in your build directory with the
``-help=1`` parameter, you should see the regular libFuzzer help. On
Linux for example:

::

   $ FUZZER=StunParser obj-asan/dist/bin/firefox -help=1

You should see an output similar to this:

::

   Running Fuzzer tests...
   Usage:

   To run fuzzing pass 0 or more directories.
   obj-asan/dist/bin/firefox [-flag1=val1 [-flag2=val2 ...] ] [dir1 [dir2 ...] ]

   To run individual tests without fuzzing pass 1 or more files:
   obj-asan/dist/bin/firefox [-flag1=val1 [-flag2=val2 ...] ] file1 [file2 ...]

   Flags: (strictly in form -flag=value)
    verbosity                      1       Verbosity level.
    seed                           0       Random seed. If 0, seed is generated.
    runs                           -1      Number of individual test runs (-1 for infinite runs).
    max_len                        0       Maximum length of the test input. If 0, libFuzzer tries to guess a good value based on the corpus and reports it.
   ...


Reproducing a Crash
'''''''''''''''''''

In order to reproduce a crash from a given test file, simply put the
file as the only argument on the command line, e.g.

::

   $ FUZZER=StunParser obj-asan/dist/bin/firefox test.bin

This should reproduce the given problem.


FuzzManager and libFuzzer
^^^^^^^^^^^^^^^^^^^^^^^^^

Our FuzzManager project comes with a harness for running libFuzzer with
an optional connection to a FuzzManager server instance. Note that this
connection is not mandatory, even without a server you can make use of
the local harness.

You can find the harness
`here <https://github.com/MozillaSecurity/FuzzManager/tree/master/misc/afl-libfuzzer>`__.

An example invocation for the harness to use with StunParser could look
like this:

::

   FUZZER=StunParser python /path/to/afl-libfuzzer-daemon.py --fuzzmanager \
       --stats libfuzzer-stunparser.stats --libfuzzer-auto-reduce-min 500 --libfuzzer-auto-reduce 30 \
       --tool libfuzzer-stunparser --libfuzzer --libfuzzer-instances 6 obj-asan/dist/bin/firefox \
       -max_len=256 -use_value_profile=1 -rss_limit_mb=3000 corpus-stunparser

What this does is

-  run libFuzzer on the ``StunParser`` target with 6 parallel instances
   using the corpus in the ``corpus-stunparser`` directory (with the
   specified libFuzzer options such as ``-max_len`` and
   ``-use_value_profile``)
-  automatically reduce the corpus and restart if it grew by 30% (and
   has at least 500 files)
-  use FuzzManager (need a local ``.fuzzmanagerconf`` and a
   ``firefox.fuzzmanagerconf`` binary configuration as described in the
   FuzzManager manual) and submit crashes as ``libfuzzer-stunparser``
   tool
-  write statistics to the ``libfuzzer-stunparser.stats`` file


Troubleshooting
~~~~~~~~~~~~~~~


Fuzzing Interface: Error: No testing callback found
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This error means that the fuzzing callback with the name you specified
using the ``FUZZER`` environment variable could not be found. Reasons
for are typically either a misspelled name or that your code wasn't
built (check your ``moz.build`` file and build log).


``mach build`` doesn't seem to update my fuzzing code
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Keep in mind you always need to run both the ``mach build`` and
``mach gtest nonexistant`` commands in order to update your fuzzing
code. The latter rebuilds the gtest version of ``libxul``, containing
your code.
