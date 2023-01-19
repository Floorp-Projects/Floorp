.. _mozilla_projects_nss_nss_3_12_4_release_notes:

NSS 3.12.4 release notes
========================

.. container::

   .. code::

      2009-08-20

   *Newsgroup:*\ `mozilla.dev.tech.crypto <news://news.mozilla.org/mozilla.dev.tech.crypto>`__
   .. rubric:: Introduction
      :name: Introduction

   Network Security Services (NSS) 3.12.4 is a patch release for NSS 3.12. The bug fixes in NSS
   3.12.4 are described in the "`Bugs Fixed <#bugsfixed>`__" section below.

   NSS 3.12.4 is tri-licensed under the MPL 1.1/GPL 2.0/LGPL 2.1.

   .. rubric:: Distribution Information
      :name: Distribution_Information

   This release is built from the source, at the CVS repository rooted at cvs.mozilla.org:/cvsroot,
   with the CVS tag ``NSS_3_12_4_RTM``. 

   NSS 3.12.4 requires `NSPR 4.8 <https://www.mozilla.org/projects/nspr/release-notes/>`__. This is
   not a hard requirement. Our QA tested NSS 3.12.4 with NSPR 4.8, but it should work with NSPR
   4.7.1 or later.

   You can check out the source from CVS by

   .. note::

      cvs co -r NSPR_4_8_RTM NSPR
      cvs co -r NSS_3_12_4_RTM NSS

   See the `Documentation <#docs>`__ section for the build instructions.

   NSS 3.12.4 source is also available on ``ftp.mozilla.org`` for secure HTTPS download:

   -  Source tarball:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_12_4_RTM/src/.

   .. rubric:: Major changes in NSS 3.12.4
      :name: Major_changes_in_NSS_3.12.4

   -  NSS 3.12.4 is the version that we submitted to NIST for FIPS 140-2 validation.
      Currently NSS 3.12.4 is in the "Review Pending" state in the FIPS 140-2 pre-validation
      list at http://csrc.nist.gov/groups/STM/cmvp/documents/140-1/140InProcess.pdf
   -  Added CRL Distribution Point support (see cert.h).
      **CERT_DecodeCRLIssuingDistributionPoint**
      **CERT_FindCRLIssuingDistPointExten**
   -  The old documentation of the expression matching syntax rules was
      incorrect, and the new corrected documentation is as follows for
      public nssutil functions (see portreq.h):

      -  **PORT_RegExpValid**
      -  **PORT_RegExpSearch**
      -  **PORT_RegExpCaseSearch**

   -  These functions will match a string with a shell expression. The expressions
      accepted are based loosely on the expressions accepted by zsh.
      Expected return values:

      -  NON_SXP if exp is a standard string
      -  INVALID_SXP if exp is a shell expression, but invalid
      -  VALID_SXP if exp is a valid shell expression

      Expression matching rules:

      -  \* matches anything
      -  ? matches one character
      -  \\ will escape a special character
      -  $ matches the end of the string
      -  Bracketed expressions:
         [abc] matches one occurrence of a, b, or c.
         [^abc] matches any character except a, b, or c.
         To be matched between [ and ], these characters must be escaped: \\ ]
         No other characters need be escaped between brackets.
         Unnecessary escaping is permitted.
      -  [a-z] matches any character between a and z, inclusive.
         The two range-definition characters must be alphanumeric ASCII.
         If one is upper case and the other is lower case, then the ASCII
         non-alphanumeric characters between Z and a will also be in range.
      -  [^a-z] matches any character except those between a and z, inclusive.
         These forms cannot be combined, e.g [a-gp-z] does not work.
      -  Exclusions:
         As a top level, outter-most expression only, the expression
         foo~bar will match the expression foo, provided it does not also
         match the expression bar. Either expression or both may be a union.
         Except between brackets, any unescaped ~ is an exclusion.
         At most one exclusion is permitted.
         Exclusions cannot be nested (contain other exclusions).
         example: \*~abc will match any string except abc
      -  Unions:
         (foo|bar) will match either the expression foo, or the expression bar.
         At least one '|' separator is required. More are permitted.
         Expressions inside unions may not include unions or exclusions.
         Inside a union, to be matched and not treated as a special character,
         these characters must be escaped: \\ ( \| ) [ ~ except when they occur
         inside a bracketed expression, where only \\ and ] require escaping.

   -  New functions in the nss shared library:

      -  PK11_IsInternalKeySlot (see pk11pub.h)
      -  SECMOD_OpenNewSlot (see pk11pub.h)

   -  New error codes (see secerr.h):

      -  SEC_ERROR_BAD_INFO_ACCESS_METHOD
      -  SEC_ERROR_CRL_IMPORT_FAILED

   -  New OIDs (see secoidt.h)

      -  SEC_OID_X509_ANY_POLICY

   -  The nssckbi PKCS #11 module's version changed to 1.75.
   -  Obsolete code for Win16 has been removed.
   -  Support for OpenVMS has been removed.

   .. rubric:: Bugs Fixed
      :name: Bugs_Fixed

   The following bugs have been fixed in NSS 3.12.4.

   -  `Bug 321755 <https://bugzilla.mozilla.org/show_bug.cgi?id=321755>`__: implement
      crlDistributionPoint extension in libPKIX
   -  `Bug 391434 <https://bugzilla.mozilla.org/show_bug.cgi?id=391434>`__: avoid multiple
      encoding/decoding of PKIX_PL_OID to and from ascii string
   -  `Bug 405297 <https://bugzilla.mozilla.org/show_bug.cgi?id=405297>`__: Problems building
      nss/lib/ckfw/capi/ with MingW GCC
   -  `Bug 420991 <https://bugzilla.mozilla.org/show_bug.cgi?id=420991>`__: libPKIX returns wrong
      NSS error code
   -  `Bug 427135 <https://bugzilla.mozilla.org/show_bug.cgi?id=427135>`__: Add super-H (sh3,4)
      architecture support
   -  `Bug 431958 <https://bugzilla.mozilla.org/show_bug.cgi?id=431958>`__: Improve DES and SHA512
      for x86_64 platform
   -  `Bug 433791 <https://bugzilla.mozilla.org/show_bug.cgi?id=433791>`__: Win16 support should be
      deleted from NSS
   -  `Bug 449332 <https://bugzilla.mozilla.org/show_bug.cgi?id=449332>`__: SECU_ParseCommandLine
      does not validate its inputs
   -  `Bug 453735 <https://bugzilla.mozilla.org/show_bug.cgi?id=453735>`__: When using cert9
      (SQLite3) DB, set or change master password fails
   -  `Bug 463544 <https://bugzilla.mozilla.org/show_bug.cgi?id=463544>`__: warning: passing enum\*
      for an int\* argument in pkix_validate.c
   -  `Bug 469588 <https://bugzilla.mozilla.org/show_bug.cgi?id=469588>`__: Coverity errors reported
      for softoken
   -  `Bug 470055 <https://bugzilla.mozilla.org/show_bug.cgi?id=470055>`__:
      pkix_HttpCertStore_FindSocketConnection reuses closed socket
   -  `Bug 470070 <https://bugzilla.mozilla.org/show_bug.cgi?id=470070>`__: Multiple object leaks
      reported by tinderbox
   -  `Bug 470479 <https://bugzilla.mozilla.org/show_bug.cgi?id=470479>`__: IO timeout during cert
      fetching makes libpkix abort validation.
   -  `Bug 470500 <https://bugzilla.mozilla.org/show_bug.cgi?id=470500>`__: Firefox 3.1b2 Crash
      Report [[@ nssutil3.dll@0x34c0 ]
   -  `Bug 482742 <https://bugzilla.mozilla.org/show_bug.cgi?id=482742>`__: Enable building util
      independently of the rest of nss
   -  `Bug 483653 <https://bugzilla.mozilla.org/show_bug.cgi?id=483653>`__: unable to build
      certutil.exe for fennec/wince
   -  `Bug 485145 <https://bugzilla.mozilla.org/show_bug.cgi?id=485145>`__: Miscellaneous crashes in
      signtool on Windows
   -  `Bug 485155 <https://bugzilla.mozilla.org/show_bug.cgi?id=485155>`__: NSS_ENABLE_PKIX_VERIFY=1
      causes sec_error_unknown_issuer errors
   -  `Bug 485527 <https://bugzilla.mozilla.org/show_bug.cgi?id=485527>`__: Rename the \_X86\_ macro
      in lib/freebl
   -  `Bug 485658 <https://bugzilla.mozilla.org/show_bug.cgi?id=485658>`__: vfychain -p reports
      revoked cert
   -  `Bug 485745 <https://bugzilla.mozilla.org/show_bug.cgi?id=485745>`__: modify fipstest.c to
      support CAVS 7.1 DRBG testing
   -  `Bug 486304 <https://bugzilla.mozilla.org/show_bug.cgi?id=486304>`__: cert7.db/cert8.db
      corruption when importing a large certificate (>64K)
   -  `Bug 486405 <https://bugzilla.mozilla.org/show_bug.cgi?id=486405>`__: Allocator mismatches in
      pk12util.c
   -  `Bug 486537 <https://bugzilla.mozilla.org/show_bug.cgi?id=486537>`__: Disable execstack in
      freebl x86_64 builds on Linux
   -  `Bug 486698 <https://bugzilla.mozilla.org/show_bug.cgi?id=486698>`__: Facilitate the building
      of major components independently and in a chain manner by downstream distributions
   -  `Bug 486999 <https://bugzilla.mozilla.org/show_bug.cgi?id=486999>`__: Calling
      SSL_SetSockPeerID a second time leaks the previous value
   -  `Bug 487007 <https://bugzilla.mozilla.org/show_bug.cgi?id=487007>`__: Make lib/jar conform to
      NSS coding style
   -  `Bug 487162 <https://bugzilla.mozilla.org/show_bug.cgi?id=487162>`__: ckfw/capi build failure
      on windows
   -  `Bug 487239 <https://bugzilla.mozilla.org/show_bug.cgi?id=487239>`__: nssutil.rc doesn't
      compile on WinCE
   -  `Bug 487254 <https://bugzilla.mozilla.org/show_bug.cgi?id=487254>`__: sftkmod.c uses POSIX
      file IO Functions on WinCE
   -  `Bug 487255 <https://bugzilla.mozilla.org/show_bug.cgi?id=487255>`__: sdb.c uses POSIX file IO
      Functions on WinCE
   -  `Bug 487487 <https://bugzilla.mozilla.org/show_bug.cgi?id=487487>`__: CERT_NameToAscii reports
      !Invalid AVA! whenever value exceeds 384 bytes
   -  `Bug 487736 <https://bugzilla.mozilla.org/show_bug.cgi?id=487736>`__: libpkix passes wrong
      argument to DER_DecodeTimeChoice and crashes
   -  `Bug 487858 <https://bugzilla.mozilla.org/show_bug.cgi?id=487858>`__: Remove obsolete build
      options MOZILLA_SECURITY_BUILD and MOZILLA_BSAFE_BUILD
   -  `Bug 487884 <https://bugzilla.mozilla.org/show_bug.cgi?id=487884>`__: object leak in libpkix
      library upon error
   -  `Bug 488067 <https://bugzilla.mozilla.org/show_bug.cgi?id=488067>`__: PK11_ImportCRL reports
      SEC_ERROR_CRL_NOT_FOUND when it fails to import a CRL
   -  `Bug 488350 <https://bugzilla.mozilla.org/show_bug.cgi?id=488350>`__: NSPR-free freebl
      interface need to do post tests only in fips mode.
   -  `Bug 488396 <https://bugzilla.mozilla.org/show_bug.cgi?id=488396>`__: DBM needs to be FIPS
      certifiable.
   -  `Bug 488550 <https://bugzilla.mozilla.org/show_bug.cgi?id=488550>`__: crash in certutil or pp
      when printing cert with empty subject name
   -  `Bug 488992 <https://bugzilla.mozilla.org/show_bug.cgi?id=488992>`__: Fix
      lib/freebl/win_rand.c warnings
   -  `Bug 489010 <https://bugzilla.mozilla.org/show_bug.cgi?id=489010>`__: stop exporting mktemp
      and dbopen (again)
   -  `Bug 489287 <https://bugzilla.mozilla.org/show_bug.cgi?id=489287>`__: Resolve a few remaining
      issues with NSS's new revocation flags
   -  `Bug 489710 <https://bugzilla.mozilla.org/show_bug.cgi?id=489710>`__: byteswap optimize for
      MSVC++
   -  `Bug 490154 <https://bugzilla.mozilla.org/show_bug.cgi?id=490154>`__: Cryptokey framework
      requires module to implement GenerateKey when they support KeyPairGeneration
   -  `Bug 491044 <https://bugzilla.mozilla.org/show_bug.cgi?id=491044>`__: Remove support for VMS
      (a.k.a., OpenVMS) from NSS
   -  `Bug 491174 <https://bugzilla.mozilla.org/show_bug.cgi?id=491174>`__: CERT_PKIXVerifyCert
      reports wrong error code when EE cert is expired
   -  `Bug 491919 <https://bugzilla.mozilla.org/show_bug.cgi?id=491919>`__: cert.h doesn't have
      valid functions prototypes
   -  `Bug 492131 <https://bugzilla.mozilla.org/show_bug.cgi?id=492131>`__: A failure to import a
      cert from a P12 file leaves error code set to zero
   -  `Bug 492385 <https://bugzilla.mozilla.org/show_bug.cgi?id=492385>`__: crash freeing named CRL
      entry on shutdown
   -  `Bug 493135 <https://bugzilla.mozilla.org/show_bug.cgi?id=493135>`__: bltest crashes if it
      can't open the input file
   -  `Bug 493364 <https://bugzilla.mozilla.org/show_bug.cgi?id=493364>`__: can't build with
      --disable-dbm option when not cross-compiling
   -  `Bug 493693 <https://bugzilla.mozilla.org/show_bug.cgi?id=493693>`__: SSE2 instructions for
      bignum are not implemented on OS/2
   -  `Bug 493912 <https://bugzilla.mozilla.org/show_bug.cgi?id=493912>`__: sqlite3_reset should be
      invoked in sdb_FindObjectsInit when error occurs
   -  `Bug 494073 <https://bugzilla.mozilla.org/show_bug.cgi?id=494073>`__: update RSA/DSA
      powerupself tests to be compliant for 2011
   -  `Bug 494087 <https://bugzilla.mozilla.org/show_bug.cgi?id=494087>`__: Passing NULL as the
      value of cert_pi_trustAnchors causes a crash in cert_pkixSetParam
   -  `Bug 494107 <https://bugzilla.mozilla.org/show_bug.cgi?id=494107>`__: During NSS_NoDB_Init(),
      softoken tries but fails to load libsqlite3.so crash [@ @0x0 ]
   -  `Bug 495097 <https://bugzilla.mozilla.org/show_bug.cgi?id=495097>`__: sdb_mapSQLError returns
      signed int
   -  `Bug 495103 <https://bugzilla.mozilla.org/show_bug.cgi?id=495103>`__:
      NSS_InitReadWrite(sql:<dbdir>) causes NSS to look for sql:<dbdir>/libnssckbi.so
   -  `Bug 495365 <https://bugzilla.mozilla.org/show_bug.cgi?id=495365>`__: Add const to the
      'nickname' parameter of SEC_CertNicknameConflict
   -  `Bug 495656 <https://bugzilla.mozilla.org/show_bug.cgi?id=495656>`__:
      NSS_InitReadWrite(sql:<configdir>) leaves behind a pkcs11.txu file if libnssckbi.so is in
      <configdir>.
   -  `Bug 495717 <https://bugzilla.mozilla.org/show_bug.cgi?id=495717>`__: Unable to compile
      nss/cmd/certutil/keystuff.c on WinCE
   -  `Bug 496961 <https://bugzilla.mozilla.org/show_bug.cgi?id=496961>`__: provide truncated HMAC
      support for testing tool fipstest
   -  `Bug 497002 <https://bugzilla.mozilla.org/show_bug.cgi?id=497002>`__: Lab required nspr-free
      freebl changes.
   -  `Bug 497217 <https://bugzilla.mozilla.org/show_bug.cgi?id=497217>`__: The first random value
      ever generated by the RNG should be discarded
   -  `Bug 498163 <https://bugzilla.mozilla.org/show_bug.cgi?id=498163>`__: assert if profile path
      contains cyrillic chars. [[@isspace - secmod_argIsBlank - secmod_argHasBlanks -
      secmod_formatPair - secmod_mkNewModuleSpec]
   -  `Bug 498509 <https://bugzilla.mozilla.org/show_bug.cgi?id=498509>`__: Produce debuggable
      optimized builds for Mozilla on MacOSX
   -  `Bug 498511 <https://bugzilla.mozilla.org/show_bug.cgi?id=498511>`__: Produce debuggable
      optimized NSS builds for Mozilla on Linux
   -  `Bug 499385 <https://bugzilla.mozilla.org/show_bug.cgi?id=499385>`__: DRBG Reseed function
      needs to be tested on POST
   -  `Bug 499825 <https://bugzilla.mozilla.org/show_bug.cgi?id=499825>`__: utilrename.h is missing
      from Solaris packages
   -  `Bug 502961 <https://bugzilla.mozilla.org/show_bug.cgi?id=502961>`__: Allocator mismatch in
      pk11mode
   -  `Bug 502965 <https://bugzilla.mozilla.org/show_bug.cgi?id=502965>`__: Allocator mismatch in
      sdrtest
   -  `Bug 502972 <https://bugzilla.mozilla.org/show_bug.cgi?id=502972>`__: Another allocator
      mismatch in sdrtest
   -  `Bug 504398 <https://bugzilla.mozilla.org/show_bug.cgi?id=504398>`__:
      pkix_pl_AIAMgr_GetHTTPCerts could crash if SEC_GetRegisteredHttpClient fails
   -  `Bug 504405 <https://bugzilla.mozilla.org/show_bug.cgi?id=504405>`__: pkix_pl_CrlDp_Create
      will fail on alloc success because of a missing !
   -  `Bug 504408 <https://bugzilla.mozilla.org/show_bug.cgi?id=504408>`__: pkix_pl_CrlDp_Create
      will always fail if dp->distPointType != generalName
   -  `Bug 504456 <https://bugzilla.mozilla.org/show_bug.cgi?id=504456>`__: Exploitable heap
      overflow in NSS shell expression (filename globbing) parsing
   -  `Bug 505559 <https://bugzilla.mozilla.org/show_bug.cgi?id=505559>`__: Need function to
      identify the one and only default internal private key slot.
   -  `Bug 505561 <https://bugzilla.mozilla.org/show_bug.cgi?id=505561>`__: Need a generic function
      a la SECMOD_OpenUserDB() that can be used on non-softoken modules.
   -  `Bug 505858 <https://bugzilla.mozilla.org/show_bug.cgi?id=505858>`__: NSS_RegisterShutdown can
      return without unlocking nssShutdownList.lock
   -  `Bug 507041 <https://bugzilla.mozilla.org/show_bug.cgi?id=507041>`__: Invalid build options
      for VC6
   -  `Bug 507228 <https://bugzilla.mozilla.org/show_bug.cgi?id=507228>`__: coreconf.dep doesn't
      need to contain the NSS version number
   -  `Bug 507422 <https://bugzilla.mozilla.org/show_bug.cgi?id=507422>`__: crash [[@ PORT_FreeArena
      - lg_mkSecretKeyRep] when PORT_NewArena fails
   -  `Bug 507482 <https://bugzilla.mozilla.org/show_bug.cgi?id=507482>`__: NSS 3.12.3 (and later)
      doesn't build on AIX 5.1
   -  `Bug 507937 <https://bugzilla.mozilla.org/show_bug.cgi?id=507937>`__: pwdecrypt program
      problems
   -  `Bug 508259 <https://bugzilla.mozilla.org/show_bug.cgi?id=508259>`__: Pk11mode crashed on
      Linux2.4
   -  `Bug 508467 <https://bugzilla.mozilla.org/show_bug.cgi?id=508467>`__: libpkix ocsp checker
      should use date argument to obtain the time for cert validity verification
   -  `Bug 510367 <https://bugzilla.mozilla.org/show_bug.cgi?id=510367>`__: Fix the UTF8 characters
      in the nickname string for AC Raíz Certicamara S.A.

   .. rubric:: Documentation
      :name: Documentation

   For a list of the primary NSS documentation pages on developer.mozilla.org, see NSS. New and
   revised documents available since the release of NSS 3.12 include the following:

   -  :ref:`mozilla_projects_nss_reference_building_and_installing_nss_build_instructions`

   .. rubric:: Compatibility
      :name: Compatibility

   NSS 3.12.4 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.12.4 shared libraries
   without recompiling or relinking.  Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in `NSS Public Functions </ref/nssfunctions.html>`__ will remain
   compatible with future versions of the NSS shared libraries.

   .. rubric:: Feedback
      :name: Feedback

   Bugs discovered should be reported by filing a bug report with `mozilla.org
   Bugzilla <https://bugzilla.mozilla.org/>`__ (product NSS).