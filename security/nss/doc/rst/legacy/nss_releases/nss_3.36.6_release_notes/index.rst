.. _mozilla_projects_nss_nss_3_36_6_release_notes:

NSS 3.36.6 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.36.6 is a patch release for NSS 3.36. The bug fixes in NSS
   3.36.6 are described in the "Bugs Fixed" section below.



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_36_6_RTM. NSS 3.36.6 requires NSPR 4.19 or newer.

   NSS 3.36.6 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_36_6_RTM/src/

.. _new_in_nss_3.36.6:

`New in NSS 3.36.6 <#new_in_nss_3.36.6>`__
------------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   No new functionality is introduced in this release. This is a patch release to fix CVE-2018-12404

.. _bugs_fixed_in_nss_3.36.6:

`Bugs fixed in NSS 3.36.6 <#bugs_fixed_in_nss_3.36.6>`__
--------------------------------------------------------

.. container::

   `Bug 1485864 <https://bugzilla.mozilla.org/show_bug.cgi?id=1485864>`__ - Cache side-channel
   variant of the Bleichenbacher attack (CVE-2018-12404)

   `Bug 1389967 <https://bugzilla.mozilla.org/show_bug.cgi?id=1389967>`__ and `Bug
   1448748 <https://bugzilla.mozilla.org/show_bug.cgi?id=1448748>`__ - Fixes for MinGW on x64
   platforms.

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.36.6 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.36.6 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).