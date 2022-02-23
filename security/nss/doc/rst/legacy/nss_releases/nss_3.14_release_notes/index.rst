.. _mozilla_projects_nss_nss_3_14_release_notes:

NSS 3.14 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.14, which is a minor release with the
   following new features:

   -  Support for TLS 1.1 (RFC 4346)
   -  Experimental support for DTLS 1.0 (RFC 4347) and DTLS-SRTP (RFC 5764)
   -  Support for AES-CTR, AES-CTS, and AES-GCM
   -  Support for Keying Material Exporters for TLS (RFC 5705)

   In addition to the above new features, the following major changes have been introduced:

   -  Support for certificate signatures using the MD5 hash algorithm is now disabled by default.
   -  The NSS license has changed to MPL 2.0. Previous releases were released under a MPL 1.1/GPL
      2.0/LGPL 2.1 tri-license. For more information about MPL 2.0, please see
      http://www.mozilla.org/MPL/2.0/FAQ.html. For an additional explantation on GPL/LGPL
      compatibility, see security/nss/COPYING in the source code.
   -  Export and DES cipher suites are disabled by default. Non-ECC AES and Triple DES cipher suites
      are enabled by default.

   NSS 3.14 source tarballs can be downloaded from
   https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_14_RTM/src/. The CVS tag is
   NSS_3_14_RTM.

.. _new_in_nss_3.14:

`New in NSS 3.14 <#new_in_nss_3.14>`__
--------------------------------------

.. container::

   The sections that follow discuss specific changes in NSS 3.14 in more detail.

   -  Support for TLS 1.1 (RFC 4346) has been added
      (https://bugzilla.mozilla.org/show_bug.cgi?id=565047).

      .. container::

         To better support TLS 1.1 and future versions of TLS, a new version range API was
         introduced to allow applications to specify the desired minimum and maximum versions. These
         functions are intended to replace the now-deprecated use of the SSL_ENABLE_SSL3 and
         SSL_ENABLE_TLS socket options. The following functions have been added to the libssl
         library included in NSS 3.14

         -  SSL_VersionRangeGet (in ssl.h)
         -  SSL_VersionRangeGetDefault (in ssl.h)
         -  SSL_VersionRangeGetSupported (in ssl.h)
         -  SSL_VersionRangeSet (in ssl.h)
         -  SSL_VersionRangeSetDefault (in ssl.h)

   -  To better ensure interoperability with peers that support TLS 1.1, NSS has altered how it
      handles certain SSL protocol layer events. Such changes may present interoperability concerns
      when enabling TLS 1.1.

      .. container::

         -  When connecting to a server, the record layer version of the initial ClientHello will be
            at most { 3, 1 } (TLS 1.0), even when attempting to negotiate TLS 1.1
            (https://bugzilla.mozilla.org/show_bug.cgi?id=774547)
         -  The choice of client_version sent during renegotiations has changed. See the
            "`Changes <#changes>`__" section below.

   -  Experimental Support for DTLS (RFC 4347) and DTLS-SRTP (RFC 5764)

      DTLS client and server support has been added in NSS 3.14. Because the test coverage and
      interoperability testing is not yet at the same level as other NSS code, this feature should
      be considered "experimental" and may contain bugs.

      The following functions have been added to the libssl library included in NSS 3.14:

      -  DTLS_ImportFD (in ssl.h)
      -  DTLS_GetHandshakeTimeout (in ssl.h)
      -  SSL_GetSRTPCipher (in ssl.h)
      -  SSL_SetRTPCiphers (in ssl.h)

   -  Support for AES-GCM

      .. container::

         Support for AES-GCM has been added to the NSS PKCS #11 module (softoken), based upon the
         draft 7 of PKCS #11 v2.30.

         **WARNING**: Because of ambiguity in the current draft text, applications should ONLY use
         GCM in single-part mode (C_Encrypt/C_Decrypt). They should NOT use multi-part APIs
         (C_EncryptUpdate/C_DecryptUpdate).

   -  Support for application-defined certificate chain validation callback when using libpkix

      .. container::

         To better support per-application security policies, a new callback has been added for
         applications that use libpkix to verify certificates. Applications may use this callback to
         inform libpkix whether or not candidate certificate chains meet application-specific
         security policies, allowing libpkix to continue discovering certificate paths until it can
         find a chain that satisfies the policies.

         The following types have been added in NSS 3.14

         -  CERTChainVerifyCallback (in certt.h)
         -  CERTChainVerifyCallbackFunc (in certt.h)
         -  cert_pi_chainVerifyCallback, a new option for CERTValParamInType (in certt.h)
         -  A new error code: SEC_ERROR_APPLICATION_CALLBACK_ERROR (in secerr.h)

   -  New for PKCS #11

      .. container::

         PKCS #11 mechanisms:

         -  CKM_AES_CTS
         -  CKM_AES_CTR
         -  CKM_AES_GCM (see warnings against using C_EncryptUpdate/C_DecryptUpdate above)
         -  CKM_SHA224_KEY_DERIVATION
         -  CKM_SHA256_KEY_DERIVATION
         -  CKM_SHA384_KEY_DERIVATION
         -  CKM_SHA512_KEY_DERIVATION

   Changes in NSS 3.14

.. _changes_in_nss_3.14:

`Changes in NSS 3.14 <#changes_in_nss_3.14>`__
----------------------------------------------

.. container::

   -  `Bug 333601 <https://bugzilla.mozilla.org/show_bug.cgi?id=333601>`__ - Performance
      enhancements for Intel Macs

      When building for Intel Macs, NSS will now take advantage of optimized assembly code for
      common operations. These changes have the observed effect of doubling RSA performance.

   -  `Bug 792681 <https://bugzilla.mozilla.org/show_bug.cgi?id=792681>`__ - New default cipher
      suites

      The default cipher suites in NSS 3.14 have been changed to better reflect the current security
      landscape. The defaults now better match the set that most major Web browsers enable by
      default.

   -  `Bug 783448 <https://bugzilla.mozilla.org/show_bug.cgi?id=783448>`__ - When performing an SSL
      renegotiation, the client_version that is sent in the renegotiation ClientHello will be set to
      match the client_version that was sent in the initial ClientHello. This is needed for
      compatibility with IIS.

   -  Certificate signatures that make use of the MD5 hash algorithm will now be rejected by
      default. Support for MD5 may be manually enabled (but is discouraged) by setting the
      environment variable of "NSS_HASH_ALG_SUPPORT=+MD5" or by using the NSS_SetAlgorithmPolicy
      function. Note that SSL cipher suites with "MD5" in their names are NOT disabled by this
      change; those cipher suites use HMAC-MD5, not plain MD5, and are still considered safe.

   -  Maximum key sizes for RSA and Diffie-Hellman keys have been increased to 16K bits.

   -  Command line utilities tstclnt, strsclnt, and selfserv have changed. The old options to
      disable SSL 2, SSL 3 and TLS 1.0 have been removed and replaced with a new -V option that
      specifies the enabled range of protocol versions (see usage output of those tools).

.. _bugs_fixed_in_nss_3.14:

`Bugs fixed in NSS 3.14 <#bugs_fixed_in_nss_3.14>`__
----------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.14:

   https://bugzilla.mozilla.org/buglist.cgi?list_id=4643675;resolution=FIXED;classification=Components;query_format=advanced;product=NSS;target_milestone=3.14