.. _mozilla_projects_nss_nss_3_12_release_notes_html:

NSS_3.12_release_notes.html
===========================

.. _nss_3.12_release_notes:

`NSS 3.12 Release Notes <#nss_3.12_release_notes>`__
----------------------------------------------------

.. container::

.. _17_june_2008:

`17 June 2008 <#17_june_2008>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Newsgroup: `mozilla.dev.tech.crypto <news://news.mozilla.org/mozilla.dev.tech.crypto>`__

`Contents <#contents>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `Introduction <#introduction>`__
   -  `Distribution Information <#distribution_information>`__
   -  `New in NSS 3.12 <#new_in_nss_3.12>`__
   -  `Bugs Fixed <#bugs_fixed>`__
   -  `Documentation <#documentation>`__
   -  `Compatibility <#compatibility>`__
   -  `Feedback <#feedback>`__

   --------------

`Introduction <#introduction>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Network Security Services (NSS) 3.12 is a minor release with the following new features:

   -  SQLite-Based Shareable Certificate and Key Databases
   -  libpkix: an RFC 3280 Compliant Certificate Path Validation Library
   -  Camellia cipher support
   -  TLS session ticket extension (RFC 5077)

   NSS 3.12 is tri-licensed under the MPL 1.1/GPL 2.0/LGPL 2.1.
   Note: Firefox 3 uses NSS 3.12, but not the new SQLite-based shareable certificate and key
   databases. We missed the deadline to enable that feature in Firefox 3.

   --------------



`Distribution Information <#distribution_information>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The CVS tag for the NSS 3.12 release is NSS_3_12_RTM. NSS 3.12 requires `NSPR
   4.7.1 <https://www.mozilla.org/projects/nspr/release-notes/nspr471.html>`__.
   See the `Documentation <#docs>`__ section for the build instructions.
   NSS 3.12 source and binary distributions are also available on ftp.mozilla.org for secure HTTPS
   download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_12_RTM/src/.
   -  Binary distributions:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_12_RTM/. Both debug and
      optimized builds are provided. Go to the subdirectory for your platform, DBG (debug) or OPT
      (optimized), to get the tar.gz or zip file. The tar.gz or zip file expands to an nss-3.12
      directory containing three subdirectories:

      -  include - NSS header files
      -  lib - NSS shared libraries
      -  bin - `NSS Tools <https://www.mozilla.org/projects/security/pki/nss/tools/>`__ and test
         programs

   You also need to download the NSPR 4.7.1 binary distributions to get the NSPR 4.7.1 header files
   and shared libraries, which NSS 3.12 requires. NSPR 4.7.1 binary distributions are in
   https://ftp.mozilla.org/pub/mozilla.org/nspr/releases/v4.7.1/.
   NSS 3.12 libraries have the following versions:

   -  sqlite3: 3.3.17
   -  nssckbi: 1.70
   -  softokn3 and freebl3: 3.12.0.3
   -  other NSS libraries: 3.12.0.3

   --------------

.. _new_in_nss_3.12:

`New in NSS 3.12 <#new_in_nss_3.12>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  3 new shared library are shipped with NSS 3.12:

      -  nssutil
      -  sqlite
      -  nssdbm

   -  1 new include file is shipped with NSS3.12:

      -  utilrename.h

   -  New functions in the nss shared library:

      -  CERT_CheckNameSpace (see cert.h)
      -  CERT_EncodeCertPoliciesExtension (see cert.h)
      -  CERT_EncodeInfoAccessExtension (see cert.h)
      -  CERT_EncodeInhibitAnyExtension (see cert.h)
      -  CERT_EncodeNoticeReference (see cert.h)
      -  CERT_EncodePolicyConstraintsExtension (see cert.h)
      -  CERT_EncodePolicyMappingExtension (see cert.h)
      -  CERT_EncodeSubjectKeyID (see certdb/cert.h)
      -  CERT_EncodeUserNotice (see cert.h)
      -  CERT_FindCRLEntryReasonExten (see cert.h)
      -  CERT_FindCRLNumberExten (see cert.h)
      -  CERT_FindNameConstraintsExten (see cert.h)
      -  CERT_GetClassicOCSPDisabledPolicy (see cert.h)
      -  CERT_GetClassicOCSPEnabledHardFailurePolicy (see cert.h)
      -  CERT_GetClassicOCSPEnabledSoftFailurePolicy (see cert.h)
      -  CERT_GetPKIXVerifyNistRevocationPolicy (see cert.h)
      -  CERT_GetUsePKIXForValidation (see cert.h)
      -  CERT_GetValidDNSPatternsFromCert (see cert.h)
      -  CERT_NewTempCertificate (see cert.h)
      -  CERT_SetOCSPTimeout (see certhigh/ocsp.h)
      -  CERT_SetUsePKIXForValidation (see cert.h)
      -  CERT_PKIXVerifyCert (see cert.h)
      -  HASH_GetType (see sechash.h)
      -  NSS_InitWithMerge (see nss.h)
      -  PK11_CreateMergeLog (see pk11pub.h)
      -  PK11_CreateGenericObject (see pk11pub.h)
      -  PK11_CreatePBEV2AlgorithmID (see pk11pub.h)
      -  PK11_DestroyMergeLog (see pk11pub.h)
      -  PK11_GenerateKeyPairWithOpFlags (see pk11pub.h)
      -  PK11_GetPBECryptoMechanism (see pk11pub.h)
      -  PK11_IsRemovable (see pk11pub.h)
      -  PK11_MergeTokens (see pk11pub.h)
      -  PK11_WriteRawAttribute (see pk11pub.h)
      -  SECKEY_ECParamsToBasePointOrderLen (see keyhi.h)
      -  SECKEY_ECParamsToKeySize (see keyhi.h)
      -  SECMOD_DeleteModuleEx (see secmod.h)
      -  SEC_GetRegisteredHttpClient (see ocsp.h)
      -  SEC_PKCS5IsAlgorithmPBEAlgTag (see secpkcs5.h)
      -  VFY_CreateContextDirect (see cryptohi.h)
      -  VFY_CreateContextWithAlgorithmID (see cryptohi.h)
      -  VFY_VerifyDataDirect (see cryptohi.h)
      -  VFY_VerifyDataWithAlgorithmID (see cryptohi.h)
      -  VFY_VerifyDigestDirect (see cryptohi.h)
      -  VFY_VerifyDigestWithAlgorithmID (see cryptohi.h)

   -  New macros for Camellia support (see blapit.h):

      -  NSS_CAMELLIA
      -  NSS_CAMELLIA_CBC
      -  CAMELLIA_BLOCK_SIZE

   -  New macros for RSA (see blapit.h):

      -  RSA_MAX_MODULUS_BITS
      -  RSA_MAX_EXPONENT_BITS

   -  New macros in certt.h:

      -  X.509 v3

         -  KU_ENCIPHER_ONLY
         -  CERT_MAX_SERIAL_NUMBER_BYTES
         -  CERT_MAX_DN_BYTES

      -  PKIX

         -  CERT_REV_M_DO_NOT_TEST_USING_THIS_METHOD
         -  CERT_REV_M_TEST_USING_THIS_METHOD
         -  CERT_REV_M_ALLOW_NETWORK_FETCHING
         -  CERT_REV_M_FORBID_NETWORK_FETCHING
         -  CERT_REV_M_ALLOW_IMPLICIT_DEFAULT_SOURCE
         -  CERT_REV_M_IGNORE_IMPLICIT_DEFAULT_SOURCE
         -  CERT_REV_M_SKIP_TEST_ON_MISSING_SOURCE
         -  CERT_REV_M_REQUIRE_INFO_ON_MISSING_SOURCE
         -  CERT_REV_M_IGNORE_MISSING_FRESH_INFO
         -  CERT_REV_M_FAIL_ON_MISSING_FRESH_INFO
         -  CERT_REV_M_STOP_TESTING_ON_FRESH_INFO
         -  CERT_REV_M_CONTINUE_TESTING_ON_FRESH_INFO
         -  CERT_REV_MI_TEST_EACH_METHOD_SEPARATELY
         -  CERT_REV_MI_TEST_ALL_LOCAL_INFORMATION_FIRST
         -  CERT_REV_MI_NO_OVERALL_INFO_REQUIREMENT
         -  CERT_REV_MI_REQUIRE_SOME_FRESH_INFO_AVAILABLE
         -  CERT_POLICY_FLAG_NO_MAPPING
         -  CERT_POLICY_FLAG_EXPLICIT
         -  CERT_POLICY_FLAG_NO_ANY
         -  CERT_ENABLE_LDAP_FETCH
         -  CERT_ENABLE_HTTP_FETCH

   -  New macro in utilrename.h:

      -  SMIME_AES_CBC_128

   -  The nssckbi PKCS #11 module's version changed to 1.70.
   -  In pkcs11n.h, all the \_NETSCAPE\_ macros are renamed with \_NSS\_

      -  For example, CKO_NETSCAPE_CRL becomes CKO_NSS_CRL.

   -  New for PKCS #11 (see pkcs11t.h for details):

      -  CKK: Keys

         -  CKK_CAMELLIA

      -  CKM: Mechanisms

         -  CKM_SHA224_RSA_PKCS
         -  CKM_SHA224_RSA_PKCS_PSS
         -  CKM_SHA224
         -  CKM_SHA224_HMAC
         -  CKM_SHA224_HMAC_GENERAL
         -  CKM_SHA224_KEY_DERIVATION
         -  CKM_CAMELLIA_KEY_GEN
         -  CKM_CAMELLIA_ECB
         -  CKM_CAMELLIA_CBC
         -  CKM_CAMELLIA_MAC
         -  CKM_CAMELLIA_MAC_GENERAL
         -  CKM_CAMELLIA_CBC_PAD
         -  CKM_CAMELLIA_ECB_ENCRYPT_DATA
         -  CKM_CAMELLIA_CBC_ENCRYPT_DATA

      -  CKG: MFGs

         -  CKG_MGF1_SHA224

   -  New error codes (see secerr.h):

      -  SEC_ERROR_NOT_INITIALIZED
      -  SEC_ERROR_TOKEN_NOT_LOGGED_IN
      -  SEC_ERROR_OCSP_RESPONDER_CERT_INVALID
      -  SEC_ERROR_OCSP_BAD_SIGNATURE
      -  SEC_ERROR_OUT_OF_SEARCH_LIMITS
      -  SEC_ERROR_INVALID_POLICY_MAPPING
      -  SEC_ERROR_POLICY_VALIDATION_FAILED
      -  SEC_ERROR_UNKNOWN_AIA_LOCATION_TYPE
      -  SEC_ERROR_BAD_HTTP_RESPONSE
      -  SEC_ERROR_BAD_LDAP_RESPONSE
      -  SEC_ERROR_FAILED_TO_ENCODE_DATA
      -  SEC_ERROR_BAD_INFO_ACCESS_LOCATION
      -  SEC_ERROR_LIBPKIX_INTERNAL

   -  New mechanism flags (see secmod.h)

      -  PUBLIC_MECH_AES_FLAG
      -  PUBLIC_MECH_SHA256_FLAG
      -  PUBLIC_MECH_SHA512_FLAG
      -  PUBLIC_MECH_CAMELLIA_FLAG

   -  New OIDs (see secoidt.h)

      -  new EC Signature oids

         -  SEC_OID_ANSIX962_ECDSA_SIGNATURE_RECOMMENDED_DIGEST
         -  SEC_OID_ANSIX962_ECDSA_SIGNATURE_SPECIFIED_DIGEST
         -  SEC_OID_ANSIX962_ECDSA_SHA224_SIGNATURE
         -  SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE
         -  SEC_OID_ANSIX962_ECDSA_SHA384_SIGNATURE
         -  SEC_OID_ANSIX962_ECDSA_SHA512_SIGNATURE

      -  More id-ce and id-pe OIDs from RFC 3280

         -  SEC_OID_X509_HOLD_INSTRUCTION_CODE
         -  SEC_OID_X509_DELTA_CRL_INDICATOR
         -  SEC_OID_X509_ISSUING_DISTRIBUTION_POINT
         -  SEC_OID_X509_CERT_ISSUER
         -  SEC_OID_X509_FRESHEST_CRL
         -  SEC_OID_X509_INHIBIT_ANY_POLICY
         -  SEC_OID_X509_SUBJECT_INFO_ACCESS

      -  Camellia OIDs (RFC3657)

         -  SEC_OID_CAMELLIA_128_CBC
         -  SEC_OID_CAMELLIA_192_CBC
         -  SEC_OID_CAMELLIA_256_CBC

      -  PKCS 5 V2 OIDS

         -  SEC_OID_PKCS5_PBKDF2
         -  SEC_OID_PKCS5_PBES2
         -  SEC_OID_PKCS5_PBMAC1
         -  SEC_OID_HMAC_SHA1
         -  SEC_OID_HMAC_SHA224
         -  SEC_OID_HMAC_SHA256
         -  SEC_OID_HMAC_SHA384
         -  SEC_OID_HMAC_SHA512
         -  SEC_OID_PKIX_TIMESTAMPING
         -  SEC_OID_PKIX_CA_REPOSITORY
         -  SEC_OID_ISO_SHA1_WITH_RSA_SIGNATURE

   -  Changed OIDs (see secoidt.h)

      -  SEC_OID_PKCS12_KEY_USAGE changed to SEC_OID_BOGUS_KEY_USAGE
      -  SEC_OID_ANSIX962_ECDSA_SIGNATURE_WITH_SHA1_DIGEST changed to
         SEC_OID_ANSIX962_ECDSA_SHA1_SIGNATURE
      -  Note: SEC_OID_ANSIX962_ECDSA_SIGNATURE_WITH_SHA1_DIGEST is also kept for compatibility
         reasons.

   -  TLS Session ticket extension (off by default)

      -  See SSL_ENABLE_SESSION_TICKETS in ssl.h

   -  New SSL error codes (see sslerr.h)

      -  SSL_ERROR_UNSUPPORTED_EXTENSION_ALERT
      -  SSL_ERROR_CERTIFICATE_UNOBTAINABLE_ALERT
      -  SSL_ERROR_UNRECOGNIZED_NAME_ALERT
      -  SSL_ERROR_BAD_CERT_STATUS_RESPONSE_ALERT
      -  SSL_ERROR_BAD_CERT_HASH_VALUE_ALERT
      -  SSL_ERROR_RX_UNEXPECTED_NEW_SESSION_TICKET
      -  SSL_ERROR_RX_MALFORMED_NEW_SESSION_TICKET

   -  New TLS cipher suites (see sslproto.h):

      -  TLS_RSA_WITH_CAMELLIA_128_CBC_SHA
      -  TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA
      -  TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA
      -  TLS_RSA_WITH_CAMELLIA_256_CBC_SHA
      -  TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA
      -  TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA

   -  Note: the following TLS cipher suites are declared but are not yet implemented:

      -  TLS_DH_DSS_WITH_CAMELLIA_128_CBC_SHA
      -  TLS_DH_RSA_WITH_CAMELLIA_128_CBC_SHA
      -  TLS_DH_ANON_WITH_CAMELLIA_128_CBC_SHA
      -  TLS_DH_DSS_WITH_CAMELLIA_256_CBC_SHA
      -  TLS_DH_RSA_WITH_CAMELLIA_256_CBC_SHA
      -  TLS_DH_ANON_WITH_CAMELLIA_256_CBC_SHA
      -  TLS_ECDH_anon_WITH_NULL_SHA
      -  TLS_ECDH_anon_WITH_RC4_128_SHA
      -  TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA
      -  TLS_ECDH_anon_WITH_AES_128_CBC_SHA
      -  TLS_ECDH_anon_WITH_AES_256_CBC_SHA

   --------------

.. _bugs_fixed:

`Bugs Fixed <#bugs_fixed>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The following bugs have been fixed in NSS 3.12.

   -  `Bug 354403 <https://bugzilla.mozilla.org/show_bug.cgi?id=354403>`__: nssList_CreateIterator
      returns pointer to a freed memory if the function fails to allocate a lock
   -  `Bug 399236 <https://bugzilla.mozilla.org/show_bug.cgi?id=399236>`__: pkix wrapper must print
      debug output into stderr
   -  `Bug 399300 <https://bugzilla.mozilla.org/show_bug.cgi?id=399300>`__: PKIX error results not
      freed after use.
   -  `Bug 414985 <https://bugzilla.mozilla.org/show_bug.cgi?id=414985>`__: Crash in
      pkix_pl_OcspRequest_Destroy
   -  `Bug 421870 <https://bugzilla.mozilla.org/show_bug.cgi?id=421870>`__: Strsclnt crashed in PKIX
      tests.
   -  `Bug 429388 <https://bugzilla.mozilla.org/show_bug.cgi?id=429388>`__: vfychain.main leaks
      memory
   -  `Bug 396044 <https://bugzilla.mozilla.org/show_bug.cgi?id=396044>`__: Warning: usage of
      uninitialized variable in ckfw/object.c(174)
   -  `Bug 396045 <https://bugzilla.mozilla.org/show_bug.cgi?id=396045>`__: Warning: usage of
      uninitialized variable in ckfw/mechanism.c(719)
   -  `Bug 401986 <https://bugzilla.mozilla.org/show_bug.cgi?id=401986>`__: Mac OS X leopard build
      failure in legacydb
   -  `Bug 325805 <https://bugzilla.mozilla.org/show_bug.cgi?id=325805>`__: diff considers
      mozilla/security/nss/cmd/pk11util/scripts/pkey a binary file
   -  `Bug 385151 <https://bugzilla.mozilla.org/show_bug.cgi?id=385151>`__: Remove the link time
      dependency from NSS to Softoken
   -  `Bug 387892 <https://bugzilla.mozilla.org/show_bug.cgi?id=387892>`__: Add Entrust root CA
      certificate(s) to NSS
   -  `Bug 433386 <https://bugzilla.mozilla.org/show_bug.cgi?id=433386>`__: when system clock is off
      by more than two days, OSCP check fails, can result in crash if user tries to view certificate
      [[@ SECITEM_CompareItem_Util] [[@ memcmp]
   -  `Bug 396256 <https://bugzilla.mozilla.org/show_bug.cgi?id=396256>`__: certutil and pp do not
      print all the GeneralNames in a CRLDP extension
   -  `Bug 398019 <https://bugzilla.mozilla.org/show_bug.cgi?id=398019>`__: correct confusing and
      erroneous comments in DER_AsciiToTime
   -  `Bug 422866 <https://bugzilla.mozilla.org/show_bug.cgi?id=422866>`__: vfychain -pp command
      crashes in NSS_shutdown
   -  `Bug 345779 <https://bugzilla.mozilla.org/show_bug.cgi?id=345779>`__: Useless assignment
      statements in ec_GF2m_pt_mul_mont
   -  `Bug 349011 <https://bugzilla.mozilla.org/show_bug.cgi?id=349011>`__: please stop exporting
      these crmf\_ symbols
   -  `Bug 397178 <https://bugzilla.mozilla.org/show_bug.cgi?id=397178>`__: Crash when entering
      chrome://pippki/content/resetpassword.xul in URL bar
   -  `Bug 403822 <https://bugzilla.mozilla.org/show_bug.cgi?id=403822>`__:
      pkix_pl_OcspRequest_Create can leave some members uninitialized
   -  `Bug 403910 <https://bugzilla.mozilla.org/show_bug.cgi?id=403910>`__:
      CERT_FindUserCertByUsage() returns wrong certificate if multiple certs with same subject
      available
   -  `Bug 404919 <https://bugzilla.mozilla.org/show_bug.cgi?id=404919>`__: memory leak in
      sftkdb_ReadSecmodDB() (sftkmod.c)
   -  `Bug 406120 <https://bugzilla.mozilla.org/show_bug.cgi?id=406120>`__: Allow application to
      specify OCSP timeout
   -  `Bug 361025 <https://bugzilla.mozilla.org/show_bug.cgi?id=361025>`__: Support for Camellia
      Cipher Suites to TLS RFC4132
   -  `Bug 376417 <https://bugzilla.mozilla.org/show_bug.cgi?id=376417>`__: PK11_GenerateKeyPair
      needs to get the key usage from the caller.
   -  `Bug 391291 <https://bugzilla.mozilla.org/show_bug.cgi?id=391291>`__: Shared Database
      Integrity checks not yet implemented.
   -  `Bug 391292 <https://bugzilla.mozilla.org/show_bug.cgi?id=391292>`__: Shared Database
      implementation slow
   -  `Bug 391294 <https://bugzilla.mozilla.org/show_bug.cgi?id=391294>`__: Shared Database
      implementation really slow on network file systems
   -  `Bug 392521 <https://bugzilla.mozilla.org/show_bug.cgi?id=392521>`__: Automatic shared db
      update fails if user opens database R/W but never supplies a password
   -  `Bug 392522 <https://bugzilla.mozilla.org/show_bug.cgi?id=392522>`__: Integrity hashes must be
      updated when passwords are changed.
   -  `Bug 401610 <https://bugzilla.mozilla.org/show_bug.cgi?id=401610>`__: Shared DB fails on IOPR
      tests
   -  `Bug 388120 <https://bugzilla.mozilla.org/show_bug.cgi?id=388120>`__: build error due to
      SEC_BEGIN_PROTOS / SEC_END_PROTOS are undefined
   -  `Bug 415264 <https://bugzilla.mozilla.org/show_bug.cgi?id=415264>`__: Make Security use of new
      NSPR rotate macros
   -  `Bug 317052 <https://bugzilla.mozilla.org/show_bug.cgi?id=317052>`__: lib/base/whatnspr.c is
      obsolete
   -  `Bug 317323 <https://bugzilla.mozilla.org/show_bug.cgi?id=317323>`__: Set NSPR31_LIB_PREFIX to
      empty explicitly for WIN95 and WINCE builds
   -  `Bug 320336 <https://bugzilla.mozilla.org/show_bug.cgi?id=320336>`__: SECITEM_AllocItem
      returns a non-NULL pointer if the allocation of its 'data' buffer fails
   -  `Bug 327529 <https://bugzilla.mozilla.org/show_bug.cgi?id=327529>`__: Can't pass 0 as an
      unnamed null pointer argument to CERT_CreateRDN
   -  `Bug 334683 <https://bugzilla.mozilla.org/show_bug.cgi?id=334683>`__: Extraneous semicolons
      cause Empty declaration compiler warnings
   -  `Bug 335275 <https://bugzilla.mozilla.org/show_bug.cgi?id=335275>`__: Compile with the GCC
      flag -Werror-implicit-function-declaration
   -  `Bug 354565 <https://bugzilla.mozilla.org/show_bug.cgi?id=354565>`__: fipstest sha_test needs
      to detect SHA tests that are incorrectly configured for BIT oriented implementations
   -  `Bug 356595 <https://bugzilla.mozilla.org/show_bug.cgi?id=356595>`__: On Windows,
      RNG_SystemInfoForRNG calls GetCurrentProcess, which returns the constant (HANDLE)-1.
   -  `Bug 357015 <https://bugzilla.mozilla.org/show_bug.cgi?id=357015>`__: On Windows,
      ReadSystemFiles reads 21 files as opposed to 10 files in C:\WINDOWS\system32.
   -  `Bug 361076 <https://bugzilla.mozilla.org/show_bug.cgi?id=361076>`__: Clean up the
      USE_PTHREADS related code in coreconf/SunOS5.mk.
   -  `Bug 361077 <https://bugzilla.mozilla.org/show_bug.cgi?id=361077>`__: Clean up the
      USE_PTHREADS related code in coreconf/HP-UX*.mk.
   -  `Bug 402114 <https://bugzilla.mozilla.org/show_bug.cgi?id=402114>`__: Fix the incorrect
      function prototypes of SSL handshake callbacks
   -  `Bug 402308 <https://bugzilla.mozilla.org/show_bug.cgi?id=402308>`__: Fix miscellaneous
      compiler warnings in nss/cmd
   -  `Bug 402777 <https://bugzilla.mozilla.org/show_bug.cgi?id=402777>`__: lib/util can't be built
      stand-alone.
   -  `Bug 407866 <https://bugzilla.mozilla.org/show_bug.cgi?id=407866>`__: Contributed improvement
      to security/nss/lib/freebl/mpi/mp_comba.c
   -  `Bug 410587 <https://bugzilla.mozilla.org/show_bug.cgi?id=410587>`__: SSL_GetChannelInfo
      returns SECSuccess on invalid arguments
   -  `Bug 416508 <https://bugzilla.mozilla.org/show_bug.cgi?id=416508>`__: Fix a \_MSC_VER typo in
      sha512.c, and use SEC_BEGIN_PROTOS/SEC_END_PROTOS in secport.h
   -  `Bug 419242 <https://bugzilla.mozilla.org/show_bug.cgi?id=419242>`__: 'all' is not the default
      makefile target in lib/softoken and lib/softoken/legacydb
   -  `Bug 419523 <https://bugzilla.mozilla.org/show_bug.cgi?id=419523>`__: Export
      Cert_NewTempCertificate.
   -  `Bug 287061 <https://bugzilla.mozilla.org/show_bug.cgi?id=287061>`__: CRL number should be a
      big integer, not ulong
   -  `Bug 301213 <https://bugzilla.mozilla.org/show_bug.cgi?id=301213>`__: Combine internal libpkix
      function tests into a single statically linked program
   -  `Bug 324740 <https://bugzilla.mozilla.org/show_bug.cgi?id=324740>`__: add generation of SIA
      and AIA extensions to certutil
   -  `Bug 339737 <https://bugzilla.mozilla.org/show_bug.cgi?id=339737>`__: LIBPKIX OCSP checking
      calls CERT_VerifyCert
   -  `Bug 358785 <https://bugzilla.mozilla.org/show_bug.cgi?id=358785>`__: Merge NSS_LIBPKIX_BRANCH
      back to trunk
   -  `Bug 365966 <https://bugzilla.mozilla.org/show_bug.cgi?id=365966>`__: infinite recursive call
      in VFY_VerifyDigestDirect
   -  `Bug 382078 <https://bugzilla.mozilla.org/show_bug.cgi?id=382078>`__: pkix default http client
      returns error when try to get an ocsp response.
   -  `Bug 384926 <https://bugzilla.mozilla.org/show_bug.cgi?id=384926>`__: libpkix build problems
   -  `Bug 389411 <https://bugzilla.mozilla.org/show_bug.cgi?id=389411>`__: Mingw build error -
      undefined reference to \`_imp__PKIX_ERRORNAMES'
   -  `Bug 389904 <https://bugzilla.mozilla.org/show_bug.cgi?id=389904>`__: avoid multiple
      decoding/encoding while creating and using PKIX_PL_X500Name
   -  `Bug 390209 <https://bugzilla.mozilla.org/show_bug.cgi?id=390209>`__: pkix AIA manager tries
      to get certs using AIA url with OCSP access method
   -  `Bug 390233 <https://bugzilla.mozilla.org/show_bug.cgi?id=390233>`__: umbrella bug for libPKIX
      cert validation failures discovered from running vfyserv
   -  `Bug 390499 <https://bugzilla.mozilla.org/show_bug.cgi?id=390499>`__: libpkix does not check
      cached cert chain for revocation
   -  `Bug 390502 <https://bugzilla.mozilla.org/show_bug.cgi?id=390502>`__: libpkix fails cert
      validation when no valid CRL (NIST validation policy is always enforced)
   -  `Bug 390530 <https://bugzilla.mozilla.org/show_bug.cgi?id=390530>`__: libpkix does not support
      time override
   -  `Bug 390536 <https://bugzilla.mozilla.org/show_bug.cgi?id=390536>`__: Cert validation
      functions must validate leaf cert themselves
   -  `Bug 390554 <https://bugzilla.mozilla.org/show_bug.cgi?id=390554>`__: all PKIX_NULLCHECK\_
      errors are reported as PKIX ALLOC ERROR
   -  `Bug 390888 <https://bugzilla.mozilla.org/show_bug.cgi?id=390888>`__: CERT_Verify\* functions
      should be able to use libPKIX
   -  `Bug 391457 <https://bugzilla.mozilla.org/show_bug.cgi?id=391457>`__: libpkix does not check
      for object ref leak at shutdown
   -  `Bug 391774 <https://bugzilla.mozilla.org/show_bug.cgi?id=391774>`__: PKIX_Shutdown is not
      called by nssinit.c
   -  `Bug 393174 <https://bugzilla.mozilla.org/show_bug.cgi?id=393174>`__: Memory leaks in
      ocspclnt/PKIX.
   -  `Bug 395093 <https://bugzilla.mozilla.org/show_bug.cgi?id=395093>`__:
      pkix_pl_HttpCertStore_ProcessCertResponse is unable to process certs in DER format
   -  `Bug 395224 <https://bugzilla.mozilla.org/show_bug.cgi?id=395224>`__: Don't reject certs with
      critical NetscapeCertType extensions in libPKIX
   -  `Bug 395427 <https://bugzilla.mozilla.org/show_bug.cgi?id=395427>`__: PKIX_PL_Initialize must
      not call NSS_Init
   -  `Bug 395850 <https://bugzilla.mozilla.org/show_bug.cgi?id=395850>`__: build of libpkix tests
      creates links to nonexistant shared libraries and breaks windows build
   -  `Bug 398401 <https://bugzilla.mozilla.org/show_bug.cgi?id=398401>`__: Memory leak in PKIX
      init.
   -  `Bug 399326 <https://bugzilla.mozilla.org/show_bug.cgi?id=399326>`__: libpkix is unable to
      validate cert for certUsageStatusResponder
   -  `Bug 400947 <https://bugzilla.mozilla.org/show_bug.cgi?id=400947>`__: thread unsafe operation
      in PKIX_PL_HashTable_Add cause selfserv to crash.
   -  `Bug 402773 <https://bugzilla.mozilla.org/show_bug.cgi?id=402773>`__: Verify the list of
      public header files in NSS 3.12
   -  `Bug 403470 <https://bugzilla.mozilla.org/show_bug.cgi?id=403470>`__: Strsclnt + tstclnt
      crashes when PKIX enabled.
   -  `Bug 403685 <https://bugzilla.mozilla.org/show_bug.cgi?id=403685>`__: Application crashes
      after having called CERT_PKIXVerifyCert
   -  `Bug 408434 <https://bugzilla.mozilla.org/show_bug.cgi?id=408434>`__: Crash with PKIX based
      verify
   -  `Bug 411614 <https://bugzilla.mozilla.org/show_bug.cgi?id=411614>`__: Explicit Policy does not
      seem to work.
   -  `Bug 417024 <https://bugzilla.mozilla.org/show_bug.cgi?id=417024>`__: Convert libpkix error
      code into nss error code
   -  `Bug 422859 <https://bugzilla.mozilla.org/show_bug.cgi?id=422859>`__: libPKIX builds &
      validates chain to root not in the caller-provided anchor list
   -  `Bug 425516 <https://bugzilla.mozilla.org/show_bug.cgi?id=425516>`__: need to destroy data
      pointed by CERTValOutParam array in case of error
   -  `Bug 426450 <https://bugzilla.mozilla.org/show_bug.cgi?id=426450>`__: PKIX_PL_HashTable_Remove
      leaks hashtable key object
   -  `Bug 429230 <https://bugzilla.mozilla.org/show_bug.cgi?id=429230>`__: memory leak in
      pkix_CheckCert function
   -  `Bug 392696 <https://bugzilla.mozilla.org/show_bug.cgi?id=392696>`__: Fix copyright
      boilerplate in all new PKIX code
   -  `Bug 300928 <https://bugzilla.mozilla.org/show_bug.cgi?id=300928>`__: Integrate libpkix to NSS
   -  `Bug 303457 <https://bugzilla.mozilla.org/show_bug.cgi?id=303457>`__: extensions newly
      supported in libpkix must be marked supported
   -  `Bug 331096 <https://bugzilla.mozilla.org/show_bug.cgi?id=331096>`__: NSS Softoken must detect
      forks on all unix-ish platforms
   -  `Bug 390710 <https://bugzilla.mozilla.org/show_bug.cgi?id=390710>`__:
      CERTNameConstraintsTemplate is incorrect
   -  `Bug 416928 <https://bugzilla.mozilla.org/show_bug.cgi?id=416928>`__: DER decode error on this
      policy extension
   -  `Bug 375019 <https://bugzilla.mozilla.org/show_bug.cgi?id=375019>`__: Cache-enable
      pkix_OcspChecker_Check
   -  `Bug 391454 <https://bugzilla.mozilla.org/show_bug.cgi?id=391454>`__: libPKIX does not honor
      NSS's override trust flags
   -  `Bug 403682 <https://bugzilla.mozilla.org/show_bug.cgi?id=403682>`__: CERT_PKIXVerifyCert
      never succeeds
   -  `Bug 324744 <https://bugzilla.mozilla.org/show_bug.cgi?id=324744>`__: add generation of policy
      extensions to certutil
   -  `Bug 390973 <https://bugzilla.mozilla.org/show_bug.cgi?id=390973>`__: Add long option names to
      SECU_ParseCommandLine
   -  `Bug 161326 <https://bugzilla.mozilla.org/show_bug.cgi?id=161326>`__: need API to convert
      dotted OID format to/from octet representation
   -  `Bug 376737 <https://bugzilla.mozilla.org/show_bug.cgi?id=376737>`__: CERT_ImportCerts
      routinely sets VALID_PEER or VALID_CA OVERRIDE trust flags
   -  `Bug 390381 <https://bugzilla.mozilla.org/show_bug.cgi?id=390381>`__: libpkix rejects cert
      chain when root CA cert has no basic constraints
   -  `Bug 391183 <https://bugzilla.mozilla.org/show_bug.cgi?id=391183>`__: rename libPKIX error
      string number type to pkix error number types
   -  `Bug 397122 <https://bugzilla.mozilla.org/show_bug.cgi?id=397122>`__: NSS 3.12 alpha treats a
      key3.db with no global salt as having no password
   -  `Bug 405966 <https://bugzilla.mozilla.org/show_bug.cgi?id=405966>`__: Unknown signature OID
      1.3.14.3.2.29 causes sec_error_bad_signature, 3.11 ignores it
   -  `Bug 413010 <https://bugzilla.mozilla.org/show_bug.cgi?id=413010>`__: CERT_CompareRDN may
      return a false match
   -  `Bug 417664 <https://bugzilla.mozilla.org/show_bug.cgi?id=417664>`__: false positive crl
      revocation test on ppc/ppc64 NSS_ENABLE_PKIX_VERIFY=1
   -  `Bug 404526 <https://bugzilla.mozilla.org/show_bug.cgi?id=404526>`__: glibc detected free():
      invalid pointer
   -  `Bug 300929 <https://bugzilla.mozilla.org/show_bug.cgi?id=300929>`__: Certificate Policy
      extensions not supported
   -  `Bug 129303 <https://bugzilla.mozilla.org/show_bug.cgi?id=129303>`__: NSS needs to expose
      interfaces to deal with multiple token sources of certs.
   -  `Bug 217538 <https://bugzilla.mozilla.org/show_bug.cgi?id=217538>`__: softoken databases
      cannot be shared between multiple processes
   -  `Bug 294531 <https://bugzilla.mozilla.org/show_bug.cgi?id=294531>`__: Design new interfaces
      for certificate path building and verification for libPKIX
   -  `Bug 326482 <https://bugzilla.mozilla.org/show_bug.cgi?id=326482>`__: NSS ECC performance
      problems (intel)
   -  `Bug 391296 <https://bugzilla.mozilla.org/show_bug.cgi?id=391296>`__: Need an update helper
      for Shared Databases
   -  `Bug 395090 <https://bugzilla.mozilla.org/show_bug.cgi?id=395090>`__: remove duplication of
      pkcs7 code from pkix_pl_httpcertstore.c
   -  `Bug 401026 <https://bugzilla.mozilla.org/show_bug.cgi?id=401026>`__: Need to provide a way to
      modify and create new PKCS #11 objects.
   -  `Bug 403680 <https://bugzilla.mozilla.org/show_bug.cgi?id=403680>`__: CERT_PKIXVerifyCert
      fails if CRLs are missing, implement cert_pi_revocationFlags
   -  `Bug 427706 <https://bugzilla.mozilla.org/show_bug.cgi?id=427706>`__: NSS_3_12_RC1 crashes in
      passwordmgr tests
   -  `Bug 426245 <https://bugzilla.mozilla.org/show_bug.cgi?id=426245>`__: Assertion failure went
      undetected by tinderbox
   -  `Bug 158242 <https://bugzilla.mozilla.org/show_bug.cgi?id=158242>`__: PK11_PutCRL is very
      memory inefficient
   -  `Bug 287563 <https://bugzilla.mozilla.org/show_bug.cgi?id=287563>`__: Please make
      cert_CompareNameWithConstraints a non-static function
   -  `Bug 301496 <https://bugzilla.mozilla.org/show_bug.cgi?id=301496>`__: NSS_Shutdown failure in
      p7sign
   -  `Bug 324878 <https://bugzilla.mozilla.org/show_bug.cgi?id=324878>`__: crlutil -L outputs false
      CRL names
   -  `Bug 337010 <https://bugzilla.mozilla.org/show_bug.cgi?id=337010>`__: OOM crash [[@
      NSC_DigestKey] Dereferencing possibly NULL att
   -  `Bug 343231 <https://bugzilla.mozilla.org/show_bug.cgi?id=343231>`__: certutil issues certs
      for invalid requests
   -  `Bug 353371 <https://bugzilla.mozilla.org/show_bug.cgi?id=353371>`__: Klocwork 91117 - Null
      Pointer Dereference in CERT_CertChainFromCert
   -  `Bug 353374 <https://bugzilla.mozilla.org/show_bug.cgi?id=353374>`__: Klocwork 76494 - Null
      ptr derefs in CERT_FormatName
   -  `Bug 353375 <https://bugzilla.mozilla.org/show_bug.cgi?id=353375>`__: Klocwork 76513 - Null
      ptr deref in nssCertificateList_DoCallback
   -  `Bug 353413 <https://bugzilla.mozilla.org/show_bug.cgi?id=353413>`__: Klocwork 76541 free
      uninitialized pointer in CERT_FindCertURLExtension
   -  `Bug 353416 <https://bugzilla.mozilla.org/show_bug.cgi?id=353416>`__: Klocwork 76593 null ptr
      deref in nssCryptokiPrivateKey_SetCertificate
   -  `Bug 353423 <https://bugzilla.mozilla.org/show_bug.cgi?id=353423>`__: Klocwork bugs in
      nss/lib/pk11wrap/dev3hack.c
   -  `Bug 353739 <https://bugzilla.mozilla.org/show_bug.cgi?id=353739>`__: Klocwork Null ptr
      dereferences in instance.c
   -  `Bug 353741 <https://bugzilla.mozilla.org/show_bug.cgi?id=353741>`__: klocwork cascading
      memory leak in mpp_make_prime
   -  `Bug 353742 <https://bugzilla.mozilla.org/show_bug.cgi?id=353742>`__: klocwork null ptr
      dereference in ocsp_DecodeResponseBytes
   -  `Bug 353748 <https://bugzilla.mozilla.org/show_bug.cgi?id=353748>`__: klocwork null ptr
      dereferences in pki3hack.c
   -  `Bug 353760 <https://bugzilla.mozilla.org/show_bug.cgi?id=353760>`__: klocwork null pointer
      dereference in p7decode.c
   -  `Bug 353763 <https://bugzilla.mozilla.org/show_bug.cgi?id=353763>`__: klocwork Null ptr
      dereferences in pk11cert.c
   -  `Bug 353773 <https://bugzilla.mozilla.org/show_bug.cgi?id=353773>`__: klocwork Null ptr
      dereferences in pk11nobj.c
   -  `Bug 353777 <https://bugzilla.mozilla.org/show_bug.cgi?id=353777>`__: Klocwork Null ptr
      dereferences in pk11obj.c
   -  `Bug 353780 <https://bugzilla.mozilla.org/show_bug.cgi?id=353780>`__: Klocwork NULL ptr
      dereferences in pkcs11.c
   -  `Bug 353865 <https://bugzilla.mozilla.org/show_bug.cgi?id=353865>`__: klocwork Null ptr deref
      in softoken/pk11db.c
   -  `Bug 353888 <https://bugzilla.mozilla.org/show_bug.cgi?id=353888>`__: klockwork IDs for
      ssl3con.c
   -  `Bug 353895 <https://bugzilla.mozilla.org/show_bug.cgi?id=353895>`__: klocwork Null ptr derefs
      in pki/pkibase.c
   -  `Bug 353902 <https://bugzilla.mozilla.org/show_bug.cgi?id=353902>`__: klocwork bugs in
      stanpcertdb.c
   -  `Bug 353903 <https://bugzilla.mozilla.org/show_bug.cgi?id=353903>`__: klocwork oom crash in
      softoken/keydb.c
   -  `Bug 353908 <https://bugzilla.mozilla.org/show_bug.cgi?id=353908>`__: klocwork OOM crash in
      tdcache.c
   -  `Bug 353909 <https://bugzilla.mozilla.org/show_bug.cgi?id=353909>`__: klocwork ptr dereference
      before NULL check in devutil.c
   -  `Bug 353912 <https://bugzilla.mozilla.org/show_bug.cgi?id=353912>`__: Misc klocwork bugs in
      lib/ckfw
   -  `Bug 354008 <https://bugzilla.mozilla.org/show_bug.cgi?id=354008>`__: klocwork bugs in freebl
   -  `Bug 359331 <https://bugzilla.mozilla.org/show_bug.cgi?id=359331>`__: modutil -changepw strict
      shutdown failure
   -  `Bug 373367 <https://bugzilla.mozilla.org/show_bug.cgi?id=373367>`__: verify OCSP response
      signature in libpkix without decoding and reencoding
   -  `Bug 390542 <https://bugzilla.mozilla.org/show_bug.cgi?id=390542>`__: libpkix fails to
      validate a chain that consists only of one self issued, trusted cert
   -  `Bug 390728 <https://bugzilla.mozilla.org/show_bug.cgi?id=390728>`__:
      pkix_pl_OcspRequest_Create throws an error if it was not able to get AIA location
   -  `Bug 397825 <https://bugzilla.mozilla.org/show_bug.cgi?id=397825>`__: libpkix: ifdef code that
      uses user object types
   -  `Bug 397832 <https://bugzilla.mozilla.org/show_bug.cgi?id=397832>`__: libpkix leaks memory if
      a macro calls a function that returns an error
   -  `Bug 402727 <https://bugzilla.mozilla.org/show_bug.cgi?id=402727>`__: functions responsible
      for creating an object leak if subsequent function code produces an error
   -  `Bug 402731 <https://bugzilla.mozilla.org/show_bug.cgi?id=402731>`__:
      pkix_pl_Pk11CertStore_CrlQuery will crash if fails to acquire DP cache.
   -  `Bug 406647 <https://bugzilla.mozilla.org/show_bug.cgi?id=406647>`__: libpkix does not use
      user defined revocation checkers
   -  `Bug 407064 <https://bugzilla.mozilla.org/show_bug.cgi?id=407064>`__:
      pkix_pl_LdapCertStore_BuildCrlList should not fail if a crl fails to be decoded
   -  `Bug 421216 <https://bugzilla.mozilla.org/show_bug.cgi?id=421216>`__: libpkix test nss_thread
      leaks a test certificate
   -  `Bug 301259 <https://bugzilla.mozilla.org/show_bug.cgi?id=301259>`__: signtool Usage message
      is unhelpful
   -  `Bug 389781 <https://bugzilla.mozilla.org/show_bug.cgi?id=389781>`__: NSS should be built
      size-optimized in browser builds on Linux, Windows, and Mac
   -  `Bug 90426 <https://bugzilla.mozilla.org/show_bug.cgi?id=90426>`__: use of obsolete typedefs
      in public NSS headers
   -  `Bug 113323 <https://bugzilla.mozilla.org/show_bug.cgi?id=113323>`__: The first argument to
      PK11_FindCertFromNickname should be const.
   -  `Bug 132485 <https://bugzilla.mozilla.org/show_bug.cgi?id=132485>`__: built-in root certs slot
      description is empty
   -  `Bug 177184 <https://bugzilla.mozilla.org/show_bug.cgi?id=177184>`__: NSS_CMSDecoder_Cancel
      might have a leak
   -  `Bug 232392 <https://bugzilla.mozilla.org/show_bug.cgi?id=232392>`__: Erroneous root CA tests
      in NSS Libraries
   -  `Bug 286642 <https://bugzilla.mozilla.org/show_bug.cgi?id=286642>`__: util should be in a
      shared library
   -  `Bug 287052 <https://bugzilla.mozilla.org/show_bug.cgi?id=287052>`__: Function to get CRL
      Entry reason code has incorrect prototype and implementation
   -  `Bug 299308 <https://bugzilla.mozilla.org/show_bug.cgi?id=299308>`__: Need additional APIs in
      the CRL cache for libpkix
   -  `Bug 335039 <https://bugzilla.mozilla.org/show_bug.cgi?id=335039>`__:
      nssCKFWCryptoOperation_UpdateCombo is not declared
   -  `Bug 340917 <https://bugzilla.mozilla.org/show_bug.cgi?id=340917>`__: crlutil should init NSS
      read-only for some options
   -  `Bug 350948 <https://bugzilla.mozilla.org/show_bug.cgi?id=350948>`__: freebl macro change can
      give 1% improvement in RSA performance on amd64
   -  `Bug 352439 <https://bugzilla.mozilla.org/show_bug.cgi?id=352439>`__: Reference leaks in
      modutil
   -  `Bug 369144 <https://bugzilla.mozilla.org/show_bug.cgi?id=369144>`__: certutil needs option to
      generate SubjectKeyID extension
   -  `Bug 391771 <https://bugzilla.mozilla.org/show_bug.cgi?id=391771>`__: pk11_config_name and
      pk11_config_strings leaked on shutdown
   -  `Bug 401194 <https://bugzilla.mozilla.org/show_bug.cgi?id=401194>`__: crash in lg_FindObjects
      on win64
   -  `Bug 405652 <https://bugzilla.mozilla.org/show_bug.cgi?id=405652>`__: In the TLS ClientHello
      message the gmt_unix_time is incorrect
   -  `Bug 424917 <https://bugzilla.mozilla.org/show_bug.cgi?id=424917>`__: Performance regression
      with studio 12 compiler
   -  `Bug 391770 <https://bugzilla.mozilla.org/show_bug.cgi?id=391770>`__: OCSP_Global.monitor is
      leaked on shutdown
   -  `Bug 403687 <https://bugzilla.mozilla.org/show_bug.cgi?id=403687>`__: move pkix functions to
      certvfypkix.c, turn off EV_TEST_HACK
   -  `Bug 428105 <https://bugzilla.mozilla.org/show_bug.cgi?id=428105>`__: CERT_SetOCSPTimeout is
      not defined in any public header file
   -  `Bug 213359 <https://bugzilla.mozilla.org/show_bug.cgi?id=213359>`__: enhance PK12util to
      extract certs from p12 file
   -  `Bug 329067 <https://bugzilla.mozilla.org/show_bug.cgi?id=329067>`__: NSS encodes cert
      distinguished name attributes with wrong string type
   -  `Bug 339906 <https://bugzilla.mozilla.org/show_bug.cgi?id=339906>`__: sec_pkcs12_install_bags
      passes uninitialized variables to functions
   -  `Bug 396484 <https://bugzilla.mozilla.org/show_bug.cgi?id=396484>`__: certutil doesn't
      truncate existing temporary files when writing them
   -  `Bug 251594 <https://bugzilla.mozilla.org/show_bug.cgi?id=251594>`__: Certificate from PKCS#12
      file with colon in friendlyName not selectable for signing/encryption
   -  `Bug 321584 <https://bugzilla.mozilla.org/show_bug.cgi?id=321584>`__: NSS PKCS12 decoder fails
      to import bags without nicknames
   -  `Bug 332633 <https://bugzilla.mozilla.org/show_bug.cgi?id=332633>`__: remove duplicate header
      files in nss/cmd/sslsample
   -  `Bug 335019 <https://bugzilla.mozilla.org/show_bug.cgi?id=335019>`__: pk12util takes friendly
      name from key, not cert
   -  `Bug 339173 <https://bugzilla.mozilla.org/show_bug.cgi?id=339173>`__: mem leak whenever
      SECMOD_HANDLE_STRING_ARG called in loop
   -  `Bug 353904 <https://bugzilla.mozilla.org/show_bug.cgi?id=353904>`__: klocwork Null ptr deref
      in secasn1d.c
   -  `Bug 366390 <https://bugzilla.mozilla.org/show_bug.cgi?id=366390>`__: correct misleading
      function names in fipstest
   -  `Bug 370536 <https://bugzilla.mozilla.org/show_bug.cgi?id=370536>`__: Memory leaks in pointer
      tracker code in DEBUG builds only
   -  `Bug 372242 <https://bugzilla.mozilla.org/show_bug.cgi?id=372242>`__: CERT_CompareRDN uses
      incorrect algorithm
   -  `Bug 379753 <https://bugzilla.mozilla.org/show_bug.cgi?id=379753>`__: S/MIME should support
      AES
   -  `Bug 381375 <https://bugzilla.mozilla.org/show_bug.cgi?id=381375>`__: ocspclnt doesn't work on
      Windows
   -  `Bug 398693 <https://bugzilla.mozilla.org/show_bug.cgi?id=398693>`__: DER_AsciiToTime produces
      incorrect output for dates 1950-1970
   -  `Bug 420212 <https://bugzilla.mozilla.org/show_bug.cgi?id=420212>`__: Empty cert DNs handled
      badly, display as !INVALID AVA!
   -  `Bug 420979 <https://bugzilla.mozilla.org/show_bug.cgi?id=420979>`__: vfychain ignores -b TIME
      option when -p option is present
   -  `Bug 403563 <https://bugzilla.mozilla.org/show_bug.cgi?id=403563>`__: Implement the TLS
      session ticket extension (STE)
   -  `Bug 400917 <https://bugzilla.mozilla.org/show_bug.cgi?id=400917>`__: Want exported function
      that outputs all host names for DNS name matching
   -  `Bug 315643 <https://bugzilla.mozilla.org/show_bug.cgi?id=315643>`__:
      test_buildchain_resourcelimits won't build
   -  `Bug 353745 <https://bugzilla.mozilla.org/show_bug.cgi?id=353745>`__: klocwork null ptr
      dereference in PKCS12 decoder
   -  `Bug 338367 <https://bugzilla.mozilla.org/show_bug.cgi?id=338367>`__: The GF2M_POPULATE and
      GFP_POPULATE should check the ecCurve_map array index bounds before use
   -  `Bug 201139 <https://bugzilla.mozilla.org/show_bug.cgi?id=201139>`__: SSLTap should display
      plain text for NULL cipher suites
   -  `Bug 233806 <https://bugzilla.mozilla.org/show_bug.cgi?id=233806>`__: Support NIST CRL policy
   -  `Bug 279085 <https://bugzilla.mozilla.org/show_bug.cgi?id=279085>`__: NSS tools display public
      exponent as negative number
   -  `Bug 363480 <https://bugzilla.mozilla.org/show_bug.cgi?id=363480>`__: ocspclnt needs option to
      take cert from specified file
   -  `Bug 265715 <https://bugzilla.mozilla.org/show_bug.cgi?id=265715>`__: remove unused hsearch.c
      DBM code
   -  `Bug 337361 <https://bugzilla.mozilla.org/show_bug.cgi?id=337361>`__: Leaks in jar_parse_any
      (security/nss/lib/jar/jarver.c)
   -  `Bug 338453 <https://bugzilla.mozilla.org/show_bug.cgi?id=338453>`__: Leaks in
      security/nss/lib/jar/jarfile.c
   -  `Bug 351408 <https://bugzilla.mozilla.org/show_bug.cgi?id=351408>`__: Leaks in
      JAR_JAR_sign_archive (security/nss/lib/jar/jarjart.c)
   -  `Bug 351443 <https://bugzilla.mozilla.org/show_bug.cgi?id=351443>`__: Remove unused code from
      mozilla/security/nss/lib/jar
   -  `Bug 351510 <https://bugzilla.mozilla.org/show_bug.cgi?id=351510>`__: Remove USE_MOZ_THREAD
      code from mozilla/security/lib/jar
   -  `Bug 118830 <https://bugzilla.mozilla.org/show_bug.cgi?id=118830>`__: NSS public header files
      should be C++ safe
   -  `Bug 123996 <https://bugzilla.mozilla.org/show_bug.cgi?id=123996>`__: certutil -H doesn't
      document certutil -C -a
   -  `Bug 178894 <https://bugzilla.mozilla.org/show_bug.cgi?id=178894>`__: Quick decoder updates
      for lib/certdb and lib/certhigh
   -  `Bug 220115 <https://bugzilla.mozilla.org/show_bug.cgi?id=220115>`__: CKM_INVALID_MECHANISM
      should be an unsigned long constant.
   -  `Bug 330721 <https://bugzilla.mozilla.org/show_bug.cgi?id=330721>`__: Remove OS/2 VACPP
      compiler support from NSS
   -  `Bug 408260 <https://bugzilla.mozilla.org/show_bug.cgi?id=408260>`__: certutil usage doesn't
      give enough information about trust arguments
   -  `Bug 410226 <https://bugzilla.mozilla.org/show_bug.cgi?id=410226>`__: leak in
      create_objects_from_handles
   -  `Bug 415007 <https://bugzilla.mozilla.org/show_bug.cgi?id=415007>`__:
      PK11_FindCertFromDERSubjectAndNickname is dead code
   -  `Bug 416267 <https://bugzilla.mozilla.org/show_bug.cgi?id=416267>`__: compiler warnings on
      solaris due to extra semicolon in SEC_ASN1_MKSUB
   -  `Bug 419763 <https://bugzilla.mozilla.org/show_bug.cgi?id=419763>`__: logger thread should be
      joined on exit
   -  `Bug 424471 <https://bugzilla.mozilla.org/show_bug.cgi?id=424471>`__: counter overflow in
      bltest
   -  `Bug 229335 <https://bugzilla.mozilla.org/show_bug.cgi?id=229335>`__: Remove certificates that
      expired in August 2004 from tree
   -  `Bug 346551 <https://bugzilla.mozilla.org/show_bug.cgi?id=346551>`__: init SECItem derTemp in
      crmf_encode_popoprivkey
   -  `Bug 395080 <https://bugzilla.mozilla.org/show_bug.cgi?id=395080>`__: Double backslash in
      sysDir filenames causes problems on OS/2
   -  `Bug 341371 <https://bugzilla.mozilla.org/show_bug.cgi?id=341371>`__: certutil lacks a way to
      request a certificate with an existing key
   -  `Bug 382292 <https://bugzilla.mozilla.org/show_bug.cgi?id=382292>`__: add support for Camellia
      to cmd/symkeyutil
   -  `Bug 385642 <https://bugzilla.mozilla.org/show_bug.cgi?id=385642>`__: Add additional cert
      usage(s) for certutil's -V -u option
   -  `Bug 175741 <https://bugzilla.mozilla.org/show_bug.cgi?id=175741>`__: strict aliasing bugs in
      mozilla/dbm
   -  `Bug 210584 <https://bugzilla.mozilla.org/show_bug.cgi?id=210584>`__: CERT_AsciiToName doesn't
      accept all valid values
   -  `Bug 298540 <https://bugzilla.mozilla.org/show_bug.cgi?id=298540>`__: vfychain usage option
      should be improved and documented
   -  `Bug 323570 <https://bugzilla.mozilla.org/show_bug.cgi?id=323570>`__: Make dbck Debug mode
      work with Softoken
   -  `Bug 371470 <https://bugzilla.mozilla.org/show_bug.cgi?id=371470>`__: vfychain needs option to
      verify for specific date
   -  `Bug 387621 <https://bugzilla.mozilla.org/show_bug.cgi?id=387621>`__: certutil's random noise
      generator isn't very efficient
   -  `Bug 390185 <https://bugzilla.mozilla.org/show_bug.cgi?id=390185>`__: signtool error message
      wrongly uses the term database
   -  `Bug 391651 <https://bugzilla.mozilla.org/show_bug.cgi?id=391651>`__: Need config.mk file for
      Windows Vista
   -  `Bug 396322 <https://bugzilla.mozilla.org/show_bug.cgi?id=396322>`__: Fix secutil's code and
      NSS tools that print public keys
   -  `Bug 417641 <https://bugzilla.mozilla.org/show_bug.cgi?id=417641>`__: miscellaneous minor NSS
      bugs
   -  `Bug 334914 <https://bugzilla.mozilla.org/show_bug.cgi?id=334914>`__: hopefully useless null
      check of out it in JAR_find_next
   -  `Bug 95323 <https://bugzilla.mozilla.org/show_bug.cgi?id=95323>`__: ckfw should support cipher
      operations.
   -  `Bug 337088 <https://bugzilla.mozilla.org/show_bug.cgi?id=337088>`__: Coverity 405,
      PK11_ParamToAlgid() in mozilla/security/nss/lib/pk11wrap/pk11mech.c
   -  `Bug 339907 <https://bugzilla.mozilla.org/show_bug.cgi?id=339907>`__: oaep_xor_with_h1
      allocates and leaks sha1cx
   -  `Bug 341122 <https://bugzilla.mozilla.org/show_bug.cgi?id=341122>`__: Coverity 633
      SFTK_DestroySlotData uses slot->slotLock then checks it for NULL
   -  `Bug 351140 <https://bugzilla.mozilla.org/show_bug.cgi?id=351140>`__: Coverity 995, potential
      crash in ecgroup_fromNameAndHex
   -  `Bug 362278 <https://bugzilla.mozilla.org/show_bug.cgi?id=362278>`__: lib/util includes header
      files from other NSS directories
   -  `Bug 228190 <https://bugzilla.mozilla.org/show_bug.cgi?id=228190>`__: Remove unnecessary
      NSS_ENABLE_ECC defines from manifest.mn
   -  `Bug 412906 <https://bugzilla.mozilla.org/show_bug.cgi?id=412906>`__: remove sha.c and sha.h
      from lib/freebl
   -  `Bug 353543 <https://bugzilla.mozilla.org/show_bug.cgi?id=353543>`__: valgrind uninitialized
      memory read in nssPKIObjectCollection_AddInstances
   -  `Bug 377548 <https://bugzilla.mozilla.org/show_bug.cgi?id=377548>`__: NSS QA test program
      certutil's default DSA prime is only 512 bits
   -  `Bug 333405 <https://bugzilla.mozilla.org/show_bug.cgi?id=333405>`__: item cleanup is unused
      DEADCODE in SECITEM_AllocItem loser
   -  `Bug 288730 <https://bugzilla.mozilla.org/show_bug.cgi?id=288730>`__: compiler warnings in
      certutil
   -  `Bug 337251 <https://bugzilla.mozilla.org/show_bug.cgi?id=337251>`__: warning: /\* within
      comment
   -  `Bug 362967 <https://bugzilla.mozilla.org/show_bug.cgi?id=362967>`__: export
      SECMOD_DeleteModuleEx
   -  `Bug 389248 <https://bugzilla.mozilla.org/show_bug.cgi?id=389248>`__: NSS build failure when
      NSS_ENABLE_ECC is not defined
   -  `Bug 390451 <https://bugzilla.mozilla.org/show_bug.cgi?id=390451>`__: Remembered passwords
      lost when changing Master Password
   -  `Bug 418546 <https://bugzilla.mozilla.org/show_bug.cgi?id=418546>`__: reference leak in
      CERT_PKIXVerifyCert
   -  `Bug 390074 <https://bugzilla.mozilla.org/show_bug.cgi?id=390074>`__: OS/2 sign.cmd doesn't
      find sqlite3.dll
   -  `Bug 417392 <https://bugzilla.mozilla.org/show_bug.cgi?id=417392>`__: certutil -L -n reports
      bogus trust flags

   --------------

`Documentation <#documentation>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   For a list of the primary NSS documentation pages on mozilla.org, see `NSS
   Documentation <../index.html#Documentation>`__. New and revised documents available since the
   release of NSS 3.11 include the following:

   -  :ref:`mozilla_projects_nss_reference_building_and_installing_nss_build_instructions`
   -  `NSS Shared DB <http://wiki.mozilla.org/NSS_Shared_DB>`__
   -  :ref:`mozilla_projects_nss_reference_nss_environment_variables`

   --------------

`Compatibility <#compatibility>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   NSS 3.12 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.12 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in `NSS Public Functions <../ref/nssfunctions.html>`__ will remain
   compatible with future versions of the NSS shared libraries.

   --------------

`Feedback <#feedback>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Bugs discovered should be reported by filing a bug report with `mozilla.org
   Bugzilla <https://bugzilla.mozilla.org/>`__\ (product NSS).