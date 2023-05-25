.. _mozilla_projects_nss_nss_3_14_4_release_notes:

NSS 3.14.4 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.14.4 is a patch release for NSS 3.14. The bug fixes in NSS
   3.14.4 are described in the "Bugs Fixed" section below.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The CVS tag is NSS_3_14_4_RTM. NSS 3.14.4 requires NSPR 4.9.5 or newer.

   NSS 3.14.4 source distributions are also available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_14_4_RTM/src/

.. _security_advisories:

`Security Advisories <#security_advisories>`__
----------------------------------------------

.. container::

   The following security-relevant bugs have been resolved in NSS 3.14.4. Users are encouraged to
   upgrade immediately.

   -  `Bug 894370 <https://bugzilla.mozilla.org/show_bug.cgi?id=894370>`__ - (CVE-2013-1739) Avoid
      uninitialized data read in the event of a decryption failure.

.. _new_in_nss_3.14.4:

`New in NSS 3.14.4 <#new_in_nss_3.14.4>`__
------------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  No new major functionality is introduced in this release. This release is a patch release to
      address `CVE-2013-1739 <http://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2013-1739>`__.

.. _bugs_fixed_in_nss_3.14.4:

`Bugs fixed in NSS 3.14.4 <#bugs_fixed_in_nss_3.14.4>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  https://bugzilla.mozilla.org/buglist.cgi?bug_id=894370%2C832942%2C863947&bug_id_type=anyexact&list_id=8338081&resolution=FIXED&classification=Components&query_format=advanced&product=NSS

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.14.4 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.14.4 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).