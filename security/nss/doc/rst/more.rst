.. _more_documentation:

.. warning::
   This NSS documentation was just imported from our legacy MDN repository.
   It currently is very deprecated and likely incorrect or broken in many places.

More Documentation
------------------

.. container::

   **Network Security Services** (**NSS**) is a set of libraries designed to support cross-platform
   development of security-enabled client and server applications. Applications built with NSS can
   support SSL v3, TLS, PKCS #5, PKCS #7, PKCS #11, PKCS #12, S/MIME, X.509 v3 certificates, and
   other security standards.

   For detailed information on standards supported, see :ref:`mozilla_projects_nss_overview`. For a
   list of frequently asked questions, see the :ref:`mozilla_projects_nss_faq`.

   NSS is available under the Mozilla Public License. For information on downloading NSS releases as
   tar files, see :ref:`mozilla_projects_nss_nss_sources_building_testing`.

   If you're a developer and would like to contribute to NSS, you might want to read the documents
   :ref:`mozilla_projects_nss_an_overview_of_nss_internals` and
   :ref:`mozilla_projects_nss_getting_started_with_nss`.

   .. rubric:: Background Information
      :name: Background_Information

   :ref:`mozilla_projects_nss_overview`
      Provides a brief summary of NSS and its capabilities.
   :ref:`mozilla_projects_nss_faq`
      Answers basic questions about NSS.
   `Introduction to Public-Key Cryptography <https://developer.mozilla.org/en-US/docs/Archive/Security/Introduction_to_Public-Key_Cryptography>`__
      Explains the basic concepts of public-key cryptography that underlie NSS.
   `Introduction to SSL <https://developer.mozilla.org/en-US/docs/Archive/Security/Introduction_to_SSL>`__
      Introduces the SSL protocol, including information about cryptographic ciphers supported by
      SSL and the steps involved in the SSL handshake.

   .. rubric:: Getting Started
      :name: Getting_Started

   :ref:`mozilla_projects_nss_nss_releases`
      This page contains information about the current and past releases of NSS.
   :ref:`mozilla_projects_nss_nss_sources_building_testing`
      Instructions on how to build NSS on the different supported platforms.
   `Get Mozilla Source Code Using Mercurial <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Source_Code/Mercurial>`__
      Information about with working with Mercurial.
   `Get Mozilla Source Code Using CVS (deprecated) <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Source_Code/CVS>`__
      Old deprecated CVS documentation.

   .. rubric:: NSS APIs
      :name: NSS_APIs

   :ref:`mozilla_projects_nss_introduction_to_network_security_services`
      Provides an overview of the NSS libraries and what you need to know to use them.
   :ref:`mozilla_projects_nss_ssl_functions`
      Summarizes the SSL APIs exported by the NSS shared libraries.
   :ref:`mozilla_projects_nss_reference`
      API used to invoke SSL operations.
   :ref:`mozilla_projects_nss_nss_api_guidelines`
      Explains how the libraries and code are organized, and guidelines for developing code (naming
      conventions, error handling, thread safety, etc.)
   :ref:`mozilla_projects_nss_nss_tech_notes`
      Links to NSS technical notes, which provide latest information about new NSS features and
      supplementary documentation for advanced topics in programming with NSS.

   .. rubric:: Tools, testing, and other technical details
      :name: Tools_testing_and_other_technical_details

   :ref:`mozilla_projects_nss_building`
      Describe how to check out and build NSS releases.

   :ref:`mozilla_projects_nss_nss_developer_tutorial`
      How to make changes in NSS. Coding style, maintaining ABI compatibility.

   :ref:`mozilla_projects_nss_tools`
      Tools for developing, debugging, and managing applications that use NSS.
   :ref:`mozilla_projects_nss_nss_sample_code`
      Demonstrates how NSS can be used for cryptographic operations, certificate handling, SSL, etc.
   :ref:`mozilla_projects_nss_nss_third-party_code`
      A list of third-party code included in the NSS library.
   `NSS 3.2 Test Suite <https://www-archive.mozilla.org/projects/security/pki/nss/testnss_32.html>`__
      **Archived version.** Describes how to run the standard NSS tests.
   `NSS Performance Reports <https://www-archive.mozilla.org/projects/security/pki/nss/performance_reports.html>`__
      **Archived version.** Links to performance reports for NSS 3.2 and later releases.
   `Encryption Technologies Available in NSS 3.11 <https://www-archive.mozilla.org/projects/security/pki/nss/nss-3.11/nss-3.11-algorithms.html>`__
      **Archived version.** Lists the cryptographic algorithms used by NSS 3.11.
   `NSS 3.1 Loadable Root Certificates <https://www-archive.mozilla.org/projects/security/pki/nss/loadable_certs.html>`__
      **Archived version.** Describes the scheme for loading root CA certificates.
   `cert7.db <https://www-archive.mozilla.org/projects/security/pki/nss/db_formats.html>`__
      **Archived version.** General format of the cert7.db database.

   .. rubric:: PKCS #11 information
      :name: PKCS_11_information

   -  :ref:`mozilla_projects_nss_pkcs11`
   -  :ref:`mozilla_projects_nss_pkcs11_implement`
   -  :ref:`mozilla_projects_nss_pkcs11_module_specs`
   -  :ref:`mozilla_projects_nss_pkcs11_faq`
   -  `Using the JAR Installation Manager to Install a PKCS #11 Cryptographic
      Module <https://developer.mozilla.org/en-US/docs/PKCS11_Jar_Install>`__
   -  `PKCS #11 Conformance Testing - Archived
      version <https://www-archive.mozilla.org/projects/security/pki/pkcs11/>`__

   .. rubric:: CA certificates pre-loaded into NSS
      :name: CA_certificates_pre-loaded_into_NSS

   -  `Mozilla CA certificate policy <https://www.mozilla.org/projects/security/certs/policy/>`__
   -  `List of pre-loaded CA certificates <https://wiki.mozilla.org/CA/Included_Certificates>`__

      -  Consumers of this list must consider the trust bit setting for each included root
         certificate. `More
         Information <https://www.imperialviolet.org/2012/01/30/mozillaroots.html>`__, `Extracting
         roots and their trust bits <https://github.com/agl/extract-nss-root-certs>`__

   .. rubric:: NSS is built on top of Netscape Portable Runtime (NSPR)
      :name: NSS_is_built_on_top_of_Netscape_Portable_Runtime_NSPR

   `Netscape Portable Runtime <NSPR>`__
      NSPR project page.
   `NSPR Reference <https://developer.mozilla.org/en-US/docs/Mozilla/Projects/NSPR/Reference>`__
      NSPR API documentation.

   .. rubric:: Additional Information
      :name: Additional_Information

   -  `Using the window.crypto object from
      JavaScript <https://developer.mozilla.org/en-US/docs/JavaScript_crypto>`__
   -  :ref:`mozilla_projects_nss_http_delegation`
   -  :ref:`mozilla_projects_nss_tls_cipher_suite_discovery`
   -  :ref:`mozilla_projects_nss_certificate_download_specification`
   -  :ref:`mozilla_projects_nss_fips_mode_-_an_explanation`
   -  :ref:`mozilla_projects_nss_key_log_format`

   .. rubric:: Planning
      :name: Planning

   Information on NSS planning can be found at `wiki.mozilla.org <https://wiki.mozilla.org/NSS>`__,
   including:

   -  `FIPS Validation <https://wiki.mozilla.org/FIPS_Validation>`__
   -  `NSS Roadmap page <https://wiki.mozilla.org/NSS:Roadmap>`__
   -  `NSS Improvement
      Project <https://fedoraproject.org/wiki/User:Mitr/NSS:DeveloperFriendliness>`__

.. _Related_Topics:

Related Topics
~~~~~~~~~~~~~~

-  `Security <https://developer.mozilla.org/en-US/docs/Security>`__

