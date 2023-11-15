.. _mozilla_projects_nss_nss_3_52_release_notes:

NSS 3.52 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.52 on **1 May 2020**.

   The NSS team would like to recognize first-time contributors:

   -  zhujianwei7
   -  Hans Petter Jansson



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_52_RTM. NSS 3.52 requires NSPR 4.25 or newer.

   NSS 3.52 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_52_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _notable_changes_in_nss_3.52:

`Notable Changes in NSS 3.52 <#notable_changes_in_nss_3.52>`__
--------------------------------------------------------------

.. container::

   -  `Bug 1603628 <https://bugzilla.mozilla.org/show_bug.cgi?id=1603628>`__ - Update NSS to support
      PKCS #11 v3.0.

      -  Note: This change modifies the CK_GCM_PARAMS struct to include the ulIvBits field which,
         prior to PKCS #11 v3.0, was ambiguously defined and not included in the NSS definition. If
         an application is recompiled with NSS 3.52+, this field must be initialized to a value
         corresponding to ulIvLen. Alternatively, defining NSS_PKCS11_2_0_COMPAT will yield the old
         definition. See the bug for more information.

   -  `Bug 1623374 <https://bugzilla.mozilla.org/show_bug.cgi?id=1623374>`__ - Support new PKCS #11
      v3.0 Message Interface for AES-GCM and ChaChaPoly.
   -  `Bug 1612493 <https://bugzilla.mozilla.org/show_bug.cgi?id=1612493>`__ - Integrate AVX2
      ChaCha20, Poly1305, and ChaCha20Poly1305 from HACL*.

.. _bugs_fixed_in_nss_3.52:

`Bugs fixed in NSS 3.52 <#bugs_fixed_in_nss_3.52>`__
----------------------------------------------------

.. container::

   -  `Bug 1633498 <https://bugzilla.mozilla.org/show_bug.cgi?id=1633498>`__ - Fix unused variable
      'getauxval' error on iOS compilation.
   -  `Bug 1630721 <https://bugzilla.mozilla.org/show_bug.cgi?id=1630721>`__ - Add Softoken
      functions for FIPS.
   -  `Bug 1630458 <https://bugzilla.mozilla.org/show_bug.cgi?id=1630458>`__ - Fix problem of GYP
      MSVC builds not producing debug symbol files.
   -  `Bug 1629663 <https://bugzilla.mozilla.org/show_bug.cgi?id=1629663>`__ - Add IKEv1 Quick Mode
      KDF.
   -  `Bug 1629661 <https://bugzilla.mozilla.org/show_bug.cgi?id=1629661>`__ - MPConfig calls in SSL
      initialize policy before NSS is initialized.
   -  `Bug 1629655 <https://bugzilla.mozilla.org/show_bug.cgi?id=1629655>`__ - Support temporary
      session objects in ckfw.
   -  `Bug 1629105 <https://bugzilla.mozilla.org/show_bug.cgi?id=1629105>`__ - Add PKCS11 v3.0
      functions to module debug logger.
   -  `Bug 1626751 <https://bugzilla.mozilla.org/show_bug.cgi?id=1626751>`__ - Fix error in
      generation of fuzz32 docker image after updates.
   -  `Bug 1625133 <https://bugzilla.mozilla.org/show_bug.cgi?id=1625133>`__ - Fix implicit
      declaration of function 'getopt' error.
   -  `Bug 1624864 <https://bugzilla.mozilla.org/show_bug.cgi?id=1624864>`__ - Allow building of
      gcm-arm32-neon on non-armv7 architectures.
   -  `Bug 1624402 <https://bugzilla.mozilla.org/show_bug.cgi?id=1624402>`__ - Fix compilation error
      in Firefox Android.
   -  `Bug 1624130 <https://bugzilla.mozilla.org/show_bug.cgi?id=1624130>`__ - Require
      CK_FUNCTION_LIST structs to be packed.
   -  `Bug 1624377 <https://bugzilla.mozilla.org/show_bug.cgi?id=1624377>`__ - Fix clang warning for
      unknown argument '-msse4'.
   -  `Bug 1623374 <https://bugzilla.mozilla.org/show_bug.cgi?id=1623374>`__ - Support new PKCS #11
      v3.0 Message Interface for AES-GCM and ChaChaPoly.
   -  `Bug 1623184 <https://bugzilla.mozilla.org/show_bug.cgi?id=1623184>`__ - Fix freebl_cpuid for
      querying Extended Features.
   -  `Bug 1622555 <https://bugzilla.mozilla.org/show_bug.cgi?id=1622555>`__ - Fix argument parsing
      in lowhashtest.
   -  `Bug 1620799 <https://bugzilla.mozilla.org/show_bug.cgi?id=1620799>`__ - Introduce
      NSS_DISABLE_GCM_ARM32_NEON to build on arm32 without NEON support.
   -  `Bug 1619102 <https://bugzilla.mozilla.org/show_bug.cgi?id=1619102>`__ - Add workaround option
      to include both DTLS and TLS versions in DTLS supported_versions.
   -  `Bug 1619056 <https://bugzilla.mozilla.org/show_bug.cgi?id=1619056>`__ - Update README: TLS
      1.3 is not experimental anymore.
   -  `Bug 1618915 <https://bugzilla.mozilla.org/show_bug.cgi?id=1618915>`__ - Fix UBSAN issue in
      ssl_ParseSessionTicket.
   -  `Bug 1618739 <https://bugzilla.mozilla.org/show_bug.cgi?id=1618739>`__ - Don't assert fuzzer
      behavior in SSL_ParseSessionTicket.
   -  `Bug 1617968 <https://bugzilla.mozilla.org/show_bug.cgi?id=1617968>`__ - Update Delegated
      Credentials implementation to draft-07.
   -  `Bug 1617533 <https://bugzilla.mozilla.org/show_bug.cgi?id=1617533>`__ - Update HACL\*
      dependencies for libintvector.h
   -  `Bug 1613238 <https://bugzilla.mozilla.org/show_bug.cgi?id=1613238>`__ - Add vector
      accelerated SHA2 for POWER 8+.
   -  `Bug 1612493 <https://bugzilla.mozilla.org/show_bug.cgi?id=1612493>`__ - Integrate AVX2
      ChaCha20, Poly1305, and ChaCha20Poly1305 from HACL*.
   -  `Bug 1612281 <https://bugzilla.mozilla.org/show_bug.cgi?id=1612281>`__ - Maintain PKCS11
      C_GetAttributeValue semantics on attributes that lack NSS database columns.
   -  `Bug 1612260 <https://bugzilla.mozilla.org/show_bug.cgi?id=1612260>`__ - Add Wycheproof RSA
      test vectors.
   -  `Bug 1608250 <https://bugzilla.mozilla.org/show_bug.cgi?id=1608250>`__ - broken fipstest
      handling of KI_len.
   -  `Bug 1608245 <https://bugzilla.mozilla.org/show_bug.cgi?id=1608245>`__ - Consistently handle
      NULL slot/session.
   -  `Bug 1603801 <https://bugzilla.mozilla.org/show_bug.cgi?id=1603801>`__ - Avoid dcache
      pollution from sdb_measureAccess().
   -  `Bug 1603628 <https://bugzilla.mozilla.org/show_bug.cgi?id=1603628>`__ - Update NSS to support
      PKCS #11 v3.0.
   -  `Bug 1561637 <https://bugzilla.mozilla.org/show_bug.cgi?id=1561637>`__ - TLS 1.3 does not work
      in FIPS mode.
   -  `Bug 1531906 <https://bugzilla.mozilla.org/show_bug.cgi?id=1531906>`__ - Fix overzealous
      assertion when evicting a cached sessionID or using external cache.
   -  `Bug 1465613 <https://bugzilla.mozilla.org/show_bug.cgi?id=1465613>`__ - Fix issue where
      testlib makefile build produced extraneous object files.
   -  `Bug 1619959 <https://bugzilla.mozilla.org/show_bug.cgi?id=1619959>`__ - Properly handle
      multi-block SEED ECB inputs.
   -  `Bug 1630925 <https://bugzilla.mozilla.org/show_bug.cgi?id=1630925>`__ - Guard all instances
      of NSSCMSSignedData.signerInfo to avoid a CMS crash
   -  `Bug 1571677 <https://bugzilla.mozilla.org/show_bug.cgi?id=1571677>`__ - Name Constraints
      validation: CN treated as DNS name even when syntactically invalid as DNS name

   This Bugzilla query returns all the bugs fixed in NSS 3.52:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.52

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.52 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.52 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).