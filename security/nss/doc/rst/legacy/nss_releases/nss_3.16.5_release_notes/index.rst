.. _mozilla_projects_nss_nss_3_16_5_release_notes:

NSS 3.16.5 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.16.5 is a patch release for NSS 3.16. The bug fixes in NSS
   3.16.5 are described in the "Bugs Fixed" section below.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_16_5_RTM. NSS 3.16.5 requires NSPR 4.10.6 or newer.

   NSS 3.16.5 source distributions are also available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_16_5_RTM/src/

.. _security_advisories:

`Security Advisories <#security_advisories>`__
----------------------------------------------

.. container::

   The following security-relevant bugs have been resolved in NSS 3.16.5. Users are encouraged to
   upgrade immediately.

   -  `Bug 1064636 <https://bugzilla.mozilla.org/show_bug.cgi?id=1064636>`__ - (CVE-2014-1568) RSA
      Signature Forgery in NSS. See also `MFSA
      2014-73 <https://www.mozilla.org/security/announce/2014/mfsa2014-73.html>`__ for details.

.. _new_in_nss_3.16.5:

`New in NSS 3.16.5 <#new_in_nss_3.16.5>`__
------------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   No new functionality is introduced in this release. This is a patch release to fix a bug that
   caused NSS to accept forged RSA signatures.

   A new symbol, \_SGN_VerifyPKCS1DigestInfo is exported in this release. As with all exported NSS
   symbols that have a leading underscore '_', this is an internal symbol for NSS use only.
   Applications that use or depend on these symbols can and will break in future NSS releases.

.. _bugs_fixed_in_nss_3.16.5:

`Bugs fixed in NSS 3.16.5 <#bugs_fixed_in_nss_3.16.5>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `Bug 1064636 <https://bugzilla.mozilla.org/show_bug.cgi?id=1064636>`__ - (CVE-2014-1568) RSA
      Signature Forgery in NSS

`Acknowledgements <#acknowledgements>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The NSS development team would like to thank Antoine Delignat-Lavaud, security researcher at
   Inria Paris in team Prosecco, and the Advanced Threat Research team at Intel Security, who both
   independently discovered and reported this issue, for responsibly disclosing the issue by
   providing advance copies of their research.

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.16.5 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.16.5 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).