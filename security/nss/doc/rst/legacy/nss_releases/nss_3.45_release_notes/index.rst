.. _mozilla_projects_nss_nss_3_45_release_notes:

NSS 3.45 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.45 on **5 July 2019**, which is a
   minor release.

   The NSS team would like to recognize first-time contributors:

   -  Bastien Abadie
   -  Christopher Patton
   -  Jeremie Courreges-Anglas
   -  Marcus Burghardt
   -  Michael Shigorin
   -  Tomas Mraz

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_45_RTM. NSS 3.45 requires NSPR 4.21 or newer.

   NSS 3.45 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_45_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _new_in_nss_3.45:

`New in NSS 3.45 <#new_in_nss_3.45>`__
--------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. rubric:: New Functions
      :name: new_functions

   -  in *pk11pub.h*:

      -  **PK11_FindRawCertsWithSubject** - Finds all certificates on the given slot with the given
         subject distinguished name and returns them as DER bytes. If no such certificates can be
         found, returns SECSuccess and sets ``*results`` to NULL. If a failure is encountered while
         fetching any of the matching certificates, SECFailure is returned and ``*results`` will be
         NULL.

.. _notable_changes_in_nss_3.45:

`Notable Changes in NSS 3.45 <#notable_changes_in_nss_3.45>`__
--------------------------------------------------------------

.. container::

   -  `Bug 1540403 <https://bugzilla.mozilla.org/show_bug.cgi?id=1540403>`__ - Implement Delegated
      Credentials
      (`draft-ietf-tls-subcerts <https://datatracker.ietf.org/doc/draft-ietf-tls-subcerts/>`__)

      -  This adds a new experimental function: **SSL_DelegateCredential**
      -  **Note**: In 3.45, ``selfserv`` does not yet support delegated credentials. See `Bug
         1548360 <https://bugzilla.mozilla.org/show_bug.cgi?id=1548360>`__.
      -  **Note**: In 3.45 the SSLChannelInfo is left unmodified, while an upcoming change in 3.46
         will set ``SSLChannelInfo.authKeyBits`` to that of the delegated credential for better
         policy enforcement. See `Bug
         1563078 <https://bugzilla.mozilla.org/show_bug.cgi?id=1563078>`__.

   -  `Bug 1550579 <https://bugzilla.mozilla.org/show_bug.cgi?id=1550579>`__ - Replace ARM32
      Curve25519 implementation with one from
      `fiat-crypto <https://github.com/mit-plv/fiat-crypto>`__
   -  `Bug 1551129 <https://bugzilla.mozilla.org/show_bug.cgi?id=1551129>`__ - Support static
      linking on Windows
   -  `Bug 1552262 <https://bugzilla.mozilla.org/show_bug.cgi?id=1552262>`__ - Expose a function
      **PK11_FindRawCertsWithSubject** for finding certificates with a given subject on a given slot
   -  `Bug 1546229 <https://bugzilla.mozilla.org/show_bug.cgi?id=1546229>`__ - Add IPSEC IKE support
      to softoken
   -  `Bug 1554616 <https://bugzilla.mozilla.org/show_bug.cgi?id=1554616>`__ - Add support for the
      Elbrus lcc compiler (<=1.23)
   -  `Bug 1543874 <https://bugzilla.mozilla.org/show_bug.cgi?id=1543874>`__ - Expose an external
      clock for SSL

      -  This adds new experimental functions: **SSL_SetTimeFunc**, **SSL_CreateAntiReplayContext**,
         **SSL_SetAntiReplayContext**, and **SSL_ReleaseAntiReplayContext**.
      -  The experimental function **SSL_InitAntiReplay** is removed.

   -  `Bug 1546477 <https://bugzilla.mozilla.org/show_bug.cgi?id=1546477>`__ - Various changes in
      response to the ongoing FIPS review

      -  Note: The source package size has increased substantially due to the new FIPS test vectors.
         This will likely prompt follow-on work, but please accept our apologies in the meantime.

.. _certificate_authority_changes:

`Certificate Authority Changes <#certificate_authority_changes>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  The following CA certificates were **Removed**:

      -  `Bug 1552374 <https://bugzilla.mozilla.org/show_bug.cgi?id=1552374>`__ - CN = Certinomis -
         Root CA

         -  SHA-256 Fingerprint: 2A99F5BC1174B73CBB1D620884E01C34E51CCB3978DA125F0E33268883BF4158

.. _bugs_fixed_in_nss_3.45:

`Bugs fixed in NSS 3.45 <#bugs_fixed_in_nss_3.45>`__
----------------------------------------------------

.. container::

   -  `Bug 1540541 <https://bugzilla.mozilla.org/show_bug.cgi?id=1540541>`__ - Don't unnecessarily
      strip leading 0's from key material during PKCS11 import
      (`CVE-2019-11719 <https://bugzilla.mozilla.org/show_bug.cgi?id=CVE-2019-11719>`__)

   -  `Bug 1515342 <https://bugzilla.mozilla.org/show_bug.cgi?id=1515342>`__ - More thorough input
      checking (`CVE-2019-11729) <https://bugzilla.mozilla.org/show_bug.cgi?id=CVE-2019-11729>`__

   -  

      .. container::

         `Bug 1552208 <https://bugzilla.mozilla.org/show_bug.cgi?id=1552208>`__ - Prohibit use of
         RSASSA-PKCS1-v1_5 algorithms in TLS 1.3
         (`CVE-2019-11727 <https://bugzilla.mozilla.org/show_bug.cgi?id=CVE-2019-11727>`__)

   -  `Bug 1227090 <https://bugzilla.mozilla.org/show_bug.cgi?id=1227090>`__ - Fix a potential
      divide-by-zero in makePfromQandSeed from lib/freebl/pqg.c (static analysis)

   -  `Bug 1227096 <https://bugzilla.mozilla.org/show_bug.cgi?id=1227096>`__ - Fix a potential
      divide-by-zero in PQG_VerifyParams from lib/freebl/pqg.c  (static analysis)

   -  `Bug 1509432 <https://bugzilla.mozilla.org/show_bug.cgi?id=1509432>`__ - De-duplicate code
      between mp_set_long and mp_set_ulong

   -  `Bug 1515011 <https://bugzilla.mozilla.org/show_bug.cgi?id=1515011>`__ - Fix a mistake with
      ChaCha20-Poly1305 test code where tags could be faked. Only relevant for clients that might
      have copied the unit test code verbatim

   -  `Bug 1550022 <https://bugzilla.mozilla.org/show_bug.cgi?id=1550022>`__ - Ensure nssutil3 gets
      built on Android

   -  `Bug 1528174 <https://bugzilla.mozilla.org/show_bug.cgi?id=1528174>`__ - ChaCha20Poly1305
      should no longer modify output length on failure

   -  `Bug 1549382 <https://bugzilla.mozilla.org/show_bug.cgi?id=1549382>`__ - Don't leak in PKCS#11
      modules if C_GetSlotInfo() returns error

   -  `Bug 1551041 <https://bugzilla.mozilla.org/show_bug.cgi?id=1551041>`__ - Fix builds using GCC
      < 4.3 on big-endian architectures

   -  

      .. container::

         `Bug 1554659 <https://bugzilla.mozilla.org/show_bug.cgi?id=1554659>`__ - Add versioning to
         OpenBSD builds to fix link time errors using NSS

   -  `Bug 1553443 <https://bugzilla.mozilla.org/show_bug.cgi?id=1553443>`__ - Send session ticket
      only after handshake is marked as finished

   -  `Bug 1550708 <https://bugzilla.mozilla.org/show_bug.cgi?id=1550708>`__ - Fix gyp scripts on
      Solaris SPARC so that libfreebl_64fpu_3.so builds

   -  `Bug 1554336 <https://bugzilla.mozilla.org/show_bug.cgi?id=1554336>`__ - Optimize away
      unneeded loop in mpi.c

   -  `Bug 1559906 <https://bugzilla.mozilla.org/show_bug.cgi?id=1559906>`__ - fipstest: use
      CKM_TLS12_MASTER_KEY_DERIVE instead of vendor specific mechanism

   -  `Bug 1558126 <https://bugzilla.mozilla.org/show_bug.cgi?id=1558126>`__ -
      TLS_AES_256_GCM_SHA384 should be marked as FIPS compatible

   -  `Bug 1555207 <https://bugzilla.mozilla.org/show_bug.cgi?id=1555207>`__ -
      HelloRetryRequestCallback return code for rejecting 0-RTT

   -  `Bug 1556591 <https://bugzilla.mozilla.org/show_bug.cgi?id=1556591>`__ - Eliminate races in
      uses of PK11_SetWrapKey

   -  `Bug 1558681 <https://bugzilla.mozilla.org/show_bug.cgi?id=1558681>`__ - Stop using a global
      for anti-replay of TLS 1.3 early data

   -  `Bug 1561510 <https://bugzilla.mozilla.org/show_bug.cgi?id=1561510>`__ - Fix a bug where
      removing -arch XXX args from CC didn't work

   -  `Bug 1561523 <https://bugzilla.mozilla.org/show_bug.cgi?id=1561523>`__ - Add a string for the
      new-ish error SSL_ERROR_MISSING_POST_HANDSHAKE_AUTH_EXTENSION

   This Bugzilla query returns all the bugs fixed in NSS 3.45:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.45

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.45 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.45 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).