.. _mozilla_projects_nss_nss_3_28_release_notes:

NSS 3.28 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The Network Security Services (NSS) team has released NSS 3.28, which is a minor release.



`Distribution information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The hg tag is NSS_3_28_RTM. NSS 3.28 requires Netscape Portable Runtime(NSPR) 4.13.1 or newer.

   NSS 3.28 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_28_RTM/src/

.. _new_in_nss_3.28:

`New in NSS 3.28 <#new_in_nss_3.28>`__
--------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  NSS includes support for `TLS 1.3 draft
      -18 <https://datatracker.ietf.org/doc/html/draft-ietf-tls-tls13-18>`__.  This includes a
      number of improvements to TLS 1.3:

      -  The signed certificate timestamp, used in certificate transparency, is supported in TLS 1.3
         (`bug 1252745 <https://bugzilla.mozilla.org/show_bug.cgi?id=1252745>`__).
      -  Key exporters for TLS 1.3 are supported (`bug
         1310610 <https://bugzilla.mozilla.org/show_bug.cgi?id=1310610>`__).  This includes the
         early key exporter, which can be used if 0-RTT is enabled. Note that there is a difference
         between TLS 1.3 and key exporters in older versions of TLS.  TLS 1.3 does not distinguish
         between an empty context and no context.
      -  The TLS 1.3 (draft) protocol can be enabled, by defining NSS_ENABLE_TLS_1_3=1 when building
         NSS.

   -  NSS includes support for `the X25519 key exchange
      algorithm <https://datatracker.ietf.org/doc/html/rfc7748>`__ (`bug
      957105 <https://bugzilla.mozilla.org/show_bug.cgi?id=957105>`__), which is supported and
      enabled by default in all versions of TLS.

   .. rubric:: New Functions
      :name: new_functions

   -  in ssl.h

      -  **SSL_ExportEarlyKeyingMaterial** implements a key exporter based on the TLS 1.3 early
         exporter secret.  This API is equivalent in function to SSL_ExportKeyingMaterial, but it
         can only succeed if 0-RTT was attempted (on the client) or accepted (on the server).

      -  **SSL_SendAdditionalKeyShares** configures a TLS 1.3 client so that it generates additional
         key shares when sending a ClientHello.

      -  **SSL_SignatureSchemePrefSet** allows an application to set which signature schemes should
         be supported in TLS and to specify the preference order of those schemes.

      -  **SSL_SignatureSchemePrefGet** allows an application to learn the currently supported and
         enabled signature schemes for a socket.

.. _request_to_test_and_prepare_for_tls_1.3:

`Request to test and prepare for TLS 1.3 <#request_to_test_and_prepare_for_tls_1.3>`__
--------------------------------------------------------------------------------------

.. container::

   This release contains improved support for TLS 1.3, however, the code that supports TLS 1.3 is
   still disabled by default (not built).

   For the future NSS 3.29 release, it is planned that standard builds of NSS will support the TLS
   1.3 protocol (although the maximum TLS protocol version enabled by default will remain at TLS
   1.2).

   We know that some applications which use NSS, query NSS for the supported range of SSL/TLS
   protocols, and will enable the maximum enabled protocol version. In NSS 3.29, those applications
   will therefore enable support for the TLS 1.3 protocol.

   In order to prepare for this future change, we'd like to encourage all users of NSS to override
   the standard NSS 3.28 build configuration, by defining NSS_ENABLE_TLS_1_3=1 at build time.  This
   will enable support for TLS 1.3. Please give feedback to the NSS developers for any compatibility
   issues that you encounter in your tests.

.. _notable_changes_in_nss_3.28:

`Notable Changes in NSS 3.28 <#notable_changes_in_nss_3.28>`__
--------------------------------------------------------------

.. container::

   -  NSS can no longer be compiled with support for additional elliptic curves (the
      NSS_ECC_MORE_THAN_SUITE_B option, `bug
      1253912 <https://bugzilla.mozilla.org/show_bug.cgi?id=1253912>`__).  This was previously
      possible by replacing certain NSS source files.
   -  NSS will now detect the presence of tokens that support additional elliptic curves and enable
      those curves for use in TLS (`bug
      1303648 <https://bugzilla.mozilla.org/show_bug.cgi?id=1303648>`__). Note that this detection
      has a one-off performance cost, which can be avoided by using the SSL_NamedGroupConfig
      function, to limit supported groups to those that NSS provides.
   -  PKCS#11 bypass for TLS is no longer supported and has been removed (`bug
      1303224 <https://bugzilla.mozilla.org/show_bug.cgi?id=1303224>`__).
   -  Support for "export" grade SSL/TLS cipher suites has been removed (`bug
      1252849 <https://bugzilla.mozilla.org/show_bug.cgi?id=1252849>`__).
   -  NSS now uses the signature schemes definition in TLS 1.3 (`bug
      1309446 <https://bugzilla.mozilla.org/show_bug.cgi?id=1309446>`__).  This also affects TLS
      1.2. NSS will now only generate signatures with the combinations of hash and signature scheme
      that are defined in TLS 1.3, even when negotiating TLS 1.2.

      -  This means that SHA-256 will only be used with P-256 ECDSA certificates, SHA-384 with P-384
         certificates, and SHA-512 with P-521 certificates.  SHA-1 is permitted (in TLS 1.2 only)
         with any certificate for backward compatibility reasons.
      -  New functions to configure signature schemes are provided: **SSL_SignatureSchemePrefSet,
         SSL_SignatureSchemePrefGet**. The old SSL_SignaturePrefSet and SSL_SignaturePrefSet
         functions are now deprecated.
      -  NSS will now no longer assume that default signature schemes are supported by a peer if
         there was no commonly supported signature scheme.

   -  NSS will now check if RSA-PSS signing is supported by the token that holds the private key
      prior to using it for TLS (`bug
      1311950 <https://bugzilla.mozilla.org/show_bug.cgi?id=1311950>`__).
   -  The certificate validation code contains checks to no longer trust certificates that are
      issued by old WoSign and StartCom CAs, after October 21, 2016. This is equivalent to the
      behavior that Mozilla will release with Firefox 51. Background information can be found in
      `Mozilla's blog
      post <https://blog.mozilla.org/security/2016/10/24/distrusting-new-wosign-and-startcom-certificates/>`__.

.. _bugs_fixed_in_nss_3.28:

`Bugs fixed in NSS 3.28 <#bugs_fixed_in_nss_3.28>`__
----------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.28:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.28

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.28 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.28 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).