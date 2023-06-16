.. _mozilla_projects_nss_nss_3_23_release_notes:

NSS 3.23 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.23, which is a minor release.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_23_RTM. NSS 3.23 requires NSPR 4.12 or newer.

   NSS 3.23 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_23_RTM/src/

.. _new_in_nss_3.23:

`New in NSS 3.23 <#new_in_nss_3.23>`__
--------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  ChaCha20/Poly1305 cipher and TLS cipher suites now supported (`bug
      917571 <https://bugzilla.mozilla.org/show_bug.cgi?id=917571>`__, `bug
      1227905 <https://bugzilla.mozilla.org/show_bug.cgi?id=1227905>`__)

   -

      .. container::

         Experimental-only support TLS 1.3 1-RTT mode (draft-11). This code is not ready for
         production use.

   .. rubric:: New Functions
      :name: new_functions

   -  *in ssl.h*

      -  **SSL_SetDowngradeCheckVersion** - Set maximum version for new ServerRandom anti-downgrade
         mechanism. Clients that perform a version downgrade (which is a dangerous practice) call
         this with the highest version number that they possibly support.  This gives them access to
         the `version downgrade protection from TLS
         1.3 <https://tlswg.github.io/tls13-spec/#client-hello>`__.

.. _notable_changes_in_nss_3.23:

`Notable Changes in NSS 3.23 <#notable_changes_in_nss_3.23>`__
--------------------------------------------------------------

.. container::

   -  The copy of SQLite shipped with NSS has been updated to version 3.10.2 (`bug
      1234698 <https://bugzilla.mozilla.org/show_bug.cgi?id=1234698>`__)
   -  The list of TLS extensions sent in the TLS handshake has been reordered to increase
      compatibility of the Extended Master Secret with servers (`bug
      1243641 <https://bugzilla.mozilla.org/show_bug.cgi?id=1243641>`__)
   -  The build time environment variable NSS_ENABLE_ZLIB has been renamed to NSS_SSL_ENABLE_ZLIB
      (`Bug 1243872 <https://bugzilla.mozilla.org/show_bug.cgi?id=1243872>`__).
   -  The build time environment variable NSS_DISABLE_CHACHAPOLY was added, which can be used to
      prevent compilation of the ChaCha20/Poly1305 code.
   -  The following CA certificates were **Removed**

      -  CN = Staat der Nederlanden Root CA

         -  SHA-256 Fingerprint:
            D4:1D:82:9E:8C:16:59:82:2A:F9:3F:CE:62:BF:FC:DE:26:4F:C8:4E:8B:95:0C:5F:F2:75:D0:52:35:46:95:A3

      -  CN = NetLock Minositett Kozjegyzoi (Class QA) Tanusitvanykiado

         -  SHA-256 Fingerprint:
            E6:06:DD:EE:E2:EE:7F:5C:DE:F5:D9:05:8F:F8:B7:D0:A9:F0:42:87:7F:6A:17:1E:D8:FF:69:60:E4:CC:5E:A5

      -  CN = NetLock Kozjegyzoi (Class A) Tanusitvanykiado

         -  SHA-256 Fingerprint:
            7F:12:CD:5F:7E:5E:29:0E:C7:D8:51:79:D5:B7:2C:20:A5:BE:75:08:FF:DB:5B:F8:1A:B9:68:4A:7F:C9:F6:67

      -  CN = NetLock Uzleti (Class B) Tanusitvanykiado

         -  SHA-256 Fingerprint:
            39:DF:7B:68:2B:7B:93:8F:84:71:54:81:CC:DE:8D:60:D8:F2:2E:C5:98:87:7D:0A:AA:C1:2B:59:18:2B:03:12

      -  CN = NetLock Expressz (Class C) Tanusitvanykiado

         -  SHA-256 Fingerprint:
            0B:5E:ED:4E:84:64:03:CF:55:E0:65:84:84:40:ED:2A:82:75:8B:F5:B9:AA:1F:25:3D:46:13:CF:A0:80:FF:3F

      -  Friendly Name: VeriSign Class 1 Public PCA – G2

         -  SHA-256 Fingerprint:
            34:1D:E9:8B:13:92:AB:F7:F4:AB:90:A9:60:CF:25:D4:BD:6E:C6:5B:9A:51:CE:6E:D0:67:D0:0E:C7:CE:9B:7F

      -  Friendly Name: VeriSign Class 3 Public PCA

         -  SHA-256 Fingerprint:
            A4:B6:B3:99:6F:C2:F3:06:B3:FD:86:81:BD:63:41:3D:8C:50:09:CC:4F:A3:29:C2:CC:F0:E2:FA:1B:14:03:05

      -  Friendly Name: VeriSign Class 3 Public PCA – G2

         -  SHA-256 Fingerprint:
            83:CE:3C:12:29:68:8A:59:3D:48:5F:81:97:3C:0F:91:95:43:1E:DA:37:CC:5E:36:43:0E:79:C7:A8:88:63:8B

      -  CN = CA Disig

         -  SHA-256 Fingerprint:
            92:BF:51:19:AB:EC:CA:D0:B1:33:2D:C4:E1:D0:5F:BA:75:B5:67:90:44:EE:0C:A2:6E:93:1F:74:4F:2F:33:CF

   -  The following CA certificates were **Added**

      -  CN = SZAFIR ROOT CA2

         -  SHA-256 Fingerprint:
            A1:33:9D:33:28:1A:0B:56:E5:57:D3:D3:2B:1C:E7:F9:36:7E:B0:94:BD:5F:A7:2A:7E:50:04:C8:DE:D7:CA:FE

      -  CN = Certum Trusted Network CA 2

         -  SHA-256 Fingerprint:
            B6:76:F2:ED:DA:E8:77:5C:D3:6C:B0:F6:3C:D1:D4:60:39:61:F4:9E:62:65:BA:01:3A:2F:03:07:B6:D0:B8:04

   -  The following CA certificate had the Email **trust bit turned on**

      -  CN = Actalis Authentication Root CA

         -  SHA-256 Fingerprint:
            55:92:60:84:EC:96:3A:64:B9:6E:2A:BE:01:CE:0B:A8:6A:64:FB:FE:BC:C7:AA:B5:AF:C1:55:B3:7F:D7:60:66

.. _security_fixes_in_nss_3.23:

`Security Fixes in NSS 3.23 <#security_fixes_in_nss_3.23>`__
------------------------------------------------------------

.. container::

   -  `Bug 1245528 <https://bugzilla.mozilla.org/show_bug.cgi?id=1245528>`__ /
      `CVE-2016-1950 <http://www.cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2016-1950>`__ - Fixed a
      heap-based buffer overflow related to the parsing of certain ASN.1 structures. An attacker
      could create a specially-crafted certificate which, when parsed by NSS, would cause a crash or
      execution of arbitrary code with the permissions of the user.

.. _bugs_fixed_in_nss_3.23:

`Bugs fixed in NSS 3.23 <#bugs_fixed_in_nss_3.23>`__
----------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.23:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.23

`Acknowledgements <#acknowledgements>`__
----------------------------------------

.. container::

   The NSS development team would like to thank security researcher Francis Gabriel for responsibly
   disclosing the issue in `Bug 1245528 <https://bugzilla.mozilla.org/show_bug.cgi?id=1245528>`__.

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.23 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.23 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).