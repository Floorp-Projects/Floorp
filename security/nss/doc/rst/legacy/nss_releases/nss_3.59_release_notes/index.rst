.. _mozilla_projects_nss_nss_3_59_release_notes:

NSS 3.59 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.59 on **13 November 2020**, which is
   a minor release.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_59_RTM. NSS 3.59 requires NSPR 4.29 or newer.

   NSS 3.59 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_59_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _notable_changes_in_nss_3.59:

`Notable Changes in NSS 3.59 <#notable_changes_in_nss_3.59>`__
--------------------------------------------------------------

.. container::

   -  Exported two existing functions from libnss,  CERT_AddCertToListHeadWithData and
      CERT_AddCertToListTailWithData

.. _build_requirements:

`Build Requirements <#build_requirements>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  NSS will soon require GCC 4.8 or newer. Gyp-based builds will stop supporting older GCC
      versions in the next release, NSS 3.60 planned for December, followed later by the make-based
      builds. Users of older GCC versions can continue to use the make-based build system while they
      upgrade to newer versions of GCC.

.. _bugs_fixed_in_nss_3.59:

`Bugs fixed in NSS 3.59 <#bugs_fixed_in_nss_3.59>`__
----------------------------------------------------

.. container::

   -  `Bug 1607449 <https://bugzilla.mozilla.org/show_bug.cgi?id=1607449>`__ - Lock
      cert->nssCertificate to prevent a potential data race
   -  `Bug 1672823 <https://bugzilla.mozilla.org/show_bug.cgi?id=1672823>`__ - Add Wycheproof test
      cases for HMAC, HKDF, and DSA
   -  `Bug 1663661 <https://bugzilla.mozilla.org/show_bug.cgi?id=1663661>`__ - Guard against NULL
      token in nssSlot_IsTokenPresent
   -  `Bug 1670835 <https://bugzilla.mozilla.org/show_bug.cgi?id=1670835>`__ - Support enabling and
      disabling signatures via Crypto Policy
   -  `Bug 1672291 <https://bugzilla.mozilla.org/show_bug.cgi?id=1672291>`__ - Resolve libpkix OCSP
      failures on SHA1 self-signed root certs when SHA1 signatures are disabled.
   -  `Bug 1644209 <https://bugzilla.mozilla.org/show_bug.cgi?id=1644209>`__ - Fix broken
      SelectedCipherSuiteReplacer filter to solve some test intermittents
   -  `Bug 1672703 <https://bugzilla.mozilla.org/show_bug.cgi?id=1672703>`__ - Tolerate the first
      CCS in TLS 1.3  to fix a regression in our  CVE-2020-25648 fix that broke purple-discord
   -  `Bug 1666891 <https://bugzilla.mozilla.org/show_bug.cgi?id=1666891>`__ - Support key
      wrap/unwrap with RSA-OAEP
   -  `Bug 1667989 <https://bugzilla.mozilla.org/show_bug.cgi?id=1667989>`__ - Fix gyp linking on
      Solaris
   -  `Bug 1668123 <https://bugzilla.mozilla.org/show_bug.cgi?id=1668123>`__ - Export
      CERT_AddCertToListHeadWithData and CERT_AddCertToListTailWithData from libnss
   -  `Bug 1634584 <https://bugzilla.mozilla.org/show_bug.cgi?id=1634584>`__ - Set
      CKA_NSS_SERVER_DISTRUST_AFTER for Trustis FPS Root CA
   -  `Bug 1663091 <https://bugzilla.mozilla.org/show_bug.cgi?id=1663091>`__ - Remove unnecessary
      assertions in the streaming ASN.1 decoder that affected decoding certain PKCS8 private keys
      when using NSS debug builds
   -  `Bug 1670839 <https://bugzilla.mozilla.org/show_bug.cgi?id=1670839>`__ - Use ARM crypto
      extension for AES, SHA1 and SHA2 on MacOS.

   This Bugzilla query returns all the bugs fixed in NSS 3.59:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.59

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.59 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.59 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).