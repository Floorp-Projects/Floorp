.. _mozilla_projects_nss_nss_3_21_release_notes:

NSS 3.21 release notes
======================

.. container::

   2016-01-07, this page has been updated to include additional information about the release. The
   sections "Security Fixes" and "Acknowledgements" have been added.

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.21, which is a minor release.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_21_RTM. NSS 3.21 requires NSPR 4.10.10 or newer.

   NSS 3.21 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_21_RTM/src/

.. _security_fixes_in_nss_3.21:

`Security Fixes in NSS 3.21 <#security_fixes_in_nss_3.21>`__
------------------------------------------------------------

.. container::

   -  `Bug 1158489 <https://bugzilla.mozilla.org/show_bug.cgi?id=1158489>`__
      ` <https://bugzilla.mozilla.org/show_bug.cgi?id=1138554>`__ /
      `CVE-2015-7575 <http://www.cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2015-7575>`__ - Prevent
      MD5 Downgrade in TLS 1.2 Signatures.

.. _new_in_nss_3.21:

`New in NSS 3.21 <#new_in_nss_3.21>`__
--------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  ``certutil`` now supports a ``--rename`` option to change a nickname (`bug
      1142209 <https://bugzilla.mozilla.org/show_bug.cgi?id=1142209>`__)
   -  TLS extended master secret extension (`RFC
      7627 <https://datatracker.ietf.org/doc/html/rfc7627>`__) is supported (`bug
      1117022 <https://bugzilla.mozilla.org/show_bug.cgi?id=1117022>`__)
   -  New info functions added for use during mid-handshake callbacks (`bug
      1084669 <https://bugzilla.mozilla.org/show_bug.cgi?id=1084669>`__)

   .. rubric:: New Functions
      :name: new_functions

   -  *in nss.h*

      -  **NSS_OptionSet** - sets NSS global options
      -  **NSS_OptionGet** - gets the current value of NSS global options

   -  *in secmod.h*

      -  **SECMOD_CreateModuleEx** - Create a new SECMODModule structure from module name string,
         module parameters string, NSS specific parameters string, and NSS configuration parameter
         string. The module represented by the module structure is not loaded. The difference with
         **SECMOD_CreateModule** is the new function handles NSS configuration parameter strings.

   -  *in ssl.h*

      -  **SSL_GetPreliminaryChannelInfo** - obtains information about a TLS channel prior to the
         handshake being completed, for use with the callbacks that are invoked during the handshake
      -  **SSL_SignaturePrefSet** - configures the enabled signature and hash algorithms for TLS
      -  **SSL_SignaturePrefGet** - retrieves the currently configured signature and hash algorithms
      -  **SSL_SignatureMaxCount** - obtains the maximum number signature algorithms that can be
         configured with **SSL_SignaturePrefSet**

   -  *in utilpars.h*

      -  **NSSUTIL_ArgParseModuleSpecEx** - takes a module spec and breaks it into shared library
         string, module name string, module parameters string, NSS specific parameters string, and
         NSS configuration parameter strings. The returned strings must be freed by the caller. The
         difference with **NSS_ArgParseModuleSpec** is the new function handles NSS configuration
         parameter strings.
      -  **NSSUTIL_MkModuleSpecEx** - take a shared library string, module name string, module
         parameters string, NSS specific parameters string, and NSS configuration parameter string
         and returns a module string which the caller must free when it is done. The difference with
         **NSS_MkModuleSpec** is the new function handles NSS configuration parameter strings.

   .. rubric:: New Types
      :name: new_types

   -  *in pkcs11t.h*

      -  **CK_TLS12_MASTER_KEY_DERIVE_PARAMS{_PTR}** - parameters {or pointer} for
         **CKM_TLS12_MASTER_KEY_DERIVE**
      -  **CK_TLS12_KEY_MAT_PARAMS{_PTR}** - parameters {or pointer} for
         **CKM_TLS12_KEY_AND_MAC_DERIVE**
      -  **CK_TLS_KDF_PARAMS{_PTR}** - parameters {or pointer} for **CKM_TLS_KDF**
      -  **CK_TLS_MAC_PARAMS{_PTR}** - parameters {or pointer} for **CKM_TLS_MAC**

   -  *in sslt.h*

      -  **SSLHashType** - identifies a hash function
      -  **SSLSignatureAndHashAlg** - identifies a signature and hash function
      -  **SSLPreliminaryChannelInfo** - provides information about the session state prior to
         handshake completion

   .. rubric:: New Macros
      :name: new_macros

   -  *in nss.h*

      -  **NSS_RSA_MIN_KEY_SIZE** - used with NSS_OptionSet and NSS_OptionGet to set or get the
         minimum RSA key size
      -  **NSS_DH_MIN_KEY_SIZE** - used with NSS_OptionSet and NSS_OptionGet to set or get the
         minimum DH key size
      -  **NSS_DSA_MIN_KEY_SIZE** - used with NSS_OptionSet and NSS_OptionGet to set or get the
         minimum DSA key size

   -  *in pkcs11t.h*

      -  **CKM_TLS12_MASTER_KEY_DERIVE** - derives TLS 1.2 master secret
      -  **CKM_TLS12_KEY_AND_MAC_DERIVE** - derives TLS 1.2 traffic key and IV
      -  **CKM_TLS12_MASTER_KEY_DERIVE_DH** - derives TLS 1.2 master secret for DH (and ECDH) cipher
         suites
      -  **CKM_TLS12_KEY_SAFE_DERIVE** and **CKM_TLS_KDF** are identifiers for additional PKCS#12
         mechanisms for TLS 1.2 that are currently unused in NSS.
      -  **CKM_TLS_MAC** - computes TLS Finished MAC

   -  *in secoidt.h*

      -  **NSS_USE_ALG_IN_SSL_KX** - policy flag indicating that keys are used in TLS key exchange

   -  *in sslerr.h*

      -  **SSL_ERROR_RX_SHORT_DTLS_READ** - error code for failure to include a complete DTLS record
         in a UDP packet
      -  **SSL_ERROR_NO_SUPPORTED_SIGNATURE_ALGORITHM** - error code for when no valid signature and
         hash algorithm is available
      -  **SSL_ERROR_UNSUPPORTED_SIGNATURE_ALGORITHM** - error code for when an unsupported
         signature and hash algorithm is configured
      -  **SSL_ERROR_MISSING_EXTENDED_MASTER_SECRET** - error code for when the extended master
         secret is missing after having been negotiated
      -  **SSL_ERROR_UNEXPECTED_EXTENDED_MASTER_SECRET** - error code for receiving an extended
         master secret when previously not negotiated

   -  *in sslt.h*

      -  **SSL_ENABLE_EXTENDED_MASTER_SECRET** - configuration to enable the TLS extended master
         secret extension (`RFC 7627 <https://datatracker.ietf.org/doc/html/rfc7627>`__)
      -  **ssl_preinfo_version** - used with **SSLPreliminaryChannelInfo** to indicate that a TLS
         version has been selected
      -  **ssl_preinfo_cipher_suite** - used with **SSLPreliminaryChannelInfo** to indicate that a
         TLS cipher suite has been selected
      -  **ssl_preinfo_all** - used with **SSLPreliminaryChannelInfo** to indicate that all
         preliminary information has been set

.. _notable_changes_in_nss_3.21:

`Notable Changes in NSS 3.21 <#notable_changes_in_nss_3.21>`__
--------------------------------------------------------------

.. container::

   -  NSS now builds with elliptic curve ciphers enabled by default (`bug
      1205688 <https://bugzilla.mozilla.org/show_bug.cgi?id=1205688>`__)
   -  NSS now builds with warnings as errors (`bug
      1182667 <https://bugzilla.mozilla.org/show_bug.cgi?id=1182667>`__)
   -  The following CA certificates were **Removed**

      -  CN = VeriSign Class 4 Public Primary Certification Authority - G3

         -  SHA1 Fingerprint: C8:EC:8C:87:92:69:CB:4B:AB:39:E9:8D:7E:57:67:F3:14:95:73:9D

      -  CN = UTN-USERFirst-Network Applications

         -  SHA1 Fingerprint: 5D:98:9C:DB:15:96:11:36:51:65:64:1B:56:0F:DB:EA:2A:C2:3E:F1

      -  CN = TC TrustCenter Universal CA III

         -  SHA1 Fingerprint: 96:56:CD:7B:57:96:98:95:D0:E1:41:46:68:06:FB:B8:C6:11:06:87

      -  CN = A-Trust-nQual-03

         -  SHA-1 Fingerprint: D3:C0:63:F2:19:ED:07:3E:34:AD:5D:75:0B:32:76:29:FF:D5:9A:F2

      -  CN = USERTrust Legacy Secure Server CA

         -  SHA-1 Fingerprint: 7C:2F:91:E2:BB:96:68:A9:C6:F6:BD:10:19:2C:6B:52:5A:1B:BA:48

      -  Friendly Name: Digital Signature Trust Co. Global CA 1

         -  SHA-1 Fingerprint: 81:96:8B:3A:EF:1C:DC:70:F5:FA:32:69:C2:92:A3:63:5B:D1:23:D3

      -  Friendly Name: Digital Signature Trust Co. Global CA 3

         -  SHA-1 Fingerprint: AB:48:F3:33:DB:04:AB:B9:C0:72:DA:5B:0C:C1:D0:57:F0:36:9B:46

      -  CN = UTN - DATACorp SGC

         -  SHA-1 Fingerprint: 58:11:9F:0E:12:82:87:EA:50:FD:D9:87:45:6F:4F:78:DC:FA:D6:D4

      -  O = TÜRKTRUST Bilgi İletişim ve Bilişim Güvenliği Hizmetleri A.Ş. (c) Kasım 2005

         -  SHA-1 Fingerprint: B4:35:D4:E1:11:9D:1C:66:90:A7:49:EB:B3:94:BD:63:7B:A7:82:B7

   -  The following CA certificate had the Websites **trust bit turned off**

      -  OU = Equifax Secure Certificate Authority

         -  SHA1 Fingerprint: D2:32:09:AD:23:D3:14:23:21:74:E4:0D:7F:9D:62:13:97:86:63:3A

   -  The following CA certificates were **Added**

      -  CN = Certification Authority of WoSign G2

         -  SHA1 Fingerprint: FB:ED:DC:90:65:B7:27:20:37:BC:55:0C:9C:56:DE:BB:F2:78:94:E1

      -  CN = CA WoSign ECC Root

         -  SHA1 Fingerprint: D2:7A:D2:BE:ED:94:C0:A1:3C:C7:25:21:EA:5D:71:BE:81:19:F3:2B

      -  CN = OISTE WISeKey Global Root GB CA

         -  SHA1 Fingerprint: 0F:F9:40:76:18:D3:D7:6A:4B:98:F0:A8:35:9E:0C:FD:27:AC:CC:ED

   -  The version number of the updated root CA list has been set to 2.6

.. _bugs_fixed_in_nss_3.21:

`Bugs fixed in NSS 3.21 <#bugs_fixed_in_nss_3.21>`__
----------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.21:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.21

`Acknowledgements <#acknowledgements>`__
----------------------------------------

.. container::

   The NSS development team would like to thank Karthikeyan Bhargavan from
   `INRIA <http://inria.fr/>`__ for responsibly disclosing the issue in `Bug
   1158489 <https://bugzilla.mozilla.org/show_bug.cgi?id=1158489>`__.

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.21 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.21 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).