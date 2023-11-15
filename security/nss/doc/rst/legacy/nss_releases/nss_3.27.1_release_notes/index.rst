.. _mozilla_projects_nss_nss_3_27_1_release_notes:

NSS 3.27.1 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.27.1 is a patch release for NSS 3.27.



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_27_1_RTM. NSS 3.27.1 requires NSPR 4.13 or newer.

   NSS 3.27.1 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_27_1_RTM/src/

.. _new_in_nss_3.27.1:

`New in NSS 3.27.1 <#new_in_nss_3.27.1>`__
------------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   No new functionality is introduced in this release. This is a patch release to address a TLS
   compatibility issue which some applications experienced with NSS 3.27.

.. _notable_changes_in_nss_3.27.1:

`Notable Changes in NSS 3.27.1 <#notable_changes_in_nss_3.27.1>`__
------------------------------------------------------------------

.. container::

   Availability of the TLS 1.3 (draft) implementation has been re-disabled in the default build.

   Previous versions of NSS made TLS 1.3 (draft) available only when compiled with
   NSS_ENABLE_TLS_1_3. NSS 3.27 set this value on by default, allowing TLS 1.3 (draft) to be
   disabled using NSS_DISABLE_TLS_1_3, although the maximum version used by default remained TLS
   1.2.

   However, some applications query the list of protocol versions that are supported by the NSS
   library, enabling all supported TLS protocol versions. Because NSS 3.27 enabled compilation of
   TLS 1.3 (draft) by default, it caused those applications to enable TLS 1.3 (draft). This resulted
   in connectivity failures, as some TLS servers are version 1.3 intolerant, and failed to negotiate
   an earlier TLS version with NSS 3.27 clients.

   NSS 3.27.1 once again requires NSS_ENABLE_TLS_1_3 to be deliberately set to enable TLS 1.3
   (draft).

.. _bugs_fixed_in_nss_3.27.1:

`Bugs fixed in NSS 3.27.1 <#bugs_fixed_in_nss_3.27.1>`__
--------------------------------------------------------

.. container::

   -  The following bug has been fixed in NSS 3.27.1: `Re-disable TLS 1.3 by
      default <https://bugzilla.mozilla.org/show_bug.cgi?id=1306985>`__

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.27.1 shared libraries are backwards compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.27.1 shared libraries
   without recompiling or relinking. Applications that restrict their use of NSS APIs to the
   functions listed in NSS Public Functions will remain compatible with future versions of the NSS
   shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).