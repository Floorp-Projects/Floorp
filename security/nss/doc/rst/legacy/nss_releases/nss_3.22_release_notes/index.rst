.. _mozilla_projects_nss_nss_3_22_release_notes:

NSS 3.22 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.22, which is a minor release.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_22_RTM. NSS 3.22 requires NSPR 4.11 or newer.

   NSS 3.22 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_22_RTM/src/

.. _new_in_nss_3.22:

`New in NSS 3.22 <#new_in_nss_3.22>`__
--------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  RSA-PSS signatures are now supported (`bug
      1215295 <https://bugzilla.mozilla.org/show_bug.cgi?id=1215295>`__)

      -  New functions ``PK11_SignWithMechanism()`` and ``PK11_SignWithMechanism()`` are provided to
         allow RSA keys to be used with PSS.

   -  Pseudorandom functions based on hashes other than SHA-1 are now supported with PBKDF (`bug
      554827 <https://bugzilla.mozilla.org/show_bug.cgi?id=554827>`__).

      -  ``PK11_CreatePBEV2AlgorithmID()`` now supports ``SEC_OID_PKCS5_PBKDF2`` with
         ``cipherAlgTag`` and ``prfAlgTag`` set to ``SEC_OID_HMAC_SHA256``, ``SEC_OID_HMAC_SHA224``,
         ``SEC_OID_HMAC_SHA384``, or ``SEC_OID_HMAC_SHA512``.

   -  Enforce an External Policy on NSS from a config file (`bug
      1009429 <https://bugzilla.mozilla.org/show_bug.cgi?id=1009429>`__)

      -  you can now add a config= line to pkcs11.txt (assuming you are using sql databases), which
         will force NSS to restrict the application to certain cryptographic algorithms and
         protocols. A complete list can be found in :ref:`mozilla_projects_nss_nss_config_options`.

   .. rubric:: New Functions
      :name: new_functions

   -  *in pk11pub.h*

      -  **PK11_SignWithMechanism** - This function is an extended version ``PK11_Sign()``.
      -  **PK11_VerifyWithMechanism** - This function is an extended version of ``PK11_Verify()``.

         -  These functions take an explicit mechanism and parameters as arguments rather than
            inferring it from the key type using ``PK11_MapSignKeyType()``.  The mechanism type
            CKM_RSA_PKCS_PSS is now supported for RSA in addition to CKM_RSA_PKCS.  The
            CK_RSA_PKCS_PSS mechanism takes a parameter of type CK_RSA_PKCS_PSS_PARAMS.

   -  *in ssl.h*

      -  **SSL_PeerSignedCertTimestamps** - Get signed_certificate_timestamp TLS extension data
      -  **SSL_SetSignedCertTimestamps** - Set signed_certificate_timestamp TLS extension data

   .. rubric:: New Types
      :name: new_types

   -  *in secoidt.h*

      -  The following are added to SECOidTag:

         -  SEC_OID_AES_128_GCM
         -  SEC_OID_AES_192_GCM
         -  SEC_OID_AES_256_GCM
         -  SEC_OID_IDEA_CBC
         -  SEC_OID_RC2_40_CBC
         -  SEC_OID_DES_40_CBC
         -  SEC_OID_RC4_40
         -  SEC_OID_RC4_56
         -  SEC_OID_NULL_CIPHER
         -  SEC_OID_HMAC_MD5
         -  SEC_OID_TLS_RSA
         -  SEC_OID_TLS_DHE_RSA
         -  SEC_OID_TLS_DHE_DSS
         -  SEC_OID_TLS_DH_RSA
         -  SEC_OID_TLS_DH_DSS
         -  SEC_OID_TLS_DH_ANON
         -  SEC_OID_TLS_ECDHE_ECDSA
         -  SEC_OID_TLS_ECDHE_RSA
         -  SEC_OID_TLS_ECDH_ECDSA
         -  SEC_OID_TLS_ECDH_RSA
         -  SEC_OID_TLS_ECDH_ANON
         -  SEC_OID_TLS_RSA_EXPORT
         -  SEC_OID_TLS_DHE_RSA_EXPORT
         -  SEC_OID_TLS_DHE_DSS_EXPORT
         -  SEC_OID_TLS_DH_RSA_EXPORT
         -  SEC_OID_TLS_DH_DSS_EXPORT
         -  SEC_OID_TLS_DH_ANON_EXPORT
         -  SEC_OID_APPLY_SSL_POLICY

   -  in sslt.h

      -  **ssl_signed_cert_timestamp_xtn** is added to ``SSLExtensionType``.

   .. rubric:: New Macros
      :name: new_macros

   -  in nss.h

      -  NSS_RSA_MIN_KEY_SIZE
      -  NSS_DH_MIN_KEY_SIZE
      -  NSS_DSA_MIN_KEY_SIZE
      -  NSS_TLS_VERSION_MIN_POLICY
      -  NSS_TLS_VERSION_MAX_POLICY
      -  NSS_DTLS_VERSION_MIN_POLICY
      -  NSS_DTLS_VERSION_MAX_POLICY

   -  *in pkcs11t.h*

      -  **CKP_PKCS5_PBKD2_HMAC_GOSTR3411** - PRF based on HMAC with GOSTR3411 for PBKDF (not
         supported)
      -  **CKP_PKCS5_PBKD2_HMAC_SHA224** - PRF based on HMAC with SHA-224 for PBKDF
      -  **CKP_PKCS5_PBKD2_HMAC_SHA256** - PRF based on HMAC with SHA-256 for PBKDF
      -  **CKP_PKCS5_PBKD2_HMAC_SHA384** - PRF based on HMAC with SHA-256 for PBKDF
      -  **CKP_PKCS5_PBKD2_HMAC_SHA512** - PRF based on HMAC with SHA-256 for PBKDF
      -  **CKP_PKCS5_PBKD2_HMAC_SHA512_224** - PRF based on HMAC with SHA-512 truncated to 224 bits
         for PBKDF (not supported)
      -  **CKP_PKCS5_PBKD2_HMAC_SHA512_256** - PRF based on HMAC with SHA-512 truncated to 256 bits
         for PBKDF (not supported)

   -  *in secoidt.h*

      -  NSS_USE_ALG_IN_SSL
      -  NSS_USE_POLICY_IN_SSL

   -  *in ssl.h*

      -  **SSL_ENABLE_SIGNED_CERT_TIMESTAMPS**

   -  *in sslt.h*

      -  **SSL_MAX_EXTENSIONS** is updated to 13

.. _notable_changes_in_nss_3.22:

`Notable Changes in NSS 3.22 <#notable_changes_in_nss_3.22>`__
--------------------------------------------------------------

.. container::

   -  NSS C++ tests are built by default, requiring a C++11 compiler.  Set the NSS_DISABLE_GTESTS
      variable to 1 to disable building these tests.

.. _bugs_fixed_in_nss_3.22:

`Bugs fixed in NSS 3.22 <#bugs_fixed_in_nss_3.22>`__
----------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.22:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.22

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.22 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.22 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).