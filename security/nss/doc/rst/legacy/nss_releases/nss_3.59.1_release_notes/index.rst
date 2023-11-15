.. _mozilla_projects_nss_nss_3_59_1_release_notes:

NSS 3.59.1 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.59.1 on **18 December 2020**, which
   is a patch release for NSS 3.59.



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_59_1_RTM. NSS 3.59.1 requires NSPR 4.29 or newer.

   NSS 3.59.1 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_59_1_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _bugs_fixed_in_nss_3.59.1:

`Bugs fixed in NSS 3.59.1 <#bugs_fixed_in_nss_3.59.1>`__
--------------------------------------------------------

.. container::

   -  `Bug 1679290 <https://bugzilla.mozilla.org/show_bug.cgi?id=1679290>`__ - Fix potential
      deadlock with certain third-party PKCS11 modules.

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.59.1 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.59.1 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).