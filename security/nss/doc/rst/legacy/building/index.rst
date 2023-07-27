.. _mozilla_projects_nss_building_ported:

Building NSS
============

`Introduction <#introduction>`__
--------------------------------

.. container::

   This page has detailed information on how to build NSS. Because NSS is a cross-platform library
   that builds on many different platforms and has many options, it may be complex to build. Please
   read these instructions carefully before attempting to build.

.. _build_environment:

`Build environment <#build_environment>`__
------------------------------------------

.. container::

   NSS needs a C and C++ compiler. It has minimal dependencies, including only standard C and C++
   libraries, plus `zlib <https://www.zlib.net/>`__.

   For building, you also need `make <https://www.gnu.org/software/make/>`__. Ideally, also install
   `gyp <https://gyp.gsrc.io/>`__ and `ninja <https://ninja-build.org/>`__ and put them on your
   path. This is recommended, as the build is faster and more reliable.

`Windows <#windows>`__
~~~~~~~~~~~~~~~~~~~~~~

.. container::

   NSS compilation on Windows uses the same shared build system as Mozilla Firefox. You must first
   install the `Windows
   Prerequisites <https://firefox-source-docs.mozilla.org/setup/windows_build.html>`__,
   including **MozillaBuild**.

   You can also build NSS on the Windows Subsystem for Linux, but the resulting binaries aren't
   usable by other Windows applications.

.. _get_the_source:

`Get the source <#get_the_source>`__
------------------------------------

.. container::

   NSS and NSPR use Mercurial for source control like other Mozilla projects. To check out the
   latest sources for NSS and NSPR--which may not be part of a stable release--use the following
   commands:

   .. code::

      hg clone https://hg.mozilla.org/projects/nspr
      hg clone https://hg.mozilla.org/projects/nss

   To get the source of a specific release, see :ref:`mozilla_projects_nss_nss_releases`.

`Build <#build>`__
------------------

.. container::

   Build NSS using our build script:

   .. code::

      nss/build.sh

   This builds both NSPR and NSS.

.. _build_with_make:

`Build with make <#build_with_make>`__
--------------------------------------

.. container::

   Alternatively, there is a ``make`` target called "nss_build_all", which produces a similar
   result. This supports some alternative options, but can be a lot slower.

   .. code::

      make -C nss nss_build_all USE_64=1

   The make-based build system for NSS uses a variety of variables to control the build. Below are
   some of the variables, along with possible values they may be set to.

   BUILD_OPT
      0
         Build a debug (non-optimized) version of NSS. *This is the default.*
      1
         Build an optimized (non-debug) version of NSS.

   USE_64
      0
         Build for a 32-bit environment/ABI. *This is the default.*
      1
         Build for a 64-bit environment/ABI. *This is recommended.*

   USE_ASAN
      0
         Do not create an `AddressSanitizer <http://clang.llvm.org/docs/AddressSanitizer.html>`__
         build. *This is the default.*
      1
         Create an AddressSanitizer build.

.. _unit_testing:

`Unit testing <#unit_testing>`__
--------------------------------

.. container::

   NSS contains extensive unit tests. Scripts to run these are found in the ``tests`` directory.
   Run the standard suite by:

   .. code::

      HOST=localhost DOMSUF=localdomain USE_64=1 nss/tests/all.sh

.. _unit_test_configuration:

`Unit test configuration <#unit_test_configuration>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   | NSS tests are configured using environment variables.
   | The scripts will attempt to infer values for ``HOST`` and ``DOMSUF``, but can fail. Replace
     ``localhost`` and ``localdomain`` with the hostname and domain suffix for your host. You need
     to be able to connect to ``$HOST.$DOMSUF``.

   If you don't have a domain suffix you can add an entry to ``/etc/hosts`` (on
   Windows,\ ``c:\Windows\System32\drivers\etc\hosts``) as follows:

   .. code::

      127.0.0.1 localhost.localdomain

   Validate this opening a command shell and typing: ``ping localhost.localdomain``.

   Remove the ``USE_64=1`` override if using a 32-bit build.

.. _test_results:

`Test results <#test_results>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Running all tests can take a considerable amount of time.

   Test output is stored in ``tests_results/security/$HOST.$NUMBER/``. The file ``results.html``
   summarizes the results, ``output.log`` captures all the test output.

   Other subdirectories of ``nss/tests`` contain scripts that run a subset of the full suite. Those
   can be run directly instead of ``all.sh``, which might save some time at the cost of coverage.
