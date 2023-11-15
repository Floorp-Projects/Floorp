.. _mozilla_projects_nss_nss_3_50_release_notes:

NSS 3.50 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.50 on **7 February 2020**, which is a
   minor release.



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_50_RTM. NSS 3.50 requires NSPR 4.25 or newer.

   NSS 3.50 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_50_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _notable_changes_in_nss_3.50:

`Notable Changes in NSS 3.50 <#notable_changes_in_nss_3.50>`__
--------------------------------------------------------------

.. container::

   -  Verified primitives from HACL\* were updated, bringing performance improvements for several
      platforms.

      -  Note that Intel processors with SSE4 but without AVX are currently unable to use the
         improved ChaCha20/Poly1305 due to a build issue; such platforms will fall-back to less
         optimized algorithms. See `Bug 1609569 for
         details. <https://bugzilla.mozilla.org/show_bug.cgi?id=1609569>`__

   -  Updated DTLS 1.3 implementation to Draft-30. See `Bug 1599514 for
      details. <https://bugzilla.mozilla.org/show_bug.cgi?id=1599514>`__
   -  Added NIST SP800-108 KBKDF - PKCS#11 implementation. See `Bug 1599603 for
      details. <https://bugzilla.mozilla.org/show_bug.cgi?id=1599603>`__

.. _bugs_fixed_in_nss_3.50:

`Bugs fixed in NSS 3.50 <#bugs_fixed_in_nss_3.50>`__
----------------------------------------------------

.. container::

   -  `Bug 1599514 <https://bugzilla.mozilla.org/show_bug.cgi?id=1599514>`__ - Update DTLS 1.3
      implementation to Draft-30
   -  `Bug 1603438 <https://bugzilla.mozilla.org/show_bug.cgi?id=1603438>`__ - Fix native tools
      build failure due to lack of zlib include dir if external
   -  `Bug 1599603 <https://bugzilla.mozilla.org/show_bug.cgi?id=1599603>`__ - NIST SP800-108 KBKDF
      - PKCS#11 implementation
   -  `Bug 1606992 <https://bugzilla.mozilla.org/show_bug.cgi?id=1606992>`__ - Cache the most
      recent PBKDF1 password hash, to speed up repeated SDR operations, important with the increased
      KDF iteration counts. NSS 3.49.1 sped up PBKDF2 operations, though PBKDF1 operations are also
      relevant for older NSS databases (also included in NSS 3.49.2)
   -  `Bug 1608895 <https://bugzilla.mozilla.org/show_bug.cgi?id=1608895>`__ - Gyp builds on
      taskcluster broken by Setuptools v45.0.0 (for lacking Python3)
   -  `Bug 1574643 <https://bugzilla.mozilla.org/show_bug.cgi?id=1574643>`__ - Upgrade HACL\*
      verified implementations of ChaCha20, Poly1305, and 64-bit Curve25519
   -  `Bug 1608327 <https://bugzilla.mozilla.org/show_bug.cgi?id=1608327>`__ - Two problems with
      NEON-specific code in freebl
   -  `Bug 1575843 <https://bugzilla.mozilla.org/show_bug.cgi?id=1575843>`__ - Detect AArch64 CPU
      features on FreeBSD
   -  `Bug 1607099 <https://bugzilla.mozilla.org/show_bug.cgi?id=1607099>`__ - Remove the buildbot
      configuration
   -  `Bug 1585429 <https://bugzilla.mozilla.org/show_bug.cgi?id=1585429>`__ - Add more HKDF test
      vectors
   -  `Bug 1573911 <https://bugzilla.mozilla.org/show_bug.cgi?id=1573911>`__ - Add more RSA test
      vectors
   -  `Bug 1605314 <https://bugzilla.mozilla.org/show_bug.cgi?id=1605314>`__ - Compare all 8 bytes
      of an mp_digit when clamping in Windows assembly/mp_comba
   -  `Bug 1604596 <https://bugzilla.mozilla.org/show_bug.cgi?id=1604596>`__ - Update Wycheproof
      vectors and add support for CBC, P256-ECDH, and CMAC tests
   -  `Bug 1608493 <https://bugzilla.mozilla.org/show_bug.cgi?id=1608493>`__ - Use AES-NI for
      non-GCM AES ciphers on platforms with no assembly-optimized implementation, such as macOS.
   -  `Bug 1547639 <https://bugzilla.mozilla.org/show_bug.cgi?id=1547639>`__ - Update zlib in NSS to
      1.2.11
   -  `Bug 1609181 <https://bugzilla.mozilla.org/show_bug.cgi?id=1609181>`__ - Detect ARM (32-bit)
      CPU features on FreeBSD
   -  `Bug 1602386 <https://bugzilla.mozilla.org/show_bug.cgi?id=1602386>`__ - Fix build on
      FreeBSD/powerpc\*
   -  `Bug 1608151 <https://bugzilla.mozilla.org/show_bug.cgi?id=1608151>`__ - Introduce
      NSS_DISABLE_ALTIVEC
   -  `Bug 1612623 <https://bugzilla.mozilla.org/show_bug.cgi?id=1612623>`__ - Depend on NSPR 4.25
   -  `Bug 1609673 <https://bugzilla.mozilla.org/show_bug.cgi?id=1609673>`__ - Fix a crash when NSS
      is compiled without libnssdbm support, but the nssdbm shared object is available anyway.

   This Bugzilla query returns all the bugs fixed in NSS 3.50:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.50

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.50 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.50 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).