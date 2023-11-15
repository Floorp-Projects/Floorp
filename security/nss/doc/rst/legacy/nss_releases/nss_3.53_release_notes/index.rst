.. _mozilla_projects_nss_nss_3_53_release_notes:

NSS 3.53 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team released Network Security Services (NSS) 3.53 on **29 May 2020**. NSS 3.53 will be a
   long-term support release, supporting Firefox 78 ESR.

   The NSS team would like to recognize first-time contributors:

   -  Jan-Marek Glogowski
   -  Jeff Walden



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_53_RTM. NSS 3.53 requires NSPR 4.25 or newer.

   NSS 3.53 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_53_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _notable_changes_in_nss_3.53:

`Notable Changes in NSS 3.53 <#notable_changes_in_nss_3.53>`__
--------------------------------------------------------------

.. container::

   -  When using the Makefiles, NSS can be built in parallel, speeding up those builds to more
      similar performance as the build.sh/ninja/gyp system. (`Bug
      290526 <https://bugzilla.mozilla.org/show_bug.cgi?id=290526>`__)
   -  SEED is now moved into a new freebl directory freebl/deprecated (`Bug
      1636389 <https://bugzilla.mozilla.org/show_bug.cgi?id=1636389>`__).

      -  SEED will be disabled by default in a future release of NSS. At that time, users will need
         to set the compile-time flag (`Bug
         1622033 <https://bugzilla.mozilla.org/show_bug.cgi?id=1622033>`__) to disable that
         deprecation in order to use the algorithm.
      -  Algorithms marked as deprecated will ultimately be removed.

   -  Several root certificates in the Mozilla program now set the CKA_NSS_SERVER_DISTRUST_AFTER
      attribute, which NSS consumers can query to further refine trust decisions. (`Bug
      1618404, <https://bugzilla.mozilla.org/show_bug.cgi?id=1618404>`__ `Bug
      1621159 <https://bugzilla.mozilla.org/show_bug.cgi?id=1621159>`__) If a builtin certificate
      has a CKA_NSS_SERVER_DISTRUST_AFTER timestamp before the  SCT or NotBefore date of a
      certificate that builtin issued, then clients can elect not to trust it.

      -  This attribute provides a more graceful phase-out for certificate authorities than complete
         removal from the root certificate builtin store.

.. _bugs_fixed_in_nss_3.53:

`Bugs fixed in NSS 3.53 <#bugs_fixed_in_nss_3.53>`__
----------------------------------------------------

.. container::

   -  `Bug 1640260 <https://bugzilla.mozilla.org/show_bug.cgi?id=1640260>`__ - Initialize PBE params
      (ASAN fix)
   -  `Bug 1618404 <https://bugzilla.mozilla.org/show_bug.cgi?id=1618404>`__ - Set
      CKA_NSS_SERVER_DISTRUST_AFTER for Symantec root certs
   -  `Bug 1621159 <https://bugzilla.mozilla.org/show_bug.cgi?id=1621159>`__ - Set
      CKA_NSS_SERVER_DISTRUST_AFTER for Consorci AOC, GRCA, and SK ID root certs
   -  `Bug 1629414 <https://bugzilla.mozilla.org/show_bug.cgi?id=1629414>`__ - PPC64: Correct
      compilation error between VMX vs. VSX vector instructions
   -  `Bug 1639033 <https://bugzilla.mozilla.org/show_bug.cgi?id=1639033>`__ - Fix various compile
      warnings in NSS
   -  `Bug 1640041 <https://bugzilla.mozilla.org/show_bug.cgi?id=1640041>`__ - Fix a null pointer in
      security/nss/lib/ssl/sslencode.c:67
   -  `Bug 1640042 <https://bugzilla.mozilla.org/show_bug.cgi?id=1640042>`__ - Fix a null pointer in
      security/nss/lib/ssl/sslsock.c:4460
   -  `Bug 1638289 <https://bugzilla.mozilla.org/show_bug.cgi?id=1638289>`__ - Avoid multiple
      definitions of SHA{256,384,512}_\* symbols when linking libfreeblpriv3.so in Firefox on
      ppc64le
   -  `Bug 1636389 <https://bugzilla.mozilla.org/show_bug.cgi?id=1636389>`__ - Relocate deprecated
      SEED algorithm
   -  `Bug 1637083 <https://bugzilla.mozilla.org/show_bug.cgi?id=1637083>`__ - lib/ckfw: No such
      file or directory. Stop.
   -  `Bug 1561331 <https://bugzilla.mozilla.org/show_bug.cgi?id=1561331>`__ - Additional modular
      inverse test
   -  `Bug 1629553 <https://bugzilla.mozilla.org/show_bug.cgi?id=1629553>`__ - Rework and cleanup
      gmake builds
   -  `Bug 1438431 <https://bugzilla.mozilla.org/show_bug.cgi?id=1438431>`__ - Remove mkdepend and
      "depend" make target
   -  `Bug 290526 <https://bugzilla.mozilla.org/show_bug.cgi?id=290526>`__ - Support parallel
      building of NSS when using the Makefiles
   -  `Bug 1636206 <https://bugzilla.mozilla.org/show_bug.cgi?id=1636206>`__ - HACL\* update after
      changes in libintvector.h
   -  `Bug 1636058 <https://bugzilla.mozilla.org/show_bug.cgi?id=1636058>`__ - Fix building NSS on
      Debian s390x, mips64el, and riscv64
   -  `Bug 1622033 <https://bugzilla.mozilla.org/show_bug.cgi?id=1622033>`__ - Add option to build
      without SEED

   This Bugzilla query returns all the bugs fixed in NSS 3.53:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.53

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.53 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.53 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).