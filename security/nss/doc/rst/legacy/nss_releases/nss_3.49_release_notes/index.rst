.. _mozilla_projects_nss_nss_3_49_release_notes:

NSS 3.49 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.49 on **3 January 2020**, which is a
   minor release.

   The NSS team would like to recognize first-time contributors:

   -  Alex Henrie



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_49_RTM. NSS 3.49 requires NSPR 4.24 or newer.

   NSS 3.49 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_49_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _notable_changes_in_nss_3.49:

`Notable Changes in NSS 3.49 <#notable_changes_in_nss_3.49>`__
--------------------------------------------------------------

.. container::

   -  The legacy DBM database, **libnssdbm**, is no longer built by default when using gyp builds.
      See `Bug 1594933 <https://bugzilla.mozilla.org/show_bug.cgi?id=1594933>`__ for details.

.. _bugs_fixed_in_nss_3.49:

`Bugs fixed in NSS 3.49 <#bugs_fixed_in_nss_3.49>`__
----------------------------------------------------

.. container::

   -  `Bug 1513586 <https://bugzilla.mozilla.org/show_bug.cgi?id=1513586>`__ - Set downgrade
      sentinel for client TLS versions lower than 1.2.
   -  `Bug 1606025 <https://bugzilla.mozilla.org/show_bug.cgi?id=1606025>`__ - Remove
      -Wmaybe-uninitialized warning in sslsnce.c
   -  `Bug 1606119 <https://bugzilla.mozilla.org/show_bug.cgi?id=1606119>`__ - Fix PPC HW Crypto
      build failure
   -  `Bug 1605545 <https://bugzilla.mozilla.org/show_bug.cgi?id=1605545>`__ - Memory leak in
      Pk11Install_Platform_Generate
   -  `Bug 1602288 <https://bugzilla.mozilla.org/show_bug.cgi?id=1602288>`__ - Fix build failure due
      to missing posix signal.h
   -  `Bug 1588714 <https://bugzilla.mozilla.org/show_bug.cgi?id=1588714>`__ - Implement
      CheckARMSupport for Win64/aarch64
   -  `Bug 1585189 <https://bugzilla.mozilla.org/show_bug.cgi?id=1585189>`__ - NSS database uses
      3DES instead of AES to encrypt DB entries
   -  `Bug 1603257 <https://bugzilla.mozilla.org/show_bug.cgi?id=1603257>`__ - Fix UBSAN issue in
      softoken CKM_NSS_CHACHA20_CTR initialization
   -  `Bug 1590001 <https://bugzilla.mozilla.org/show_bug.cgi?id=1590001>`__ - Additional HRR Tests
      (CVE-2019-17023)
   -  `Bug 1600144 <https://bugzilla.mozilla.org/show_bug.cgi?id=1600144>`__ - Treat ClientHello
      with message_seq of 1 as a second ClientHello
   -  `Bug 1603027 <https://bugzilla.mozilla.org/show_bug.cgi?id=1603027>`__ - Test that ESNI is
      regenerated after HelloRetryRequest
   -  `Bug 1593167 <https://bugzilla.mozilla.org/show_bug.cgi?id=1593167>`__ - Intermittent
      mis-reporting potential security risk SEC_ERROR_UNKNOWN_ISSUER
   -  `Bug 1535787 <https://bugzilla.mozilla.org/show_bug.cgi?id=1535787>`__ - Fix
      automation/release/nss-release-helper.py on MacOS
   -  `Bug 1594933 <https://bugzilla.mozilla.org/show_bug.cgi?id=1594933>`__ - Disable building DBM
      by default
   -  `Bug 1562548 <https://bugzilla.mozilla.org/show_bug.cgi?id=1562548>`__ - Improve GCM
      perfomance on aarch32

   This Bugzilla query returns all the bugs fixed in NSS 3.49:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.49

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.49 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.49 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).