.. _mozilla_projects_nss_overview:

Overview of NSS
===============

.. container::

   .. rubric:: Open Source Crypto Libraries
      :name: Open_Source_Crypto_Libraries

.. _proven_application_security_architecture:

`Proven Application Security Architecture <#proven_application_security_architecture>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   If you want to add support for SSL, S/MIME, or other Internet security standards to your
   application, you can use Network Security Services (NSS) to implement all your security features.
   NSS provides a complete open-source implementation of the crypto libraries used by AOL, Red Hat,
   Google, and other companies in a variety of products, including the following:

   -  `Mozilla products <https://www.mozilla.org/products/>`__, including
      `Firefox <https://www.mozilla.com/firefox/>`__,
      `Thunderbird <https://www.mozilla.com/thunderbird/>`__,
      `SeaMonkey <https://seamonkey-project.org/>`__, and `Firefox
      OS <https://support.mozilla.org/en-US/products/firefox-os>`__.
   -  AOL Instant Messenger (AIM)
   -  Open source client applications such as `Evolution <https://wiki.gnome.org/Apps/Evolution>`__,
      `Pidgin <https://pidgin.im/>`__, `Apache OpenOffice <https://www.openoffice.org/>`__, and
      `LibreOffice <https://www.libreoffice.org>`__.
   -  Server products from `Red Hat <https://www.redhat.com/en/technologies>`__: `Red Hat Directory
      Server <https://www.redhat.com/en/technologies/cloud-computing/directory-server>`__, `Red Hat
      Certificate
      System <https://www.redhat.com/en/technologies/cloud-computing/certificate-system>`__, and the
      `mod_nss <https://directory.fedoraproject.org/docs/389ds/administration/mod-nss.html>`__ SSL
      module for the Apache web server.
   -  Server products from Oracle (formerly Sun Java Enterprise System), including `Oracle
      Communications Messaging
      Server <https://www.oracle.com/industries/communications/enterprise/products/messaging-server/index.html>`__
      and `Oracle Directory Server Enterprise
      Edition <http://www.oracle.com/technetwork/middleware/id-mgmt/overview/index-085178.html>`__.
   -  `SUSE Linux Enterprise Server <https://www.suse.com/products/server/>`__ supports NSS and the
      `mod_nss <https://documentation.suse.com/sles/11-SP4/html/SLES-all/cha-apache2.html#sec-apache2-nss>`__
      SSL module for the Apache web server.

   NSS includes a framework to which developers and OEMs can contribute patches, such as assembler
   code, to optimize performance on their platforms. NSS 3.x has been certified on 18 platforms.

   For more detailed information about NSS, see `wiki.mozilla.org <https://wiki.mozilla.org/NSS>`__
   and `NSS FAQ <NSS_FAQ>`__.

   Source code for a Java interface to NSS is available in the Mozilla CVS tree. For details, see
   `Network Security Services for Java <JSS>`__.

   NSS makes use of Netscape Portable Runtime
   (`NSPR <https://developer.mozilla.org/en-US/docs/Mozilla/Projects/NSPR>`__), a platform-neutral
   open-source API for system functions designed to facilitate cross-platform development. Like NSS,
   NSPR has been battle-tested in multiple products. For more information, see the `NSPR Project
   Page <https://developer.mozilla.org/en-US/docs/Mozilla/Projects/NSPR>`__.

.. _interoperability_and_open_standards:

`Interoperability and Open Standards <#interoperability_and_open_standards>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   You can use NSS to support a range of security standards in your application, including the
   following:

   -  `SSL v3 </en-US/Glossary#SSL>`__. The Secure Sockets Layer (SSL) protocol allows mutual
      authentication between a client and server and the establishment of an authenticated and
      encrypted connection.
   -  TLS v1.3 (`RFC 8446 <https://datatracker.ietf.org/doc/html/rfc8446>`__), `TLS v1.2 (RFC
      5246 <https://datatracker.ietf.org/doc/html/rfc5246>`__), `TLS v1.1 (RFC
      4346 <https://datatracker.ietf.org/doc/html/rfc4346>`__), `TLS v1
      ( <https://www.ietf.org/rfc/rfc2246.txt>`__\ `RFC
      2246 <https://datatracker.ietf.org/doc/html/rfc2246>`__). The Transport Layer Security (TLS)
      protocol from the IETF that supersedes SSL.
   -  `PKCS #1 <https://www.rsasecurity.com/rsalabs/pkcs/pkcs-1/index.html>`__. RSA standard that
      governs implementation of public-key cryptography based on the RSA algorithm.
   -  `PKCS #3 <https://www.rsasecurity.com/rsalabs/pkcs/pkcs-3/index.html>`__. RSA standard that
      governs implementation of Diffie-Hellman key agreement.
   -  `PKCS #5 <https://www.rsasecurity.com/rsalabs/pkcs/pkcs-5/index.html>`__. RSA standard that
      governs password-based cryptography, for example to encrypt private keys for storage.
   -  `PKCS #7 <https://www.rsasecurity.com/rsalabs/pkcs/pkcs-7/index.html>`__. RSA standard that
      governs the application of cryptography to data, for example digital signatures and digital
      envelopes.
   -  `PKCS #8 <https://www.rsasecurity.com/rsalabs/pkcs/pkcs-8/index.html>`__. RSA standard that
      governs the storage and encryption of private keys.
   -  `PKCS #9 <https://www.rsasecurity.com/rsalabs/pkcs/pkcs-9/index.html>`__. RSA standard that
      governs selected attribute types, including those used with PKCS #7, PKCS #8, and PKCS #10.
   -  `PKCS #10 <https://www.rsasecurity.com/rsalabs/pkcs/pkcs-10/index.html>`__. RSA standard that
      governs the syntax for certificate requests.
   -  `PKCS #11 <https://www.rsasecurity.com/rsalabs/pkcs/pkcs-11/index.html>`__. RSA standard that
      governs communication with cryptographic tokens (such as hardware accelerators and smart
      cards) and permits application independence from specific algorithms and implementations.
   -  `PKCS #12 <https://www.rsasecurity.com/rsalabs/pkcs/pkcs-12/index.html>`__. RSA standard that
      governs the format used to store or transport private keys, certificates, and other secret
      material.
   -  `S/MIME (RFC 2311 and RFC 2633) </en-US/Glossary#S.2FMIME>`__. IETF message specification
      (based on the popular Internet MIME standard) that provides a consistent way to send and
      receive signed and encrypted MIME data.
   -  `X.509 v3 <https://developer.mozilla.org/en-US/docs/Mozilla/Security/x509_Certificates>`__.
      ITU standard that governs the format of certificates used for authentication in public-key
      cryptography.
   -  `OCSP (RFC 2560) </en-US/Glossary#OCSP>`__. The Online Certificate Status Protocol (OCSP)
      governs real-time confirmation of certificate validity.
   -  `PKIX Certificate and CRL Profile ( <https://www.ietf.org/rfc/rfc3280.txt>`__\ `RFC
      3280 <https://datatracker.ietf.org/doc/html/rfc3280>`__). The first part of the four-part
      standard under development by the Public-Key Infrastructure (X.509) working group of the IETF
      (known at PKIX) for a public-key infrastructure for the Internet.
   -  RSA, DSA, ECDSA, Diffie-Hellman, EC Diffie-Hellman,
      `AES <https://en.wikipedia.org/wiki/Advanced_Encryption_Standard>`__, Triple DES, DES, RC2,
      RC4, SHA-1, SHA-256, SHA-384, SHA-512, MD2, MD5, HMAC: Common cryptographic algorithms used in
      public-key and symmetric-key cryptography.
   -  FIPS 186-2 pseudorandom number generator.

   For complete details, see `Encryption
   Technologies <https://www.mozilla.org/projects/security/pki/nss/nss-3.11/nss-3.11-algorithms.html>`__.

.. _fips_140_validation_and_niscc_testing:

`FIPS 140 Validation and NISCC Testing <#fips_140_validation_and_niscc_testing>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The NSS software crypto module has been validated three times for conformance to FIPS 140 at
   Security Levels 1 and 2. For more information, see the `NSS FIPS <FIPS_Mode_-_an_explanation>`__
   page (`Or this one <https://wiki.mozilla.org/FIPS_Validation>`__).

   The NSS libraries passed the NISCC
   `TLS/SSL <https://www.niscc.gov.uk/niscc/docs/re-20030930-00749.pdf?lang=en>`__ and
   `S/MIME <https://www.uniras.gov.uk/niscc/docs/re-20031104-00752.pdf?lang=en>`__ test suites (1.6
   million test cases of invalid input data).

.. _complete_software_development_kit:

`Complete Software Development Kit <#complete_software_development_kit>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   In addition to libraries and APIs, NSS provides :ref:`mozilla_projects_nss_tools` required for
   debugging, diagnostics, certificate and key management, cryptography module management, and other
   development tasks.

   NSS comes with an extensive and growing set of :ref:`mozilla_projects_nss#documentation`,
   including introductory material, API references, man pages for command-line tools, and
   :ref:`mozilla_projects_nss_nss_sample_code`.

   NSS is available as source and shared (dynamic) libraries. Every NSS release is backward
   compatible with previous releases, allowing NSS users to upgrade to the new NSS shared libraries
   without recompiling or relinking their applications.

.. _open-source_licensing_and_distribution:

`Open-Source Licensing and Distribution <#open-source_licensing_and_distribution>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   NSS is available under the `Mozilla Public License <https://www.mozilla.org/MPL/>`__, version 2.
   The latest source code is available for free worldwide from https://www.mozilla.org and its
   mirror sites.