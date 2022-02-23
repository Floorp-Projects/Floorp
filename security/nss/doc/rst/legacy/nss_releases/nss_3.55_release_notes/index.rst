.. _mozilla_projects_nss_nss_3_55_release_notes:

NSS 3.55 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.55 on **24 July 2020**, which is a
   minor release.

   The NSS team would like to recognize first-time contributors:

   -  Danh

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_55_RTM. NSS 3.55 requires NSPR 4.27 or newer.

   NSS 3.55 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_55_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _notable_changes_in_nss_3.55:

`Notable Changes in NSS 3.55 <#notable_changes_in_nss_3.55>`__
--------------------------------------------------------------

.. container::

   -  P384 and P521 elliptic curve implementations are replaced with verifiable implementations from
      `Fiat-Crypto <https://github.com/mit-plv/fiat-crypto>`__ and
      `ECCKiila <https://gitlab.com/nisec/ecckiila/>`__. Special thanks to the Network and
      Information Security Group (NISEC) at Tampere University.
   -  PK11_FindCertInSlot is added. With this function, a given slot can be queried with a
      DER-Encoded certificate, providing performance and usability improvements over other
      mechanisms. See `Bug 1649633 <https://bugzilla.mozilla.org/show_bug.cgi?id=1649633>`__ for
      more details.
   -  DTLS 1.3 implementation is updated to draft-38. See `Bug
      1647752 <https://bugzilla.mozilla.org/show_bug.cgi?id=1647752>`__ for details.
   -  NSPR dependency updated to 4.27.

.. _known_issues:

`Known Issues <#known_issues>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  On some platforms, using the Makefile builds fails to locate seccomon.h; ensure you are using
      make all rather than just make. Another potential workaround is to use the gyp-based build.sh
      script. If this affects you, please help us narrow down the cause in `Bug
      1653975. <https://bugzilla.mozilla.org/show_bug.cgi?id=1653975>`__

.. _bugs_fixed_in_nss_3.55:

`Bugs fixed in NSS 3.55 <#bugs_fixed_in_nss_3.55>`__
----------------------------------------------------

.. container::

   -  `Bug 1631583 <https://bugzilla.mozilla.org/show_bug.cgi?id=1631583>`__ (CVE-2020-6829,
      CVE-2020-12400)  - Replace P384 and P521 with new, verifiable implementations from
      `Fiat-Crypto <https://github.com/mit-plv/fiat-crypto>`__ and
      `ECCKiila <https://gitlab.com/nisec/ecckiila/>`__.
   -  `Bug 1649487 <https://bugzilla.mozilla.org/show_bug.cgi?id=1649487>`__ - Move overzealous
      assertion in VFY_EndWithSignature.
   -  `Bug 1631573 <https://bugzilla.mozilla.org/show_bug.cgi?id=1631573>`__ (CVE-2020-12401) -
      Remove unnecessary scalar padding.
   -  `Bug 1636771 <https://bugzilla.mozilla.org/show_bug.cgi?id=1636771>`__ (CVE-2020-12403) -
      Explicitly disable multi-part ChaCha20 (which was not functioning correctly) and more strictly
      enforce tag length.
   -  `Bug 1649648 <https://bugzilla.mozilla.org/show_bug.cgi?id=1649648>`__ - Don't memcpy zero
      bytes (sanitizer fix).
   -  `Bug 1649316 <https://bugzilla.mozilla.org/show_bug.cgi?id=1649316>`__ - Don't memcpy zero
      bytes (sanitizer fix).
   -  `Bug 1649322 <https://bugzilla.mozilla.org/show_bug.cgi?id=1649322>`__ - Don't memcpy zero
      bytes (sanitizer fix).
   -  `Bug 1653202 <https://bugzilla.mozilla.org/show_bug.cgi?id=1653202>`__ - Fix initialization
      bug in blapitest when compiled with NSS_DISABLE_DEPRECATED_SEED.
   -  `Bug 1646594 <https://bugzilla.mozilla.org/show_bug.cgi?id=1646594>`__ - Fix AVX2 detection in
      makefile builds.
   -  `Bug 1649633 <https://bugzilla.mozilla.org/show_bug.cgi?id=1649633>`__ - Add
      PK11_FindCertInSlot to search a given slot for a DER-encoded certificate.
   -  `Bug 1651520 <https://bugzilla.mozilla.org/show_bug.cgi?id=1651520>`__ - Fix slotLock race in
      NSC_GetTokenInfo.
   -  `Bug 1647752 <https://bugzilla.mozilla.org/show_bug.cgi?id=1647752>`__ - Update DTLS 1.3
      implementation to draft-38.
   -  `Bug 1649190 <https://bugzilla.mozilla.org/show_bug.cgi?id=1649190>`__ - Run cipher, sdr, and
      ocsp tests under standard test cycle in CI.
   -  `Bug 1649226 <https://bugzilla.mozilla.org/show_bug.cgi?id=1649226>`__ - Add Wycheproof ECDSA
      tests.
   -  `Bug 1637222 <https://bugzilla.mozilla.org/show_bug.cgi?id=1637222>`__ - Consistently enforce
      IV requirements for DES and 3DES.
   -  `Bug 1067214 <https://bugzilla.mozilla.org/show_bug.cgi?id=1067214>`__ - Enforce minimum
      PKCS#1 v1.5 padding length in RSA_CheckSignRecover.
   -  `Bug 1643528 <https://bugzilla.mozilla.org/show_bug.cgi?id=1643528>`__ - Fix compilation error
      with -Werror=strict-prototypes.
   -  `Bug 1646324 <https://bugzilla.mozilla.org/show_bug.cgi?id=1646324>`__ - Advertise PKCS#1
      schemes for certificates in the signature_algorithms extension.
   -  `Bug 1652331 <https://bugzilla.mozilla.org/show_bug.cgi?id=1652331>`__ - Update NSS 3.55 NSPR
      version to 4.27.

   This Bugzilla query returns all the bugs fixed in NSS 3.55:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.55

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.55 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.55 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).