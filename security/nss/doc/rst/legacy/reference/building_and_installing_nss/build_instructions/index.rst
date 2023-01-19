.. _mozilla_projects_nss_reference_building_and_installing_nss_build_instructions:

Build instructions
==================

.. container::

   .. note::

      These instructions are outdated.  Use the :ref:`mozilla_projects_nss_building` page for more
      recent information.

   Numerous optional features of NSS builds are controlled through make variables.

   gmake is GNU make, usually your Linux-distro-regular "make" binary file, unless maybe it is a BSD
   make. Make variables may be set on the gmake command line, e.g.,

   .. code::

        gmake variable=value variable=value target1 target2

   or defined in the environment, e.g. (for POSIX shells),

   .. code::

        variable=value; export variable
        gmake target1 target2

   Here are some (not all) of the make variables that affect NSS builds:

   -  BUILD_OPT: If set to 1, means do optimized non-DEBUG build. Default is DEBUG, non-optimized
      build.
   -  USE_DEBUG_RTL: If set to 1, on Windows, causes build with debug version of the C run-time
      library.
   -  NS_USE_GCC: On platforms where gcc is not the native compiler, tells NSS to build with gcc
      instead of the native compiler. Default is to build with the native compiler.
   -  USE_64: On platforms that support both 32-bit and 64-bit ABIs, tells NSS to build for the
      64-bit ABI. Default is 32-bit ABI, except on platforms that do not support a 32-bit ABI.
   -  MOZ_DEBUG_SYMBOLS: tells NSS to build with debug symbols, even in an optimized build. On
      windows, in both DEBUG and optimized builds, when using MSVC, tells NSS to put symbols in a
      .pdb file. Required to build with MSVC 8 (2005 Express). Default is not to put debug symbols
      into optimized builds, and for MSVC, is to put symbols into the .exe or .dll file.
   -  NSDISTMODE: If set to 'copy', mozilla/dist/<OBJ_STUFF>/bin/\* real files instead of symbolic
      links.

   These variables should be either undefined, or set to "1". Results are undefined for variables
   set to "0".

   For Windows, install
   the `MozillaBuild <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions/Windows_Prerequisites#mozillabuild>`__ environment
   and Microsoft Visual Studio 2010. (The free edition works, and other versions like Visual Studio
   2008 and Visual Studio 2012 may also work.) Use start-shell-msvc2010.bat from MozillaBuild to get
   a bash shell with the PATH already configured, and execute these instructions from within that
   bash shell.

   For RHEL-5, you need to use the new assembler. You can install the new assembler as root as
   follows:

   .. code::

      yum install binutils220

   You can then use the new assembler by adding /usr/libexec/binutils220 to the beginning of your
   build path. This can be done in sh or bash as follows:

   .. code::

      export PATH=/usr/libexec/binutils220:$PATH

   The following build instructions should work for all platforms (with some platform-specific
   changes as noted).

.. _build_instructions_for_recent_versions_(mercurial):

`Build Instructions for Recent Versions (Mercurial) <#build_instructions_for_recent_versions_(mercurial)>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   #. Clone the NSPR and NSS repositories.

      .. code::

         hg clone https://hg.mozilla.org/projects/nspr
         hg clone https://hg.mozilla.org/projects/nss

   #. If you want to build a releases other than the tips of these repositories, then switch to the
      release tags:

      .. code::

         cd nspr
         hg update NSPR_4_9_5_RTM
         cd ../nss
         hg update NSS_3_14_2_RTM
         cd ..

   #. Set environment variables:

      #. If you want a non-debug optimized build, set ``BUILD_OPT=1`` in your environment.
         Otherwise, you get a debug build. On Windows, if you want a debug build with the system's
         debug RTL libraries, set ``USE_DEBUG_RTL=1`` in your environment.
      #. On Unix platforms, except Alpha/OSF1, if you want a build for the system's 64-bit ABI, set
         ``USE_64=1`` in your environment. By default, NSS builds for the 32-bit environment on all
         platforms except Alpha/OSF1.
      #. To build with ``gcc`` on platforms other than Linux and Windows, you need to set two more
         environment variables:

         -  ``NS_USE_GCC=1``
            ``NO_MDUPDATE=1``

      #. For HP-UX, you must set the environment variable ``USE_PTHREADS`` to 1.

   #. ``cd nss``

   #. ``gmake nss_build_all``

   The output of the build will be in the ``dist`` directory alongside the ``nspr`` and ``nss``
   directories.

   For information on troubleshooting the build system, see
   :ref:`mozilla_projects_nss_reference_troubleshoot`.

.. _build_instructions_for_older_versions_(cvs):

`Build Instructions for Older Versions (CVS) <#build_instructions_for_older_versions_(cvs)>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   #. Set the environment variable ``CVSROOT`` to
      ``:pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot``

   #. ``cvs login`` (if you haven't before).

   #. Check out NSPR and NSS:

      .. code::

         cvs co -r NSPR_4_9_5_RTM NSPR
         cvs co -r NSS_3_14_2_RTM NSS

   #. Set environment variables as described in the Mercurial-based instructions.

   #. ``cd mozilla/security/nss``

   #. ``gmake nss_build_all``

   The output of the build will be in ``mozilla/dist`` subdirectory.

   For information on troubleshooting the build system, see
   :ref:`mozilla_projects_nss_reference_troubleshoot`.