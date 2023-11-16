.. _mozilla_projects_nss_nss_3_12_2_release_notes_html:

NSS_3.12.2_release_notes.html
=============================

.. _nss_3.12.2_release_notes:

`NSS 3.12.2 Release Notes <#nss_3.12.2_release_notes>`__
--------------------------------------------------------

.. container::

.. _2008-10-20:

`2008-10-20 <#2008-10-20>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Newsgroup: `mozilla.dev.tech.crypto <news://news.mozilla.org/mozilla.dev.tech.crypto>`__

`Contents <#contents>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `Introduction <#introduction>`__
   -  `Distribution Information <#distribution_information>`__
   -  `New in NSS 3.12.2 <#new_in_nss_3.12.2>`__
   -  `Bugs Fixed <#bugs_fixed>`__
   -  `Documentation <#documentation>`__
   -  `Compatibility <#compatibility>`__
   -  `Feedback <#feedback>`__

   --------------

`Introduction <#introduction>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Network Security Services (NSS) 3.12.2 is a patch release for NSS 3.12. The bug fixes in NSS
   3.12.2 are described in the "`Bugs Fixed <#bugs_fixed>`__" section below.
   NSS 3.12.2 is tri-licensed under the MPL 1.1/GPL 2.0/LGPL 2.1.

   --------------



`Distribution Information <#distribution_information>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The CVS tag for the NSS 3.12.2 release is NSS_3_12_2_RTM. NSS 3.12.2 requires `NSPR
   4.7.1 <https://www.mozilla.org/projects/nspr/release-notes/nspr471.html>`__.
   See the `Documentation <#documentation>`__ section for the build instructions.
   NSS 3.12.2 source and binary distributions are also available on ftp.mozilla.org for secure HTTPS
   download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_12_2_RTM/src/.
   -  Binary distributions:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_12_2_RTM/. Both debug and
      optimized builds are provided. Go to the subdirectory for your platform, DBG (debug) or OPT
      (optimized), to get the tar.gz or zip file. The tar.gz or zip file expands to an nss-3.12.2
      directory containing three subdirectories:

      -  include - NSS header files
      -  lib - NSS shared libraries
      -  bin< - `NSS Tools <https://www.mozilla.org/projects/security/pki/nss/tools/>`__ and test
         programs

   You also need to download the NSPR 4.7.1 binary distributions to get the NSPR 4.7.1 header files
   and shared libraries, which NSS 3.12.2 requires. NSPR 4.7.1 binary distributions are in
   https://ftp.mozilla.org/pub/mozilla.org/nspr/releases/v4.7.1/.

   --------------

.. _new_in_nss_3.12.2:

`New in NSS 3.12.2 <#new_in_nss_3.12.2>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  New functions in the nss shared library:

      -  SEC_PKCS12AddCertOrChainAndKey (see p12.h)

   -  New PKCS11 errors (see secerr.h)

      -  SEC_ERROR_PKCS11_GENERAL_ERROR
      -  SEC_ERROR_PKCS11_FUNCTION_FAILED
      -  SEC_ERROR_PKCS11_DEVICE_ERROR

   --------------

.. _bugs_fixed:

`Bugs Fixed <#bugs_fixed>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The following bugs have been fixed in NSS 3.12.2.

   -  `Bug 200704 <https://bugzilla.mozilla.org/show_bug.cgi?id=200704>`__: PKCS11: invalid session
      handle 0
   -  `Bug 205434 <https://bugzilla.mozilla.org/show_bug.cgi?id=205434>`__: Fully implement new
      libPKIX cert verification API from bug 294531
   -  `Bug 302670 <https://bugzilla.mozilla.org/show_bug.cgi?id=302670>`__: Use the installed
      libz.so where available
   -  `Bug 305693 <https://bugzilla.mozilla.org/show_bug.cgi?id=305693>`__: shlibsign generates PQG
      for every run
   -  `Bug 311483 <https://bugzilla.mozilla.org/show_bug.cgi?id=311483>`__: exposing
      includeCertChain as a parameter to SEC_PKCS12AddCertAndKey
   -  `Bug 390527 <https://bugzilla.mozilla.org/show_bug.cgi?id=390527>`__: get rid of pkixErrorMsg
      variable in PKIX_Error
   -  `Bug 391560 <https://bugzilla.mozilla.org/show_bug.cgi?id=391560>`__: libpkix does not
      consistently return PKIX_ValidateNode tree that truly represent failure reasons
   -  `Bug 408260 <https://bugzilla.mozilla.org/show_bug.cgi?id=408260>`__: certutil usage doesn't
      give enough information about trust arguments
   -  `Bug 412311 <https://bugzilla.mozilla.org/show_bug.cgi?id=412311>`__: Replace
      PR_INTERVAL_NO_WAIT with PR_INTERVAL_NO_TIMEOUT in client initialization calls
   -  `Bug 423839 <https://bugzilla.mozilla.org/show_bug.cgi?id=423839>`__: Add multiple PKCS#11
      token password command line option to NSS tools.
   -  `Bug 432260 <https://bugzilla.mozilla.org/show_bug.cgi?id=432260>`__: [[@
      pkix_pl_HttpDefaultClient_HdrCheckComplete - PKIX_PL_Memcpy] crashes when there is no
      content-length header in the http response
   -  `Bug 436599 <https://bugzilla.mozilla.org/show_bug.cgi?id=436599>`__: PKIX: AIA extension is
      not used in some Bridge CA / known certs configuration
   -  `Bug 437804 <https://bugzilla.mozilla.org/show_bug.cgi?id=437804>`__: certutil -R for cert
      renewal should derive the subject from the cert if none is specified.
   -  `Bug 444974 <https://bugzilla.mozilla.org/show_bug.cgi?id=444974>`__: Crash upon reinsertion
      of E-Identity smartcard
   -  `Bug 447563 <https://bugzilla.mozilla.org/show_bug.cgi?id=447563>`__: modutil -add prints no
      error explanation on failure
   -  `Bug 448431 <https://bugzilla.mozilla.org/show_bug.cgi?id=448431>`__: PK11_CreateMergeLog()
      declaration causes gcc warning when compiling with -Wstrict-prototypes
   -  `Bug 449334 <https://bugzilla.mozilla.org/show_bug.cgi?id=449334>`__: pk12util has duplicate
      options letters
   -  `Bug 449725 <https://bugzilla.mozilla.org/show_bug.cgi?id=449725>`__: signver is still using
      static libraries.
   -  `Bug 450427 <https://bugzilla.mozilla.org/show_bug.cgi?id=450427>`__: Add COMODO ECC
      Certification Authority certificate to NSS
   -  `Bug 450536 <https://bugzilla.mozilla.org/show_bug.cgi?id=450536>`__: Remove obsolete XP_MAC
      code
   -  `Bug 451024 <https://bugzilla.mozilla.org/show_bug.cgi?id=451024>`__: certutil.exe crashes
      with Segmentation fault inside PR_Cleanup
   -  `Bug 451927 <https://bugzilla.mozilla.org/show_bug.cgi?id=451927>`__:
      security/coreconf/WINNT6.0.mk has invalid defines
   -  `Bug 452751 <https://bugzilla.mozilla.org/show_bug.cgi?id=452751>`__: Slot leak in
      PK11_FindSlotsByNames
   -  `Bug 452865 <https://bugzilla.mozilla.org/show_bug.cgi?id=452865>`__: Remove obsolete linker
      flags needed when libnss3 was linked with libsoftokn3
   -  `Bug 454961 <https://bugzilla.mozilla.org/show_bug.cgi?id=454961>`__: Fix the implementation
      and use of pr_fgets in signtool
   -  `Bug 455348 <https://bugzilla.mozilla.org/show_bug.cgi?id=455348>`__: Change hyphens to
      underscores in DEBUG_$(shell whoami).
   -  `Bug 455424 <https://bugzilla.mozilla.org/show_bug.cgi?id=455424>`__: nssilckt.h defines the
      enumeration constant 'Lock'
   -  `Bug 456036 <https://bugzilla.mozilla.org/show_bug.cgi?id=456036>`__: Stubs for deprecated
      functions in lib/certdb/stanpcertdb.c should set the PR_NOT_IMPLEMENTED_ERROR error.
   -  `Bug 456854 <https://bugzilla.mozilla.org/show_bug.cgi?id=456854>`__: CERT_DecodeCertPackage
      does not set NSPR error code upon error
   -  `Bug 457980 <https://bugzilla.mozilla.org/show_bug.cgi?id=457980>`__: hundreds of kilobytes of
      useless strings in libPKIX
   -  `Bug 457984 <https://bugzilla.mozilla.org/show_bug.cgi?id=457984>`__: Enable PKCS11 module
      logging in optimized builds
   -  `Bug 458905 <https://bugzilla.mozilla.org/show_bug.cgi?id=458905>`__: Memory leaks in PKIX
      bridge certificates.
   -  `Bug 459231 <https://bugzilla.mozilla.org/show_bug.cgi?id=459231>`__: Memory leak in cert
      fetching - AIA extension.
   -  `Bug 459248 <https://bugzilla.mozilla.org/show_bug.cgi?id=459248>`__: Support Intel AES
      extensions.
   -  `Bug 459359 <https://bugzilla.mozilla.org/show_bug.cgi?id=459359>`__: ForwardBuilderState
      object is leaked when AIA path incorrect
   -  `Bug 459481 <https://bugzilla.mozilla.org/show_bug.cgi?id=459481>`__: NSS build problem with
      GCC 3.4.6 on OS/2

   --------------

`Documentation <#documentation>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   For a list of the primary NSS documentation pages on mozilla.org, see `NSS
   Documentation <../index.html#Documentation>`__. New and revised documents available since the
   release of NSS 3.11 include the following:

   -  `Build Instructions for NSS 3.11.4 and above <../nss-3.11.4/nss-3.11.4-build.html>`__
   -  `NSS Shared DB <http://wiki.mozilla.org/NSS_Shared_DB>`__

   --------------

`Compatibility <#compatibility>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   NSS 3.12.2 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.12.2 shared libraries
   without recompiling or relinking.  Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in `NSS Public Functions <../ref/nssfunctions.html>`__ will remain
   compatible with future versions of the NSS shared libraries.

   --------------

`Feedback <#feedback>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Bugs discovered should be reported by filing a bug report with `mozilla.org
   Bugzilla <https://bugzilla.mozilla.org/>`__ (product NSS).