.. _mozilla_projects_nss_nss_3_19_1_release_notes:

NSS 3.19.1 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.19.1 is a security release for NSS 3.19. The bug fixes in NSS
   3.19.1 are described in the "Bugs Fixed" section below.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_19_1_RTM. NSS 3.19.1 requires NSPR 4.10.8 or newer.

   NSS 3.19.1 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_19_1_RTM/src/

.. _security_fixes_in_nss_3.19.1:

`Security Fixes in NSS 3.19.1 <#security_fixes_in_nss_3.19.1>`__
----------------------------------------------------------------

.. container::

   -  `Bug
      1138554 <https://bugzilla.mozilla.org/show_bug.cgi?id=1138554>`__ / `CVE-2015-4000 <http://www.cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2015-4000>`__ -
      The minimum strength of keys that libssl will accept for finite field algorithms (RSA,
      Diffie-Hellman, and DSA) have been increased to 1023 bits.

.. _new_in_nss_3.19.1:

`New in NSS 3.19.1 <#new_in_nss_3.19.1>`__
------------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   No new functionality is introduced in this release. This patch release includes a fix for the
   recently published `logjam attack <https://weakdh.org/>`__.

.. _notable_changes_in_nss_3.19.1:

`Notable Changes in NSS 3.19.1 <#notable_changes_in_nss_3.19.1>`__
------------------------------------------------------------------

.. container::

   -  NSS reports the bit length of keys more accurately.  Thus, the SECKEY_PublicKeyStrength and
      SECKEY_PublicKeyStrengthInBits functions could report smaller values for values that have
      leading zero values. This affects the key strength values that are reported by
      SSL_GetChannelInfo.
   -  The minimum size of keys that NSS will generate, import, or use has been raised:

      -  The minimum modulus size for RSA keys is now 512 bits
      -  The minimum modulus size for DSA keys is now 1023 bits
      -  The minimum modulus size for Diffie-Hellman keys is now 1023 bits

.. _bugs_fixed_in_nss_3.19.1:

`Bugs fixed in NSS 3.19.1 <#bugs_fixed_in_nss_3.19.1>`__
--------------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.19.1:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.19.1

`Acknowledgements <#acknowledgements>`__
----------------------------------------

.. container::

   The NSS development team would like to thank Matthew Green and Karthikeyan Bhargavan for
   responsibly disclosing the issue in `bug
   1138554 <https://bugzilla.mozilla.org/show_bug.cgi?id=1138554>`__.

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.19.1 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.19.1 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

   **Note:** NSS 3.19.1 increases the minimum size of keys it is willing to use. This has been shown
   to break some applications. :ref:`mozilla_projects_nss_nss_3_19_2_release_notes` reverts the
   behaviour to the NSS 3.19 and earlier limits.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).