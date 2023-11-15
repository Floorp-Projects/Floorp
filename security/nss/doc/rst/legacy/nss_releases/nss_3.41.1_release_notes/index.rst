.. _mozilla_projects_nss_nss_3_41_1_release_notes:

NSS 3.41.1 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.41.1 is a patch release for NSS 3.41. The bug fixes in NSS
   3.41.1 are described in the "Bugs Fixed" section below. It was released on 22 January 2019.



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_41_1_RTM. NSS 3.41.1 requires NSPR 4.20 or newer.

   NSS 3.41.1 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_41_1_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _new_in_nss_3.41.1:

`New in NSS 3.41.1 <#new_in_nss_3.41.1>`__
------------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   No new functionality is introduced in this release. This is a patch release to fix bugs.

.. _bugs_fixed_in_nss_3.41.1:

`Bugs fixed in NSS 3.41.1 <#bugs_fixed_in_nss_3.41.1>`__
--------------------------------------------------------

.. container::

   -  Bug 1507135 and Bug 1507174 - Add additional null checks to several CMS functions to fix a
      rare CMS crash. Thanks to Hanno Böck and Damian Poddebniak for the discovery and fixes.
      (CVE-2018-18508)

   This bugzilla query returns all bugs fixed in 3.41.1:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.41.1

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.41.1 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.41.1 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).