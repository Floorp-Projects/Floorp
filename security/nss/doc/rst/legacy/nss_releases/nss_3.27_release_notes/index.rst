.. _mozilla_projects_nss_nss_3_27_release_notes:

NSS 3.27 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The Network Security Services (NSS) team has released NSS 3.27, which is a minor release.



`Distribution information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The hg tag is NSS_3_27_RTM. NSS 3.27 requires Netscape Portable Runtime(NSPR) 4.13 or newer.

   NSS 3.27 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_27_RTM/src/

.. _new_in_nss_3.27:

`New in NSS 3.27 <#new_in_nss_3.27>`__
--------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  Allow custom named group priorities for TLS key exchange handshake (SSL_NamedGroupConfig).
   -  Added support for RSA-PSS signatures in TLS 1.2 and TLS 1.3

   .. rubric:: New Functions
      :name: new_functions

   -  in ssl.h

      -  SSL_NamedGroupConfig

.. _notable_changes_in_nss_3.27:

`Notable Changes in NSS 3.27 <#notable_changes_in_nss_3.27>`__
--------------------------------------------------------------

.. container::

   -  *UPDATE 2016-10-02:*

      -  The maximum TLS version supported has been increased to TLS 1.3 (draft).
      -  Although the maximum TLS version enabled by default is still TLS 1.2, there are
         applications that query the list of TLS protocol versions supported by NSS, and enable all
         supported versions. For those applications, updating to NSS 3.27 may result in TLS 1.3
         (draft) to be enabled.
      -  The TLS 1.3 (draft) protocol can be disabled, by defining symbol NSS_DISABLE_TLS_1_3 when
         building NSS.

   -  NPN can not be enabled anymore.
   -  Hard limits on the maximum number of TLS records encrypted with the same key are enforced.
   -  Disabled renegotiation in DTLS.
   -  The following CA certificates were **Removed**

      -  CN = IGC/A, O = PM/SGDN, OU = DCSSI

         -  SHA256 Fingerprint:
            B9:BE:A7:86:0A:96:2E:A3:61:1D:AB:97:AB:6D:A3:E2:1C:10:68:B9:7D:55:57:5E:D0:E1:12:79:C1:1C:89:32

      -  CN = Juur-SK, O = AS Sertifitseerimiskeskus

         -  SHA256 Fingerprint:
            EC:C3:E9:C3:40:75:03:BE:E0:91:AA:95:2F:41:34:8F:F8:8B:AA:86:3B:22:64:BE:FA:C8:07:90:15:74:E9:39

      -  CN = EBG Elektronik Sertifika Hizmet Sağlayıcısı

         -  SHA-256 Fingerprint:
            35:AE:5B:DD:D8:F7:AE:63:5C:FF:BA:56:82:A8:F0:0B:95:F4:84:62:C7:10:8E:E9:A0:E5:29:2B:07:4A:AF:B2

      -  CN = S-TRUST Authentication and Encryption Root CA 2005:PN

         -  SHA-256 Fingerprint:
            37:D8:DC:8A:F7:86:78:45:DA:33:44:A6:B1:BA:DE:44:8D:8A:80:E4:7B:55:79:F9:6B:F6:31:76:8F:9F:30:F6

      -  O = VeriSign, Inc., OU = Class 1 Public Primary Certification Authority

         -  SHA-256 Fingerprint:
            51:84:7C:8C:BD:2E:9A:72:C9:1E:29:2D:2A:E2:47:D7:DE:1E:3F:D2:70:54:7A:20:EF:7D:61:0F:38:B8:84:2C

      -  O = VeriSign, Inc., OU = Class 2 Public Primary Certification Authority - G2

         -  SHA-256 Fingerprint:
            3A:43:E2:20:FE:7F:3E:A9:65:3D:1E:21:74:2E:AC:2B:75:C2:0F:D8:98:03:05:BC:50:2C:AF:8C:2D:9B:41:A1

      -  O = VeriSign, Inc., OU = Class 3 Public Primary Certification Authority

         -  SHA-256 Fingerprint:
            E7:68:56:34:EF:AC:F6:9A:CE:93:9A:6B:25:5B:7B:4F:AB:EF:42:93:5B:50:A2:65:AC:B5:CB:60:27:E4:4E:70

      -  O = Equifax, OU = Equifax Secure Certificate Authority

         -  SHA-256 Fingerprint:
            08:29:7A:40:47:DB:A2:36:80:C7:31:DB:6E:31:76:53:CA:78:48:E1:BE:BD:3A:0B:01:79:A7:07:F9:2C:F1:78

      -  CN = Equifax Secure eBusiness CA-1

         -  SHA-256 Fingerprint:
            CF:56:FF:46:A4:A1:86:10:9D:D9:65:84:B5:EE:B5:8A:51:0C:42:75:B0:E5:F9:4F:40:BB:AE:86:5E:19:F6:73

      -  CN = Equifax Secure Global eBusiness CA-1

         -  SHA-256 Fingerprint:
            5F:0B:62:EA:B5:E3:53:EA:65:21:65:16:58:FB:B6:53:59:F4:43:28:0A:4A:FB:D1:04:D7:7D:10:F9:F0:4C:07

.. _bugs_fixed_in_nss_3.27:

`Bugs fixed in NSS 3.27 <#bugs_fixed_in_nss_3.27>`__
----------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.27:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.27

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.27 shared libraries are backwards compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.27 shared libraries
   without recompiling or relinking. Applications that restrict their use of NSS APIs to the
   functions listed in NSS Public Functions will remain compatible with future versions of the NSS
   shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).