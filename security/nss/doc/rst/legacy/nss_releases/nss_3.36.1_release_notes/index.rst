.. _mozilla_projects_nss_nss_3_36_1_release_notes:

NSS 3.36.1 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.36.1 is a patch release for NSS 3.36.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_36_1_RTM. NSS 3.36.1 requires NSPR 4.19 or newer.

   NSS 3.36.1 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/security/nss/releases/NSS_3_36_1_RTM/src/

.. _new_in_nss_3.xx:

`New in NSS 3.XX <#new_in_nss_3.xx>`__
--------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   No new functionality is introduced in this release. This is a patch release to fix regression
   bugs.

.. _notable_changes_in_nss_3.36.1:

`Notable Changes in NSS 3.36.1 <#notable_changes_in_nss_3.36.1>`__
------------------------------------------------------------------

.. container::

   -  In NSS version 3.35 the iteration count in optimized builds, which is used for password based
      encryption algorithm related to encrypted PKCS#7 or PKCS#12 data, was increased to one million
      iterations. That change had caused an interoperability regression with operating systems that
      are limited to 600 K iterations. NSS 3.36.1 has been changed to use the same 600 K limit.

.. _bugs_fixed_in_nss_3.36.1:

`Bugs fixed in NSS 3.36.1 <#bugs_fixed_in_nss_3.36.1>`__
--------------------------------------------------------

.. container::

   -  Certain smartcard operations could result in a deadlock.

   This Bugzilla query returns all the bugs fixed in NSS 3.36.1:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.36.1

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.36.1 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.36.1 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).