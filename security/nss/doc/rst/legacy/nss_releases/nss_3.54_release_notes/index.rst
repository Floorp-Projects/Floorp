.. _mozilla_projects_nss_nss_3_54_release_notes:

NSS 3.54 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.54 on **26 June 2020**, which is a
   minor release.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_54_RTM. NSS 3.54 requires NSPR 4.26 or newer.

   NSS 3.54 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_54_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _notable_changes_in_nss_3.54:

`Notable Changes in NSS 3.54 <#notable_changes_in_nss_3.54>`__
--------------------------------------------------------------

.. container::

   -  Support for TLS 1.3 external pre-shared keys (`Bug
      1603042 <https://bugzilla.mozilla.org/show_bug.cgi?id=1603042>`__).
   -  Use ARM Cryptography Extension for SHA256, when available. (`Bug
      1528113 <https://bugzilla.mozilla.org/show_bug.cgi?id=1528113>`__).

.. _certificate_authority_changes:

`Certificate Authority Changes <#certificate_authority_changes>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  The following CA certificates were Added:

      -  `Bug 1645186 <https://bugzilla.mozilla.org/show_bug.cgi?id=1645186>`__ - certSIGN Root CA
         G2

         -  SHA-256 Fingerprint: 657CFE2FA73FAA38462571F332A2363A46FCE7020951710702CDFBB6EEDA3305

      -  `Bug 1645174 <https://bugzilla.mozilla.org/show_bug.cgi?id=1645174>`__ - e-Szigno Root CA
         2017

         -  SHA-256 Fingerprint: BEB00B30839B9BC32C32E4447905950641F26421B15ED089198B518AE2EA1B99

      -  `Bug 1641716 <https://bugzilla.mozilla.org/show_bug.cgi?id=1641716>`__ - Microsoft ECC Root
         Certificate Authority 2017

         -  SHA-256 Fingerprint: 358DF39D764AF9E1B766E9C972DF352EE15CFAC227AF6AD1D70E8E4A6EDCBA02

      -  `Bug 1641716 <https://bugzilla.mozilla.org/show_bug.cgi?id=1641716>`__ - Microsoft RSA Root
         Certificate Authority 2017

         -  SHA-256 Fingerprint: C741F70F4B2A8D88BF2E71C14122EF53EF10EBA0CFA5E64CFA20F418853073E0

   -  The following CA certificates were Removed:

      -  `Bug 1645199 <https://bugzilla.mozilla.org/show_bug.cgi?id=1645199>`__ - AddTrust Class 1
         CA Root

         -  SHA-256 Fingerprint:
            8C7209279AC04E275E16D07FD3B775E80154B5968046E31F52DD25766324E9A7

      -  `Bug 1645199 <https://bugzilla.mozilla.org/show_bug.cgi?id=1645199>`__ - AddTrust External
         CA Root

         -  SHA-256 Fingerprint:
            687FA451382278FFF0C8B11F8D43D576671C6EB2BCEAB413FB83D965D06D2FF2

      -  `Bug 1641718 <https://bugzilla.mozilla.org/show_bug.cgi?id=1641718>`__ - LuxTrust Global
         Root 2

         -  SHA-256 Fingerprint: 54455F7129C20B1447C418F997168F24C58FC5023BF5DA5BE2EB6E1DD8902ED5

      -  `Bug 1639987 <https://bugzilla.mozilla.org/show_bug.cgi?id=1639987>`__ - Staat der
         Nederlanden Root CA - G2

         -  SHA-256 Fingerprint: 668C83947DA63B724BECE1743C31A0E6AED0DB8EC5B31BE377BB784F91B6716F

      -  `Bug 1618402 <https://bugzilla.mozilla.org/show_bug.cgi?id=1618402>`__ - Symantec Class 2
         Public Primary Certification Authority - G4

         -  SHA-256 Fingerprint: FE863D0822FE7A2353FA484D5924E875656D3DC9FB58771F6F616F9D571BC592

      -  `Bug 1618402 <https://bugzilla.mozilla.org/show_bug.cgi?id=1618402>`__ - Symantec Class 1
         Public Primary Certification Authority - G4

         -  SHA-256 Fingerprint: 363F3C849EAB03B0A2A0F636D7B86D04D3AC7FCFE26A0A9121AB9795F6E176DF

      -  `Bug 1618402 <https://bugzilla.mozilla.org/show_bug.cgi?id=1618402>`__ - VeriSign Class 3
         Public Primary Certification Authority - G3

         -  SHA-256 Fingerprint: EB04CF5EB1F39AFA762F2BB120F296CBA520C1B97DB1589565B81CB9A17B7244

   -  A number of certificates had their Email trust bit disabled. See `Bug
      1618402 <https://bugzilla.mozilla.org/show_bug.cgi?id=1618402#c0>`__ for a complete list.

.. _bugs_fixed_in_nss_3.54:

`Bugs fixed in NSS 3.54 <#bugs_fixed_in_nss_3.54>`__
----------------------------------------------------

.. container::

   -  `Bug 1528113 <https://bugzilla.mozilla.org/show_bug.cgi?id=1528113>`__ - Use ARM Cryptography
      Extension for SHA256.
   -  `Bug 1603042 <https://bugzilla.mozilla.org/show_bug.cgi?id=1603042>`__ - Add TLS 1.3 external
      PSK support.
   -  `Bug 1642802 <https://bugzilla.mozilla.org/show_bug.cgi?id=1642802>`__ - Add uint128 support
      for HACL\* curve25519 on Windows.
   -  `Bug 1645186 <https://bugzilla.mozilla.org/show_bug.cgi?id=1645186>`__ - Add "certSIGN Root CA
      G2" root certificate.
   -  `Bug 1645174 <https://bugzilla.mozilla.org/show_bug.cgi?id=1645174>`__ - Add Microsec's
      "e-Szigno Root CA 2017" root certificate.
   -  `Bug 1641716 <https://bugzilla.mozilla.org/show_bug.cgi?id=1641716>`__ - Add Microsoft's
      non-EV root certificates.
   -  `Bug 1621151 <https://bugzilla.mozilla.org/show_bug.cgi?id=1621151>`__ - Disable email trust
      bit for "O=Government Root Certification Authority; C=TW" root.
   -  `Bug 1645199 <https://bugzilla.mozilla.org/show_bug.cgi?id=1645199>`__ - Remove AddTrust root
      certificates.
   -  `Bug 1641718 <https://bugzilla.mozilla.org/show_bug.cgi?id=1641718>`__ - Remove "LuxTrust
      Global Root 2" root certificate.
   -  `Bug 1639987 <https://bugzilla.mozilla.org/show_bug.cgi?id=1639987>`__ - Remove "Staat der
      Nederlanden Root CA - G2" root certificate.
   -  `Bug 1618402 <https://bugzilla.mozilla.org/show_bug.cgi?id=1618402>`__ - Remove Symantec root
      certificates and disable email trust bit.
   -  `Bug 1640516 <https://bugzilla.mozilla.org/show_bug.cgi?id=1640516>`__ - NSS 3.54 should
      depend on NSPR 4.26.
   -  `Bug 1642146 <https://bugzilla.mozilla.org/show_bug.cgi?id=1642146>`__ - Fix undefined
      reference to \`PORT_ZAlloc_stub' in seed.c.
   -  `Bug 1642153 <https://bugzilla.mozilla.org/show_bug.cgi?id=1642153>`__ - Fix infinite
      recursion building NSS.
   -  `Bug 1642638 <https://bugzilla.mozilla.org/show_bug.cgi?id=1642638>`__ - Fix fuzzing assertion
      crash.
   -  `Bug 1642871 <https://bugzilla.mozilla.org/show_bug.cgi?id=1642871>`__ - Enable
      SSL_SendSessionTicket after resumption.
   -  `Bug 1643123 <https://bugzilla.mozilla.org/show_bug.cgi?id=1643123>`__ - Support
      SSL_ExportEarlyKeyingMaterial with External PSKs.
   -  `Bug 1643557 <https://bugzilla.mozilla.org/show_bug.cgi?id=1643557>`__ - Fix numerous compile
      warnings in NSS.
   -  `Bug 1644774 <https://bugzilla.mozilla.org/show_bug.cgi?id=1644774>`__ - SSL gtests to use
      ClearServerCache when resetting self-encrypt keys.
   -  `Bug 1645479 <https://bugzilla.mozilla.org/show_bug.cgi?id=1645479>`__ - Don't use
      SECITEM_MakeItem in secutil.c.
   -  `Bug 1646520 <https://bugzilla.mozilla.org/show_bug.cgi?id=1646520>`__ - Stricter enforcement
      of ASN.1 INTEGER encoding.

   This Bugzilla query returns all the bugs fixed in NSS 3.54:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.54

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.54 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.54 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).