.. _mozilla_projects_nss_nss_3_39_release_notes:

NSS 3.39 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.39, which is a minor release.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_39_RTM. NSS 3.39 requires NSPR 4.20 or newer.

   NSS 3.39 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_39_RTM/src/

.. _new_in_nss_3.39:

`New in NSS 3.39 <#new_in_nss_3.39>`__
--------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  The ``tstclnt`` and ``selfserv`` utilities added support for configuring the enabled TLS
      signature schemes using the ``-J`` parameter.

   -  NSS will use RSA-PSS keys to authenticate in TLS.  Support for these keys is disabled by
      default but can be enabled using ``SSL_SignatureSchemePrefSet()``.

   -  ``certutil`` added the ability to delete an orphan private key from an NSS key database.

   -  Added the ``nss-policy-check`` utility, which can be used to check an NSS policy configuration
      for problems.

   -  A PKCS#11 URI can be used as an identifier for a PKCS#11 token.

   .. rubric:: New Functions
      :name: new_functions

   -  in cert.h

      -  **CERT_GetCertKeyType** - Query the Key Type associated with the given certificate.

   -  utilpars.h

      -  **NSSUTIL_AddNSSFlagToModuleSpec** - A helper function for modifying the PKCS#11 module
         configuration. It can be used to add a single flag to the Flags= section inside the spec's
         NSS= section.

.. _notable_changes_in_nss_3.39:

`Notable Changes in NSS 3.39 <#notable_changes_in_nss_3.39>`__
--------------------------------------------------------------

.. container::

   -  The TLS 1.3 implementation uses the final version number from `RFC
      8446 <https://datatracker.ietf.org/doc/html/rfc8446>`__.
   -  Previous versions of NSS accepted an RSA PKCS#1 v1.5 signature where the DigestInfo structure
      was missing the NULL parameter.
      Starting with version 3.39, NSS requires the encoding to contain the NULL parameter.
   -  The ``tstclnt`` and ``selfserv`` test utilities no longer accept the -z parameter, as support
      for TLS compression was removed in a previous NSS version.
   -  The CA certificates list was updated to version 2.26.
   -  The following CA certificates were **Added**:

      -  OU = GlobalSign Root CA - R6

         -  SHA-256 Fingerprint: 2CABEAFE37D06CA22ABA7391C0033D25982952C453647349763A3AB5AD6CCF69

      -  CN = OISTE WISeKey Global Root GC CA

         -  SHA-256 Fingerprint: 8560F91C3624DABA9570B5FEA0DBE36FF11A8323BE9486854FB3F34A5571198D

   -  The following CA certificate was **Removed**:

      -  CN = ComSign

         -  SHA-256 Fingerprint: AE4457B40D9EDA96677B0D3C92D57B5177ABD7AC1037958356D1E094518BE5F2

   -  The following CA certificates had the **Websites trust bit disabled**:

      -  CN = Certplus Root CA G1

         -  SHA-256 Fingerprint: 152A402BFCDF2CD548054D2275B39C7FCA3EC0978078B0F0EA76E561A6C7433E

      -  CN = Certplus Root CA G2

         -  SHA-256 Fingerprint: 6CC05041E6445E74696C4CFBC9F80F543B7EABBB44B4CE6F787C6A9971C42F17

      -  CN = OpenTrust Root CA G1

         -  SHA-256 Fingerprint: 56C77128D98C18D91B4CFDFFBC25EE9103D4758EA2ABAD826A90F3457D460EB4

      -  CN = OpenTrust Root CA G2

         -  SHA-256 Fingerprint: 27995829FE6A7515C1BFE848F9C4761DB16C225929257BF40D0894F29EA8BAF2

      -  CN = OpenTrust Root CA G3

         -  SHA-256 Fingerprint: B7C36231706E81078C367CB896198F1E3208DD926949DD8F5709A410F75B6292

.. _bugs_fixed_in_nss_3.39:

`Bugs fixed in NSS 3.39 <#bugs_fixed_in_nss_3.39>`__
----------------------------------------------------

.. container::

   -  `Bug 1483128 <https://bugzilla.mozilla.org/show_bug.cgi?id=1483128>`__ - NSS responded to an
      SSLv2-compatible ClientHello with a ServerHello that had an all-zero random (CVE-2018-12384)

   This Bugzilla query returns all the bugs fixed in NSS 3.39:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.39

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.39 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.39 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).