.. _mozilla_projects_nss_nss_3_27_2_release_notes:

NSS 3.27.2 Release Notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.27.2 is a patch release for NSS 3.27.



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_27_2_RTM. NSS 3.27.2 requires NSPR 4.13 or newer.

   NSS 3.27.2 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      `https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_27_2_RTM/src/ <https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_27_1_RTM/src/>`__

.. _new_in_nss_3.27.2:

`New in NSS 3.27.2 <#new_in_nss_3.27.2>`__
------------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   No new functionality is introduced in this release. This is a patch release to address a memory
   leak in the ``SSL_SetTrustAnchors()`` function.

.. _notable_changes_in_nss_3.27.2:

`Notable Changes in NSS 3.27.2 <#notable_changes_in_nss_3.27.2>`__
------------------------------------------------------------------

.. container::

   The ``SSL_SetTrustAnchors()`` function is used to set the distinguished names that an NSS server
   includes in its TLS CertificateRequest message.  If this function is not used, NSS will include
   the distinguished names for all trust anchors installed in the database.  This can be a lengthy
   list.

   Previous versions of NSS leaked the memory used to store distinguished names when
   ``SSL_SetTrustAnchors()`` was used.  This release fixes that error.

.. _bugs_fixed_in_nss_3.27.2:

`Bugs fixed in NSS 3.27.2 <#bugs_fixed_in_nss_3.27.2>`__
--------------------------------------------------------

.. container::

   -  The following bug has been fixed in NSS 3.27.2: `Bug 1318561 - SSL_SetTrustAnchors
      leaks <https://bugzilla.mozilla.org/show_bug.cgi?id=1318561>`__

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.27.2 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.27.2 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).