.. _mozilla_projects_nss_nss_3_19_2_release_notes:

NSS 3.19.2 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.19.2 is a patch release for NSS 3.19 that addresses
   compatibility issues in NSS 3.19.1.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_19_2_RTM. NSS 3.19.2 requires NSPR 4.10.8 or newer.

   NSS 3.19.2 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_19_2_RTM/src/

.. _new_in_nss_3.19.2:

`New in NSS 3.19.2 <#new_in_nss_3.19.2>`__
------------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   No new functionality is introduced in this release.

.. _notable_changes_in_nss_3.19.2:

`Notable Changes in NSS 3.19.2 <#notable_changes_in_nss_3.19.2>`__
------------------------------------------------------------------

.. container::

   -  `Bug 1172128 <https://bugzilla.mozilla.org/show_bug.cgi?id=1172128>`__ - In NSS 3.19.1, the
      minimum key sizes that the freebl cryptographic implementation (part of the softoken
      cryptographic module used by default by NSS) was willing to generate or use was increased -
      for RSA keys, to 512 bits, and for DH keys, 1023 bits. This was done as part of a security fix
      for `Bug 1138554 <https://bugzilla.mozilla.org/show_bug.cgi?id=1138554>`__ /
      `CVE-2015-4000 <http://www.cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2015-4000>`__.
      Applications that requested or attempted to use keys smaller then the minimum size would fail.
      However, this change in behaviour unintentionally broke existing NSS applications that need to
      generate or use such keys, via APIs such as SECKEY_CreateRSAPrivateKey or
      SECKEY_CreateDHPrivateKey.
      In NSS 3.19.2, this change in freebl behaviour has been reverted. The fix for `Bug
      1138554 <https://bugzilla.mozilla.org/show_bug.cgi?id=1138554>`__ has been moved to libssl,
      and will now only affect the minimum keystrengths used in SSL/TLS.
      **Note:** Future versions of NSS *may* increase the minimum keysizes required by the freebl
      module. Consumers of NSS are **strongly** encouraged to migrate to stronger cryptographic
      strengths as soon as possible.

.. _bugs_fixed_in_nss_3.19.2:

`Bugs fixed in NSS 3.19.2 <#bugs_fixed_in_nss_3.19.2>`__
--------------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.19.2:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.19.2

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.19.2 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.19.2 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).