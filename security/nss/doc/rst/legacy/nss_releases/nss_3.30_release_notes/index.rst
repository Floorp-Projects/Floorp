.. _mozilla_projects_nss_nss_3_30_release_notes:

NSS 3.30 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The Network Security Services (NSS) team has released NSS 3.30, which is a minor release.

.. _distribution_information:

`Distribution information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The hg tag is NSS_3_30_RTM. NSS 3.30 requires Netscape Portable Runtime (NSPR); 4.13.1 or newer.

   NSS 3.30 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_30_RTM/src/

.. _new_in_nss_3.30:

`New in NSS 3.30 <#new_in_nss_3.30>`__
--------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  In the PKCS#11 root CA module (nssckbi), CAs with positive trust are marked with a new boolean
      attribute, CKA_NSS_MOZILLA_CA_POLICY, set to true. Applications that need to distinguish them
      from other root CAs, may use the exported function PK11_HasAttributeSet.
   -  Support for callback functions that can be used to monitor SSL/TLS alerts that are sent or
      received.

   .. rubric:: New Functions
      :name: new_functions

   -  *in cert.h*

      -  **CERT_CompareAVA** - performs a comparison of two CERTAVA structures, and returns a
         SECComparison result.

   -  *in pk11pub.h*

      -  **PK11_HasAttributeSet** - allows to check if a PKCS#11 object in a given slot has a
         specific boolean attribute set.

   -  *in ssl.h*

      -  **SSL_AlertReceivedCallback** - register a callback function, that will be called whenever
         an SSL/TLS alert is received
      -  **SSL_AlertSentCallback** - register a callback function, that will be called whenever an
         SSL/TLS alert is sent
      -  **SSL_SetSessionTicketKeyPair** - configures an asymmetric key pair, for use in wrapping
         session ticket keys, used by the server. This function currently only accepts an RSA
         public/private key pair.

   .. rubric:: New Macros
      :name: new_macros

   -  *in ciferfam.h*

      -  **PKCS12_AES_CBC_128, PKCS12_AES_CBC_192, PKCS12_AES_CBC_256** - cipher family identifiers
         corresponding to the PKCS#5 v2.1 AES based encryption schemes used in the PKCS#12 support
         in NSS

   -  *in pkcs11n.h*

      -  **CKA_NSS_MOZILLA_CA_POLICY** - identifier for a boolean PKCS#11 attribute, that should be
         set to true, if a CA is present because of it's acceptance according to the Mozilla CA
         Policy

.. _notable_changes_in_nss_3.30:

`Notable Changes in NSS 3.30 <#notable_changes_in_nss_3.30>`__
--------------------------------------------------------------

.. container::

   -  The TLS server code has been enhanced to support session tickets when no RSA certificate (e.g.
      only an ECDSA certificate) is configured.
   -  RSA-PSS signatures produced by key pairs with a modulus bit length that is not a multiple of 8
      are now supported.
   -  The pk12util tool now supports importing and exporting data encrypted in the AES based schemes
      defined in PKCS#5 v2.1.

.. _bugs_fixed_in_nss_3.30:

`Bugs fixed in NSS 3.30 <#bugs_fixed_in_nss_3.30>`__
----------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.30:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.30

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.30 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.30 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).