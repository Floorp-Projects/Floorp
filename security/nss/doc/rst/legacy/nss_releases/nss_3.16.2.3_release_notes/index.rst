.. _mozilla_projects_nss_nss_3_16_2_3_release_notes:

NSS 3.16.2.3 release notes
==========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.16.2.3 is a patch release for NSS 3.16. The bug fixes in NSS
   3.16.2.3 are described in the "Bugs Fixed" section below.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_16_2_3_RTM. NSS 3.16.2.3 requires NSPR 4.10.6 or newer.

   NSS 3.16.2.3 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_16_2_3_RTM/src/

.. _new_in_nss_3.16.2.3:

`New in NSS 3.16.2.3 <#new_in_nss_3.16.2.3>`__
----------------------------------------------

.. container::

   This patch release fixes a bug and contains a backport of the TLS_FALLBACK_SCSV feature, which
   was originally made available in NSS 3.17.1.

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `TLS_FALLBACK_SCSV <https://datatracker.ietf.org/doc/html/draft-ietf-tls-downgrade-scsv-00>`__
      is a signaling cipher suite value that indicates a handshake is the result of TLS version
      fallback.

.. _new_macros:

`New Macros <#new_macros>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  *in ssl.h*

      -  **SSL_ENABLE_FALLBACK_SCSV** - an SSL socket option that enables TLS_FALLBACK_SCSV. Off by
         default.

   -  *in sslerr.h*

      -  **SSL_ERROR_INAPPROPRIATE_FALLBACK_ALERT** - a new SSL error code.

   -  *in sslproto.h*

      -  **TLS_FALLBACK_SCSV** - a signaling cipher suite value that indicates a handshake is the
         result of TLS version fallback.

.. _notable_changes_in_nss_3.16.2.3:

`Notable Changes in NSS 3.16.2.3 <#notable_changes_in_nss_3.16.2.3>`__
----------------------------------------------------------------------

.. container::

   -  `Bug 1057161 <https://bugzilla.mozilla.org/show_bug.cgi?id=1057161>`__: Check that an imported
      elliptic curve public key is valid. Previously NSS would only validate the peer's public key
      before performing ECDH key agreement. Now EC public keys are validated at import time.

.. _bugs_fixed_in_nss_3.16.2.3:

`Bugs fixed in NSS 3.16.2.3 <#bugs_fixed_in_nss_3.16.2.3>`__
------------------------------------------------------------

.. container::

   -  `Bug 1057161 <https://bugzilla.mozilla.org/show_bug.cgi?id=1057161>`__ - NSS hangs with 100%
      CPU on invalid EC key
   -  `Bug 1036735 <https://bugzilla.mozilla.org/show_bug.cgi?id=1036735>`__ - Add support for
      draft-ietf-tls-downgrade-scsv to NSS

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.16.2.3 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.16.2.3 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).