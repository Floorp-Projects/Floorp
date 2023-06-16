.. _mozilla_projects_nss_nss_3_20_2_release_notes:

NSS 3.20.2 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.20.2 is a security patch release for NSS 3.20. The bug fixes in
   NSS 3.20.2 are described in the "Security Fixes" section below.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_20_2_RTM. NSS 3.20.2 requires NSPR 4.10.10 or newer.

   NSS 3.20.2 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_20_2_RTM/src/

.. _security_fixes_in_nss_3.20.2:

`Security Fixes in NSS 3.20.2 <#security_fixes_in_nss_3.20.2>`__
----------------------------------------------------------------

.. container::

   -  `Bug 1158489 <https://bugzilla.mozilla.org/show_bug.cgi?id=1158489>`__
      ` <https://bugzilla.mozilla.org/show_bug.cgi?id=1138554>`__ /
      `CVE-2015-7575 <http://www.cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2015-7575>`__ - Prevent
      MD5 Downgrade in TLS 1.2 Signatures.

.. _new_in_nss_3.20.2:

`New in NSS 3.20.2 <#new_in_nss_3.20.2>`__
------------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   No new functionality is introduced in this release.

`Acknowledgements <#acknowledgements>`__
----------------------------------------

.. container::

   The NSS development team would like to thank Karthikeyan Bhargavan from
   `INRIA <http://inria.fr/>`__ for responsibly disclosing the issue in `Bug
   1158489 <https://bugzilla.mozilla.org/show_bug.cgi?id=1158489>`__.

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.20.2 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.20.2 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).