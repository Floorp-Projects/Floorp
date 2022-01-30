.. _mozilla_projects_nss_nss_3_63_1_release_notes:

NSS 3.63.1 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.63.1 was released on **6 April 2021**.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_63_1_RTM. NSS 3.63.1 requires NSPR 4.30 or newer.

   NSS 3.63.1 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_63_1_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _bugs_fixed_in_nss_3.63.1:

`Bugs fixed in NSS 3.63.1 <#bugs_fixed_in_nss_3.63.1>`__
--------------------------------------------------------

.. container::

   -  REVERTING Bug 1692094 - Turn off Websites Trust Bit for 'Chambers of Commerce Root - 2008' and
      'Global Chambersign Root - 2008’.

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.63.1 shared libraries are backwards-compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.63.1 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report on
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).

`Notes <#notes>`__
------------------

.. container::

   This version of NSS contains a minor update to the root CAs due to a delay in deprecation.

   This revert is temporary in order to prevent breaking websites with Firefox 88 and the change has
   been reinstated in NSS 3.64 for Firefox 89.