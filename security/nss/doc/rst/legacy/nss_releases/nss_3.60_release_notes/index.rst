.. _mozilla_projects_nss_nss_3_60_release_notes:

NSS 3.60 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.60 on **11 December 2020**, which is
   a minor release.

   The NSS team would like to recognize first-time contributors:

   -  yogesh



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_60_RTM. NSS 3.60 requires NSPR 4.29 or newer.

   NSS 3.60 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_60_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _notable_changes_in_nss_3.60:

`Notable Changes in NSS 3.60 <#notable_changes_in_nss_3.60>`__
--------------------------------------------------------------

.. container::

   -  TLS 1.3 Encrypted Client Hello (draft-ietf-tls-esni-08) support has been added, replacing the
      ESNI (draft-ietf-tls-esni-01). See `bug
      1654332 <https://bugzilla.mozilla.org/show_bug.cgi?id=1654332>`__ for more information.

.. _certificate_authority_changes:

`Certificate Authority Changes <#certificate_authority_changes>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  The following CA certificates were added:

      -  `Bug 1678166 <https://bugzilla.mozilla.org/show_bug.cgi?id=1678166>`__ - NAVER Global Root
         Certification Authority

         -  SHA-256 Fingerprint: 88F438DCF8FFD1FA8F429115FFE5F82AE1E06E0C70C375FAAD717B34A49E7265

   -  The following CA certificates were removed in `bug
      1670769 <https://bugzilla.mozilla.org/show_bug.cgi?id=1670769>`__:

      -  GeoTrust Global CA

         -  SHA-256 Fingerprint:
            FF856A2D251DCD88D36656F450126798CFABAADE40799C722DE4D2B5DB36A73A

      -  GeoTrust Primary Certification Authority

         -  SHA-256 Fingerprint: 37D51006C512EAAB626421F1EC8C92013FC5F82AE98EE533EB4619B8DEB4D06C

      -  GeoTrust Primary Certification Authority - G3

         -  SHA-256 Fingerprint: B478B812250DF878635C2AA7EC7D155EAA625EE82916E2CD294361886CD1FBD4

      -  thawte Primary Root CA

         -  SHA-256 Fingerprint: 8D722F81A9C113C0791DF136A2966DB26C950A971DB46B4199F4EA54B78BFB9F

      -  thawte Primary Root CA - G3

         -  SHA-256 Fingerprint: 4B03F45807AD70F21BFC2CAE71C9FDE4604C064CF5FFB686BAE5DBAAD7FDD34C

      -  VeriSign Class 3 Public Primary Certification Authority - G4

         -  SHA-256 Fingerprint: 69DDD7EA90BB57C93E135DC85EA6FCD5480B603239BDC454FC758B2A26CF7F79

      -  VeriSign Class 3 Public Primary Certification Authority - G5

         -  SHA-256 Fingerprint: 9ACFAB7E43C8D880D06B262A94DEEEE4B4659989C3D0CAF19BAF6405E41AB7DF

      -  thawte Primary Root CA - G2

         -  SHA-256 Fingerprint: A4310D50AF18A6447190372A86AFAF8B951FFB431D837F1E5688B45971ED1557

      -  GeoTrust Universal CA

         -  SHA-256 Fingerprint: A0459B9F63B22559F5FA5D4C6DB3F9F72FF19342033578F073BF1D1B46CBB912

      -  GeoTrust Universal CA 2

         -  SHA-256 Fingerprint: A0234F3BC8527CA5628EEC81AD5D69895DA5680DC91D1CB8477F33F878B95B0B

.. _bugs_fixed_in_nss_3.60:

`Bugs fixed in NSS 3.60 <#bugs_fixed_in_nss_3.60>`__
----------------------------------------------------

.. container::

   -  Bug 1654332 - Implement Encrypted Client Hello (draft-ietf-tls-esni-08) in NSS.
   -  Bug 1678189 - Update CA list version to 2.46.
   -  Bug 1670769 - Remove 10 GeoTrust, thawte, and VeriSign root certs from NSS.
   -  Bug 1678166 - Add NAVER Global Root Certification Authority root cert to NSS.
   -  Bug 1678384 - Add a build flag to allow building nssckbi-testlib in m-c.
   -  Bug 1570539 - Remove -X alt-server-hello option from tstclnt.
   -  Bug 1675523 - Fix incorrect pkcs11t.h value CKR_PUBLIC_KEY_INVALID.
   -  Bug 1642174 - Fix PowerPC ABI version 1 build failure.
   -  Bug 1674819 - Fix undefined shift in fuzzer mode.
   -  Bug 1678990 - Fix ARM crypto extensions detection on macOS.
   -  Bug 1679290 - Fix lock order inversion and potential deadlock with libnsspem.
   -  Bug 1680400 - Fix memory leak in PK11_UnwrapPrivKey.

   This Bugzilla query returns all the bugs fixed in NSS 3.60:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.60

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.60 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.60 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).