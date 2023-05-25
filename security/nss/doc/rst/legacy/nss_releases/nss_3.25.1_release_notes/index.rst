.. _mozilla_projects_nss_nss_3_25_1_release_notes:

NSS 3.25.1 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.25.1 is a patch release for NSS 3.25.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_25_1_RTM. NSS 3.25.1 requires NSPR 4.12 or newer.

   NSS 3.25.1 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_25_1_RTM/src/

.. _new_in_nss_3.25.1:

`New in NSS 3.25.1 <#new_in_nss_3.25.1>`__
------------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   No new functionality is introduced in this release. This is a patch release to address a TLS
   compatibility issue that some client applications experienced with NSS 3.25.

.. _notable_changes_in_nss_3.25.1:

`Notable Changes in NSS 3.25.1 <#notable_changes_in_nss_3.25.1>`__
------------------------------------------------------------------

.. container::

   MD5 signature algorithms sent by the server in CertificateRequest messages are now properly
   ignored. Previously, with rare server configurations, an MD5 signature algorithm might have been
   selected for client authentication and caused the client to abort the connection soon after.

.. _bugs_fixed_in_nss_3.25.1:

`Bugs fixed in NSS 3.25.1 <#bugs_fixed_in_nss_3.25.1>`__
--------------------------------------------------------

.. container::

   -  The following bug has been fixed in NSS 3.25.1: `Ignore MD5 signature algorithms in
      certificate requests <https://bugzilla.mozilla.org/show_bug.cgi?id=1304407>`__

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.25.1 shared libraries are backwards compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.25.1 shared libraries
   without recompiling or relinking. Applications that restrict their use of NSS APIs to the
   functions listed in NSS Public Functions will remain compatible with future versions of the NSS
   shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).