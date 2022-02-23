.. _mozilla_projects_nss_nss_3_17_3_release_notes:

NSS 3.17.3 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.17.3 is a patch release for NSS 3.17. The bug fixes in NSS
   3.17.3 are described in the "Bugs Fixed" section below.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_17_3_RTM. NSS 3.17.3 requires NSPR 4.10.7 or newer.

   NSS 3.17.3 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_17_3_RTM/src/

.. _new_in_nss_3.17.3:

`New in NSS 3.17.3 <#new_in_nss_3.17.3>`__
------------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  Support for TLS_FALLBACK_SCSV has been added to the ssltap and tstclnt utilities.

.. _notable_changes_in_nss_3.17.3:

`Notable Changes in NSS 3.17.3 <#notable_changes_in_nss_3.17.3>`__
------------------------------------------------------------------

.. container::

   -  The QuickDER decoder now decodes lengths robustly (CVE-2014-1569).
   -  The following CA certificates were **Removed**

      -  CN = GTE CyberTrust Global Root

         -  SHA1 Fingerprint: 97:81:79:50:D8:1C:96:70:CC:34:D8:09:CF:79:44:31:36:7E:F4:74

      -  CN = Thawte Server CA

         -  SHA1 Fingerprint: 23:E5:94:94:51:95:F2:41:48:03:B4:D5:64:D2:A3:A3:F5:D8:8B:8C

      -  CN = Thawte Premium Server CA

         -  SHA1 Fingerprint: 62:7F:8D:78:27:65:63:99:D2:7D:7F:90:44:C9:FE:B3:F3:3E:FA:9A

      -  CN = America Online Root Certification Authority 1

         -  SHA-1 Fingerprint: 39:21:C1:15:C1:5D:0E:CA:5C:CB:5B:C4:F0:7D:21:D8:05:0B:56:6A

      -  CN = America Online Root Certification Authority 2

         -  SHA-1 Fingerprint: 85:B5:FF:67:9B:0C:79:96:1F:C8:6E:44:22:00:46:13:DB:17:92:84

   -  The following CA certificates had the Websites and Code Signing **trust bits turned off**

      -  OU = Class 3 Public Primary Certification Authority - G2

         -  SHA1 Fingerprint: 85:37:1C:A6:E5:50:14:3D:CE:28:03:47:1B:DE:3A:09:E8:F8:77:0F

      -  CN = Equifax Secure eBusiness CA-1

         -  SHA1 Fingerprint: DA:40:18:8B:91:89:A3:ED:EE:AE:DA:97:FE:2F:9D:F5:B7:D1:8A:41

   -  The following CA certificates were **Added**

      -  CN = COMODO RSA Certification Authority

         -  SHA1 Fingerprint: AF:E5:D2:44:A8:D1:19:42:30:FF:47:9F:E2:F8:97:BB:CD:7A:8C:B4

      -  CN = USERTrust RSA Certification Authority

         -  SHA1 Fingerprint: 2B:8F:1B:57:33:0D:BB:A2:D0:7A:6C:51:F7:0E:E9:0D:DA:B9:AD:8E

      -  CN = USERTrust ECC Certification Authority

         -  SHA1 Fingerprint: D1:CB:CA:5D:B2:D5:2A:7F:69:3B:67:4D:E5:F0:5A:1D:0C:95:7D:F0

      -  CN = GlobalSign ECC Root CA - R4

         -  SHA1 Fingerprint: 69:69:56:2E:40:80:F4:24:A1:E7:19:9F:14:BA:F3:EE:58:AB:6A:BB

      -  CN = GlobalSign ECC Root CA - R5

         -  SHA1 Fingerprint: 1F:24:C6:30:CD:A4:18:EF:20:69:FF:AD:4F:DD:5F:46:3A:1B:69:AA

   -  The version number of the updated root CA list has been set to 2.2

.. _bugs_fixed_in_nss_3.17.3:

`Bugs fixed in NSS 3.17.3 <#bugs_fixed_in_nss_3.17.3>`__
--------------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.17.3:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.17.3

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.17.3 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.17.3 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).