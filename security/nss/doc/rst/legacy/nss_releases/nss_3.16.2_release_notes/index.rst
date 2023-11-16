.. _mozilla_projects_nss_nss_3_16_2_release_notes:

NSS 3.16.2 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.16.2 is a patch release for NSS 3.16. The bug fixes in NSS
   3.16.2 are described in the "Bugs Fixed" section below.



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_16_2_RTM. NSS 3.16.2 requires NSPR 4.10.6 or newer.

   NSS 3.16.2 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_16_2_RTM/src/

.. _new_in_nss_3.16.2:

`New in NSS 3.16.2 <#new_in_nss_3.16.2>`__
------------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  DTLS 1.2 is supported.
   -  The TLS application layer protocol negotiation (ALPN) extension is also supported on the
      server side.
   -  RSA-OEAP is supported. Use the new PK11_PrivDecrypt and PK11_PubEncrypt functions with the
      CKM_RSA_PKCS_OAEP mechanism.
   -  New Intel AES assembly code for 32-bit and 64-bit Windows, contributed by Shay Gueron and Vlad
      Krasnov of Intel.

   .. rubric:: New Functions
      :name: new_functions

   -  *in cert.h*

      -  **CERT_AddExtensionByOID** - adds an extension to a certificate. It is the same as
         CERT_AddExtension except that the OID is represented by a SECItem instead of a SECOidTag.

   -  *in pk11pub.h*

      -  **PK11_PrivDecrypt** - decrypts with a private key. The algorithm is specified with a
         CK_MECHANISM_TYPE.
      -  **PK11_PubEncrypt** - encrypts with a public key. The algorithm is specified with a
         CK_MECHANISM_TYPE.

   .. rubric:: New Macros
      :name: new_macros

   -  *in sslerr.h*

      -  **SSL_ERROR_NEXT_PROTOCOL_NO_CALLBACK** - An SSL error code that means the next protcol
         negotiation extension was enabled, but the callback was cleared prior to being needed.
      -  **SSL_ERROR_NEXT_PROTOCOL_NO_PROTOCOL** - An SSL error code that means the server supports
         no protocols that the client advertises in the ALPN extension.

.. _notable_changes_in_nss_3.16.2:

`Notable Changes in NSS 3.16.2 <#notable_changes_in_nss_3.16.2>`__
------------------------------------------------------------------

.. container::

   -  The btoa command has a new command-line option -w *suffix*, which causes the output to be
      wrapped in BEGIN/END lines with the given suffix. Use "c" as a shorthand for the suffix
      CERTIFICATE.
   -  The certutil commands supports additionals types of subject alt name extensions:

      -  --extSAN *type:name[,type:name]...*

   -  The certutil commands supports generic certificate extensions, by loading binary data from
      files, which have been prepared using external tools, or which have been extracted and dumped
      to file from other existing certificates:

      -  --dump-ext-val *OID*
      -  --extGeneric *OID:critical-flag:filename[,OID:critical-flag:filename]...*

   -  The certutil command has three new certificate usage specifiers:

      -  L:  certificateUsageSSLCA
      -  A: certificateUsageAnyCA
      -  Y: certificateUsageVerifyCA

   -  The pp command has a new command-line option -u, which means "use UTF-8". The default is to
      show a non-ASCII character as ".".
   -  On Linux, NSS is built with the -ffunction-sections -fdata-sections compiler flags and the
      --gc-sections linker flag to allow unused functions to be discarded.

.. _bugs_fixed_in_nss_3.16.2:

`Bugs fixed in NSS 3.16.2 <#bugs_fixed_in_nss_3.16.2>`__
--------------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.16.2:
   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.16.2
