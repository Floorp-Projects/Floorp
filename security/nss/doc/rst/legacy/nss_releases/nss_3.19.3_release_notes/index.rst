.. _mozilla_projects_nss_nss_3_19_3_release_notes:

NSS 3.19.3 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.19.3 is a patch release for NSS 3.19. The bug fixes in NSS
   3.19.3 are described in the "Bugs Fixed" section below.



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_19_3_RTM. NSS 3.19.3 requires NSPR 4.10.8 or newer.

   NSS 3.19.3 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_19_3_RTM/src/

.. _new_in_nss_3.19.3:

`New in NSS 3.19.3 <#new_in_nss_3.19.3>`__
------------------------------------------

.. container::

   No new functionality is introduced in this release. This is a patch release to update the list of
   root CA certificates.

.. _notable_changes_in_nss_3.19.3:

`Notable Changes in NSS 3.19.3 <#notable_changes_in_nss_3.19.3>`__
------------------------------------------------------------------

.. container::

   -  The following CA certificates were **Removed**

      -  CN = Buypass Class 3 CA 1

         -  SHA1 Fingerprint: 61:57:3A:11:DF:0E:D8:7E:D5:92:65:22:EA:D0:56:D7:44:B3:23:71

      -  CN = TÜRKTRUST Elektronik Sertifika Hizmet Sağlayıcısı

         -  SHA1 Fingerprint: 79:98:A3:08:E1:4D:65:85:E6:C2:1E:15:3A:71:9F:BA:5A:D3:4A:D9

      -  CN = SG TRUST SERVICES RACINE

         -  SHA1 Fingerprint: 0C:62:8F:5C:55:70:B1:C9:57:FA:FD:38:3F:B0:3D:7B:7D:D7:B9:C6

      -  CN = TC TrustCenter Universal CA I

         -  SHA-1 Fingerprint: 6B:2F:34:AD:89:58:BE:62:FD:B0:6B:5C:CE:BB:9D:D9:4F:4E:39:F3

      -  CN = TC TrustCenter Class 2 CA II

         -  SHA-1 Fingerprint: AE:50:83:ED:7C:F4:5C:BC:8F:61:C6:21:FE:68:5D:79:42:21:15:6E

   -  The following CA certificate had the Websites **trust bit turned off**

      -  CN = ComSign Secured CA

         -  SHA1 Fingerprint: F9:CD:0E:2C:DA:76:24:C1:8F:BD:F0:F0:AB:B6:45:B8:F7:FE:D5:7A

   -  The following CA certificates were **Added**

      -  CN = TÜRKTRUST Elektronik Sertifika Hizmet Sağlayıcısı H5

         -  SHA1 Fingerprint: C4:18:F6:4D:46:D1:DF:00:3D:27:30:13:72:43:A9:12:11:C6:75:FB

      -  CN = TÜRKTRUST Elektronik Sertifika Hizmet Sağlayıcısı H6

         -  SHA1 Fingerprint: 8A:5C:8C:EE:A5:03:E6:05:56:BA:D8:1B:D4:F6:C9:B0:ED:E5:2F:E0

      -  CN = Certinomis - Root CA

         -  SHA1 Fingerprint: 9D:70:BB:01:A5:A4:A0:18:11:2E:F7:1C:01:B9:32:C5:34:E7:88:A8

   -  The version number of the updated root CA list has been set to 2.5

.. _bugs_fixed_in_nss_3.19.3:

`Bugs fixed in NSS 3.19.3 <#bugs_fixed_in_nss_3.19.3>`__
--------------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.19.3:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.19.3

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.19.3 shared libraries are backward compatible with all older NSS 3.19 shared libraries. A
   program linked with older NSS 3.19 shared libraries will work with NSS 3.19.3 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).