.. _mozilla_projects_nss_nss_sources_building_testing:

NSS sources building testing
============================

.. container::

   Getting the source code of :ref:`mozilla_projects_nss`, how to build it, and how to run its test
   suite.

.. _getting_source_code_and_a_quick_overview:

`Getting source code, and a quick overview <#getting_source_code_and_a_quick_overview>`__
-----------------------------------------------------------------------------------------

.. container::

   The easiest way is to download archives of NSS releases from `Mozilla's download
   server <https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/>`__. Find the directory
   that contains the highest version number. Because NSS depends on the base library
   `NSPR <https://developer.mozilla.org/en-US/docs/NSPR>`__, you should download the archive that
   combines both NSS and NSPR.

   If you are a software developer and intend to contribute enhancements to NSS, you should obtain
   the latest development snapshot of NSS using mercurial/hg (a `distributed source control
   management tool <https://www.mercurial-scm.org/>`__). In order to get started, anonymous
   read-only access is sufficient. Create a new directory on your computer that you will use as your
   local work area, and run the following commands.

   .. code:: sh

      hg clone https://hg.mozilla.org/projects/nspr
      hg clone https://hg.mozilla.org/projects/nss

   After the above commands complete, you should have two local directories, named nspr and nss,
   next to each other.

   (Historical information: NSPR and NSS source code have recently been re-organized into a new
   directory structure. In past versions, all files were located in a directory hierarchy that
   started with the "mozilla" prefix. The NSPR base library was located in directory
   mozilla/nsprpub. The subdirectories dbm, security/dbm, security/coreconf, security/nss were part
   of the NSS sources.)

   The nss directory contains the following important subdirectories:

   -  nss/coreconf
      Contains knowledge for cross platform building.
   -  nss/lib
      Contains all the library code that is used to create the runtime libraries used by
      applications.
   -  nss/cmd
      Contains a set of various tool programs that are built using NSS. Several tools are general
      purpose and can be used to inspect and manipulate the storage files that software using the
      NSS library creates and modifies. Other tools are only used for testing purposes. However, all
      these tools are good examples of how to write software that makes use of the NSS library.
   -  nss/test
      This directory contains the NSS test suite, which is routinely used to ensure that changes to
      NSS don't introduce regressions.
   -  nss/gtests
      Code for NSS unit tests running in `Googletest <https://github.com/abseil/googletest>`__.

   It is important to mention the difference between internal NSS code and exported interfaces.
   Software that would like to use the NSS library must use only the exported interfaces. These can
   be found by looking at the files with the .def file extension, inside the nss/lib directory
   hierarchy. Any C function that isn't contained in .def files is strictly for private use within
   NSS, and applications and test tools are not allowed to call them. For any functions that are
   listed in the .def files, NSS promises that the binary function interface (ABI) will remain
   stable.

.. _building_nss:

`Building NSS <#building_nss>`__
--------------------------------

.. container::

   NSS is built using `gyp <https://gyp.gsrc.io/>`__ and `ninja <https://ninja-build.org/>`__, or
   with `make <https://www.gnu.org/software/make/>`__ on systems that don't have those tools. The
   :ref:`mozilla_projects_nss_building` include more information.

   Once the build is done, you can find the build output below directory dist/?, where ? will be a
   name dynamically derived from your system's architecture. Exported header files for NSS
   applications can be found in directory "include", library files in directory "lib", and the tools
   in directory "bin". In order to run the tools, you should set your system environment to use the
   libraries of your build from the "lib" directory, e.g., using the LD_LIBRARY_PATH or
   DYLD_LIBRARY_PATH environment variable.

.. _running_the_nss_test_suite:

`Running the NSS test suite <#running_the_nss_test_suite>`__
------------------------------------------------------------

.. container::

   This is an important part of development work, in order to ensure your changes don't introduce
   regressions. When adding new features to NSS, tests for the new feature should be added as well.

   You must build NSS prior to running the tests. After the build on your computer has succeeded,
   before you can run the tests on your computer, it might be necessary to set additional
   environment variables. The NSS tests will start TCP/IP server tools on your computer, and in
   order for that to work, the NSS test suite needs to know which hostname can be used by client
   tools to connect to the server tools. On machines that are configured with a hostname that has
   been registered in your network's DNS, this should work automatically. In other environments (for
   example in home networks), you could set the HOST and DOMSUF (for domain suffix) environment
   variables to tell the NSS suite which hostname to use. As a test, it must be possible to
   successfully use the command "ping $HOST.$DOMSUF" on your computer (ping reports receiving
   replies). On many computers the variables HOST=localhost DOMSUF=localdomain works. In case you
   built NSS in 64 bits, you need to set the USE_64 environment variable to 1 to run the tests. If
   you get name resolution errors, try to disable IPv6 on the loopback device.

   After you have set the required environment variables, use "cd nss/tests" and start the tests
   using "./all.sh". The tests will take a while to complete; on a slow computer it could take a
   couple of hours.

   Once the test suite has completed, a summary will be printed that shows the number of failures.
   You can find the test suite results in directory nss/../tests_results (i.e. the results directory
   ends up next to the nss directory, not within it). Each test suite execution will create a new
   subdirectory; you should clean them up from time to time. Inside the directory you'll find text
   file output.log, which contains a detailed report of all tests being executed. In order to learn
   about the details of test failures, search the file for the uppercase test FAILED.

   If desired, it's possible to run only subsets of the tests. Read the contents of file all.sh to
   learn how that works.