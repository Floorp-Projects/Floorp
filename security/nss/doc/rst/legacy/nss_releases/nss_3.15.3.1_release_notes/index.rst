.. _mozilla_projects_nss_nss_3_15_3_1_release_notes:

NSS 3.15.3.1 release notes
==========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.15.3.1 is a patch release for NSS 3.15. The bug fixes in NSS
   3.15.3.1 are described in the "Bugs Fixed" section below.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_15_3_1_RTM. NSS 3.15.3.1 requires NSPR 4.10.2 or newer.

   NSS 3.15.3.1 source distributions are also available on ftp.mozilla.org for secure HTTPS
   download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_15_3_1_RTM/src/

.. _security_advisories:

`Security Advisories <#security_advisories>`__
----------------------------------------------

.. container::

   The following security-relevant bugs have been resolved in NSS 3.15.3.1. Users are encouraged to
   upgrade immediately.

   -  `Bug 946351 <https://bugzilla.mozilla.org/show_bug.cgi?id=946351>`__ - Misissued Google
      certificates from DCSSI

.. _new_in_nss_3.15.3.1:

`New in NSS 3.15.3.1 <#new_in_nss_3.15.3.1>`__
----------------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   No new major functionality is introduced in this release. This is a patch release to `revoke
   trust of a subordinate CA
   certificate <https://blog.mozilla.org/security/2013/12/09/revoking-trust-in-one-anssi-certificate/>`__
   that was mis-used to generate a certificate used by a network appliance.

.. _bugs_fixed_in_nss_3.15.3.1:

`Bugs fixed in NSS 3.15.3.1 <#bugs_fixed_in_nss_3.15.3.1>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `Bug 946351 <https://bugzilla.mozilla.org/show_bug.cgi?id=946351>`__ - Misissued Google
      certificates from DCSSI

   A complete list of all bugs resolved in this release can be obtained at
   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&target_milestone=3.15.3.1&product=NSS

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.15.3.1 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.15.3.1 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).