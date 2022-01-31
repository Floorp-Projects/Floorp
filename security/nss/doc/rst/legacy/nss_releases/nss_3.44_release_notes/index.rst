.. _mozilla_projects_nss_nss_3_44_release_notes:

NSS 3.44 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.44 on 10 May 2019, which is a minor
   release.

   The NSS team would like to recognize first-time contributors: Kevin Jacobs, David Carlier,
   Alexander Scheel, and Edouard Oger.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_44_RTM. NSS 3.44 requires NSPR 4.21 or newer.

   NSS 3.44 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_44_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _new_in_nss_3.44:

`New in NSS 3.44 <#new_in_nss_3.44>`__
--------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. rubric:: New Functions
      :name: new_functions

   -  *in lib/certdb/cert.h*

      -  **CERT_GetCertificateDer** - Access the DER-encoded form of a CERTCertificate.

.. _notable_changes_in_nss_3.44:

`Notable Changes in NSS 3.44 <#notable_changes_in_nss_3.44>`__
--------------------------------------------------------------

.. container::

   -  It is now possible to build NSS as a static library (Bug
      `1543545 <https://bugzilla.mozilla.org/show_bug.cgi?id=1543545>`__)
   -  Initial support for building for iOS.

.. _bugs_fixed_in_nss_3.44:

`Bugs fixed in NSS 3.44 <#bugs_fixed_in_nss_3.44>`__
----------------------------------------------------

.. container::

   -  `1501542 <https://bugzilla.mozilla.org/show_bug.cgi?id=1501542>`__ - Implement CheckARMSupport
      for Android
   -  `1531244 <https://bugzilla.mozilla.org/show_bug.cgi?id=1531244>`__ - Use \__builtin_bswap64 in
      crypto_primitives.h
   -  `1533216 <https://bugzilla.mozilla.org/show_bug.cgi?id=1533216>`__ - CERT_DecodeCertPackage()
      crash with Netscape Certificate Sequences
   -  `1533616 <https://bugzilla.mozilla.org/show_bug.cgi?id=1533616>`__ -
      sdb_GetAttributeValueNoLock should make at most one sql query, rather than one for each
      attribute
   -  `1531236 <https://bugzilla.mozilla.org/show_bug.cgi?id=1531236>`__ - Provide accessor for
      CERTCertificate.derCert
   -  `1536734 <https://bugzilla.mozilla.org/show_bug.cgi?id=1536734>`__ -
      lib/freebl/crypto_primitives.c assumes a big endian machine
   -  `1532384 <https://bugzilla.mozilla.org/show_bug.cgi?id=1532384>`__ - In NSS test certificates,
      use @example.com (not @bogus.com)
   -  `1538479 <https://bugzilla.mozilla.org/show_bug.cgi?id=1538479>`__ - Post-Handshake messages
      after async server authentication break when using record layer separation
   -  `1521578 <https://bugzilla.mozilla.org/show_bug.cgi?id=1521578>`__ - x25519 support in
      pk11pars.c
   -  `1540205 <https://bugzilla.mozilla.org/show_bug.cgi?id=1540205>`__ - freebl build fails with
      -DNSS_DISABLE_CHACHAPOLY
   -  `1532312 <https://bugzilla.mozilla.org/show_bug.cgi?id=1532312>`__ - post-handshake auth
      doesn't interoperate with OpenSSL
   -  `1542741 <https://bugzilla.mozilla.org/show_bug.cgi?id=1542741>`__ - certutil -F crashes with
      segmentation fault
   -  `1546925 <https://bugzilla.mozilla.org/show_bug.cgi?id=1546925>`__ - Allow preceding text in
      try comment
   -  `1534468 <https://bugzilla.mozilla.org/show_bug.cgi?id=1534468>`__ - Expose ChaCha20 primitive
   -  `1418944 <https://bugzilla.mozilla.org/show_bug.cgi?id=1418944>`__ - Quote CC/CXX variables
      passed to nspr
   -  `1543545 <https://bugzilla.mozilla.org/show_bug.cgi?id=1543545>`__ - Allow to build NSS as a
      static library
   -  `1487597 <https://bugzilla.mozilla.org/show_bug.cgi?id=1487597>`__ - Early data that arrives
      before the handshake completes can be read afterwards
   -  `1548398 <https://bugzilla.mozilla.org/show_bug.cgi?id=1548398>`__ - freebl_gtest not building
      on Linux/Mac
   -  `1548722 <https://bugzilla.mozilla.org/show_bug.cgi?id=1548722>`__ - Fix some Coverity
      warnings
   -  `1540652 <https://bugzilla.mozilla.org/show_bug.cgi?id=1540652>`__ - softoken/sdb.c: Logically
      dead code
   -  `1549413 <https://bugzilla.mozilla.org/show_bug.cgi?id=1549413>`__ - Android log lib is not
      included in build
   -  `1537927 <https://bugzilla.mozilla.org/show_bug.cgi?id=1537927>`__ - IPsec usage is too
      restrictive for existing deployments
   -  `1549608 <https://bugzilla.mozilla.org/show_bug.cgi?id=1549608>`__ - Signature fails with dbm
      disabled
   -  `1549848 <https://bugzilla.mozilla.org/show_bug.cgi?id=1549848>`__ - Allow building NSS for
      iOS using gyp
   -  `1549847 <https://bugzilla.mozilla.org/show_bug.cgi?id=1549847>`__ - NSS's SQLite compilation
      warnings make the build fail on iOS
   -  `1550041 <https://bugzilla.mozilla.org/show_bug.cgi?id=1550041>`__ - freebl not building on
      iOS simulator
   -  `1542950 <https://bugzilla.mozilla.org/show_bug.cgi?id=1542950>`__ - MacOS cipher test
      timeouts

   This Bugzilla query returns all the bugs fixed in NSS 3.44:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.44

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.44 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.44 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).