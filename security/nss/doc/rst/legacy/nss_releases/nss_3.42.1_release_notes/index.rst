.. _mozilla_projects_nss_nss_3_42_1_release_notes:

NSS 3.42.1 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.42.1 on 31 January 2019, which is a
   patch release.



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_42_1_RTM. NSS 3.42.1 requires NSPR 4.20 or newer.

   NSS 3.42.1 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_42_1_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _bugs_fixed_in_nss_3.42.1:

`Bugs fixed in NSS 3.42.1 <#bugs_fixed_in_nss_3.42.1>`__
--------------------------------------------------------

.. container::

   -  `Bug 1507135 <https://bugzilla.mozilla.org/show_bug.cgi?id=1507135>`__ and `Bug
      1507174 <https://bugzilla.mozilla.org/show_bug.cgi?id=1507174>`__ - Add additional null checks
      to several CMS functions to fix a rare CMS crash. Thanks to Hanno Böck and Damian Poddebniak
      for the discovery and fixes. This was originally announced in
      :ref:`mozilla_projects_nss_nss_3_42_release_notes`, but was mistakenly not included in the
      release. (`CVE-2018-18508 <https://bugzilla.mozilla.org/show_bug.cgi?id=CVE-2018-18508>`__)

   This Bugzilla query returns all the bugs fixed in NSS 3.42.1:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.42.1

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.42.1 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.42.1 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).