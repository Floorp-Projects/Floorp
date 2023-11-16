.. _mozilla_projects_nss_nss_3_30_1_release_notes:

NSS 3.30.1 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.30.1 is a security patch release for NSS 3.30. The bug fixes in
   NSS 3.30.1 are described in the "Bugs Fixed" section below.



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_30_1_RTM. NSS 3.30.1 requires NSPR 4.14 or newer.

   NSS 3.30.1 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_30_1_RTM/src/

.. _new_in_nss_3.30.1:

`New in NSS 3.30.1 <#new_in_nss_3.30.1>`__
------------------------------------------

.. container::

   No new functionality is introduced in this release.

.. _bugs_fixed_in_nss_3.30.1:

`Bugs fixed in NSS 3.30.1 <#bugs_fixed_in_nss_3.30.1>`__
--------------------------------------------------------

.. container::

   -  `Bug 1344380 <https://bugzilla.mozilla.org/show_bug.cgi?id=1344380>`__ / Out-of-bounds write
      in Base64 encoding in NSS
      (`CVE-2017-5461 <https://www.mozilla.org/en-US/security/advisories/mfsa2017-10/#CVE-2017-5461>`__)

`Acknowledgements <#acknowledgements>`__
----------------------------------------

.. container::

   The NSS development team would like to thank Ronald Crane for responsibly disclosing the issue by
   providing advance copies of their research.

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.30.1 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.30.1 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).