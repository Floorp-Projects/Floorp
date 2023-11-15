.. _mozilla_projects_nss_nss_3_58_release_notes:

NSS 3.58 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.58 on **16 October 2020**, which is a
   minor release.

   The NSS team would like to recognize first-time contributors:

   -  Ricky Stewart



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_58_RTM. NSS 3.58 requires NSPR 4.29 or newer.

   NSS 3.58 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_58_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _bugs_fixed_in_nss_3.58:

`Bugs fixed in NSS 3.58 <#bugs_fixed_in_nss_3.58>`__
----------------------------------------------------

.. container::

   -  `Bug 1641480 <https://bugzilla.mozilla.org/show_bug.cgi?id=1641480>`__ (CVE-2020-25648) -
      Tighten CCS handling for middlebox compatibility mode.
   -  `Bug 1631890 <https://bugzilla.mozilla.org/show_bug.cgi?id=1631890>`__ - Add support for
      Hybrid Public Key Encryption
      (`draft-irtf-cfrg-hpke <https://datatracker.ietf.org/doc/draft-irtf-cfrg-hpke/>`__) support.
   -  `Bug 1657255 <https://bugzilla.mozilla.org/show_bug.cgi?id=1657255>`__ - Add CI tests that
      disable SHA1/SHA2 ARM crypto extensions.
   -  `Bug 1668328 <https://bugzilla.mozilla.org/show_bug.cgi?id=1668328>`__ - Handle spaces in the
      Python path name when using gyp on Windows.
   -  `Bug 1667153 <https://bugzilla.mozilla.org/show_bug.cgi?id=1667153>`__ - Add
      PK11_ImportDataKey for data object import.
   -  `Bug 1665715 <https://bugzilla.mozilla.org/show_bug.cgi?id=1665715>`__ - Pass the embedded SCT
      list extension (if present) to TrustDomain::CheckRevocation instead of the notBefore value.

   This Bugzilla query returns all the bugs fixed in NSS 3.58:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.58

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.58 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.58 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).