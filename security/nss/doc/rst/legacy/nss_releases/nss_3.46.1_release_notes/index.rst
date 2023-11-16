.. _mozilla_projects_nss_nss_3_46_1_release_notes:

NSS 3.46.1 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.46.1 is a patch release for NSS 3.46. The bug fixes in NSS
   3.46.1 are described in the "Bugs Fixed" section below. It was released on 2 October 2019.



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_46_1_RTM. NSS 3.46.1 requires NSPR 4.22 or newer.

   NSS 3.46.1 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_46_1_RTM/src/

   Other releases are available in NSS Releases.

.. _new_in_nss_3.46.1:

`New in NSS 3.46.1 <#new_in_nss_3.46.1>`__
------------------------------------------

.. container::

   No new functionality is introduced in this release.

.. _bugs_fixed_in_nss_3.46.1:

`Bugs fixed in NSS 3.46.1 <#bugs_fixed_in_nss_3.46.1>`__
--------------------------------------------------------

.. container::

   -  `Bug 1582343 <https://bugzilla.mozilla.org/show_bug.cgi?id=1582343>`__\ - Soft token MAC
      verification not constant time
   -  `Bug 1577953 <https://bugzilla.mozilla.org/show_bug.cgi?id=1577953>`__\ - Remove arbitrary
      HKDF output limit by allocating space as needed

   This Bugzilla query returns all the bugs fixed in NSS 3.46.1:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.46.1

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.46.1 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.46.1 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__\ (product NSS).