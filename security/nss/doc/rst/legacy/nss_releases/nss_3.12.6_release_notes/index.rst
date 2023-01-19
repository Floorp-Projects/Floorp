.. _mozilla_projects_nss_nss_3_12_6_release_notes:

NSS 3.12.6 release notes
========================

.. _nss_3.12.6_release_notes:

`NSS 3.12.6 release notes <#nss_3.12.6_release_notes>`__
--------------------------------------------------------

.. container::

   .. container::

      2010-03-03
      *Newsgroup:*\ `mozilla.dev.tech.crypto <news://news.mozilla.org/mozilla.dev.tech.crypto>`__

      .. container::
         :name: section_1

         .. rubric:: Introduction
            :name: Introduction

         Network Security Services (NSS) 3.12.6 is a patch release for NSS 3.12. The bug fixes in
         NSS 3.12.6 are described in the "`Bugs
         Fixed <http://mdn.beonex.com/en/NSS_3.12.6_release_notes.html#bugsfixed>`__" section below.

         NSS 3.12.6 is tri-licensed under the MPL 1.1/GPL 2.0/LGPL 2.1.

      .. container::
         :name: section_2

         .. rubric:: Distribution Information
            :name: Distribution_Information

         | The CVS tag for the NSS 3.12.6 release is ``NSS_3_12_6_RTM``.  NSS 3.12.6 requires `NSPR
           4.8.4 <https://www.mozilla.org/projects/nspr/release-notes/>`__.
         | See the `Documentation <http://mdn.beonex.com/en/NSS_3.12.6_release_notes.html#docs>`__
           section for the build instructions.

         NSS 3.12.6 source and binary distributions are also available on ``ftp.mozilla.org`` for
         secure HTTPS download:

         -  Source tarballs:
            https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_12_6_RTM/src/.

         | You also need to download the NSPR 4.8.4 binary distributions to get the NSPR 4.8.4
           header files and shared libraries, which NSS 3.12.6 requires. NSPR 4.8.4 binary
           distributions are in https://ftp.mozilla.org/pub/mozilla.org/nspr/releases/v4.8.4/.
         |

      .. container::
         :name: section_3

         .. rubric:: New in NSS 3.12.6
            :name: New_in_NSS_3.12.6

         .. container::
            :name: section_4

            .. rubric:: SSL3 & TLS Renegotiation Indication Extension (RFC 5746)
               :name: SSL3_TLS_Renegotiation_Indication_Extension_(RFC_5746)

            -  By default, NSS 3.12.6 uses the new TLS Renegotiation Indication Extension for TLS
               renegotiation but allows simple SSL/TLS connections (without renegotiation) with
               peers that don't support the TLS Renegotiation Indication Extension.

               The behavior of NSS for renegotiation can be changed through API function calls, or
               with the following environment variables:

               -  NSS_SSL_ENABLE_RENEGOTIATION

                  -  values:

                     -  [0|n|N]: SSL_RENEGOTIATE_NEVER

                        -  Never allow renegotiation - That was the default for 3.12.5 release.

                     -  [1|u|U]: SSL_RENEGOTIATE_UNRESTRICTED

                        -  Server and client are allowed to renegotiate without any restrictions.
                           This setting was the default prior 3.12.5 and makes products vulnerable.

                     -  [2|r|R]: SSL_RENEGOTIATE_REQUIRES_XTN (default)

                        -  Only allows renegotiation if the peer's hello bears the TLS
                           renegotiation_info extension. This is the safe renegotiation.

                     -  [3|t|T]: SSL_RENEGOTIATE_TRANSITIONAL

                        -  Disallows unsafe renegotiation in server sockets only, but allows clients
                           to continue to renegotiate with vulnerable servers. This value should
                           only be used during the transition period when few servers have been
                           upgraded.

               -  NSS_SSL_REQUIRE_SAFE_NEGOTIATION

                  -  values:

                     -  1: requireSafeNegotiation = TRUE
                     -  unset: requireSafeNegotiation = FALSE

                     Controls whether safe renegotiation indication is required for initial
                     handshake. If TRUE, a connection will be dropped at initial handshake if the
                     peer server or client does not support safe renegotiation. The default setting
                     for this option is FALSE.

               These options can also be set with the following SSL options:

               -  sslOptions.enableRenegotiation
               -  sslOptions.requireSafeNegotiation
               -  New pseudo cipher suite value: TLS_EMPTY_RENEGOTIATION_INFO_SCSV (cannot be
                  negotiated)

         .. container::
            :name: section_5

            .. rubric:: TLS Server Name Indication for servers
               :name: TLS_Server_Name_Indication_for_servers

            -  | TLS Server Name Indication (SNI) for servers is almost fully implemented in NSS
                 3.12.6.
               | See `bug 360421 <https://bugzilla.mozilla.org/show_bug.cgi?id=360421>`__ for
                 details.

               Note: The TLS Server Name Indication for clients is already fully implemented in NSS.

               -  New functions for SNI *(see ssl.h for more information)*:

                  -  SSLSNISocketConfig

                     -  Return values:

                        -  SSL_SNI_CURRENT_CONFIG_IS_USED: libSSL must use the default cert and key.
                        -  SSL_SNI_SEND_ALERT: libSSL must send the "unrecognized_name" alert.

                  -  SSL_SNISocketConfigHook
                  -  SSL_ReconfigFD
                  -  SSL_ConfigServerSessionIDCacheWithOpt
                  -  SSL_SetTrustAnchors
                  -  SSL_GetNegotiatedHostInfo

               -  New enum for SNI:

                  -  SSLSniNameType *(see sslt.h)*

         .. container::
            :name: section_6

            .. rubric:: New functions
               :name: New_functions

            -  *in cert.h*

               -  CERTDistNames: Duplicate distinguished name array.
               -  CERT_DistNamesFromCertList: Generate an array of Distinguished names from a list
                  of certs.

               *in ocsp.h*

               -  CERT_CacheOCSPResponseFromSideChannel:

                  -  This function is intended for use when OCSP responses are provided via a
                     side-channel, i.e. TLS OCSP stapling (a.k.a. the status_request extension).

               *in ssl.h*

               -  SSL_GetImplementedCiphers
               -  SSL_GetNumImplementedCiphers
               -  SSL_HandshakeNegotiatedExtension

         .. container::
            :name: section_7

            .. rubric:: New error codes
               :name: New_error_codes

            -  *in sslerr.h*

               -  SSL_ERROR_UNSAFE_NEGOTIATION
               -  SSL_ERROR_RX_UNEXPECTED_UNCOMPRESSED_RECORD

         .. container::
            :name: section_8

            .. rubric:: New types
               :name: New_types

            -  *in sslt.h*

               -  SSLExtensionType

         .. container::
            :name: section_9

            .. rubric:: New environment variables
               :name: New_environment_variables

            -  SQLITE_FORCE_PROXY_LOCKING

               -  1 means force always use proxy, 0 means never use proxy, NULL means use proxy for
                  non-local files only.

            -  SSLKEYLOGFILE

               -  Key log file. If set, NSS logs RSA pre-master secrets to this file. This allows
                  packet sniffers to decrypt TLS connections.
                  See `documentation <http://mdn.beonex.com/en/NSS_Key_Log_Format.html>`__.
                  Note: The code must be built with TRACE defined to use this functionality.

      .. container::
         :name: section_10

         .. rubric:: Bugs Fixed
            :name: Bugs_Fixed

         The following bugs have been fixed in NSS 3.12.6.

         -  `Bug 275744 <https://bugzilla.mozilla.org/show_bug.cgi?id=275744>`__: Support for TLS
            compression RFC 3749
         -  `Bug 494603 <https://bugzilla.mozilla.org/show_bug.cgi?id=494603>`__: Update NSS's copy
            of sqlite3 to 3.6.22 to get numerous bug fixes
         -  `Bug 496993 <https://bugzilla.mozilla.org/show_bug.cgi?id=496993>`__: Add accessor
            functions for SSL_ImplementedCiphers
         -  `Bug 515279 <https://bugzilla.mozilla.org/show_bug.cgi?id=515279>`__:
            CERT_PKIXVerifyCert considers a certificate revoked if cert_ProcessOCSPResponse fails
            for any reason
         -  `Bug 515870 <https://bugzilla.mozilla.org/show_bug.cgi?id=515870>`__: GCC compiler
            warnings in NSS 3.12.4
         -  `Bug 518255 <https://bugzilla.mozilla.org/show_bug.cgi?id=518255>`__: The input buffer
            for SGN_Update should be declared const
         -  `Bug 519550 <https://bugzilla.mozilla.org/show_bug.cgi?id=519550>`__: Allow the
            specification of an alternate library for SQLite
         -  `Bug 524167 <https://bugzilla.mozilla.org/show_bug.cgi?id=524167>`__: Crash in [[@
            find_objects_by_template - nssToken_FindCertificateByIssuerAndSerialNumber]
         -  `Bug 526910 <https://bugzilla.mozilla.org/show_bug.cgi?id=526910>`__: maxResponseLength
            (initialized to PKIX_DEFAULT_MAX_RESPONSE_LENGTH) is too small for downloading some
            CRLs.
         -  `Bug 527759 <https://bugzilla.mozilla.org/show_bug.cgi?id=527759>`__: Add multiple roots
            to NSS (single patch)
         -  `Bug 528741 <https://bugzilla.mozilla.org/show_bug.cgi?id=528741>`__: pkix_hash throws a
            null-argument exception on empty strings
         -  `Bug 530907 <https://bugzilla.mozilla.org/show_bug.cgi?id=530907>`__: The peerID
            argument to SSL_SetSockPeerID should be declared const
         -  `Bug 531188 <https://bugzilla.mozilla.org/show_bug.cgi?id=531188>`__: Decompression
            failure with https://livechat.merlin.pl/
         -  `Bug 532417 <https://bugzilla.mozilla.org/show_bug.cgi?id=532417>`__: Build problem with
            spaces in path names
         -  `Bug 534943 <https://bugzilla.mozilla.org/show_bug.cgi?id=534943>`__: Clean up the
            makefiles in lib/ckfw/builtins
         -  `Bug 534945 <https://bugzilla.mozilla.org/show_bug.cgi?id=534945>`__: lib/dev does not
            need to include headers from lib/ckfw
         -  `Bug 535669 <https://bugzilla.mozilla.org/show_bug.cgi?id=535669>`__: Move common
            makefile code in if and else to the outside
         -  `Bug 536023 <https://bugzilla.mozilla.org/show_bug.cgi?id=536023>`__: DER_UTCTimeToTime
            and DER_GeneralizedTimeToTime ignore all bytes after an embedded null
         -  `Bug 536474 <https://bugzilla.mozilla.org/show_bug.cgi?id=536474>`__: Add support for
            logging pre-master secrets
         -  `Bug 537356 <https://bugzilla.mozilla.org/show_bug.cgi?id=537356>`__: Implement new safe
            SSL3 & TLS renegotiation
         -  `Bug 537795 <https://bugzilla.mozilla.org/show_bug.cgi?id=537795>`__: NSS_InitContext
            does not work with NSS_RegisterShutdown
         -  `Bug 537829 <https://bugzilla.mozilla.org/show_bug.cgi?id=537829>`__: Allow NSS to build
            for Android
         -  `Bug 540304 <https://bugzilla.mozilla.org/show_bug.cgi?id=540304>`__: Implement
            SSL_HandshakeNegotiatedExtension
         -  `Bug 541228 <https://bugzilla.mozilla.org/show_bug.cgi?id=541228>`__: Remove an obsolete
            NSPR version check in lib/util/secport.c
         -  `Bug 541231 <https://bugzilla.mozilla.org/show_bug.cgi?id=541231>`__: nssinit.c doesn't
            need to include ssl.h and sslproto.h.
         -  `Bug 542538 <https://bugzilla.mozilla.org/show_bug.cgi?id=542538>`__: NSS: Add function
            for recording OCSP stapled replies
         -  `Bug 544191 <https://bugzilla.mozilla.org/show_bug.cgi?id=544191>`__: Use system zlib on
            Mac OS X
         -  `Bug 544584 <https://bugzilla.mozilla.org/show_bug.cgi?id=544584>`__: segmentation fault
            when enumerating the nss database
         -  `Bug 544586 <https://bugzilla.mozilla.org/show_bug.cgi?id=544586>`__: Various
            nss-sys-init patches from Fedora
         -  `Bug 545273 <https://bugzilla.mozilla.org/show_bug.cgi?id=545273>`__: Remove unused
            function SEC_Init
         -  `Bug 546389 <https://bugzilla.mozilla.org/show_bug.cgi?id=546389>`__: nsssysinit binary
            built inside source tree

      .. container::
         :name: section_11

         .. rubric:: Documentation
            :name: Documentation

         For a list of the primary NSS documentation pages on mozilla.org, see `NSS
         Documentation <https://www.mozilla.org/projects/security/pki/nss/#documentation>`__. New
         and revised documents available since the release of NSS 3.11 include the following:

         -  `Build
            Instructions <http://mdn.beonex.com/en/NSS_reference/Building_and_installing_NSS/Build_instructions.html>`__
         -  `NSS Shared DB <http://wiki.mozilla.org/NSS_Shared_DB>`__

      .. container::
         :name: section_12

         .. rubric:: Compatibility
            :name: Compatibility

         NSS 3.12.6 shared libraries are backward compatible with all older NSS 3.x shared
         libraries. A program linked with older NSS 3.x shared libraries will work with NSS 3.12.6
         shared libraries without recompiling or relinking.  Furthermore, applications that restrict
         their use of NSS APIs to the functions listed in `NSS Public
         Functions <https://www.mozilla.org/projects/security/pki/nss/ref/nssfunctions.html>`__ will
         remain compatible with future versions of the NSS shared libraries.

      .. container::
         :name: section_13

         .. rubric:: Feedback
            :name: Feedback

         Bugs discovered should be reported by filing a bug report with `mozilla.org
         Bugzilla <https://bugzilla.mozilla.org/>`__ (product NSS).