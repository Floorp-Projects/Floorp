.. _mozilla_projects_nss_nss_3_28_4_release_notes:

NSS 3.28.4 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.28.4 is a security patch release for NSS 3.28. The bug fixes in
   NSS 3.28.4 are described in the "Bugs Fixed" section below.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_28_4_RTM. NSS 3.28.4 requires NSPR 4.13.1 or newer.

   NSS 3.28.4 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_28_4_RTM/src/

.. _new_in_nss_3.28.4:

`New in NSS 3.28.4 <#new_in_nss_3.28.4>`__
------------------------------------------

.. container::

   No new functionality is introduced in this release.

.. _bugs_fixed_in_nss_3.28.4:

`Bugs fixed in NSS 3.28.4 <#bugs_fixed_in_nss_3.28.4>`__
--------------------------------------------------------

.. container::

   -  `Bug 1344380 <https://bugzilla.mozilla.org/show_bug.cgi?id=1344380>`__ / Out-of-bounds write
      in Base64 encoding in NSS
      (`CVE-2017-5461 <https://www.mozilla.org/en-US/security/advisories/mfsa2017-10/#CVE-2017-5461>`__)
   -  `Bug 1345089 <https://bugzilla.mozilla.org/show_bug.cgi?id=1345089>`__ / DRBG flaw in NSS
      (`CVE-2017-5462 <https://www.mozilla.org/en-US/security/advisories/mfsa2017-10/#CVE-2017-5462>`__)
   -  `Bug 1342358 - Crash in
      tls13_DestroyKeyShares <https://bugzilla.mozilla.org/show_bug.cgi?id=1342358>`__

`Acknowledgements <#acknowledgements>`__
----------------------------------------

.. container::

   The NSS development team would like to thank Ronald Crane and Vladimir Klebanov for responsibly
   disclosing the issues by providing advance copies of their research.

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.28.4 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.28.4 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).