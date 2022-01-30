.. _mozilla_projects_nss_nss_3_31_1_release_notes:

NSS 3.31.1 release notes
========================

.. container::

   .. note::

      **This is a DRAFT document.** This notice will be removed when completed.

`Introduction <#introduction>`__
--------------------------------

.. container::

   The Network Security Services (NSS) team has released NSS 3.31.1, which is a patch release for
   NSS 3.31.

.. _distribution_information:

`Distribution information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The hg tag is NSS_3_31_1_RTM. NSS 3.31.1 requires Netscape Portable Runtime (NSPR) 4.15, or
   newer.

   NSS 3.31.1 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_31_1_RTM/src/

.. _new_in_nss_3.31.1:

`New in NSS 3.31.1 <#new_in_nss_3.31.1>`__
------------------------------------------

.. container::

   No new functionality is introduced in this release.

.. _bugs_fixed_in_nss_3.31.1:

`Bugs fixed in NSS 3.31.1 <#bugs_fixed_in_nss_3.31.1>`__
--------------------------------------------------------

.. container::

   -  `Bug 1381784 <https://bugzilla.mozilla.org/show_bug.cgi?id=1381784>`__ - Potential deadlock
      when using an external PKCS#11 token.

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.31.1 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.31.1 shared libraries,
   without recompiling, or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).