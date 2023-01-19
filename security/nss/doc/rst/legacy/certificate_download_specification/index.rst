.. _mozilla_projects_nss_certificate_download_specification:

NSS Certificate Download Specification
======================================

.. container::

   This document describes the data formats used by NSS 3.x for installing certificates. This
   document is currently being revised and has not yet been reviewed for accuracy.

.. _data_formats:

`Data Formats <#data_formats>`__
--------------------------------

.. container::

   NSS can accept certificates in several formats. In all cases the certificates are X509 version 1,
   2, or 3.

.. _binary_formats:

`Binary Formats <#binary_formats>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   NSS's certificate loader will recognize several binary formats. They are:

   -  **DER encoded certificate:** This is a single binary DER encoded certificate.
   -  **PKCS#7 certificate chain:** This is a single
      `PKCS#7 <ftp://ftp.rfc-editor.org/in-notes/rfc2315.txt>`__ ``SignedData`` object. The only
      significant field in the ``SignedData`` object is the ``certificates`` field, which may
      contain multiple certificates to be imported together. The contents of the ``version``,
      ``digestAlgorithms``, ``contentInfo``, ``crls``, and ``signerInfos`` fields are ignored.
   -  **Netscape Certificate Sequence:** This is another
      `PKCS#7 <ftp://ftp.rfc-editor.org/in-notes/rfc2315.txt>`__ object format, and like the
      ``SignedData`` format, it allows multiple certificates to be imported together. This format is
      simpler than the `PKCS#7 <ftp://ftp.rfc-editor.org/in-notes/rfc2315.txt>`__ ``SignedData``
      object format. It consists of a `PKCS#7 <ftp://ftp.rfc-editor.org/in-notes/rfc2315.txt>`__
      ``ContentInfo`` structure, wrapping a sequence of certificates. The ``contentType`` field OID
      must be ``netscape-cert-sequence`` (see
      :ref:`mozilla_projects_nss_certificate_download_specification#object_identifiers`). The
      ``content`` field is the following ASN.1 structure:

   .. code::

         CertificateSequence ::= SEQUENCE OF Certificate

   See the section below on
   :ref:`mozilla_projects_nss_certificate_download_specification#importing_certificate_chains` for
   more information about how multiple certificates are handled.

.. _text_formats:

`Text Formats <#text_formats>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Any of the above :ref:`mozilla_projects_nss_certificate_download_specification#binary_formats`
   can also be imported in text form. The text form begins with the following line:

   .. code::

         -----BEGIN CERTIFICATE-----

   Following this line should be the certificate data, which can be in any of the
   :ref:`mozilla_projects_nss_certificate_download_specification#binary_formats` described above.
   This data must be base64 encoded as described by `RFC
   1113 <https://datatracker.ietf.org/doc/html/rfc1113>`__. Following the data should be the
   following line:

   .. code::

         -----END CERTIFICATE-----

   In a text format download, NSS ignores any text before the first ``BEGIN CERTIFICATE`` line, and
   ignores any text after the first ``END CERTIFICATE`` line. Between those two lines, there must be
   exactly ONE item of any of the supported binary formats described above, and that one item must
   be base64 encoded. Regardless of which of the supported binary formats is used, the ``BEGIN`` and
   ``END`` lines must say ``CERTIFICATE``, and not any other word (such as ``KEY``). The ``BEGIN``
   and ``END`` lines must begin and end with 5 dashes, with no extra leading or trailing white space
   (excluding the End Of Line characters).

.. _importing_certificate_chains:

`Importing Certificate Chains <#importing_certificate_chains>`__
----------------------------------------------------------------

.. container::

   Several of the formats described above can contain several certificates. When NSS's certificate
   decoder encounters one of these collections of multiple certificates they are handled in the
   following way:

   -  The first certificate is processed in a context specific manner, depending upon how it is
      being imported. For Mozilla browsers, this handling will depend upon the mime ``Content-Type``
      that is used on the object being downloaded. For NSS-based servers it will depend upon the
      options selected in the server's administration interface.

   -  Subsequent certificates are all treated the same. If the certificates contain a
      ``BasicConstraints`` certificate extension that indicates they are CA certificates, and do not
      already exist in the local certificate database, they are added as untrusted CAs. In this way
      they may be used for certificate chain validation, as long as there is a trusted CA somewhere
      along the chain.

.. _importing_certificates_into_mozilla_browsers:

`Importing Certificates into Mozilla browsers <#importing_certificates_into_mozilla_browsers>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Mozilla browsers import certificates found in HTTP protocol responses. There are several mime
   content types that are used to indicate to the browser what type of certificate is being
   imported. These mime types are:

   -  **``application/x-x509-user-cert``** The certificate being downloaded is a user certificate
      belonging to the user operating the browser. If the private key associated with the
      certificate does not exist in the user's local key database, then an error dialog is generated
      and the certificate is not imported. If a certificate chain is being imported then the first
      certificate in the chain must be the user certificate, and any subsequent certificates will be
      added as untrusted CA certificates to the local database.
   -  **``application/x-x509-ca-cert``** The certificate being downloaded represents a Certificate
      Authority. When it is downloaded the user will be shown a sequence of dialogs that will guide
      them through the process of accepting the Certificate Authority and deciding if they wish to
      trust sites certified by the CA. If a certificate chain is being imported then the first
      certificate in the chain must be the CA certificate, and any subsequent certificates will be
      added as untrusted CA certificates to the local database.
   -  **``application/x-x509-email-cert``** The certificate being downloaded is a user certificate
      belonging to another user for use with S/MIME. If a certificate chain is being imported then
      the first certificate in the chain must be the user certificate, and any subsequent
      certificates will be added as untrusted CA certificates to the local database. This is
      intended to allow people or CAs to post their e-mail certificates on web pages for download by
      other users who want to send them encrypted mail.

   Note: the browser checks that the size of the object being downloaded matches the size of the
   encoded certificates. Therefore it is important to ensure that no extra characters, such as NULLs
   or LineFeeds are added at the end of the object.

.. _importing_certificates_into_nss-based_servers:

`Importing Certificates into NSS-based servers <#importing_certificates_into_nss-based_servers>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Consult your server's administration guide for the most accurate information. For some NSS-base
   servers, the following information is correct.

   Server certificates are imported via the server admin interface. Certificates are pasted into a
   text input field in an HTML form, and then the form is submitted to the admin server. Since the
   certificates are pasted into text fields, only the
   :ref:`mozilla_projects_nss_certificate_download_specification#text_formats` described above are
   supported for servers. The type of certificate being imported (e.g. server or CA or cert chain)
   is specified by the server administrator by selections made on the admin pages. If a certificate
   chain is being imported then the first certificate in the chain must be the server or CA
   certificate, and any subsequent certificates will be added as untrusted CA certificates to the
   local database.

.. _object_identifiers:

`Object Identifiers <#object_identifiers>`__
--------------------------------------------

.. container::

   The base of all Netscape object ids is:

   .. code::

         netscape OBJECT IDENTIFIER ::= { 2 16 840 1 113730 }

   The hexadecimal byte value of this OID when DER encoded is:

   .. code::

         0x60, 0x86, 0x48, 0x01, 0x86, 0xf8, 0x42

   The following OIDs are mentioned in this document:

   .. code::

         netscape-data-type     OBJECT IDENTIFIER :: = { netscape 2 }
         netscape-cert-sequence OBJECT IDENTIFIER :: = { netscape-data-type 5 }