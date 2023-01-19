.. _mozilla_projects_nss_nss_3_12_1_release_notes_html:

NSS_3.12.1_release_notes.html
=============================

.. _nss_3.12.1_release_notes:

`NSS 3.12.1 Release Notes <#nss_3.12.1_release_notes>`__
--------------------------------------------------------

.. container::

.. _2008-09-05:

`2008-09-05 <#2008-09-05>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Newsgroup: `mozilla.dev.tech.crypto <news://news.mozilla.org/mozilla.dev.tech.crypto>`__

`Contents <#contents>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `Introduction <#introduction>`__
   -  `Distribution Information <#distribution_information>`__
   -  `New in NSS 3.12.1 <#new_in_nss_3.12.1>`__
   -  `Bugs Fixed <#bugs_fixed>`__
   -  `Documentation <#documentation>`__
   -  `Compatibility <#compatibility>`__
   -  `Feedback <#feedback>`__

   --------------

`Introduction <#introduction>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Network Security Services (NSS) 3.12.1 is a patch release for NSS 3.12. The bug fixes in NSS
   3.12.1 are described in the "`Bugs Fixed <#bugsfixed>`__" section below.
   NSS 3.12.1 is tri-licensed under the MPL 1.1/GPL 2.0/LGPL 2.1.

   --------------

.. _distribution_information:

`Distribution Information <#distribution_information>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The CVS tag for the NSS 3.12.1 release is NSS_3_12_1_RTM. NSS 3.12.1 requires `NSPR
   4.7.1 <https://www.mozilla.org/projects/nspr/release-notes/nspr471.html>`__.
   See the `Documentation <#docs>`__ section for the build instructions.
   NSS 3.12.1 source and binary distributions are also available on ftp.mozilla.org for secure HTTPS
   download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_12_1_RTM/src/.
   -  Binary distributions:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_12_1_RTM/. Both debug and
      optimized builds are provided. Go to the subdirectory for your platform, DBG (debug) or OPT
      (optimized), to get the tar.gz or zip file. The tar.gz or zip file expands to an nss-3.12.1
      directory containing three subdirectories:

      -  include - NSS header files
      -  lib - NSS shared libraries
      -  bin - `NSS Tools <https://www.mozilla.org/projects/security/pki/nss/tools/>`__ and test
         programs

   You also need to download the NSPR 4.7.1 binary distributions to get the NSPR 4.7.1 header files
   and shared libraries, which NSS 3.12.1 requires. NSPR 4.7.1 binary distributions are in
   https://ftp.mozilla.org/pub/mozilla.org/nspr/releases/v4.7.1/.

   --------------

.. _new_in_nss_3.12.1:

`New in NSS 3.12.1 <#new_in_nss_3.12.1>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  New functions in the nss shared library:

      CERT_NameToAsciiInvertible (see cert.h)
         Convert an CERTName into its RFC1485 encoded equivalent.
         Returns a string that must be freed with PORT_Free().
         Caller chooses encoding rules.
      CERT_EncodeSubjectKeyID (see cert.h)
         Encode Certificate SKID (Subject Key ID) extension.
      PK11_GetAllSlotsForCert (see pk11pub.h)
         PK11_GetAllSlotsForCert returns all the slots that a given certificate
         exists on, since it's possible for a cert to exist on more than one
         PKCS#11 token.

   -  Levels of standards conformance strictness for CERT_NameToAsciiInvertible (see certt.h)

      CERT_N2A_READABLE
         (maximum human readability)
      CERT_N2A_STRICT
         (strict RFC compliance)
      CERT_N2A_INVERTIBLE
         (maximum invertibility)

   --------------

.. _bugs_fixed:

`Bugs Fixed <#bugs_fixed>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The following bugs have been fixed in NSS 3.12.1.

   -  `Bug 67890 <https://bugzilla.mozilla.org/show_bug.cgi?id=67890>`__: create self-signed cert
      with existing key that signed CSR
   -  `Bug 129303 <https://bugzilla.mozilla.org/show_bug.cgi?id=129303>`__: NSS needs to expose
      interfaces to deal with multiple token sources of certs.
   -  `Bug 311432 <https://bugzilla.mozilla.org/show_bug.cgi?id=311432>`__: ECC's ECL_USE_FP code
      (for Linux x86) fails pairwise consistency test
   -  `Bug 330622 <https://bugzilla.mozilla.org/show_bug.cgi?id=330622>`__: certutil's usage
      messages incorrectly document certain options
   -  `Bug 330628 <https://bugzilla.mozilla.org/show_bug.cgi?id=330628>`__: coreconf/Linux.mk should
      \_not\_ default to x86 but result in an error if host is not recognized
   -  `Bug 359302 <https://bugzilla.mozilla.org/show_bug.cgi?id=359302>`__: Remove the sslsample
      code from NSS source tree
   -  `Bug 372241 <https://bugzilla.mozilla.org/show_bug.cgi?id=372241>`__: Need more versatile form
      of CERT_NameToAscii
   -  `Bug 390296 <https://bugzilla.mozilla.org/show_bug.cgi?id=390296>`__: NSS ignores subject CN
      even when SAN contains no dNSName
   -  `Bug 401928 <https://bugzilla.mozilla.org/show_bug.cgi?id=401928>`__: Support generalized
      PKCS#5 v2 PBEs
   -  `Bug 403543 <https://bugzilla.mozilla.org/show_bug.cgi?id=403543>`__: pkix: need a way to
      enable/disable AIA cert fetching
   -  `Bug 408847 <https://bugzilla.mozilla.org/show_bug.cgi?id=408847>`__: pkix_OcspChecker_Check
      does not support specified responder (and given signercert)
   -  `Bug 414003 <https://bugzilla.mozilla.org/show_bug.cgi?id=414003>`__: Crash [[@
      CERT_DecodeCertPackage] sometimes with this testcase
   -  `Bug 415167 <https://bugzilla.mozilla.org/show_bug.cgi?id=415167>`__: Memory leak in certutil
   -  `Bug 417399 <https://bugzilla.mozilla.org/show_bug.cgi?id=417399>`__: Arena Allocation results
      are not checked in pkix_pl_InfoAccess_ParseLocation
   -  `Bug 420644 <https://bugzilla.mozilla.org/show_bug.cgi?id=420644>`__: Improve SSL tracing of
      key derivation
   -  `Bug 426886 <https://bugzilla.mozilla.org/show_bug.cgi?id=426886>`__: Use const char\* in
      PK11_ImportCertForKey
   -  `Bug 428103 <https://bugzilla.mozilla.org/show_bug.cgi?id=428103>`__: CERT_EncodeSubjectKeyID
      is not defined in any public header file
   -  `Bug 429716 <https://bugzilla.mozilla.org/show_bug.cgi?id=429716>`__: debug builds of libPKIX
      unconditionally dump socket traffic to stdout
   -  `Bug 430368 <https://bugzilla.mozilla.org/show_bug.cgi?id=430368>`__: vfychain -t option is
      undocumented
   -  `Bug 430369 <https://bugzilla.mozilla.org/show_bug.cgi?id=430369>`__: vfychain -o succeeds
      even if -pp is not specified
   -  `Bug 430399 <https://bugzilla.mozilla.org/show_bug.cgi?id=430399>`__: vfychain -pp crashes
   -  `Bug 430405 <https://bugzilla.mozilla.org/show_bug.cgi?id=430405>`__: Error log is not
      produced by CERT_PKIXVerifyCert
   -  `Bug 430743 <https://bugzilla.mozilla.org/show_bug.cgi?id=430743>`__: Update ssltap to
      understand the TLS session ticket extension
   -  `Bug 430859 <https://bugzilla.mozilla.org/show_bug.cgi?id=430859>`__: PKIX: Policy mapping
      fails verification with error invalid arguments
   -  `Bug 430875 <https://bugzilla.mozilla.org/show_bug.cgi?id=430875>`__: Document the policy for
      the order of cipher suites in SSL_ImplementedCiphers.
   -  `Bug 430916 <https://bugzilla.mozilla.org/show_bug.cgi?id=430916>`__: add sustaining asserts
   -  `Bug 431805 <https://bugzilla.mozilla.org/show_bug.cgi?id=431805>`__: leak in
      NSSArena_Destroy()
   -  `Bug 431929 <https://bugzilla.mozilla.org/show_bug.cgi?id=431929>`__: Memory leaks on error
      paths in devutil.c
   -  `Bug 432303 <https://bugzilla.mozilla.org/show_bug.cgi?id=432303>`__: Replace PKIX_PL_Memcpy
      with memcpy
   -  `Bug 433177 <https://bugzilla.mozilla.org/show_bug.cgi?id=433177>`__: Fix the GCC compiler
      warnings in lib/util and lib/freebl
   -  `Bug 433437 <https://bugzilla.mozilla.org/show_bug.cgi?id=433437>`__: vfychain ignores the -a
      option
   -  `Bug 433594 <https://bugzilla.mozilla.org/show_bug.cgi?id=433594>`__: Crash destroying OCSP
      Cert ID [[@ CERT_DestroyOCSPCertID ]
   -  `Bug 434099 <https://bugzilla.mozilla.org/show_bug.cgi?id=434099>`__: NSS relies on unchecked
      PKCS#11 object attribute values
   -  `Bug 434187 <https://bugzilla.mozilla.org/show_bug.cgi?id=434187>`__: Fix the GCC compiler
      warnings in nss/lib
   -  `Bug 434398 <https://bugzilla.mozilla.org/show_bug.cgi?id=434398>`__: libPKIX cannot find
      issuer cert immediately after checking it with OCSP
   -  `Bug 434808 <https://bugzilla.mozilla.org/show_bug.cgi?id=434808>`__: certutil -B deadlock
      when importing two or more roots
   -  `Bug 434860 <https://bugzilla.mozilla.org/show_bug.cgi?id=434860>`__: Coverity 1150 - dead
      code in ocsp_CreateCertID
   -  `Bug 436428 <https://bugzilla.mozilla.org/show_bug.cgi?id=436428>`__: remove unneeded assert
      from sec_PKCS7EncryptLength
   -  `Bug 436430 <https://bugzilla.mozilla.org/show_bug.cgi?id=436430>`__: Make NSS public headers
      compilable with NO_NSPR_10_SUPPORT defined
   -  `Bug 436577 <https://bugzilla.mozilla.org/show_bug.cgi?id=436577>`__: uninitialized variable
      in sec_pkcs5CreateAlgorithmID
   -  `Bug 438685 <https://bugzilla.mozilla.org/show_bug.cgi?id=438685>`__: libpkix doesn't try all
      the issuers in a bridge with multiple certs
   -  `Bug 438876 <https://bugzilla.mozilla.org/show_bug.cgi?id=438876>`__: signtool is still using
      static libraries.
   -  `Bug 439123 <https://bugzilla.mozilla.org/show_bug.cgi?id=439123>`__: Assertion failure in
      libpkix at shutdown
   -  `Bug 440062 <https://bugzilla.mozilla.org/show_bug.cgi?id=440062>`__: incorrect list element
      count in PKIX_List_AppendItem function
   -  `Bug 442618 <https://bugzilla.mozilla.org/show_bug.cgi?id=442618>`__: Eliminate dead function
      CERT_CertPackageType
   -  `Bug 443755 <https://bugzilla.mozilla.org/show_bug.cgi?id=443755>`__: Extra semicolon in
      PKM_TLSKeyAndMacDerive makes conditional code unconditional
   -  `Bug 443760 <https://bugzilla.mozilla.org/show_bug.cgi?id=443760>`__: Extra semicolon in
      SeqDatabase makes static analysis tool suspicious
   -  `Bug 448323 <https://bugzilla.mozilla.org/show_bug.cgi?id=448323>`__: certutil -K doesn't
      report the token and slot names for found keys
   -  `Bug 448324 <https://bugzilla.mozilla.org/show_bug.cgi?id=448324>`__: ocsp checker returns
      incorrect error code on request with invalid signing cert
   -  `Bug 449146 <https://bugzilla.mozilla.org/show_bug.cgi?id=449146>`__: Remove dead libsec
      function declarations
   -  `Bug 453227 <https://bugzilla.mozilla.org/show_bug.cgi?id=453227>`__: installation of
      PEM-encoded certificate without trailing newline fails

   --------------

`Documentation <#documentation>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   For a list of the primary NSS documentation pages on mozilla.org, see `NSS
   Documentation <../index.html#Documentation>`__. New and revised documents available since the
   release of NSS 3.11 include the following:

   -  `Build Instructions for NSS 3.11.4 and above <../nss-3.11.4/nss-3.11.4-build.html>`__
   -  `NSS Shared DB <http://wiki.mozilla.org/NSS_Shared_DB>`__

   --------------

`Compatibility <#compatibility>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   NSS 3.12.1 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.12.1 shared libraries
   without recompiling or relinking.  Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in `NSS Public Functions <../ref/nssfunctions.html>`__ will remain
   compatible with future versions of the NSS shared libraries.

   --------------

`Feedback <#feedback>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Bugs discovered should be reported by filing a bug report with `mozilla.org
   Bugzilla <https://bugzilla.mozilla.org/>`__ (product NSS).