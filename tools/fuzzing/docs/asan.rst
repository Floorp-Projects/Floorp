{{ApiRef}}

.. _What_is_Address_Sanitizer:

What is Address Sanitizer?
--------------------------

Address Sanitizer (ASan) is a fast memory error detector that detects
use-after-free and out-of-bound bugs in C/C++ programs. It uses a
compile-time instrumentation to check all reads and writes during the
execution. In addition, the runtime part replaces the ``malloc`` and
``free`` functions to check dynamically allocated memory. More
information on how ASan works can be found on `the Address Sanitizer
wiki <http://code.google.com/p/address-sanitizer/wiki/AddressSanitizerAlgorithm>`__.

.. _Downloading_artifact_builds:

Downloading artifact builds
---------------------------

For Linux and Windows users, the easiest way to get Firefox builds with
Address Sanitizer is to download a continuous integration asan build of
mozilla-central (updated at least daily):

-  mozilla-central optimized builds:
   `linux <https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/gecko.v2.mozilla-central.latest.firefox.linux64-asan-opt/artifacts/public/build/target.tar.bz2>`__
   \|
   `windows <https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/gecko.v2.mozilla-central.latest.firefox.win64-asan-opt/artifacts/public/build/target.zip>`__
   (recommended for testing)
-  mozilla-central debug builds:
   `linux <https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/gecko.v2.mozilla-central.latest.firefox.linux64-asan-debug/artifacts/public/build/target.tar.bz2>`__
   \|
   `windows <https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/gecko.v2.mozilla-central.latest.firefox.win64-asan-debug/artifacts/public/build/target.zip>`__
   (recommended for debugging if the optimized builds don't do the job)

.. _Creating_Try_builds:

Creating Try builds
-------------------

If for some reason you can't use the pre-built binaries mentioned in the
previous section (e.g. you want a non-Linux build or you need to test a
patch), you can either build Firefox yourself (see the following
section) or use the `try
server <https://wiki.mozilla.org/ReleaseEngineering/TryServer>`__ to
create the customized build for you. Pushing to try requires L1 commit
access. If you don't have this access yet you can request access (see
`Becoming A Mozilla
Committer <https://www.mozilla.org/en-US/about/governance/policies/commit/>`__
and \ `Mozilla Commit Access
Policy <https://www.mozilla.org/en-US/about/governance/policies/commit/access-policy/>`__
for the requirements). Note that this kind of access is mainly for
developers and other regular contributors.

The tree contains `several mozconfig files for creating asan
builds <https://dxr.mozilla.org/mozilla-central/search?q=path%3Abrowser%2Fconfig%2Fmozconfigs%2F+path%3Aasan>`__
(the "nightly-asan" files create release builds, whereas the
"debug-asan" files create debug+opt builds). For Linux builds, the
appropriate configuration file is used by the ``linux64-asan`` target.
If you want to create a macOS or Windows build, you'll need to copy the
appropriate configuration file over the regular debug configuration
before pushing to try. For example:

::

   cp browser/config/mozconfigs/macosx64/debug-asan browser/config/mozconfigs/macosx64/debug

You can then `push to Try in the usual
way <https://wiki.mozilla.org/Build:TryServer#How_to_push_to_try>`__
and, once the build is complete, download the appropriate build
artifact.

.. _Creating_local_builds_on_Windows:

Creating local builds on Windows
--------------------------------

On Windows, ASan is supported only in 64-bit builds.

Run ``mach bootstrap`` to get an updated clang-cl in your
``~/.mozbuild`` directory, then use the following
`mozconfig </en-US/docs/Configuring_Build_Options>`__:

::

   ac_add_options --target=x86_64-pc-mingw32
   ac_add_options --host=x86_64-pc-mingw32

   ac_add_options --enable-address-sanitizer
   ac_add_options --disable-jemalloc

   export CC="clang-cl.exe"
   export CXX="clang-cl.exe"

   export LDFLAGS="clang_rt.asan_dynamic-x86_64.lib clang_rt.asan_dynamic_runtime_thunk-x86_64.lib"
   CLANG_LIB_DIR="$(cd ~/.mozbuild/clang/lib/clang/*/lib/windows && pwd)"
   export MOZ_CLANG_RT_ASAN_LIB_PATH="${CLANG_LIB_DIR}/clang_rt.asan_dynamic-x86_64.dll"
   export LIB=$LIB:$CLANG_LIB_DIR

If you want to use a different LLVM (see the `clang-cl
instructions </en-US/docs/Mozilla/Developer_guide/Build_Instructions/Building_Firefox_on_Windows_with_clang-cl>`__),
alter CLANG_LIB_DIR as appropriate.

If you launch an ASan build under WinDbg, you may see spurious
first-chance Access Violation exceptions. These come from ASan creating
shadow memory pages on demand, and can be ignored. Run ``sxi av`` to
ignore these exceptions. (You will still catch second-chance Access
Violation exceptions if you actually crash.)

LeakSanitizer (LSan) is not supported on Windows.

.. _Creating_local_builds_on_Linux_or_Mac:

Creating local builds on Linux or Mac
-------------------------------------

.. _Build_prerequisites:

Build prerequisites
~~~~~~~~~~~~~~~~~~~

.. _LLVMClang:

LLVM/Clang
^^^^^^^^^^

The ASan instrumentation is implemented as an LLVM pass and integrated
into Clang. Any clang version that is capable of compiling Firefox has
everything needed to do an ASAN build.

.. _Building_Firefox:

Building Firefox
~~~~~~~~~~~~~~~~

.. _Getting_the_source:

Getting the source
^^^^^^^^^^^^^^^^^^

Using that or any later revision, all you need to do is to `get yourself
a clone of
mozilla-central </En/Developer_Guide/Source_Code/Mercurial>`__.

.. _Adjusting_the_build_configuration:

Adjusting the build configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Create the build configuration file ``mozconfig`` with the following
content in your mozilla-central directory:

::

   # Combined .mozconfig file for ASan on Linux+Mac

   mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/objdir-ff-asan

   # Enable ASan specific code and build workarounds
   ac_add_options --enable-address-sanitizer

   # Add ASan to our compiler flags
   export CFLAGS="-fsanitize=address -Dxmalloc=myxmalloc -fPIC"
   export CXXFLAGS="-fsanitize=address -Dxmalloc=myxmalloc -fPIC"

   # Additionally, we need the ASan flag during linking. Normally, our C/CXXFLAGS would
   # be used during linking as well but there is at least one place in our build where
   # our CFLAGS are not added during linking.
   # Note: The use of this flag causes Clang to automatically link the ASan runtime :)
   export LDFLAGS="-fsanitize=address"

   # These three are required by ASan
   ac_add_options --disable-jemalloc
   ac_add_options --disable-crashreporter
   ac_add_options --disable-elf-hack

   # Keep symbols to symbolize ASan traces later
   export MOZ_DEBUG_SYMBOLS=1
   ac_add_options --enable-debug-symbols
   ac_add_options --disable-install-strip

   # Settings for an opt build (preferred)
   # The -gline-tables-only ensures that all the necessary debug information for ASan
   # is present, but the rest is stripped so the resulting binaries are smaller.
   ac_add_options --enable-optimize="-O2 -gline-tables-only"
   ac_add_options --disable-debug

   # Settings for a debug+opt build
   #ac_add_options --enable-optimize
   #ac_add_options --enable-debug

   # MacOSX only: Uncomment and adjust this path to match your SDK
   # ac_add_options --with-macos-sdk=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.8.sdk

You may also need this, as seen in
``browser/config/mozconfigs/linux64/nightly-asan`` (the config file used
for Address Sanitizer builds used for automated testing):

::

   # ASan specific options on Linux
   ac_add_options --enable-valgrind

.. _Starting_the_build_process:

Starting the build process
^^^^^^^^^^^^^^^^^^^^^^^^^^

Now you start the build process using the regular ``./mach build``
command.

.. _Starting_Firefox:

Starting Firefox
^^^^^^^^^^^^^^^^

After the build has completed, ``./mach run`` with the usual options for
running in a debugger (``gdb``, ``lldb``, ``rr``, etc.) work fine, as do
the ``--disable-e10s`` and other options.

.. _Building_only_the_JavaScript_shell:

Building only the JavaScript shell
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you want to build only the JavaScript shell instead of doing a full
Firefox build, the build script below will probably help you to do so.
Execute this script in the ``js/src/`` subdirectory and pass a directory
name as the first parameter. The build will then be created in a new
subdirectory with that name.

::

   #! /bin/sh

   if [ -z $1 ] ; then
       echo "usage: $0 <dirname>"
   elif [ -d $1 ] ; then
       echo "directory $1 already exists"
   else
       autoconf2.13
       mkdir $1
       cd $1
       CC="clang" \
       CXX="clang++" \
       CFLAGS="-fsanitize=address" \
       CXXFLAGS="-fsanitize=address" \
       LDFLAGS="-fsanitize=address" \
               ../configure --enable-debug --enable-optimize --enable-address-sanitizer --disable-jemalloc
   fi

.. _Getting_Symbols_in_Address_Sanitizer_Traces:

Getting Symbols in Address Sanitizer Traces
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

By default, ASan traces are unsymbolized and only print the
binary/library and a memory offset instead. In order to get more useful
traces, containing symbols, there are two approaches.

.. _Using_the_LLVM_Symbolizer_recommended:

Using the LLVM Symbolizer (recommended)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

LLVM ships with a symbolizer binary that ASan will readily use to
immediately output symbolized traces. To use it, just set the
environment variable ``ASAN_SYMBOLIZER_PATH`` to reflect the location of
your ``llvm-symbolizer`` binary, before running the process. This
program is usually included in an LLVM distribution. Stacks without
symbols can also be post-processed, see bellow.

.. warning::

   .. note::

      **Warning:** On OS X, the content sandbox prevents the symbolizer
      from running.  To use llvm-symbolizer on ASan output from a
      content process, the content sandbox must be disabled. This can be
      done by setting ``MOZ_DISABLE_CONTENT_SANDBOX=1`` in your run
      environment. Setting this in .mozconfig has no effect.

.. _Post-Processing_Traces_with_asan_symbolize.py:

Post-Processing Traces with asan_symbolize.py
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Instead of using the llvm-symbolizer binary, you can also pipe the
output through the ``asan_symbolize.py`` script, shipped with LLVM
(``$LLVM_HOME/projects/compiler-rt/lib/asan/scripts/asan_symbolize.py``),
often included in LLVM distributions. The disadvantage is that the
script will need to use ``addr2line`` to get the symbols, which means
that every library will have to be loaded into memory
(including``libxul``, which takes a bit).

However, in certain situations it makes sense to use this script. For
example, if you have/received an unsymbolized trace, then you can still
use the script to turn it into a symbolized trace, given that you can
get the original binaries that produced the unsymbolized trace. In order
for the script to work in such cases, you need to ensure that the paths
in the trace point to the actual binaries, or change the paths
accordingly.

Since the output of the ``asan_symbolize.py`` script is still mangled,
you might want to pipe the output also through ``c++filt`` afterwards.

.. _Troubleshooting_Known_problems:

Troubleshooting / Known problems
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. _Cannot_specify_-o_when_generating_multiple_output_files:

Cannot specify -o when generating multiple output files
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you get the error
"``cannot specify -o when generating multiple output files"`` from
clang, disable ``elf-hack`` in your ``mozconfig`` to work around the
issue:

::

   ac_add_options --disable-elf-hack

.. _Optimized_build:

Optimized build
^^^^^^^^^^^^^^^

Since `an issue with -O2/-Os and
ASan <http://code.google.com/p/address-sanitizer/issues/detail?id=20>`__
has been resolved, the regular optimizations used by Firefox should work
without any problems. The optimized build has only a barely noticable
speed penalty and seems to be even faster than regular debug builds.

.. _No_AddressSanitizer_libc_interceptors_initialized_shows_after_running_.mach_run:

No "AddressSanitizer: **libc** interceptors initialized" shows after running ./mach run
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

::

   $ ASAN_OPTIONS=verbosity=2 ./mach run

Use the above command instead

.. _An_admin_user_name_and_password_is_required_to_enter_Developer_Mode:

"An admin user name and password" is required to enter Developer Mode
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Please enable **Developper** **mode** by:

::

   $ /usr/sbin/DevToolsSecurity -enable
   Developer mode is now enabled.

.. _Debugging_issues_that_ASan_finds:

Debugging issues that ASan finds
--------------------------------

When ASan discovers an issue it will simply print an error message and
exit the app. To stop the app in a debugger before ASan exits it, set a
breakpoint on ``__asan::ReportGenericError``. For more info on using
ASan and debugging issues that it uncovers, see the page `Address
sanitizer and a
debugger <https://github.com/google/sanitizers/wiki/AddressSanitizerAndDebugger>`__
page on the upstream wiki.

``__asan_describe_address(pointer)`` issued at the debugger prompt or
even directly in the code allows outputing lots of information about
this memory address (thread and stack of allocation, of deallocation,
whether or not it is a bit outside a known buffer, thread and stack of
allocation of this buffer, etc.). This can be useful to understand where
some buffer that is not aligned was allocated, when doing SIMD work, for
example.

`rr <https://rr-project.org/>`__ (Linux x86 only) works great with ASan
and combined, this combo allows doing some very powerful debugging
strategies.

.. _LeakSanitizer:

LeakSanitizer
-------------

LeakSanitizer (LSan) is a special execution mode for regular ASan.  It
takes advantage of how ASan tracks the set of live blocks at any given
point to print out the allocation stack of any block that is still alive
at shutdown, but is not reachable from the stack, according to a
conservative scan.  This is very useful for detecting leaks of things
such as ``char*`` that do not participate in the usual Gecko shutdown
leak detection. LSan is supported on x86_64 Linux and OS X.

LSan is enabled by default in ASan builds, as of more recent versions of
Clang. To make an ASan build not run LSan, set the environment variable
``ASAN_OPTIONS`` to ``detect_leaks=0`` (or add it as an entry to a
``:``-separated list if it is already set to something).  If you want to
enable it when it is not for some reason, set it to 1 instead of 0. If
LSan is enabled and you are using a non-debug build, you will also want
to set the environment variable ``MOZ_CC_RUN_DURING_SHUTDOWN=1``, to
ensure that we run shutdown GCs and CCs to avoid spurious leaks.

If an object that is reported by LSan is intentionally never freed, a
symbol can be added to ``build/sanitizers/lsan_suppressions.txt`` to get
LSan to ignore it.

For some more information on LSan, see the `Leak Sanitizer wiki
page <https://github.com/google/sanitizers/wiki/AddressSanitizerLeakSanitizer>`__.
