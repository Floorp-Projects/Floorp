.. _mozilla_projects_nss_nss_3_15_4_release_notes:

NSS 3.15.4 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.15.4 is a patch release for NSS 3.15. The bug fixes in NSS
   3.15.4 are described in the "Bugs Fixed" section below.



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_15_4_RTM. NSS 3.15.4 requires NSPR 4.10.2 or newer.

   NSS 3.15.4 source distributions are also available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_15_4_RTM/src/

.. _security_advisories:

`Security Advisories <#security_advisories>`__
----------------------------------------------

.. container::

   The following security-relevant bugs have been resolved in NSS 3.15.4. Users are encouraged to
   upgrade immediately.

   -  `Bug 919877 <https://bugzilla.mozilla.org/show_bug.cgi?id=919877>`__ - (CVE-2013-1740) When
      false start is enabled, libssl will sometimes return unencrypted, unauthenticated data from
      PR_Recv

.. _new_in_nss_3.15.4:

`New in NSS 3.15.4 <#new_in_nss_3.15.4>`__
------------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  Implemented OCSP querying using the HTTP GET method, which is the new default, and will fall
      back to the HTTP POST method.
   -  Implemented OCSP server functionality for testing purposes (httpserv utility).
   -  Support SHA-1 signatures with TLS 1.2 client authentication.
   -  Added the --empty-password command-line option to certutil, to be used with -N: use an empty
      password when creating a new database.
   -  Added the -w command-line option to pp: don't wrap long output lines.

.. _new_functions:

`New Functions <#new_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  CERT_ForcePostMethodForOCSP
   -  CERT_GetSubjectNameDigest
   -  CERT_GetSubjectPublicKeyDigest
   -  SSL_PeerCertificateChain
   -  SSL_RecommendedCanFalseStart
   -  SSL_SetCanFalseStartCallback

.. _new_types:

`New Types <#new_types>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  CERT_REV_M_FORCE_POST_METHOD_FOR_OCSP: When this flag is used, libpkix will never attempt to
      use the HTTP GET method for OCSP requests; it will always use POST.

.. _new_pkcs_11_mechanisms:

`New PKCS #11 Mechanisms <#new_pkcs_11_mechanisms>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   None.

.. _notable_changes_in_nss_3.15.4:

`Notable Changes in NSS 3.15.4 <#notable_changes_in_nss_3.15.4>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  Reordered the cipher suites offered in SSL/TLS client hello messages to match modern best
      practices.
   -  Updated the set of root CA certificates (version 1.96).
   -  Improved SSL/TLS false start. In addition to enabling the SSL_ENABLE_FALSE_START option, an
      application must now register a callback using the SSL_SetCanFalseStartCallback function.
   -  When building on Windows, OS_TARGET now defaults to WIN95. To use the WINNT build
      configuration, specify OS_TARGET=WINNT.

.. _bugs_fixed_in_nss_3.15.4:

`Bugs fixed in NSS 3.15.4 <#bugs_fixed_in_nss_3.15.4>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   A complete list of all bugs resolved in this release can be obtained at
   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&target_milestone=3.15.4&product=NSS

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.15.4 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.15.4 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).