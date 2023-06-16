.. _mozilla_projects_nss_nss_3_18_release_notes:

NSS 3.18 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.18, which is a minor release.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_18_RTM. NSS 3.18 requires NSPR 4.10.8 or newer.

   NSS 3.18 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_18_RTM/src/

.. _new_in_nss_3.18:

`New in NSS 3.18 <#new_in_nss_3.18>`__
--------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  When importing certificates and keys from a PKCS#12 source, it's now possible to override the
      nicknames, prior to importing them into the NSS database, using new API
      SEC_PKCS12DecoderRenameCertNicknames.
   -  The tstclnt test utility program has new command-line options -C, -D, -b and -R.
      Use -C one, two or three times to print information about the certificates received from a
      server, and information about the locally found and trusted issuer certificates, to diagnose
      server side configuration issues. It is possible to run tstclnt without providing a database
      (-D). A PKCS#11 library that contains root CA certificates can be loaded by tstclnt, which may
      either be the nssckbi library provided by NSS (-b) or another compatible library (-R).

   .. rubric:: New Functions
      :name: new_functions

   -  *in certdb.h*

      -  **SEC_CheckCrlTimes** - Check the validity of a CRL at the given time.
      -  **SEC_GetCrlTimes** - Extract the validity times from a CRL.

   -  *in p12.h*

      -  **SEC_PKCS12DecoderRenameCertNicknames** - call an application provided callback for each
         certificate found in a SEC_PKCS12DecoderContext.

   -  *in pk11pub.h*

      -  **\__PK11_SetCertificateNickname** - this is an internal symbol for NSS use only, as with
         all exported NSS symbols that have a leading underscore '_'. Applications that use or
         depend on these symbols can and will break in future NSS releases.

   .. rubric:: New Types
      :name: new_types

   -  *in p12.h*

      -  **SEC_PKCS12NicknameRenameCallback** - a function pointer definition. An application that
         uses SEC_PKCS12DecoderRenameCertNicknames must implement a callback function that
         implements this function interface.

.. _notable_changes_in_nss_3.18:

`Notable Changes in NSS 3.18 <#notable_changes_in_nss_3.18>`__
--------------------------------------------------------------

.. container::

   -  The highest TLS protocol version enabled by default has been increased from TLS 1.0 to TLS
      1.2. Similarly, the highest DTLS protocol version enabled by default has been increased from
      DTLS 1.0 to DTLS 1.2.
   -  The default key size used by certutil when creating an RSA key pair has been increased from
      1024 bits to 2048 bits.
   -  On Mac OS X, by default the softokn shared library will link with the sqlite library installed
      by the operating system, if it is version 3.5 or newer.
   -  The following CA certificates had the Websites and Code Signing **trust bits turned off**

      -  OU = Equifax Secure Certificate Authority

         -  SHA1 Fingerprint: D2:32:09:AD:23:D3:14:23:21:74:E4:0D:7F:9D:62:13:97:86:63:3A

      -  CN = Equifax Secure Global eBusiness CA-1

         -  SHA1 Fingerprint: 7E:78:4A:10:1C:82:65:CC:2D:E1:F1:6D:47:B4:40:CA:D9:0A:19:45

      -  CN = TC TrustCenter Class 3 CA II

         -  SHA1 Fingerprint: 80:25:EF:F4:6E:70:C8:D4:72:24:65:84:FE:40:3B:8A:8D:6A:DB:F5

   -  The following CA certificates were **Added**

      -  CN = Staat der Nederlanden Root CA - G3

         -  SHA1 Fingerprint: D8:EB:6B:41:51:92:59:E0:F3:E7:85:00:C0:3D:B6:88:97:C9:EE:FC

      -  CN = Staat der Nederlanden EV Root CA

         -  SHA1 Fingerprint: 76:E2:7E:C1:4F:DB:82:C1:C0:A6:75:B5:05:BE:3D:29:B4:ED:DB:BB

      -  CN = IdenTrust Commercial Root CA 1

         -  SHA1 Fingerprint: DF:71:7E:AA:4A:D9:4E:C9:55:84:99:60:2D:48:DE:5F:BC:F0:3A:25

      -  CN = IdenTrust Public Sector Root CA 1

         -  SHA1 Fingerprint: BA:29:41:60:77:98:3F:F4:F3:EF:F2:31:05:3B:2E:EA:6D:4D:45:FD

      -  CN = S-TRUST Universal Root CA

         -  SHA1 Fingerprint: 1B:3D:11:14:EA:7A:0F:95:58:54:41:95:BF:6B:25:82:AB:40:CE:9A

      -  CN = Entrust Root Certification Authority - G2

         -  SHA1 Fingerprint: 8C:F4:27:FD:79:0C:3A:D1:66:06:8D:E8:1E:57:EF:BB:93:22:72:D4

      -  CN = Entrust Root Certification Authority - EC1

         -  SHA1 Fingerprint: 20:D8:06:40:DF:9B:25:F5:12:25:3A:11:EA:F7:59:8A:EB:14:B5:47

      -  CN = CFCA EV ROOT

         -  SHA1 Fingerprint: E2:B8:29:4B:55:84:AB:6B:58:C2:90:46:6C:AC:3F:B8:39:8F:84:83

   -  The version number of the updated root CA list has been set to 2.3

.. _bugs_fixed_in_nss_3.18:

`Bugs fixed in NSS 3.18 <#bugs_fixed_in_nss_3.18>`__
----------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.18:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.18

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.18 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.18 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).