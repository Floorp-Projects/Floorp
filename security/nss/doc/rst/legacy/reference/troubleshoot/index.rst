.. _mozilla_projects_nss_reference_troubleshoot:

troubleshoot
============

.. _troubleshooting_nss_and_jss_builds:

`Troubleshooting NSS and JSS Builds <#troubleshooting_nss_and_jss_builds>`__
----------------------------------------------------------------------------

.. container::

   Newsgroup: `mozilla.dev.tech.crypto <nntp://news.mozilla.org/mozilla.dev.tech.crypto>`__

   This page summarizes information on troubleshooting the NSS and JSS build and test systems,
   including known problems and configuration suggestions.

   If you have suggestions for this page, please post them to
   `mozilla.dev.tech.crypto <nntp://news.mozilla.org/mozilla.dev.tech.crypto>`__.

.. _building_nss:

`Building NSS <#building_nss>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  Having /usr/ucb/bin in the path before /usr/ccs/bin breaks the build on 64-bit Solaris.

   -  The Solaris compiler needs to be workshop-5.0 or greater.

   -  The 64-bit builds don't support gcc.

   -  If the build fails early on the gmakein coreconf try updating your cvs tree with -P:
      cd mozilla
      cvs update -P

   -  Building a 32-bit version on a 64-bit may fail with:

      .. code:: notranslate

         /usr/include/features.h:324:26: fatal error: bits/predefs.h: No such file or directory

      In this case remember to set USE_64=1

.. _testing_nss:

`Testing NSS <#testing_nss>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The SSL stress test opens 2,048 TCP connections in quick succession. Kernel data structures may
   remain allocated for these connections for up to two minutes. Some systems may not be configured
   to allow this many simultaneous connections by default; if the stress tests fail, try increasing
   the number of simultaneous sockets supported.

.. _building_jss:

`Building JSS <#building_jss>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  **Windows Only:** The shell invoked by gmake, ``shmsdos.exe``, is likely to crash when
      invoking some Java tools on Windows. The current workaround is to use some other shell in
      place of ``shmsdos``, such as ``sh.exe``, which should be distributed with the `Cygnus
      toolkit <http://sourceware.cygnus.com/cygwin/download.html>`__ you installed to build NSS. The
      change is unfortunately rather drastic: to trick gmake, you rename the shell program.

         cd c:/Programs/cygnus/bin *(or wherever your GNU tools are installed)*
         cp shmsdos.exe shmsdos.bak *(backup shmsdos)*
         cp sh.exe shmsdos.exe *(substitute alternative shell)*

      Making this change will probably break other builds you are  making on the same machine. You
      may need to switch the shell back and forthdepending on which product you are building. We
      will try to provide a moreconvenient solution in the future. If you have the MKS toolkit
      installed,  the <tt>sh.exe</tt> that comes with this toolkit can be used as well.