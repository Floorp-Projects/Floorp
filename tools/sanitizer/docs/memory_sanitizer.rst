Memory Sanitizer
================

+--------------------------------------------------------------------+
| This page is an import from MDN and the contents might be outdated |
+--------------------------------------------------------------------+

What is Memory Sanitizer?
-------------------------

Memory Sanitizer (MSan) is a fast detector used for uninitialized memory
in C/C++ programs. It uses a compile-time instrumentation to ensure that
all memory access at runtime uses only memory that has been initialized.
Unlike most other sanitizers, MSan can easily cause false positives if
not all libraries are instrumented. This happens because MSan is
not able to observe memory initialization in uninstrumented libraries.
More information on MSan can be found on `the Memory Sanitizer
wiki <https://github.com/google/sanitizers/wiki/MemorySanitizer>`__.

Public Builds
-------------

**Note:** No public builds are available at this time yet.

Manual Build
------------

Build prerequisites
~~~~~~~~~~~~~~~~~~~

**Note:** MemorySanitizer requires **64-bit Linux** to work. Other
platforms/operating systems are not supported.

LLVM/Clang
^^^^^^^^^^

The MSan instrumentation is implemented as an LLVM pass and integrated
into Clang. As MSan is one of the newer sanitizers, we recommend using a
recent Clang version, such as Clang 3.7+.

You can find precompiled binaries for LLVM/Clang on `the LLVM releases
page <https://releases.llvm.org/download.html>`__.

Building Firefox
~~~~~~~~~~~~~~~~

.. warning::

   **Warning: Running Firefox with MemorySanitizer would require all
   external dependencies to be built with MemorySanitizer as well. To
   our knowledge, this has never been attempted yet, so the build
   configuration provided here is untested and without an appropriately
   instrumented userland, it will cause false positives.**

Getting the source
^^^^^^^^^^^^^^^^^^

If you don't have a source code repository clone yet, you need to :ref:`get
yourself a clone of Mozilla-central <Mercurial Overview>`.

Adjusting the build configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Create the build configuration file ``.mozconfig`` with the following
content in your Mozilla-central directory:

.. code:: bash

   mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/objdir-ff-msan

   # Enable LLVM specific code and build workarounds
   ac_add_options --enable-memory-sanitizer
   # If clang is already in your $PATH, then these can simply be:
   #   export CC=clang
   #   export CXX=clang++
   export CC="/path/to/clang"
   export CXX="/path/to/clang++"

   # llvm-symbolizer displays much more complete backtraces when data races are detected.
   # If it's not already in your $PATH, then uncomment this next line:
   #export LLVM_SYMBOLIZER="/path/to/llvm-symbolizer"

   # Add MSan to our compiler flags
   export CFLAGS="-fsanitize=memory"
   export CXXFLAGS="-fsanitize=memory"

   # Additionally, we need the MSan flag during linking. Normally, our C/CXXFLAGS would
   # be used during linking as well but there is at least one place in our build where
   # our CFLAGS are not added during linking.
   # Note: The use of this flag causes Clang to automatically link the MSan runtime :)
   export LDFLAGS="-fsanitize=memory"

   # These three are required by MSan
   ac_add_options --disable-jemalloc
   ac_add_options --disable-crashreporter
   ac_add_options --disable-elf-hack

   # Keep symbols to symbolize MSan traces
   export MOZ_DEBUG_SYMBOLS=1
   ac_add_options --enable-debug-symbols
   ac_add_options --disable-install-strip

   # Settings for an opt build
   ac_add_options --enable-optimize="-O2 -gline-tables-only"
   ac_add_options --disable-debug

Starting the build process
^^^^^^^^^^^^^^^^^^^^^^^^^^

Now you start the build process using the regular ``make -f client.mk``
command.

Starting Firefox
^^^^^^^^^^^^^^^^

After the build has completed, you can start Firefox from the ``objdir``
as usual.

Building the JavaScript shell
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Note:** Unlike Firefox itself, the JavaScript shell does **not**
require an instrumented userland. Calls to external libraries like
zlib are handled with special annotations inside the engine.

.. warning::

   **Warning: Certain technologies used inside the JavaScript engine are
   incompatible with MSan and must be disabled at runtime to prevent
   false positives. This includes the JITs and asm.js. Therefore always
   make sure to run with
   ``--no-ion --no-baseline --no-asmjs --no-native-regexp``.**

If you want to build only the JavaScript shell instead of doing a full
Firefox build, the build script below will probably help you to do so.
Before using it, you must, of course, adjust the path name for
``LLVM_ROOT`` to match your setup. Once you have adjusted everything,
execute this script in the ``js/src/`` subdirectory and pass a directory
name as the first parameter. The build will then be created in a new
subdirectory with that name.

.. code:: bash

   #! /bin/sh

   if [ -z $1 ] ; then
       echo "usage: $0 <dirname>"
   elif [ -d $1 ] ; then
       echo "directory $1 already exists"
   else
       autoconf2.13
       mkdir $1
       cd $1
       LLVM_ROOT="/path/to/llvm"
       CC="$LLVM_ROOT/build/bin/clang" \
       CXX="$LLVM_ROOT/build/bin/clang++" \
       CFLAGS="-fsanitize=memory" \
       CXXFLAGS="-fsanitize=memory" \
       LDFLAGS="-fsanitize=memory" \
               ../configure --enable-debug --enable-optimize --enable-memory-sanitizer --disable-jemalloc --enable-posix-nspr-emulation
       make -j 8
   fi

Using LLVM Symbolizer for faster/better traces
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

By default, MSan traces are not symbolized.

LLVM ships with the symbolizer binary ``llvm-symbolize`` that MSan will
readily use to immediately output symbolized traces if the program is
found on the ``PATH``. If your ``llvm-symbolizer`` lives outside the
``PATH``, you can set the ``MSAN_SYMBOLIZER_PATH`` environment variable
to point to your symbolizer binary.
