.. _mozilla_projects_nss_nss_3_24_release_notes:

NSS 3.24 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The Network Security Services (NSS) team has released NSS 3.24, which is a minor release.



`Distribution information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The hg tag is NSS_3_24_RTM. NSS 3.24 requires Netscape Portable Runtime(NSPR) 4.12 or newer.

   NSS 3.24 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_24_RTM/src/

.. _new_in_nss_3.24:

`New in NSS 3.24 <#new_in_nss_3.24>`__
--------------------------------------

.. container::

   NSS 3.24 includes two NSS softoken updates, a new function to configure SSL/TLS server sockets,
   and two functions to improve the use of temporary arenas.

.. _new_functionality:

`New functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  NSS softoken has been updated with the latest National Institute of Standards and Technology
      (NIST) guidance (as of 2015):

      -  Software integrity checks and POST functions are executed on shared library load. These
         checks have been disabled by default, as they can cause a performance regression. To enable
         these checks, you must define symbol NSS_FORCE_FIPS when building NSS.
      -  Counter mode and Galois/Counter Mode (GCM) have checks to prevent counter overflow.
      -  Additional CSPs are zeroed in the code.
      -  NSS softoken uses new guidance for how many Rabin-Miller tests are needed to verify a prime
         based on prime size.

   -  NSS softoken has also been updated to allow NSS to run in FIPS Level 1 (no password). This
      mode is triggered by setting the database password to the empty string. In FIPS mode, you may
      move from Level 1 to Level 2 (by setting an appropriate password), but not the reverse.
   -  A SSL_ConfigServerCert function has been added for configuring SSL/TLS server sockets with a
      certificate and private key. Use this new function in place of SSL_ConfigSecureServer,
      SSL_ConfigSecureServerWithCertChain, SSL_SetStapledOCSPResponses, and
      SSL_SetSignedCertTimestamps. SSL_ConfigServerCert automatically determines the certificate
      type from the certificate and private key. The caller is no longer required to use SSLKEAType
      explicitly to select a "slot" into which the certificate is configured (which incorrectly
      identifies a key agreement type rather than a certificate). Separate functions for configuring
      Online Certificate Status Protocol (OCSP) responses or Signed Certificate Timestamps are not
      needed, since these can be added to the optional SSLExtraServerCertData struct provided to
      SSL_ConfigServerCert.  Also, partial support for RSA Probabilistic Signature Scheme (RSA-PSS)
      certificates has been added. Although these certificates can be configured, they will not be
      used by NSS in this version.
   -  For functions that use temporary arenas, allocating a PORTCheapArena on the stack is more
      performant than allocating a PLArenaPool on the heap. Rather than declaring a PLArenaPool
      pointer and calling PORT_NewArena/PORT_FreeArena to allocate or free an instance on the heap,
      declare a PORTCheapArenaPool on the stack and use PORT_InitCheapArena/PORT_DestroyCheapArena
      to initialize and destroy it. Items allocated from the arena are still created on the heap,
      only the arena itself is stack-allocated. Note: This approach is only useful when the arena
      use is tightly bounded, for example, if it is only used in a single function.

.. _new_elements:

`New elements <#new_elements>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This section lists and briefly describes the new functions, types, and macros in NSS 3.24.

   .. rubric:: New functions
      :name: new_functions

   -  *In ssl.h*

      -  SSL_ConfigServerCert - Configures an SSL/TLS socket with a certificate, private key, and
         other information.

   -  *In secport.h*

      -  PORT_InitCheapArena - Initializes an arena that was created on the stack. (See
         PORTCheapArenaPool.)
      -  PORT_DestroyCheapArena - Destroys an arena that was created on the stack. (See
         PORTCheapArenaPool.)

   .. rubric:: New types
      :name: new_types

   -  *In sslt.h*

      -  SSLExtraServerCertData - Optionally passed as an argument to SSL_ConfigServerCert. This
         struct contains supplementary information about a certificate, such as the intended type of
         the certificate, stapled OCSP responses, or Signed Certificate Timestamps (used for
         `certificate transparency <https://datatracker.ietf.org/doc/html/rfc6962>`__).

   -  *In secport.h*

      -  PORTCheapArenaPool - A stack-allocated arena pool, to be used for temporary arena
         allocations.

   .. rubric:: New macros
      :name: new_macros

   -  *In pkcs11t.h*

      -  CKM_TLS12_MAC

   -  *In secoidt.h*

      -  SEC_OID_TLS_ECDHE_PSK - This OID governs the use of the
         TLS_ECDHE_PSK_WITH_AES_128_GCM_SHA256 cipher suite, which is used only for session
         resumption in TLS 1.3.

.. _notable_changes_in_nss_3.24:

`Notable changes in NSS 3.24 <#notable_changes_in_nss_3.24>`__
--------------------------------------------------------------

.. container::

   Additions, deprecations, and other changes in NSS 3.24 are listed below.

   -  Deprecate the following functions. (Applications should instead use the new
      SSL_ConfigServerCert function.)

      -  SSL_SetStapledOCSPResponses
      -  SSL_SetSignedCertTimestamps
      -  SSL_ConfigSecureServer
      -  SSL_ConfigSecureServerWithCertChain

   -  Deprecate the NSS_FindCertKEAType function, as it reports a misleading value for certificates
      that might be used for signing rather than key exchange.
   -  Update SSLAuthType to define a larger number of authentication key types.
   -  Deprecate the member attribute **authAlgorithm** of type SSLCipherSuiteInfo. Instead,
      applications should use the newly added attribute **authType**.
   -  Rename ssl_auth_rsa to ssl_auth_rsa_decrypt.
   -  Add a shared library (libfreeblpriv3) on Linux platforms that define FREEBL_LOWHASH.
   -  Remove most code related to SSL v2, including the ability to actively send a SSLv2-compatible
      client hello. However, the server-side implementation of the SSL/TLS protocol still supports
      processing of received v2-compatible client hello messages.
   -  Disable (by default) NSS support in optimized builds for logging SSL/TLS key material to a
      logfile if the SSLKEYLOGFILE environment variable is set. To enable the functionality in
      optimized builds, you must define the symbol NSS_ALLOW_SSLKEYLOGFILE when building NSS.
   -  Update NSS to protect it against the Cachebleed attack.
   -  Disable support for DTLS compression.
   -  Improve support for TLS 1.3. This includes support for DTLS 1.3. Note that TLS 1.3 support is
      experimental and not suitable for production use.

.. _bugs_fixed_in_nss_3.24:

`Bugs fixed in NSS 3.24 <#bugs_fixed_in_nss_3.24>`__
----------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.24:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.24

`Acknowledgements <#acknowledgements>`__
----------------------------------------

.. container::

   The NSS development team would like to thank Yuval Yarom for responsibly disclosing the
   Cachebleed attack by providing advance copies of their research.

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.24 shared libraries are backward-compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.24 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).