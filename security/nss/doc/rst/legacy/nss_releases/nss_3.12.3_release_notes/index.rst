.. _mozilla_projects_nss_nss_3_12_3_release_notes:

NSS_3.12.3_release_notes.html
=============================

.. _nss_3.12.3_release_notes:

`NSS 3.12.3 Release Notes <#nss_3.12.3_release_notes>`__
--------------------------------------------------------

.. _2009-04-01:

`2009-04-01 <#2009-04-01>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Newsgroup: `mozilla.dev.tech.crypto <news://news.mozilla.org/mozilla.dev.tech.crypto>`__

`Contents <#contents>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `Introduction <#introduction>`__
   -  `Distribution Information <#distribution_information>`__
   -  `New in NSS 3.12.3 <#new_in_nss_3.12.3>`__
   -  `Bugs Fixed <#bugs_fixed>`__
   -  `Documentation <#documentation>`__
   -  `Compatibility <#compatibility>`__
   -  `Feedback <#feedback>`__

   --------------

`Introduction <#introduction>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Network Security Services (NSS) 3.12.3 is a patch release for NSS 3.12. The bug fixes in NSS
   3.12.3 are described in the "`Bugs Fixed <#bugs_fixed>`__" section below.

   NSS 3.12.3 is tri-licensed under the MPL 1.1/GPL 2.0/LGPL 2.1.

   --------------

.. _distribution_information:

`Distribution Information <#distribution_information>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   | The CVS tag for the NSS 3.12.3 release is NSS_3_12_3_RTM.  NSS 3.12.3 requires `NSPR
     4.7.4 <https://www.mozilla.org/projects/nspr/release-notes/nspr474.html>`__.
   | See the `Documentation <#documentation>`__ section for the build instructions.

   NSS 3.12.3 source and binary distributions are also available on ftp.mozilla.org for secure HTTPS
   download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_12_3_RTM/src/.
   -  Binary distributions:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_12_3_RTM/. Both debug and
      optimized builds are provided. Go to the subdirectory for your platform, DBG (debug) or OPT
      (optimized), to get the tar.gz or zip file. The tar.gz or zip file expands to an nss-3.12.3
      directory containing three subdirectories:

      -  include - NSS header files
      -  lib - NSS shared libraries
      -  bin - `NSS Tools <https://www.mozilla.org/projects/security/pki/nss/tools/>`__ and test
         programs

   You also need to download the NSPR 4.7.4 binary distributions to get the NSPR 4.7.4 header files
   and shared libraries, which NSS 3.12.3 requires. NSPR 4.7.4 binary distributions are in
   https://ftp.mozilla.org/pub/mozilla.org/nspr/releases/v4.7.4/.

   --------------

.. _new_in_nss_3.12.3:

`New in NSS 3.12.3 <#new_in_nss_3.12.3>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  Changes in behavior:
   -  In the development of NSS 3.12.3, it became necessary to change some old library behaviors due
      to the discovery of certain vulnerabilities in the old behaviors, and to correct some errors
      that had limited NSS's ability to interoperate with cryptographic hardware and software from
      other sources.
      Most of these changes should cause NO problems for NSS users, but in some cases, some
      customers' software, hardware and/or certificates may be dependent on the old behaviors, and
      may have difficulty with the new behaviors. In anticipation of that, the NSS team has provided
      ways to easily cause NSS to revert to its previous behavior through the use of environment
      variables.
      Here is a table of the new environment variables introduced in NSS 3.12.3 and information
      about how they affect these new behaviors. The information in this table is excerpted from
      :ref:`mozilla_projects_nss_reference_nss_environment_variables`

      +--------------------------------+--------------------------------+--------------------------------+
      | **Environment Variable**       | **Value Type**                 | **Description**                |
      +--------------------------------+--------------------------------+--------------------------------+
      | NSRANDCOUNT                    | Integer                        | Sets the maximum number of     |
      |                                | (byte count)                   | bytes to read from the file    |
      |                                |                                | named in the environment       |
      |                                |                                | variable NSRANDFILE (see       |
      |                                |                                | below). Makes NSRANDFILE       |
      |                                |                                | usable with /dev/urandom.      |
      +--------------------------------+--------------------------------+--------------------------------+
      | NSS_ALLOW_WEAK_SIGNATURE_ALG   | Boolean                        | Enables the use of MD2 and MD4 |
      |                                | (any non-empty value to        | hash algorithms inside         |
      |                                | enable)                        | signatures. This was allowed   |
      |                                |                                | by default before NSS 3.12.3.  |
      +--------------------------------+--------------------------------+--------------------------------+
      | NSS_HASH_ALG_SUPPORT           | String                         | Specifies algorithms allowed   |
      |                                |                                | to be used in certain          |
      |                                |                                | applications, such as in       |
      |                                |                                | signatures on certificates and |
      |                                |                                | CRLs. See documentation at     |
      |                                |                                | `this                          |
      |                                |                                | link                           |
      |                                |                                | <https://bugzilla.mozilla.org/ |
      |                                |                                | show_bug.cgi?id=483113#c0>`__. |
      +--------------------------------+--------------------------------+--------------------------------+
      | NSS_STRICT_NOFORK              | String                         | It is an error to try to use a |
      |                                | ("1",                          | PKCS#11 crypto module in a     |
      |                                | "DISABLED",                    | process before it has been     |
      |                                | or any other non-empty value)  | initialized in that process,   |
      |                                |                                | even if the module was         |
      |                                |                                | initialized in the parent      |
      |                                |                                | process. Beginning in NSS      |
      |                                |                                | 3.12.3, Softoken will detect   |
      |                                |                                | this error. This environment   |
      |                                |                                | variable controls Softoken's   |
      |                                |                                | response to that error.        |
      |                                |                                |                                |
      |                                |                                | -  If set to "1" or unset,     |
      |                                |                                |    Softoken will trigger an    |
      |                                |                                |    assertion failure in debug  |
      |                                |                                |    builds, and will report an  |
      |                                |                                |    error in non-DEBUG builds.  |
      |                                |                                | -  If set to "DISABLED",       |
      |                                |                                |    Softoken will ignore forks, |
      |                                |                                |    and behave as it did in     |
      |                                |                                |    older versions.             |
      |                                |                                | -  If set to any other         |
      |                                |                                |    non-empty value, Softoken   |
      |                                |                                |    will report an error in     |
      |                                |                                |    both DEBUG and non-DEBUG    |
      |                                |                                |    builds.                     |
      +--------------------------------+--------------------------------+--------------------------------+
      | NSS_USE_DECODED_CKA_EC_POINT   | Boolean                        | Tells NSS to send EC key       |
      |                                | (any non-empty value to        | points across the PKCS#11      |
      |                                | enable)                        | interface in the non-standard  |
      |                                |                                | unencoded format that was used |
      |                                |                                | by default before NSS 3.12.3.  |
      |                                |                                | The new key point format is a  |
      |                                |                                | DER encoded ASN.1 OCTET        |
      |                                |                                | STRING.                        |
      +--------------------------------+--------------------------------+--------------------------------+
      | NSS_USE_SHEXP_IN_CERT_NAME     | Boolean                        | Tells NSS to allow shell-style |
      |                                | (any non-empty value to        | wildcard patterns in           |
      |                                | enable)                        | certificates to match SSL      |
      |                                |                                | server host names. This        |
      |                                |                                | behavior was the default       |
      |                                |                                | before NSS 3.12.3. The new     |
      |                                |                                | behavior conforms to RFC 2818. |
      +--------------------------------+--------------------------------+--------------------------------+

   -  New Korean SEED cipher:

      -  New macros for SEED support:

         -  *in blapit.h:*
            NSS_SEED
            NSS_SEED_CBC
            SEED_BLOCK_SIZE
            SEED_KEY_LENGTH
            *in pkcs11t.h:*
            CKK_SEED
            CKM_SEED_KEY_GEN
            CKM_SEED_ECB
            CKM_SEED_CBC
            CKM_SEED_MAC
            CKM_SEED_MAC_GENERAL
            CKM_SEED_CBC_PAD
            CKM_SEED_ECB_ENCRYPT_DATA
            CKM_SEED_CBC_ENCRYPT_DATA
            *in secmod.h:*
            PUBLIC_MECH_SEED_FLAG
            *in secmodt.h:*
            SECMOD_SEED_FLAG
            *in secoidt.h:*
            SEC_OID_SEED_CBC
            *in sslproto.h:*
            TLS_RSA_WITH_SEED_CBC_SHA
            *in sslt.h:*
            ssl_calg_seed

      -  New structure for SEED support:

         -  (see blapit.h)
            SEEDContextStr
            SEEDContext

   -  New functions in the nss shared library:

      -  CERT_RFC1485_EscapeAndQuote (see cert.h)
         CERT_CompareCerts (see cert.h)
         CERT_RegisterAlternateOCSPAIAInfoCallBack (see ocsp.h)
         PK11_GetSymKeyHandle (see pk11pqg.h)
         UTIL_SetForkState (see secoid.h)
         NSS_GetAlgorithmPolicy (see secoid.h)
         NSS_SetAlgorithmPolicy (see secoid.h)

         -  For the 2 functions above see also (in secoidt.h):
            NSS_USE_ALG_IN_CERT_SIGNATURE
            NSS_USE_ALG_IN_CMS_SIGNATURE
            NSS_USE_ALG_RESERVED

   -  Support for the Watcom C compiler is removed

      -  The file watcomfx.h is removed.

   --------------

.. _bugs_fixed:

`Bugs Fixed <#bugs_fixed>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The following bugs have been fixed in NSS 3.12.3.

   -  `Bug 159483 <https://bugzilla.mozilla.org/show_bug.cgi?id=159483>`__: cert name matching: RFC
      2818 vs. backwards compatibility (wildcards)
   -  `Bug 334678 <https://bugzilla.mozilla.org/show_bug.cgi?id=334678>`__: prng_fips1861.c
      redefines the macro BSIZE on HP-UX
   -  `Bug 335016 <https://bugzilla.mozilla.org/show_bug.cgi?id=335016>`__: mpp_pprime (Miller-Rabin
      probabilistic primality test) may choose 0 or 1 as the random integer
   -  `Bug 347037 <https://bugzilla.mozilla.org/show_bug.cgi?id=347037>`__: Make shlibsign depend on
      the softoken only
   -  `Bug 371522 <https://bugzilla.mozilla.org/show_bug.cgi?id=371522>`__: Auto-Update of CRLs
      stops after first update
   -  `Bug 380784 <https://bugzilla.mozilla.org/show_bug.cgi?id=380784>`__: PK11MODE in non FIPS
      mode failed.
   -  `Bug 394077 <https://bugzilla.mozilla.org/show_bug.cgi?id=394077>`__: libpkix need to return
      revocation status of a cert
   -  `Bug 412468 <https://bugzilla.mozilla.org/show_bug.cgi?id=412468>`__: modify certutil
   -  `Bug 417092 <https://bugzilla.mozilla.org/show_bug.cgi?id=417092>`__: Modify pkix_CertSelector
      API to return an error if cert was rejected.
   -  `Bug 426413 <https://bugzilla.mozilla.org/show_bug.cgi?id=426413>`__: Audit messages need
      distinct types
   -  `Bug 438870 <https://bugzilla.mozilla.org/show_bug.cgi?id=438870>`__: Free Freebl hashing code
      of dependencies on NSPR and libUtil
   -  `Bug 439115 <https://bugzilla.mozilla.org/show_bug.cgi?id=439115>`__: DB merge allows nickname
      conflicts in merged DB
   -  `Bug 439199 <https://bugzilla.mozilla.org/show_bug.cgi?id=439199>`__: SSE2 instructions for
      bignum are not implemented on Windows 32-bit
   -  `Bug 441321 <https://bugzilla.mozilla.org/show_bug.cgi?id=441321>`__: Tolerate incorrect
      encoding of DSA signatures in SSL 3.0 handshakes
   -  `Bug 444404 <https://bugzilla.mozilla.org/show_bug.cgi?id=444404>`__: libpkix reports unknown
      issuer for nearly all certificate errors
   -  `Bug 452391 <https://bugzilla.mozilla.org/show_bug.cgi?id=452391>`__: certutil -K incorrectly
      reports ec private key as an orphan
   -  `Bug 453234 <https://bugzilla.mozilla.org/show_bug.cgi?id=453234>`__: Support for SEED Cipher
      Suites to TLS RFC4010
   -  `Bug 453364 <https://bugzilla.mozilla.org/show_bug.cgi?id=453364>`__: Improve PK11_CipherOp
      error reporting (was: PK11_CreateContextBySymKey returns NULL
   -  `Bug 456406 <https://bugzilla.mozilla.org/show_bug.cgi?id=456406>`__: Slot list leaks in
      symkeyutil
   -  `Bug 461085 <https://bugzilla.mozilla.org/show_bug.cgi?id=461085>`__: RFE: export function
      CERT_CompareCerts
   -  `Bug 462293 <https://bugzilla.mozilla.org/show_bug.cgi?id=462293>`__: Crash on fork after
      Softoken is dlClose'd on some Unix platforms in NSS 3.12
   -  `Bug 463342 <https://bugzilla.mozilla.org/show_bug.cgi?id=463342>`__: move some headers to
      freebl/softoken
   -  `Bug 463452 <https://bugzilla.mozilla.org/show_bug.cgi?id=463452>`__: SQL DB creation does not
      set files protections to 0600
   -  `Bug 463678 <https://bugzilla.mozilla.org/show_bug.cgi?id=463678>`__: Need to add RPATH to
      64-bit libraries on HP-UX
   -  `Bug 464088 <https://bugzilla.mozilla.org/show_bug.cgi?id=464088>`__: Option to build NSS
      without dbm (handy for WinCE)
   -  `Bug 464223 <https://bugzilla.mozilla.org/show_bug.cgi?id=464223>`__: Certutil didn't accept
      certificate request to sign.
   -  `Bug 464406 <https://bugzilla.mozilla.org/show_bug.cgi?id=464406>`__: Fix signtool regressions
   -  `Bug 465270 <https://bugzilla.mozilla.org/show_bug.cgi?id=465270>`__: uninitialised value in
      devutil.c::create_object()
   -  `Bug 465273 <https://bugzilla.mozilla.org/show_bug.cgi?id=465273>`__: dead assignment in
      devutil.c::nssSlotArray_Clone()
   -  `Bug 465926 <https://bugzilla.mozilla.org/show_bug.cgi?id=465926>`__: During import of PKCS
      #12 files
   -  `Bug 466180 <https://bugzilla.mozilla.org/show_bug.cgi?id=466180>`__:
      SSL_ConfigMPServerSIDCache with default parameters fails on {Net
   -  `Bug 466194 <https://bugzilla.mozilla.org/show_bug.cgi?id=466194>`__: CERT_DecodeTrustString
      should take a const char \* input trusts string.
   -  `Bug 466736 <https://bugzilla.mozilla.org/show_bug.cgi?id=466736>`__: Incorrect use of
      NSS_USE_64 in lib/libpkix/pkix_pl_nss/system/pkix_pl_object.c
   -  `Bug 466745 <https://bugzilla.mozilla.org/show_bug.cgi?id=466745>`__: random number generator
      fails on windows ce
   -  `Bug 467298 <https://bugzilla.mozilla.org/show_bug.cgi?id=467298>`__: SQL DB code uses local
      cache on local file system
   -  `Bug 468279 <https://bugzilla.mozilla.org/show_bug.cgi?id=468279>`__: softoken crash importing
      email cert into newly upgraded DB
   -  `Bug 468532 <https://bugzilla.mozilla.org/show_bug.cgi?id=468532>`__: Trusted CA trust flags
      not being honored in CERT_VerifyCert
   -  `Bug 469583 <https://bugzilla.mozilla.org/show_bug.cgi?id=469583>`__: Coverity: uninitialized
      variable used in sec_pkcs5CreateAlgorithmID
   -  `Bug 469944 <https://bugzilla.mozilla.org/show_bug.cgi?id=469944>`__: when built with
      Microsoft compilers
   -  `Bug 470351 <https://bugzilla.mozilla.org/show_bug.cgi?id=470351>`__: crlutil build fails on
      Windows because it calls undeclared isatty
   -  `Bug 471539 <https://bugzilla.mozilla.org/show_bug.cgi?id=471539>`__: Stop honoring digital
      signatures in certificates and CRLs based on weak hashes
   -  `Bug 471665 <https://bugzilla.mozilla.org/show_bug.cgi?id=471665>`__: NSS reports incorrect
      sizes for (AES) symmetric keys
   -  `Bug 471715 <https://bugzilla.mozilla.org/show_bug.cgi?id=471715>`__: Add cert to nssckbi to
      override rogue md5-collision CA cert
   -  `Bug 472291 <https://bugzilla.mozilla.org/show_bug.cgi?id=472291>`__: crash in libpkix object
      leak tests due to null pointer dereferencing in pkix_build.c:3218.
   -  `Bug 472319 <https://bugzilla.mozilla.org/show_bug.cgi?id=472319>`__: Vfychain validates chain
      even if revoked certificate.
   -  `Bug 472749 <https://bugzilla.mozilla.org/show_bug.cgi?id=472749>`__: Softoken permits AES
      keys of ANY LENGTH to be created
   -  `Bug 473147 <https://bugzilla.mozilla.org/show_bug.cgi?id=473147>`__: pk11mode tests fails on
      AIX when using shareable DBs.
   -  `Bug 473357 <https://bugzilla.mozilla.org/show_bug.cgi?id=473357>`__: ssltap incorrectly
      parses handshake messages that span record boundaries
   -  `Bug 473365 <https://bugzilla.mozilla.org/show_bug.cgi?id=473365>`__: Incompatible argument in
      pkix_validate.c.
   -  `Bug 473505 <https://bugzilla.mozilla.org/show_bug.cgi?id=473505>`__: softoken's C_Initialize
      and C_Finalize should succeed after a fork in a child process
   -  `Bug 473944 <https://bugzilla.mozilla.org/show_bug.cgi?id=473944>`__: Trust anchor is not
      trusted when requireFreshInfo flag is set.
   -  `Bug 474532 <https://bugzilla.mozilla.org/show_bug.cgi?id=474532>`__: Softoken cannot import
      certs with empty subjects and non-empty nicknames
   -  `Bug 474777 <https://bugzilla.mozilla.org/show_bug.cgi?id=474777>`__: Wrong deallocation when
      modifying CRL.
   -  `Bug 476126 <https://bugzilla.mozilla.org/show_bug.cgi?id=476126>`__: CERT_AsciiToName fails
      when AVAs in an RDN are separated by '+'
   -  `Bug 477186 <https://bugzilla.mozilla.org/show_bug.cgi?id=477186>`__: Infinite loop in
      CERT_GetCertChainFromCert
   -  `Bug 477777 <https://bugzilla.mozilla.org/show_bug.cgi?id=477777>`__: Selfserv crashed in
      client/server tests.
   -  `Bug 478171 <https://bugzilla.mozilla.org/show_bug.cgi?id=478171>`__: Consolidate the
      coreconf/XXX.mk files for Windows
   -  `Bug 478563 <https://bugzilla.mozilla.org/show_bug.cgi?id=478563>`__: Add \_MSC_VER (the cl
      version) to coreconf.
   -  `Bug 478724 <https://bugzilla.mozilla.org/show_bug.cgi?id=478724>`__: NSS build fails on
      Windows since 20090213.1 nightly build.
   -  `Bug 478931 <https://bugzilla.mozilla.org/show_bug.cgi?id=478931>`__: object leak in
      pkix_List_MergeLists function
   -  `Bug 478994 <https://bugzilla.mozilla.org/show_bug.cgi?id=478994>`__: Allow Softoken's fork
      check to be disabled
   -  `Bug 479029 <https://bugzilla.mozilla.org/show_bug.cgi?id=479029>`__: OCSP Response signature
      cert found invalid if issuer is trusted only for SSL
   -  `Bug 479601 <https://bugzilla.mozilla.org/show_bug.cgi?id=479601>`__: Wrong type (UTF8 String)
      for email addresses in subject by CERT_AsciiToName
   -  `Bug 480142 <https://bugzilla.mozilla.org/show_bug.cgi?id=480142>`__: Use sizeof on the
      correct type of ckc_x509 in lib/ckfw
   -  `Bug 480257 <https://bugzilla.mozilla.org/show_bug.cgi?id=480257>`__: OCSP fails when response
      > 1K Byte
   -  `Bug 480280 <https://bugzilla.mozilla.org/show_bug.cgi?id=480280>`__: The CKA_EC_POINT PKCS#11
      attribute is encoded in the wrong way: missing encapsulating octet string
   -  `Bug 480442 <https://bugzilla.mozilla.org/show_bug.cgi?id=480442>`__: Remove (empty)
      watcomfx.h from nss
   -  `Bug 481216 <https://bugzilla.mozilla.org/show_bug.cgi?id=481216>`__: Fix specific spelling
      errors in NSS
   -  `Bug 482702 <https://bugzilla.mozilla.org/show_bug.cgi?id=482702>`__: OCSP test with revoked
      CA cert validated as good.
   -  `Bug 483113 <https://bugzilla.mozilla.org/show_bug.cgi?id=483113>`__: add environment variable
      to disable/enable hash algorithms in cert/CRL signatures
   -  `Bug 483168 <https://bugzilla.mozilla.org/show_bug.cgi?id=483168>`__: NSS Callback API for
      looking up a default OCSP Responder URL
   -  `Bug 483963 <https://bugzilla.mozilla.org/show_bug.cgi?id=483963>`__: Assertion failure in
      OCSP tests.
   -  `Bug 484425 <https://bugzilla.mozilla.org/show_bug.cgi?id=484425>`__: Need accessor function
      to retrieve SymKey handle
   -  `Bug 484466 <https://bugzilla.mozilla.org/show_bug.cgi?id=484466>`__: sec_error_invalid_args
      with NSS_ENABLE_PKIX_VERIFY=1
   -  `Bug 485127 <https://bugzilla.mozilla.org/show_bug.cgi?id=485127>`__: bltest crashes when
      attempting rc5_cbc or rc5_ecb
   -  `Bug 485140 <https://bugzilla.mozilla.org/show_bug.cgi?id=485140>`__: Wrong command line flags
      used to build intel-aes.s with Solaris gas for x86_64
   -  `Bug 485370 <https://bugzilla.mozilla.org/show_bug.cgi?id=485370>`__: crash
   -  `Bug 485713 <https://bugzilla.mozilla.org/show_bug.cgi?id=485713>`__: Files added by Red Hat
      recently have missing texts in license headers.
   -  `Bug 485729 <https://bugzilla.mozilla.org/show_bug.cgi?id=485729>`__: Remove
      lib/freebl/mapfile.Solaris
   -  `Bug 485837 <https://bugzilla.mozilla.org/show_bug.cgi?id=485837>`__: vc90.pdb files are
      output in source directory instead of OBJDIR
   -  `Bug 486060 <https://bugzilla.mozilla.org/show_bug.cgi?id=486060>`__: sec_asn1d_parse_leaf
      uses argument uninitialized by caller pbe_PK11AlgidToParam

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

   NSS 3.12.3 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.12.3 shared libraries
   without recompiling or relinking.  Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in `NSS Public Functions <../ref/nssfunctions.html>`__ will remain
   compatible with future versions of the NSS shared libraries.

   --------------

`Feedback <#feedback>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   | Bugs discovered should be reported by filing a bug report with `mozilla.org
     Bugzilla <https://bugzilla.mozilla.org/>`__ (product NSS).