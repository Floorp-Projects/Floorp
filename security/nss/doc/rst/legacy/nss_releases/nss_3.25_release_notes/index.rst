.. _mozilla_projects_nss_nss_3_25_release_notes:

NSS 3.25 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The Network Security Services (NSS) team has released NSS 3.25, which is a minor release.

.. _distribution_information:

`Distribution information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The hg tag is NSS_3_25_RTM. NSS 3.25 requires Netscape Portable Runtime(NSPR) 4.12 or newer.

   NSS 3.25 source distributions are available on ftp.mozilla.org for secure HTTPS download at the
   following location.

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_25_RTM/src/

.. _new_in_nss_3.25:

`New in NSS 3.25 <#new_in_nss_3.25>`__
--------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  Implemented DHE key agreement for TLS 1.3.
   -  Added support for ChaCha with TLS 1.3.
   -  Added support for TLS 1.2 ciphersuites that use SHA384 as the PRF.
   -  Removed the limitation that allowed NSS to only support certificate_verify messages that used
      the same signature hash algorithm as the PRF when using TLS 1.2 client authentication.
   -  Several functions have been added to the public API of the NSS Cryptoki Framework.

   .. rubric:: New Functions
      :name: new_functions

   -  *in nssckfw.h*

      -  **NSSCKFWSlot_GetSlotID**
      -  **NSSCKFWSession_GetFWSlot**
      -  **NSSCKFWInstance_DestroySessionHandle**
      -  **NSSCKFWInstance_FindSessionHandle**

.. _notable_changes_in_nss_3.25:

`Notable Changes in NSS 3.25 <#notable_changes_in_nss_3.25>`__
--------------------------------------------------------------

.. container::

   -  An SSL socket can no longer be configured to allow both TLS 1.3 and SSL v3.
   -  Regression fix: NSS no longer reports a failure if an application attempts to disable the SSL
      v2 protocol.
   -  The trusted CA certificate list has been updated to version 2.8.
   -  The following CA certificate was **Removed**

      -  CN = Sonera Class1 CA

         -  SHA-256 Fingerprint:
            CD:80:82:84:CF:74:6F:F2:FD:6E:B5:8A:A1:D5:9C:4A:D4:B3:CA:56:FD:C6:27:4A:89:26:A7:83:5F:32:31:3D

   -  The following CA certificates were **Added**

      -  CN = Hellenic Academic and Research Institutions RootCA 2015

         -  SHA-256 Fingerprint:
            A0:40:92:9A:02:CE:53:B4:AC:F4:F2:FF:C6:98:1C:E4:49:6F:75:5E:6D:45:FE:0B:2A:69:2B:CD:52:52:3F:36

      -  CN = Hellenic Academic and Research Institutions ECC RootCA 2015

         -  SHA-256 Fingerprint:
            44:B5:45:AA:8A:25:E6:5A:73:CA:15:DC:27:FC:36:D2:4C:1C:B9:95:3A:06:65:39:B1:15:82:DC:48:7B:48:33

      -  CN = Certplus Root CA G1

         -  SHA-256 Fingerprint:
            15:2A:40:2B:FC:DF:2C:D5:48:05:4D:22:75:B3:9C:7F:CA:3E:C0:97:80:78:B0:F0:EA:76:E5:61:A6:C7:43:3E

      -  CN = Certplus Root CA G2

         -  SHA-256 Fingerprint:
            6C:C0:50:41:E6:44:5E:74:69:6C:4C:FB:C9:F8:0F:54:3B:7E:AB:BB:44:B4:CE:6F:78:7C:6A:99:71:C4:2F:17

      -  CN = OpenTrust Root CA G1

         -  SHA-256 Fingerprint:
            56:C7:71:28:D9:8C:18:D9:1B:4C:FD:FF:BC:25:EE:91:03:D4:75:8E:A2:AB:AD:82:6A:90:F3:45:7D:46:0E:B4

      -  CN = OpenTrust Root CA G2

         -  SHA-256 Fingerprint:
            27:99:58:29:FE:6A:75:15:C1:BF:E8:48:F9:C4:76:1D:B1:6C:22:59:29:25:7B:F4:0D:08:94:F2:9E:A8:BA:F2

      -  CN = OpenTrust Root CA G3

         -  SHA-256 Fingerprint:
            B7:C3:62:31:70:6E:81:07:8C:36:7C:B8:96:19:8F:1E:32:08:DD:92:69:49:DD:8F:57:09:A4:10:F7:5B:62:92

.. _bugs_fixed_in_nss_3.25:

`Bugs fixed in NSS 3.25 <#bugs_fixed_in_nss_3.25>`__
----------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.25:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.25

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.25 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.25 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).