.. _mozilla_projects_nss_nss_3_12_5_release_notes:

NSS 3.12.5 release_notes
========================

.. _nss_3.12.5_release_notes:

`NSS 3.12.5 release notes <#nss_3.12.5_release_notes>`__
--------------------------------------------------------

.. container::

   .. container::

      2009-12-02
      *Newsgroup:*\ `mozilla.dev.tech.crypto <news://news.mozilla.org/mozilla.dev.tech.crypto>`__

      --------------

      .. container::
         :name: section_1

         .. rubric:: Introduction
            :name: Introduction

         Network Security Services (NSS) 3.12.5 is a patch release for NSS 3.12. The bug fixes in
         NSS 3.12.5 are described in the "`Bugs
         Fixed <https://dev.mozilla.jp/localmdc/localmdc_5125.html#bugsfixed>`__" section below.

         NSS 3.12.5 is tri-licensed under the MPL 1.1/GPL 2.0/LGPL 2.1.

      .. container::
         :name: section_2

         .. rubric:: Distribution Information
            :name: Distribution_Information

         The CVS tag for the NSS 3.12.5 release is ``NSS_3_12_5_RTM``. 

         NSS 3.12.5 requires `NSPR 4.8 <https://www.mozilla.org/projects/nspr/release-notes/>`__.

         You can check out the source from CVS by

         .. note::

            cvs co -r NSPR_4_8_RTM NSPR
            cvs co -r NSS_3_12_5_RTM NSS

         See the `Documentation <https://dev.mozilla.jp/localmdc/localmdc_5125.html#docs>`__ section
         for the build instructions.

         NSS 3.12.5 source is also available on ``ftp.mozilla.org`` for secure HTTPS download:

         -  Source tarball:
            https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_12_5_RTM/src/.

      .. container::
         :name: section_3

         .. rubric:: New in NSS 3.12.5
            :name: New_in_NSS_3.12.5

         .. container::
            :name: section_4

            .. rubric:: SSL3 & TLS Renegotiation Vulnerability
               :name: SSL3_TLS_Renegotiation_Vulnerability

            See `CVE-2009-3555 <http://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2009-3555>`__ and
            `US-CERT VU#120541 <http://www.kb.cert.org/vuls/id/120541>`__ for more information about
            this security vulnerability.

            All SSL/TLS renegotiation is disabled by default in NSS 3.12.5. This will cause programs
            that attempt to perform renegotiation to experience failures where they formerly
            experienced successes, and is necessary for them to not be vulnerable, until such time
            as a new safe renegotiation scheme is standardized by the IETF.

            If an application depends on renegotiation feature, it can be enabled by setting the
            environment variable NSS_SSL_ENABLE_RENEGOTIATION to 1. By setting this environmental
            variable, the fix provided by these patches will have no effect and the application may
            become vulnerable to the issue.

            This default setting can also be changed within the application by using the following
            existing API functions:

            -  

               -  SECStatus SSL_OptionSet(PRFileDesc \*fd, PRInt32 option, PRBool on)
               -  SECStatus SSL_OptionSetDefault(PRInt32 option, PRBool on)

            -  There is now a new value for "option", which is:

               -  SSL_ENABLE_RENEGOTIATION

               The corresponding new values for SSL_ENABLE_RENEGOTIATION are:

               -  SSL_RENEGOTIATE_NEVER: Never renegotiate at all (default).
               -  SSL_RENEGOTIATE_UNRESTRICTED: Renegotiate without restriction, whether or not the
                  peer's client hello bears the renegotiation info extension (as we always did in
                  the past). **UNSAFE**.

         .. container::
            :name: section_5

            .. rubric:: TLS compression
               :name: TLS_compression

            -  Enable TLS compression with:

               -  SSL_ENABLE_DEFLATE: Enable TLS compression with DEFLATE. Off by default. (See
                  ssl.h)

               Error codes:

               -  SSL_ERROR_DECOMPRESSION_FAILURE (see sslerr.h)
               -  SSL_ERROR_RENEGOTIATION_NOT_ALLOWED (see sslerr.h)

         .. container::
            :name: section_6

            .. rubric:: New context initialization and shutdown functions
               :name: New_context_initialization_and_shutdown_functions

            -  See nss.h for details. The 2 new functions are:

               -  NSS_InitContext
               -  NSS_ShutdownContext

               Parameters for these functions are used to initialize softoken. These are mostly
               strings used to internationalize softoken. Memory for the strings are owned by the
               caller, who is free to free them once NSS_ContextInit returns. If the string
               parameter is NULL (as opposed to empty, zero length), then the softoken default is
               used. These are equivalent to the parameters for PK11_ConfigurePKCS11().

               See the following struct in nss.h for details:

               -  NSSInitParametersStr

         .. container::
            :name: section_7

            .. rubric:: Other new functions
               :name: Other_new_functions

            -  *In secmod.h:*

               -  SECMOD_GetSkipFirstFlag
               -  SECMOD_GetDefaultModDBFlag

               *In prlink.h*

               -  NSS_SecureMemcmp
               -  PORT_LoadLibraryFromOrigin

         .. container::
            :name: section_8

            .. rubric:: Modified functions
               :name: Modified_functions

            -  SGN_Update (see cryptohi.h)

               -  The parameter "input" of this function is changed from *unsigned char \** to
                  *const unsigned char \**.

            -  PK11_ConfigurePKCS11 (see nss.h)

               -  The name of some parameters have been slightly changed ("des" became "desc").

         .. container::
            :name: section_9

            .. rubric:: Deprecated headers
               :name: Deprecated_headers

            -  The header file key.h is deprecated. Please use keyhi.h instead.

         .. container::
            :name: section_10

            .. rubric:: Additional documentation
               :name: Additional_documentation

            -  *In pk11pub.h:*

               -  The caller of PK11_DEREncodePublicKey should free the returned SECItem with a
                  SECITEM_FreeItem(..., PR_TRUE) call.
               -  PK11_ReadRawAttribute allocates the buffer for returning the attribute value. The
                  caller of PK11_ReadRawAttribute should free the data buffer pointed to by item
                  using a SECITEM_FreeItem(item, PR_FALSE) or PORT_Free(item->data) call.

               *In secasn1.h:*

               -  If both pool and dest are NULL, the caller should free the returned SECItem with a
                  SECITEM_FreeItem(..., PR_TRUE) call. If pool is NULL but dest is not NULL, the
                  caller should free the data buffer pointed to by dest with a
                  SECITEM_FreeItem(dest, PR_FALSE) or PORT_Free(dest->data) call.

         .. container::
            :name: section_11

            .. rubric:: Environment variables
               :name: Environment_variables

            -  NSS_FIPS

               -  Will start NSS in FIPS mode.

            -  NSS_SSL_ENABLE_RENEGOTIATION
            -  NSS_SSL_REQUIRE_SAFE_NEGOTIATION

               -  See SSL3 & TLS Renegotiation Vulnerability.

      .. container::
         :name: section_12

         .. rubric:: Bugs Fixed
            :name: Bugs_Fixed

         The following bugs have been fixed in NSS 3.12.5.

         -  `Bug 510435 <https://bugzilla.mozilla.org/show_bug.cgi?id=510435>`__: Remove unused make
            variable DSO_LDFLAGS
         -  `Bug 510436 <https://bugzilla.mozilla.org/show_bug.cgi?id=510436>`__: Add macros for
            build numbers (4th component of version number) to nssutil.h
         -  `Bug 511227 <https://bugzilla.mozilla.org/show_bug.cgi?id=511227>`__: Firefox 3.0.13
            fails to compile on FreeBSD/powerpc
         -  `Bug 511312 <https://bugzilla.mozilla.org/show_bug.cgi?id=511312>`__: NSS fails to load
            softoken, looking for sqlite3.dll
         -  `Bug 511781 <https://bugzilla.mozilla.org/show_bug.cgi?id=511781>`__: Add new TLS 1.2
            cipher suites implemented in Windows 7 to ssltap
         -  `Bug 516101 <https://bugzilla.mozilla.org/show_bug.cgi?id=516101>`__: If PK11_ImportCert
            fails, it leaves the certificate undiscoverable by CERT_PKIXVerifyCert
         -  `Bug 518443 <https://bugzilla.mozilla.org/show_bug.cgi?id=518443>`__:
            PK11_ImportAndReturnPrivateKey leaks an arena
         -  `Bug 518446 <https://bugzilla.mozilla.org/show_bug.cgi?id=518446>`__:
            PK11_DEREncodePublicKey leaks a CERTSubjectPublicKeyInfo
         -  `Bug 518457 <https://bugzilla.mozilla.org/show_bug.cgi?id=518457>`__:
            SECKEY_EncodeDERSubjectPublicKeyInfo and PK11_DEREncodePublicKey are duplicate
         -  `Bug 522510 <https://bugzilla.mozilla.org/show_bug.cgi?id=522510>`__: Add deprecated
            comments to key.h and pk11func.h
         -  `Bug 522580 <https://bugzilla.mozilla.org/show_bug.cgi?id=522580>`__: NSS uses
            PORT_Memcmp for comparing secret data.
         -  `Bug 525056 <https://bugzilla.mozilla.org/show_bug.cgi?id=525056>`__: Timing attack
            against ssl3ext.c:ssl3_ServerHandleSessionTicketXtn()
         -  `Bug 526689 <https://bugzilla.mozilla.org/show_bug.cgi?id=526689>`__: SSL3 & TLS
            Renegotiation Vulnerability

      .. container::
         :name: section_13

         .. rubric:: Documentation
            :name: Documentation

         For a list of the primary NSS documentation pages on mozilla.org, see `NSS
         Documentation <https://www.mozilla.org/projects/security/pki/nss/#documentation>`__. New
         and revised documents available since the release of NSS 3.11 include the following:

         -  `Build Instructions <https://dev.mozilla.jp/localmdc/localmdc_5142.html>`__
         -  `NSS Shared DB <http://wiki.mozilla.org/NSS_Shared_DB>`__

      .. container::
         :name: section_14

         .. rubric:: Compatibility
            :name: Compatibility

         NSS 3.12.5 shared libraries are backward compatible with all older NSS 3.x shared
         libraries. A program linked with older NSS 3.x shared libraries will work with NSS 3.12.5
         shared libraries without recompiling or relinking.  Furthermore, applications that restrict
         their use of NSS APIs to the functions listed in `NSS Public
         Functions <https://www.mozilla.org/projects/security/pki/nss/ref/nssfunctions.html>`__ will
         remain compatible with future versions of the NSS shared libraries.

      .. container::
         :name: section_15

         .. rubric:: Feedback
            :name: Feedback

         Bugs discovered should be reported by filing a bug report with `mozilla.org
         Bugzilla <https://bugzilla.mozilla.org/>`__ (product NSS).

   This document was generated by *genma teruaki* on *November 28, 2010* using `texi2html
   1.82 <http://www.nongnu.org/texi2html/>`__.