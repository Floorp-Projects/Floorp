.. _mozilla_projects_nss_nss_3_47_release_notes:

NSS 3.47 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.47 on **18 October 2019**, which is a
   minor release.

   The NSS team would like to recognize first-time contributors:

   -  Christian Weisgerber
   -  Deian Stefan
   -  Jenine

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_47_RTM. NSS 3.47 requires NSPR 4.23 or newer.

   NSS 3.47 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_47_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _upcoming_changes_to_default_tls_configuration:

`Upcoming changes to default TLS configuration <#upcoming_changes_to_default_tls_configuration>`__
--------------------------------------------------------------------------------------------------

.. container::

   The next NSS team plans to make two changes to the default TLS configuration in NSS 3.48, which
   will be released in early December:

   -  `TLS 1.3 <https://datatracker.ietf.org/doc/html/rfc8446>`__ will be the default maximum TLS
      version.  See `Bug 1573118 <https://bugzilla.mozilla.org/show_bug.cgi?id=1573118>`__ for
      details.
   -  `TLS extended master secret <https://datatracker.ietf.org/doc/html/rfc7627>`__ will be enabled
      by default, where possible.  See `Bug
      1575411 <https://bugzilla.mozilla.org/show_bug.cgi?id=1575411>`__ for details.

.. _notable_changes_in_nss_3.47:

`Notable Changes in NSS 3.47 <#notable_changes_in_nss_3.47>`__
--------------------------------------------------------------

.. container::

   -  `Bug 1152625 <https://bugzilla.mozilla.org/show_bug.cgi?id=1152625>`__ - Support AES HW
      acceleration on ARMv8
   -  `Bug 1267894 <https://bugzilla.mozilla.org/show_bug.cgi?id=1267894>`__ - Allow per-socket
      run-time ordering of the cipher suites presented in ClientHello
   -  `Bug 1570501 <https://bugzilla.mozilla.org/show_bug.cgi?id=1570501>`__ - Add CMAC to FreeBL
      and PKCS #11 libraries

.. _bugs_fixed_in_nss_3.47:

`Bugs fixed in NSS 3.47 <#bugs_fixed_in_nss_3.47>`__
----------------------------------------------------

.. container::

   -  `Bug 1459141 <https://bugzilla.mozilla.org/show_bug.cgi?id=1459141>`__ - Make softoken CBC
      padding removal constant time
   -  `Bug 1589120 <https://bugzilla.mozilla.org/show_bug.cgi?id=1589120>`__ - More CBC padding
      tests
   -  `Bug 1465613 <https://bugzilla.mozilla.org/show_bug.cgi?id=1465613>`__ - Add ability to
      distrust certificates issued after a certain date for a specified root cert
   -  `Bug 1588557 <https://bugzilla.mozilla.org/show_bug.cgi?id=1588557>`__ - Bad debug statement
      in tls13con.c
   -  `Bug 1579060 <https://bugzilla.mozilla.org/show_bug.cgi?id=1579060>`__ - mozilla::pkix tag
      definitions for issuerUniqueID and subjectUniqueID shouldn't have the CONSTRUCTED bit set
   -  `Bug 1583068 <https://bugzilla.mozilla.org/show_bug.cgi?id=1583068>`__ - NSS 3.47 should pick
      up fix from bug 1575821 (NSPR 4.23)
   -  `Bug 1152625 <https://bugzilla.mozilla.org/show_bug.cgi?id=1152625>`__ - Support AES HW
      acceleration on ARMv8
   -  `Bug 1549225 <https://bugzilla.mozilla.org/show_bug.cgi?id=1549225>`__ - Disable DSA signature
      schemes for TLS 1.3
   -  `Bug 1586947 <https://bugzilla.mozilla.org/show_bug.cgi?id=1586947>`__ -
      PK11_ImportAndReturnPrivateKey does not store nickname for EC keys
   -  `Bug 1586456 <https://bugzilla.mozilla.org/show_bug.cgi?id=1586456>`__ - Unnecessary
      conditional in pki3hack, pk11load and stanpcertdb
   -  `Bug 1576307 <https://bugzilla.mozilla.org/show_bug.cgi?id=1576307>`__ - Check mechanism param
      and param length before casting to mechanism-specific structs
   -  `Bug 1577953 <https://bugzilla.mozilla.org/show_bug.cgi?id=1577953>`__ - Support longer (up to
      RFC maximum) HKDF outputs
   -  `Bug 1508776 <https://bugzilla.mozilla.org/show_bug.cgi?id=1508776>`__ - Remove refcounting
      from sftk_FreeSession (CVE-2019-11756)
   -  `Bug 1494063 <https://bugzilla.mozilla.org/show_bug.cgi?id=1494063>`__ - Support TLS Exporter
      in tstclnt and selfserv
   -  `Bug 1581024 <https://bugzilla.mozilla.org/show_bug.cgi?id=1581024>`__ - Heap overflow in NSS
      utility "derdump"
   -  `Bug 1582343 <https://bugzilla.mozilla.org/show_bug.cgi?id=1582343>`__ - Soft token MAC
      verification not constant time
   -  `Bug 1578238 <https://bugzilla.mozilla.org/show_bug.cgi?id=1578238>`__ - Handle invald tag
      sizes for CKM_AES_GCM
   -  `Bug 1576295 <https://bugzilla.mozilla.org/show_bug.cgi?id=1576295>`__ - Check all bounds when
      encrypting with SEED_CBC
   -  `Bug 1580286 <https://bugzilla.mozilla.org/show_bug.cgi?id=1580286>`__ - NSS rejects TLS 1.2
      records with large padding with SHA384 HMAC
   -  `Bug 1577448 <https://bugzilla.mozilla.org/show_bug.cgi?id=1577448>`__ - Create additional
      nested S/MIME test messages for Thunderbird
   -  `Bug 1399095 <https://bugzilla.mozilla.org/show_bug.cgi?id=1399095>`__ - Allow nss-try to be
      used to test NSPR changes
   -  `Bug 1267894 <https://bugzilla.mozilla.org/show_bug.cgi?id=1267894>`__ - libSSL should allow
      selecting the order of cipher suites in ClientHello
   -  `Bug 1581507 <https://bugzilla.mozilla.org/show_bug.cgi?id=1581507>`__ - Fix unportable grep
      expression in test scripts
   -  `Bug 1234830 <https://bugzilla.mozilla.org/show_bug.cgi?id=1234830>`__ - [CID 1242894][CID
      1242852] unused values
   -  `Bug 1580126 <https://bugzilla.mozilla.org/show_bug.cgi?id=1580126>`__ - Fix build failure on
      aarch64_be while building freebl/gcm
   -  `Bug 1385039 <https://bugzilla.mozilla.org/show_bug.cgi?id=1385039>`__ - Build NSPR tests as
      part of NSS continuous integration
   -  `Bug 1581391 <https://bugzilla.mozilla.org/show_bug.cgi?id=1581391>`__ - Fix build on
      OpenBSD/arm64 after bug #1559012
   -  `Bug 1581041 <https://bugzilla.mozilla.org/show_bug.cgi?id=1581041>`__ - mach-commands ->
      mach-completion
   -  `Bug 1558313 <https://bugzilla.mozilla.org/show_bug.cgi?id=1558313>`__ - Code bugs found by
      clang scanners.
   -  `Bug 1542207 <https://bugzilla.mozilla.org/show_bug.cgi?id=1542207>`__ - Limit policy check on
      signature algorithms to known algorithms
   -  `Bug 1560329 <https://bugzilla.mozilla.org/show_bug.cgi?id=1560329>`__ - drbg: add continuous
      self-test on entropy source
   -  `Bug 1579290 <https://bugzilla.mozilla.org/show_bug.cgi?id=1579290>`__ - ASAN builds should
      disable LSAN while building
   -  `Bug 1385061 <https://bugzilla.mozilla.org/show_bug.cgi?id=1385061>`__ - Build NSPR tests with
      NSS make; Add gyp parameters to build/run NSPR tests
   -  `Bug 1577359 <https://bugzilla.mozilla.org/show_bug.cgi?id=1577359>`__ - Build atob and btoa
      for Thunderbird
   -  `Bug 1579036 <https://bugzilla.mozilla.org/show_bug.cgi?id=1579036>`__ - Confusing error when
      trying to export non-existent cert with pk12util
   -  `Bug 1578626 <https://bugzilla.mozilla.org/show_bug.cgi?id=1578626>`__ - [CID 1453375] UB:
      decrement nullptr.
   -  `Bug 1578751 <https://bugzilla.mozilla.org/show_bug.cgi?id=1578751>`__ - Ensure a consistent
      style for pk11_find_certs_unittest.cc
   -  `Bug 1570501 <https://bugzilla.mozilla.org/show_bug.cgi?id=1570501>`__ - Add CMAC to FreeBL
      and PKCS #11 libraries
   -  `Bug 657379 <https://bugzilla.mozilla.org/show_bug.cgi?id=657379>`__ - NSS uses the wrong OID
      for signatureAlgorithm field of signerInfo in CMS for DSA and ECDSA
   -  `Bug 1576664 <https://bugzilla.mozilla.org/show_bug.cgi?id=1576664>`__ - Remove -mms-bitfields
      from mingw NSS build.
   -  `Bug 1577038 <https://bugzilla.mozilla.org/show_bug.cgi?id=1577038>`__ - add
      PK11_GetCertsFromPrivateKey to return all certificates with public keys matching a particular
      private key

   This Bugzilla query returns all the bugs fixed in NSS 3.47:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.47

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.47 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.47 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).