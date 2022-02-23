.. _mozilla_projects_nss_nss_3_36_5_release_notes:

NSS 3.36.5 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.36.5 is a patch release for NSS 3.36. The bug fixes in NSS
   3.36.5 are described in the "Bugs Fixed" section below.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_36_5_RTM. NSS 3.36.5 requires NSPR 4.19 or newer.

   NSS 3.36.5 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_36_5_RTM/src/

.. _new_in_nss_3.36.5:

`New in NSS 3.36.5 <#new_in_nss_3.36.5>`__
------------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   No new functionality is introduced in this release. This is a patch release to fix CVE-2018-12384

.. _bugs_fixed_in_nss_3.36.5:

`Bugs fixed in NSS 3.36.5 <#bugs_fixed_in_nss_3.36.5>`__
--------------------------------------------------------

.. container::

   `Bug 1483128 <https://bugzilla.mozilla.org/show_bug.cgi?id=1483128>`__ - NSS responded to an
   SSLv2-compatible ClientHello with a ServerHello that had an all-zero random (CVE-2018-12384)

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.36.5 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.36.5 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).