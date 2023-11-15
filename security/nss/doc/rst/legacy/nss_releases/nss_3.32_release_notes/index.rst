.. _mozilla_projects_nss_nss_3_32_release_notes:

NSS 3.32 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The Network Security Services (NSS) team has released NSS 3.32, which is a minor release.



`Distribution information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The hg tag is NSS_3_32_RTM. NSS 3.32 requires Netscape Portable Runtime (NSPR) 4.16, or newer.

   NSS 3.32 source distributions are available on ftp.mozilla.org, for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_32_RTM/src/

.. _notable_changes_in_nss_3.32:

`Notable Changes in NSS 3.32 <#notable_changes_in_nss_3.32>`__
--------------------------------------------------------------

.. container::

   -  Various minor improvements and correctness fixes.
   -  The Code Signing trust bit was **turned off** for all, included root certificates.
   -  The Websites (TLS/SSL) trust bit was **turned off** for the following root certificates.

      -  CN = AddTrust Class 1 CA Root

         -  SHA-256 Fingerprint:
            8C:72:09:27:9A:C0:4E:27:5E:16:D0:7F:D3:B7:75:E8:01:54:B5:96:80:46:E3:1F:52:DD:25:76:63:24:E9:A7

      -  CN = Swisscom Root CA 2

         -  SHA-256 Fingerprint:
            F0:9B:12:2C:71:14:F4:A0:9B:D4:EA:4F:4A:99:D5:58:B4:6E:4C:25:CD:81:14:0D:29:C0:56:13:91:4C:38:41

   -  The following CA certificates were **Removed**:

      -  CN = AddTrust Public CA Root

         -  SHA-256 Fingerprint:
            07:91:CA:07:49:B2:07:82:AA:D3:C7:D7:BD:0C:DF:C9:48:58:35:84:3E:B2:D7:99:60:09:CE:43:AB:6C:69:27

      -  CN = AddTrust Qualified CA Root

         -  SHA-256 Fingerprint:
            80:95:21:08:05:DB:4B:BC:35:5E:44:28:D8:FD:6E:C2:CD:E3:AB:5F:B9:7A:99:42:98:8E:B8:F4:DC:D0:60:16

      -  CN = China Internet Network Information Center EV Certificates Root

         -  SHA-256 Fingerprint:
            1C:01:C6:F4:DB:B2:FE:FC:22:55:8B:2B:CA:32:56:3F:49:84:4A:CF:C3:2B:7B:E4:B0:FF:59:9F:9E:8C:7A:F7

      -  CN = CNNIC ROOT

         -  SHA-256 Fingerprint:
            E2:83:93:77:3D:A8:45:A6:79:F2:08:0C:C7:FB:44:A3:B7:A1:C3:79:2C:B7:EB:77:29:FD:CB:6A:8D:99:AE:A7

      -  CN = ComSign Secured CA

         -  SHA-256 Fingerprint:
            50:79:41:C7:44:60:A0:B4:70:86:22:0D:4E:99:32:57:2A:B5:D1:B5:BB:CB:89:80:AB:1C:B1:76:51:A8:44:D2

      -  CN = GeoTrust Global CA 2

         -  SHA-256 Fingerprint:
            CA:2D:82:A0:86:77:07:2F:8A:B6:76:4F:F0:35:67:6C:FE:3E:5E:32:5E:01:21:72:DF:3F:92:09:6D:B7:9B:85

      -  CN = Secure Certificate Services

         -  SHA-256 Fingerprint:
            BD:81:CE:3B:4F:65:91:D1:1A:67:B5:FC:7A:47:FD:EF:25:52:1B:F9:AA:4E:18:B9:E3:DF:2E:34:A7:80:3B:E8

      -  CN = Swisscom Root CA 1

         -  SHA-256 Fingerprint:
            21:DB:20:12:36:60:BB:2E:D4:18:20:5D:A1:1E:E7:A8:5A:65:E2:BC:6E:55:B5:AF:7E:78:99:C8:A2:66:D9:2E

      -  CN = Swisscom Root EV CA 2

         -  SHA-256 Fingerprint:
            D9:5F:EA:3C:A4:EE:DC:E7:4C:D7:6E:75:FC:6D:1F:F6:2C:44:1F:0F:A8:BC:77:F0:34:B1:9E:5D:B2:58:01:5D

      -  CN = Trusted Certificate Services

         -  SHA-256 Fingerprint:
            3F:06:E5:56:81:D4:96:F5:BE:16:9E:B5:38:9F:9F:2B:8F:F6:1E:17:08:DF:68:81:72:48:49:CD:5D:27:CB:69

      -  CN = UTN-USERFirst-Hardware

         -  SHA-256 Fingerprint:
            6E:A5:47:41:D0:04:66:7E:ED:1B:48:16:63:4A:A3:A7:9E:6E:4B:96:95:0F:82:79:DA:FC:8D:9B:D8:81:21:37

      -  CN = UTN-USERFirst-Object

         -  SHA-256 Fingerprint:
            6F:FF:78:E4:00:A7:0C:11:01:1C:D8:59:77:C4:59:FB:5A:F9:6A:3D:F0:54:08:20:D0:F4:B8:60:78:75:E5:8F

.. _bugs_fixed_in_nss_3.32:

`Bugs fixed in NSS 3.32 <#bugs_fixed_in_nss_3.32>`__
----------------------------------------------------

.. container::

   NSS versions 3.28.x, 3.29.x. 3.30.x and 3.31.x contained a bug in function CERT_CompareName,
   which caused the first RDN to be ignored. NSS version 3.32 fixed this bug. (CVE-2018-5149, `Bug
   1361197 <https://bugzilla.mozilla.org/show_bug.cgi?id=1361197>`__)

   This Bugzilla query returns all the bugs fixed in NSS 3.32:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.32

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.32 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.32 shared libraries,
   without recompiling, or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (select the
   product 'NSS').