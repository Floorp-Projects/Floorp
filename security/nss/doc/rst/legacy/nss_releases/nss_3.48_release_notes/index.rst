.. _mozilla_projects_nss_nss_3_48_release_notes:

NSS 3.48 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.48 on **5 December 2019**, which is a
   minor release.

   The NSS team would like to recognize first-time contributors:

   -  Craig Disselkoen
   -  Giulio Benetti
   -  Lauri Kasanen
   -  Tom Prince



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_48_RTM. NSS 3.48 requires NSPR 4.24 or newer.

   NSS 3.48 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_48_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _notable_changes_in_nss_3.48:

`Notable Changes in NSS 3.48 <#notable_changes_in_nss_3.48>`__
--------------------------------------------------------------

.. container::

   -  `TLS 1.3 <https://datatracker.ietf.org/doc/html/rfc8446>`__ is the default maximum TLS
      version.  See `Bug 1573118 <https://bugzilla.mozilla.org/show_bug.cgi?id=1573118>`__ for
      details.
   -  `TLS extended master secret <https://datatracker.ietf.org/doc/html/rfc7627>`__ is enabled by
      default, where possible.  See `Bug
      1575411 <https://bugzilla.mozilla.org/show_bug.cgi?id=1575411>`__ for details.
   -  The master password PBE now uses 10,000 iterations by default when using the default sql
      (key4.db) storage. Because using an iteration count higher than 1 with the legacy dbm
      (key3.db) storage creates files that are incompatible with previous versions of NSS,
      applications that wish to enable it for key3.db are required to set environment variable
      NSS_ALLOW_LEGACY_DBM_ITERATION_COUNT=1. Applications may set environment variable
      NSS_MIN_MP_PBE_ITERATION_COUNT to request a higher iteration count than the library's default,
      or NSS_MAX_MP_PBE_ITERATION_COUNT to request a lower iteration count for test environments.
      See `Bug 1562671 <https://bugzilla.mozilla.org/show_bug.cgi?id=1562671>`__ for details.

.. _certificate_authority_changes:

`Certificate Authority Changes <#certificate_authority_changes>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  The following CA certificates were Added:

      -  `Bug 1591178 <https://bugzilla.mozilla.org/show_bug.cgi?id=1591178>`__ - Entrust Root
         Certification Authority - G4 Cert

         -  SHA-256 Fingerprint: DB3517D1F6732A2D5AB97C533EC70779EE3270A62FB4AC4238372460E6F01E88

.. _upcoming_changes_in_nss_3.49:

`Upcoming Changes in NSS 3.49 <#upcoming_changes_in_nss_3.49>`__
----------------------------------------------------------------

.. container::

   -  The legacy DBM database, **libnssdbm**, will no longer be built by default. See `Bug
      1594933 <https://bugzilla.mozilla.org/show_bug.cgi?id=1594933>`__ for details.

.. _bugs_fixed_in_nss_3.48:

`Bugs fixed in NSS 3.48 <#bugs_fixed_in_nss_3.48>`__
----------------------------------------------------

.. container::

   -  `Bug 1600775 <https://bugzilla.mozilla.org/show_bug.cgi?id=1600775>`__ - Require NSPR 4.24 for
      NSS 3.48
   -  `Bug 1593401 <https://bugzilla.mozilla.org/show_bug.cgi?id=1593401>`__ - Fix race condition in
      self-encrypt functions
   -  `Bug 1599545 <https://bugzilla.mozilla.org/show_bug.cgi?id=1599545>`__ - Fix assertion and add
      test for early Key Update
   -  `Bug 1597799 <https://bugzilla.mozilla.org/show_bug.cgi?id=1597799>`__ - Fix a crash in
      nssCKFWObject_GetAttributeSize
   -  `Bug 1591178 <https://bugzilla.mozilla.org/show_bug.cgi?id=1591178>`__ - Add Entrust Root
      Certification Authority - G4 certificate to NSS
   -  `Bug 1590001 <https://bugzilla.mozilla.org/show_bug.cgi?id=1590001>`__ - Prevent negotiation
      of versions lower than 1.3 after HelloRetryRequest
   -  `Bug 1596450 <https://bugzilla.mozilla.org/show_bug.cgi?id=1596450>`__ - Added a simplified
      and unified MAC implementation for HMAC and CMAC behind PKCS#11
   -  `Bug 1522203 <https://bugzilla.mozilla.org/show_bug.cgi?id=1522203>`__ - Remove an old Pentium
      Pro performance workaround
   -  `Bug 1592557 <https://bugzilla.mozilla.org/show_bug.cgi?id=1592557>`__ - Fix PRNG
      known-answer-test scripts
   -  `Bug 1586176 <https://bugzilla.mozilla.org/show_bug.cgi?id=1586176>`__ - EncryptUpdate should
      use maxout not block size (CVE-2019-11745)
   -  `Bug 1593141 <https://bugzilla.mozilla.org/show_bug.cgi?id=1593141>`__ - add \`notBefore\` or
      similar "beginning-of-validity-period" parameter to
      mozilla::pkix::TrustDomain::CheckRevocation
   -  `Bug 1591363 <https://bugzilla.mozilla.org/show_bug.cgi?id=1591363>`__ - Fix a PBKDF2 memory
      leak in NSC_GenerateKey if key length > MAX_KEY_LEN (256)
   -  `Bug 1592869 <https://bugzilla.mozilla.org/show_bug.cgi?id=1592869>`__ - Use ARM NEON for
      ctr_xor
   -  `Bug 1566131 <https://bugzilla.mozilla.org/show_bug.cgi?id=1566131>`__ - Ensure SHA-1 fallback
      disabled in TLS 1.2
   -  `Bug 1577803 <https://bugzilla.mozilla.org/show_bug.cgi?id=1577803>`__ - Mark PKCS#11 token as
      friendly if it implements CKP_PUBLIC_CERTIFICATES_TOKEN
   -  `Bug 1566126 <https://bugzilla.mozilla.org/show_bug.cgi?id=1566126>`__ - POWER GHASH Vector
      Acceleration
   -  `Bug 1589073 <https://bugzilla.mozilla.org/show_bug.cgi?id=1589073>`__ - Use of new
      PR_ASSERT_ARG in certdb.c
   -  `Bug 1590495 <https://bugzilla.mozilla.org/show_bug.cgi?id=1590495>`__ - Fix a crash in
      PK11_MakeCertFromHandle
   -  `Bug 1591742 <https://bugzilla.mozilla.org/show_bug.cgi?id=1591742>`__ - Ensure DES IV length
      is valid before usage from PKCS#11
   -  `Bug 1588567 <https://bugzilla.mozilla.org/show_bug.cgi?id=1588567>`__ - Enable mozilla::pkix
      gtests in NSS CI
   -  `Bug 1591315 <https://bugzilla.mozilla.org/show_bug.cgi?id=1591315>`__ - Update NSC_Decrypt
      length in constant time
   -  `Bug 1562671 <https://bugzilla.mozilla.org/show_bug.cgi?id=1562671>`__ - Increase NSS MP KDF
      default iteration count, by default for modern key4 storage, optionally for legacy key3.db
      storage
   -  `Bug 1590972 <https://bugzilla.mozilla.org/show_bug.cgi?id=1590972>`__ - Use -std=c99 rather
      than -std=gnu99
   -  `Bug 1590676 <https://bugzilla.mozilla.org/show_bug.cgi?id=1590676>`__ - Fix build if ARM
      doesn't support NEON
   -  `Bug 1575411 <https://bugzilla.mozilla.org/show_bug.cgi?id=1575411>`__ - Enable TLS extended
      master secret by default
   -  `Bug 1590970 <https://bugzilla.mozilla.org/show_bug.cgi?id=1590970>`__ - SSL_SetTimeFunc has
      incomplete coverage
   -  `Bug 1590678 <https://bugzilla.mozilla.org/show_bug.cgi?id=1590678>`__ - Remove
      -Wmaybe-uninitialized warning in tls13esni.c
   -  `Bug 1588244 <https://bugzilla.mozilla.org/show_bug.cgi?id=1588244>`__ - NSS changes for
      Delegated Credential key strength checks
   -  `Bug 1459141 <https://bugzilla.mozilla.org/show_bug.cgi?id=1459141>`__ - Add more CBC padding
      tests that missed NSS 3.47
   -  `Bug 1590339 <https://bugzilla.mozilla.org/show_bug.cgi?id=1590339>`__ - Fix a memory leak in
      btoa.c
   -  `Bug 1589810 <https://bugzilla.mozilla.org/show_bug.cgi?id=1589810>`__ - fix uninitialized
      variable warnings from certdata.perl
   -  `Bug 1573118 <https://bugzilla.mozilla.org/show_bug.cgi?id=1573118>`__ - Enable TLS 1.3 by
      default in NSS

   This Bugzilla query returns all the bugs fixed in NSS 3.48:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.48

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.48 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.48 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).