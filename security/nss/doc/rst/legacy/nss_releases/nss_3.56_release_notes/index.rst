.. _mozilla_projects_nss_nss_3_56_release_notes:

NSS 3.56 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.56 on **21 August 2020**, which is a
   minor release.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_56_RTM. NSS 3.56 requires NSPR 4.28 or newer.

   NSS 3.56 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_56_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _notable_changes_in_nss_3.56:

`Notable Changes in NSS 3.56 <#notable_changes_in_nss_3.56>`__
--------------------------------------------------------------

.. container::

   -  NSPR dependency updated to 4.28.
   -  The known issue where Makefile builds failed to locate seccomon.h was fixed in `Bug
      1653975 <https://bugzilla.mozilla.org/show_bug.cgi?id=1653975>`__.

.. _bugs_fixed_in_nss_3.56:

`Bugs fixed in NSS 3.56 <#bugs_fixed_in_nss_3.56>`__
----------------------------------------------------

.. container::

   -  `Bug 1650702 <https://bugzilla.mozilla.org/show_bug.cgi?id=1650702>`__ - Support SHA-1 HW
      acceleration on ARMv8
   -  `Bug 1656981 <https://bugzilla.mozilla.org/show_bug.cgi?id=1656981>`__ - Use MPI comba and
      mulq optimizations on x86-64 MacOS.
   -  `Bug 1654142 <https://bugzilla.mozilla.org/show_bug.cgi?id=1654142>`__ - Add CPU feature
      detection for Intel SHA extension.
   -  `Bug 1648822 <https://bugzilla.mozilla.org/show_bug.cgi?id=1648822>`__ - Add stricter
      validation of DH keys in FIPS mode.
   -  `Bug 1656986 <https://bugzilla.mozilla.org/show_bug.cgi?id=1656986>`__ - Properly detect arm64
      during GYP build architecture detection.
   -  `Bug 1652729 <https://bugzilla.mozilla.org/show_bug.cgi?id=1652729>`__ - Add build flag to
      disable RC2 and relocate to lib/freebl/deprecated.
   -  `Bug 1656429 <https://bugzilla.mozilla.org/show_bug.cgi?id=1656429>`__ - Correct RTT estimate
      used in 0-RTT anti-replay.
   -  `Bug 1588941 <https://bugzilla.mozilla.org/show_bug.cgi?id=1588941>`__ - Send empty
      certificate message when scheme selection fails.
   -  `Bug 1652032 <https://bugzilla.mozilla.org/show_bug.cgi?id=1652032>`__ - Fix failure to build
      in Windows arm64 makefile cross-compilation.
   -  `Bug 1625791 <https://bugzilla.mozilla.org/show_bug.cgi?id=1625791>`__ - Fix deadlock issue in
      nssSlot_IsTokenPresent.
   -  `Bug 1653975 <https://bugzilla.mozilla.org/show_bug.cgi?id=1653975>`__ - Fix 3.53 regression
      by setting "all" as the default makefile target.
   -  `Bug 1659792 <https://bugzilla.mozilla.org/show_bug.cgi?id=1659792>`__ - Fix broken libpkix
      tests with unexpired PayPal cert.
   -  `Bug 1659814 <https://bugzilla.mozilla.org/show_bug.cgi?id=1659814>`__ - Fix interop.sh
      failures with newer tls-interop commit and dependencies.
   -  `Bug 1656519 <https://bugzilla.mozilla.org/show_bug.cgi?id=1656519>`__ - Update NSPR
      dependency to 4.28.

   This Bugzilla query returns all the bugs fixed in NSS 3.56:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.56

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.56 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.56 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).