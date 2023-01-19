.. _mozilla_projects_nss_nss_3_11_10_release_notes_html:

NSS_3.11.10_release_notes.html
==============================

.. _nss_3.11.10_release_notes:

`NSS 3.11.10 Release Notes <#nss_3.11.10_release_notes>`__
----------------------------------------------------------

.. container::

.. _2008-12-10:

`2008-12-10 <#2008-12-10>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Newsgroup: <ahref="news: mozilla.dev.tech.crypto"=""
   news.mozilla.org="">mozilla.dev.tech.crypto</ahref="news:>

`Contents <#contents>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `Introduction <#introduction>`__
   -  `Distribution Information <#distribution>`__
   -  `Bugs Fixed <#bugsfixed>`__
   -  `Documentation <#docs>`__
   -  `Compatibility <#compatibility>`__
   -  `Feedback <#feedback>`__

   --------------

`Introduction <#introduction>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Network Security Services (NSS) 3.11.10 is a patch release for NSS 3.11. The bug fixes in NSS
   3.11.10 are described in the "`Bugs Fixed <#bugsfixed>`__" section below.

   --------------

.. _distribution_information:

`Distribution Information <#distribution_information>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The CVS tag for the NSS 3.11.10 release is NSS_3_11_10_RTM. NSS 3.11.10 requires `NSPR
   4.7.1 <https://www.mozilla.org/projects/nspr/release-notes/nspr471.html>`__.
   See the `Documentation <#docs>`__ section for the build instructions.
   NSS 3.11.10 source and binary distributions are also available on ftp.mozilla.org for secure
   HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_11_10_RTM/src/.
   -  Binary distributions:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_11_10_RTM/. Both debug and
      optimized builds are provided. Go to the subdirectory for your platform, DBG (debug) or OPT
      (optimized), to get the tar.gz or zip file. The tar.gz or zip file expands to an nss-3.11.10
      directory containing three subdirectories:

      -  include - NSS header files
      -  lib - NSS shared libraries
      -  bin - `NSS Tools <https://www.mozilla.org/projects/security/pki/nss/tools/>`__ and test
         programs

   You also need to download the NSPR 4.7.1 binary distributions to get the NSPR 4.7.1 header files
   and shared libraries, which NSS 3.11.10 requires. NSPR 4.7.1 binary distributions are in
   https://ftp.mozilla.org/pub/mozilla.org/nspr/releases/v4.7.1/.

   --------------

.. _bugs_fixed:

`Bugs Fixed <#bugs_fixed>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The following bugs have been fixed in NSS 3.11.10.

   -  `Bug 291384 <https://bugzilla.mozilla.org/show_bug.cgi?id=291384>`__: certutil -K behavior
      doesn't match usage
   -  `Bug 374247 <https://bugzilla.mozilla.org/show_bug.cgi?id=374247>`__: modutil -disable command
      not disabling modules' slots
   -  `Bug 384459 <https://bugzilla.mozilla.org/show_bug.cgi?id=384459>`__: Certification path
      validation fails when Authority Key Identifier extension contains key identifier
   -  `Bug 385946 <https://bugzilla.mozilla.org/show_bug.cgi?id=385946>`__: Can't import certificate
      into cert database in FIPS mode (certutil).
   -  `Bug 387892 <https://bugzilla.mozilla.org/show_bug.cgi?id=387892>`__: Add Entrust root CA
      certificate(s) to NSS
   -  `Bug 396999 <https://bugzilla.mozilla.org/show_bug.cgi?id=396999>`__: PK11_Authenticate
   -  `Bug 397478 <https://bugzilla.mozilla.org/show_bug.cgi?id=397478>`__: Lock from
      ssl_InitSymWrapKeysLock not freed on selfserv shutdown.
   -  `Bug 397486 <https://bugzilla.mozilla.org/show_bug.cgi?id=397486>`__: Session cache locks not
      freed on strsclnt shutdown.
   -  `Bug 398680 <https://bugzilla.mozilla.org/show_bug.cgi?id=398680>`__: assertion botch in
      ssl3_RegisterServerHelloExtensionSender doing second handshake with SSL_ForceHandshake
   -  `Bug 403240 <https://bugzilla.mozilla.org/show_bug.cgi?id=403240>`__: threads hanging in
      nss_InitLock
   -  `Bug 403888 <https://bugzilla.mozilla.org/show_bug.cgi?id=403888>`__: memory leak in
      trustdomain.c
   -  `Bug 416067 <https://bugzilla.mozilla.org/show_bug.cgi?id=416067>`__: certutil -L -h token
      doesn't report token authentication failure
   -  `Bug 417637 <https://bugzilla.mozilla.org/show_bug.cgi?id=417637>`__: tstclnt crashes if -p
      option is not specified
   -  `Bug 421634 <https://bugzilla.mozilla.org/show_bug.cgi?id=421634>`__: Don't send an SNI Client
      Hello extension bearing an IPv6 address
   -  `Bug 422918 <https://bugzilla.mozilla.org/show_bug.cgi?id=422918>`__: Add VeriSign Class 3
      Public Primary CA - G5 to NSS
   -  `Bug 424152 <https://bugzilla.mozilla.org/show_bug.cgi?id=424152>`__: Add thawte Primary Root
      CA to NSS
   -  `Bug 424169 <https://bugzilla.mozilla.org/show_bug.cgi?id=424169>`__: Add GeoTrust Primary
      Certification Authority root to NSS
   -  `Bug 425469 <https://bugzilla.mozilla.org/show_bug.cgi?id=425469>`__: Add multiple new roots:
      Geotrust
   -  `Bug 426568 <https://bugzilla.mozilla.org/show_bug.cgi?id=426568>`__: Add COMODO Certification
      Authority certificate to NSS
   -  `Bug 431381 <https://bugzilla.mozilla.org/show_bug.cgi?id=431381>`__: Add Network Solutions
      Certificate Authority root to NSS
   -  `Bug 431621 <https://bugzilla.mozilla.org/show_bug.cgi?id=431621>`__: Add DigiNotar Root CA
      root to NSS
   -  `Bug 431772 <https://bugzilla.mozilla.org/show_bug.cgi?id=431772>`__: add network solutions
      and diginotar root certs to NSS
   -  `Bug 442912 <https://bugzilla.mozilla.org/show_bug.cgi?id=442912>`__: fix nssckbi version
      number on 3.11 branch
   -  `Bug 443045 <https://bugzilla.mozilla.org/show_bug.cgi?id=443045>`__: Fix PK11_GenerateKeyPair
      for ECC keys on the 3.11 branch
   -  `Bug 444850 <https://bugzilla.mozilla.org/show_bug.cgi?id=444850>`__: NSS misbehaves badly in
      the presence of a disabled PKCS#11 slot
   -  `Bug 462948 <https://bugzilla.mozilla.org/show_bug.cgi?id=462948>`__: lint warnings for source
      files that include keythi.h

   --------------

`Documentation <#documentation>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   For a list of the primary NSS documentation pages on mozilla.org, see `NSS
   Documentation <../index.html#Documentation>`__. New and revised documents available since the
   release of NSS 3.9 include the following:

   -  `Build Instructions for NSS 3.11.4 and above <../nss-3.11.4/nss-3.11.4-build.html>`__

   --------------

`Compatibility <#compatibility>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   NSS 3.11.10 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.11.10 shared libraries
   without recompiling or relinking.  Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in `NSS Public Functions <../ref/nssfunctions.html>`__ will remain
   compatible with future versions of the NSS shared libraries.

   --------------

`Feedback <#feedback>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Bugs discovered should be reported by filing a bug report with `mozilla.org
   Bugzilla <https://bugzilla.mozilla.org/>`__ (product NSS).