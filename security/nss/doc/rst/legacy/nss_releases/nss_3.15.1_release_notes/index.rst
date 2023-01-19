.. _mozilla_projects_nss_nss_3_15_1_release_notes:

NSS 3.15.1 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.15.1 is a patch release for NSS 3.15. The bug fixes in NSS
   3.15.1 are described in the "Bugs Fixed" section below.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   NSS 3.15.1 source distributions are also available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_15_1_RTM/src/

.. _new_in_nss_3.15.1:

`New in NSS 3.15.1 <#new_in_nss_3.15.1>`__
------------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  TLS 1.2: TLS 1.2 (`RFC 5246 <https://datatracker.ietf.org/doc/html/rfc5246>`__) is supported.
      HMAC-SHA256 cipher suites (`RFC 5246 <https://datatracker.ietf.org/doc/html/rfc5246>`__ and
      `RFC 5289 <https://datatracker.ietf.org/doc/html/rfc5289>`__) are supported, allowing TLS to
      be used without MD5 and SHA-1. Note the following limitations.

      -  The hash function used in the signature for TLS 1.2 client authentication must be the hash
         function of the TLS 1.2 PRF, which is always SHA-256 in NSS 3.15.1.
      -  AES GCM cipher suites are not yet supported.

   .. rubric:: New Functions
      :name: new_functions

   None.

   .. rubric:: New Types
      :name: new_types

   -  *in sslprot.h*

      -  **SSL_LIBRARY_VERSION_TLS_1_2** - The protocol version of TLS 1.2 on the wire, value
         0x0303.
      -  **TLS_DHE_RSA_WITH_AES_256_CBC_SHA256**, **TLS_RSA_WITH_AES_256_CBC_SHA256**,
         **TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256**, **TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256**,
         **TLS_DHE_RSA_WITH_AES_128_CBC_SHA256**, **TLS_RSA_WITH_AES_128_CBC_SHA256**,
         **TLS_RSA_WITH_NULL_SHA256** - New TLS 1.2 only HMAC-SHA256 cipher suites.

   -  *in sslerr.h*

      -  **SSL_ERROR_UNSUPPORTED_HASH_ALGORITHM**, **SSL_ERROR_DIGEST_FAILURE**,
         **SSL_ERROR_INCORRECT_SIGNATURE_ALGORITHM** - New error codes for TLS 1.2.

   -  *in sslt.h*

      -  **ssl_hmac_sha256** - A new value in the SSLMACAlgorithm enum type.
      -  **ssl_signature_algorithms_xtn** - A new value in the SSLExtensionType enum type.

   .. rubric:: New PKCS #11 Mechanisms
      :name: new_pkcs_11_mechanisms

   None.

.. _notable_changes_in_nss_3.15.1:

`Notable Changes in NSS 3.15.1 <#notable_changes_in_nss_3.15.1>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `Bug 856060 <https://bugzilla.mozilla.org/show_bug.cgi?id=856060>`__ - Enforce name
      constraints on the common name in libpkix  when no subjectAltName is present.
   -  `Bug 875156 <https://bugzilla.mozilla.org/show_bug.cgi?id=875156>`__ - Add const to the
      function arguments of SEC_CertNicknameConflict.
   -  `Bug 877798 <https://bugzilla.mozilla.org/show_bug.cgi?id=877798>`__ - Fix ssltap to print the
      certificate_status handshake message correctly.
   -  `Bug 882829 <https://bugzilla.mozilla.org/show_bug.cgi?id=882829>`__ - On Windows, NSS
      initialization fails if NSS cannot call the RtlGenRandom function.
   -  `Bug 875601 <https://bugzilla.mozilla.org/show_bug.cgi?id=875601>`__ -
      SECMOD_CloseUserDB/SECMOD_OpenUserDB fails to reset the token delay, leading to spurious
      failures.
   -  `Bug 884072 <https://bugzilla.mozilla.org/show_bug.cgi?id=884072>`__ - Fix a typo in the
      header include guard macro of secmod.h.
   -  `Bug 876352 <https://bugzilla.mozilla.org/show_bug.cgi?id=876352>`__ - certutil now warns if
      importing a PEM file that contains a private key.
   -  `Bug 565296 <https://bugzilla.mozilla.org/show_bug.cgi?id=565296>`__ - Fix the bug that
      shlibsign exited with status 0 even though it failed.
   -  The NSS_SURVIVE_DOUBLE_BYPASS_FAILURE build option is removed.

.. _bugs_fixed_in_nss_3.15.1:

`Bugs fixed in NSS 3.15.1 <#bugs_fixed_in_nss_3.15.1>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  https://bugzilla.mozilla.org/buglist.cgi?list_id=5689256;resolution=FIXED;classification=Components;query_format=advanced;target_milestone=3.15.1;product=NSS

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.15.1 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.15.1 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).