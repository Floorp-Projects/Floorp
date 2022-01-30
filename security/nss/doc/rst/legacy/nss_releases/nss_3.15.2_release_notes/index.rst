.. _mozilla_projects_nss_nss_3_15_2_release_notes:

NSS 3.15.2 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.15.2 is a patch release for NSS 3.15. The bug fixes in NSS
   3.15.2 are described in the "Bugs Fixed" section below.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   NSS 3.15.2 source distributions are also available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_15_2_RTM/src/

.. _security_advisories:

`Security Advisories <#security_advisories>`__
----------------------------------------------

.. container::

   The following security-relevant bugs have been resolved in NSS 3.15.2. Users are encouraged to
   upgrade immediately.

   -  `Bug 894370 <https://bugzilla.mozilla.org/show_bug.cgi?id=894370>`__ - (CVE-2013-1739) Avoid
      uninitialized data read in the event of a decryption failure.

.. _new_in_nss_3.15.2:

`New in NSS 3.15.2 <#new_in_nss_3.15.2>`__
------------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  AES-GCM Ciphersuites: AES-GCM cipher suite (RFC 5288 and RFC 5289) support has been added when
      TLS 1.2 is negotiated. Specifically, the following cipher suites are now supported:

      -  TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
      -  TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
      -  TLS_DHE_RSA_WITH_AES_128_GCM_SHA256
      -  TLS_RSA_WITH_AES_128_GCM_SHA256

   .. rubric:: New Functions
      :name: new_functions

   PK11_CipherFinal has been introduced, which is a simple alias for PK11_DigestFinal.

   .. rubric:: New Types
      :name: new_types

   No new types have been introduced.

   .. rubric:: New PKCS #11 Mechanisms
      :name: new_pkcs_11_mechanisms

   No new PKCS#11 mechanisms have been introduced

.. _notable_changes_in_nss_3.15.2:

`Notable Changes in NSS 3.15.2 <#notable_changes_in_nss_3.15.2>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `Bug 880543 <https://bugzilla.mozilla.org/show_bug.cgi?id=880543>`__ - Support for AES-GCM
      ciphersuites that use the SHA-256 PRF
   -  `Bug 663313 <https://bugzilla.mozilla.org/show_bug.cgi?id=663313>`__ - MD2, MD4, and MD5
      signatures are no longer accepted for OCSP or CRLs, consistent with their handling for general
      certificate signatures.
   -  `Bug 884178 <https://bugzilla.mozilla.org/show_bug.cgi?id=884178>`__ - Add PK11_CipherFinal
      macro

.. _bugs_fixed_in_nss_3.15.2:

`Bugs fixed in NSS 3.15.2 <#bugs_fixed_in_nss_3.15.2>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `Bug 734007 <https://bugzilla.mozilla.org/show_bug.cgi?id=734007>`__ - sizeof() used
      incorrectly
   -  `Bug 900971 <https://bugzilla.mozilla.org/show_bug.cgi?id=900971>`__ - nssutil_ReadSecmodDB()
      leaks memory
   -  `Bug 681839 <https://bugzilla.mozilla.org/show_bug.cgi?id=681839>`__ - Allow
      SSL_HandshakeNegotiatedExtension to be called before the handshake is finished.
   -  `Bug 848384 <https://bugzilla.mozilla.org/show_bug.cgi?id=848384>`__ - Deprecate the SSL
      cipher policy code, as it's no longer relevant. It is no longer necessary to call
      NSS_SetDomesticPolicy because all cipher suites are now allowed by default.

   A complete list of all bugs resolved in this release can be obtained at
   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&target_milestone=3.15.2&product=NSS&list_id=7982238

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.15.2 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.15.2 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).