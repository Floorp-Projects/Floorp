.. _mozilla_projects_nss_nss_3_20_1_release_notes:

NSS 3.20.1 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.20.1 is a patch release for NSS 3.20. The bug fixes in NSS
   3.20.1 are described in the "Security Advisories" section below.



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_20_1_RTM. NSS 3.20.1 requires NSPR 4.10.10 or newer.

   NSS 3.20.1 and NSPR 4.10.10 source distributions are available on ftp.mozilla.org for secure
   HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_20_1_RTM/src/
      https://ftp.mozilla.org/pub/nspr/releases/v4.10.10/src/

.. _security_advisories:

`Security Advisories <#security_advisories>`__
----------------------------------------------

.. container::

   The following security-relevant bugs have been resolved in NSS 3.20.1. Users are encouraged to
   upgrade immediately.

   -  `Bug 1192028 <https://bugzilla.mozilla.org/show_bug.cgi?id=1192028>`__ (CVE-2015-7181) and
      `Bug 1202868 <https://bugzilla.mozilla.org/show_bug.cgi?id=1202868>`__ (CVE-2015-7182):
      Several issues existed within the ASN.1 decoder used by NSS for handling streaming BER data.
      While the majority of NSS uses a separate, unaffected DER decoder, several public routines
      also accept BER data, and thus are affected. An attacker that successfully exploited these
      issues can overflow the heap and may be able to obtain remote code execution.

   | The following security-relevant bugs have been resolved in NSPR 4.10.10, which affect NSS.
   | Because NSS includes portions of the affected NSPR code at build time, it is necessary to use
     NSPR 4.10.10 when building NSS.

   -  `Bug 1205157 <https://bugzilla.mozilla.org/show_bug.cgi?id=1205157>`__ (NSPR, CVE-2015-7183):
      A logic bug in the handling of large allocations would allow exceptionally large allocations
      to be reported as successful, without actually allocating the requested memory. This may allow
      attackers to bypass security checks and obtain control of arbitrary memory.

.. _new_in_nss_3.20.1:

`New in NSS 3.20.1 <#new_in_nss_3.20.1>`__
------------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   No new functionality is introduced in this release. This is a patch release to fix
   security-relevant bugs.

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.20.1 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.20.1 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).