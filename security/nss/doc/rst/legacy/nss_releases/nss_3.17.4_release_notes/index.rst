.. _mozilla_projects_nss_nss_3_17_4_release_notes:

NSS 3.17.4 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.17.4 is a patch release for NSS 3.17. The bug fixes in NSS
   3.17.4 are described in the "Bugs Fixed" section below.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_17_4_RTM. NSS 3.17.4 requires NSPR 4.10.7 or newer.

   NSS 3.17.4 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_17_4_RTM/src/

.. _new_in_nss_3.17.4:

`New in NSS 3.17.4 <#new_in_nss_3.17.4>`__
------------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   No new functionality is introduced in this release. This is a patch release to fix multiple bugs.

.. _notable_changes_in_nss_3.17.4:

`Notable Changes in NSS 3.17.4 <#notable_changes_in_nss_3.17.4>`__
------------------------------------------------------------------

.. container::

   -  `Bug 1084986 <https://bugzilla.mozilla.org/show_bug.cgi?id=1084986>`__: If an SSL/TLS
      connection fails, because client and server don't have any common protocol version enabled,
      NSS has been changed to report error code SSL_ERROR_UNSUPPORTED_VERSION (instead of reporting
      SSL_ERROR_NO_CYPHER_OVERLAP).
   -  `Bug 1112461 <https://bugzilla.mozilla.org/show_bug.cgi?id=1112461>`__: libpkix was fixed to
      prefer the newest certificate, if multiple certificates match.
   -  `Bug 1094492 <https://bugzilla.mozilla.org/show_bug.cgi?id=1094492>`__: fixed a memory
      corruption issue during failure of keypair generation.
   -  `Bug 1113632 <https://bugzilla.mozilla.org/show_bug.cgi?id=1113632>`__: fixed a failure to
      reload a PKCS#11 module in FIPS mode.
   -  `Bug 1119983 <https://bugzilla.mozilla.org/show_bug.cgi?id=1119983>`__: fixed interoperability
      of NSS server code with a LibreSSL client.

.. _bugs_fixed_in_nss_3.17.4:

`Bugs fixed in NSS 3.17.4 <#bugs_fixed_in_nss_3.17.4>`__
--------------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.17.4:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.17.4

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.17.4 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.17.4 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).