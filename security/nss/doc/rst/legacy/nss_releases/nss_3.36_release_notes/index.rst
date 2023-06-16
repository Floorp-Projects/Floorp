.. _mozilla_projects_nss_nss_3_36_release_notes:

NSS 3.36 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.36, which is a minor release.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_36_RTM. NSS 3.36 requires NSPR 4.19 or newer.

   NSS 3.36 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_36_RTM/src/ (make a link)

.. _new_in_nss_3.36:

`New in NSS 3.36 <#new_in_nss_3.36>`__
--------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  Experimental APIs for TLS session cache handling.

.. _notable_changes_in_nss_3.36:

`Notable Changes in NSS 3.36 <#notable_changes_in_nss_3.36>`__
--------------------------------------------------------------

.. container::

   -  Replaced existing vectorized ChaCha20 code with verified HACL\* implementation.

.. _bugs_fixed_in_nss_3.36:

`Bugs fixed in NSS 3.36 <#bugs_fixed_in_nss_3.36>`__
----------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.36:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.36

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.36 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.36 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).