.. _mozilla_projects_nss_nss_3_16_2_2_release_notes:

NSS 3.16.2.2 release notes
==========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.16.2.2 is a patch release for NSS 3.16. The bug fixes in NSS
   3.16.2.2 are described in the "Bugs Fixed" section below.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_16_2_2_RTM. NSS 3.16.2.2 requires NSPR 4.10.6 or newer.

   NSS 3.16.2.2 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_16_2_2_RTM/src/

.. _new_in_nss_3.16.2.2:

`New in NSS 3.16.2.2 <#new_in_nss_3.16.2.2>`__
----------------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   No new functionality is introduced in this release. This is a patch release to fix a regression.

.. _notable_changes_in_nss_3.16.2.2:

`Notable Changes in NSS 3.16.2.2 <#notable_changes_in_nss_3.16.2.2>`__
----------------------------------------------------------------------

.. container::

   -  `Bug 1049435 <https://bugzilla.mozilla.org/show_bug.cgi?id=1049435>`__: Change
      RSA_PrivateKeyCheck to not require p > q. This fixes a regression introduced in NSS 3.16.2
      that prevented NSS from importing some RSA private keys (such as in PKCS #12 files) generated
      by other crypto libraries.

.. _bugs_fixed_in_nss_3.16.2.2:

`Bugs fixed in NSS 3.16.2.2 <#bugs_fixed_in_nss_3.16.2.2>`__
------------------------------------------------------------

.. container::

   -  `Bug 1049435 <https://bugzilla.mozilla.org/show_bug.cgi?id=1049435>`__ - Importing an RSA
      private key fails if p < q

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.16.2.2 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.16.2.2 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).