.. _mozilla_projects_nss_nss_3_17_1_release_notes:

NSS 3.17.1 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.17.1 is a patch release for NSS 3.17. The bug fixes in NSS
   3.17.1 are described in the "Bugs Fixed" section below.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_17_1_RTM. NSS 3.17.1 requires NSPR 4.10.7 or newer.

   NSS 3.17.1 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_17_1_RTM/src/

.. _security_advisories:

`Security Advisories <#security_advisories>`__
----------------------------------------------

.. container::

   The following security-relevant bugs have been resolved in NSS 3.17.1. Users are encouraged to
   upgrade immediately.

   -  `Bug 1064636 <https://bugzilla.mozilla.org/show_bug.cgi?id=1064636>`__ - (CVE-2014-1568) RSA
      Signature Forgery in NSS. See also `MFSA
      2014-73 <https://www.mozilla.org/security/announce/2014/mfsa2014-73.html>`__ for details.

.. _new_in_nss_3.17.1:

`New in NSS 3.17.1 <#new_in_nss_3.17.1>`__
------------------------------------------

.. container::

   This patch release adds new functionality and fixes a bug that caused NSS to accept forged RSA
   signatures.

   A new symbol, \_SGN_VerifyPKCS1DigestInfo is exported in this release. As with all exported NSS
   symbols that have a leading underscore '_', this is an internal symbol for NSS use only.
   Applications that use or depend on these symbols can and will break in future NSS releases.

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `TLS_FALLBACK_SCSV <https://datatracker.ietf.org/doc/html/draft-ietf-tls-downgrade-scsv-00>`__
      is a signaling cipher suite value that indicates a handshake is the result of TLS version
      fallback.

   .. rubric:: New Macros
      :name: new_macros

   -  *in ssl.h*

      -  **SSL_ENABLE_FALLBACK_SCSV** - an SSL socket option that enables TLS_FALLBACK_SCSV. Off by
         default.

   -  *in sslerr.h*

      -  **SSL_ERROR_INAPPROPRIATE_FALLBACK_ALERT** - a new SSL error code.

   -  *in sslproto.h*

      -  **TLS_FALLBACK_SCSV** - a signaling cipher suite value that indicates a handshake is the
         result of TLS version fallback.

.. _notable_changes_in_nss_3.17.1:

`Notable Changes in NSS 3.17.1 <#notable_changes_in_nss_3.17.1>`__
------------------------------------------------------------------

.. container::

   -  `Signature algorithms now use SHA-256 instead of SHA-1 by
      default <https://bugzilla.mozilla.org/show_bug.cgi?id=1058933>`__.
   -  Added support for Linux on little-endian powerpc64.

.. _bugs_fixed_in_nss_3.17.1:

`Bugs fixed in NSS 3.17.1 <#bugs_fixed_in_nss_3.17.1>`__
--------------------------------------------------------

.. container::

   | This Bugzilla query returns all the bugs fixed in NSS 3.17.1:
   | https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.17.1

`Acknowledgements <#acknowledgements>`__
----------------------------------------

.. container::

   The NSS development team would like to thank Antoine Delignat-Lavaud, security researcher at
   Inria Paris in team Prosecco, and the Advanced Threat Research team at Intel Security, who both
   independently discovered and reported this issue, for responsibly disclosing the issue by
   providing advance copies of their research.

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.17.1 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.17.1 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).