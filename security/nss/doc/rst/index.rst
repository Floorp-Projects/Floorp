.. _mozilla_projects_nss:

Network Security Services (NSS)
===============================

.. toctree::
   :maxdepth: 2
   :glob:
   :hidden:

   build.rst
   build_artifacts.rst
   releases/index.rst
   community.rst

.. warning::
   This NSS documentation was just imported from our legacy MDN repository. It
   currently is very deprecated and likely incorrect or broken in many places.

.. container::

   **Network Security Services** (**NSS**) is a set of libraries designed to
   support cross-platform development of security-enabled client and server
   applications. Applications built with NSS can support SSL v3, TLS, PKCS #5,
   PKCS #7, PKCS #11, PKCS #12, S/MIME, X.509 v3 certificates, and other
   security standards.

   NSS is available under the Mozilla Public License v2 (MPLv2).

   If you're a developer and would like to contribute to NSS, you might want to
   read the documents:

   .. rubric:: Getting Started
      :name: Getting_Started

   :ref:`mozilla_projects_nss_building`
      This page contains information how to download, build and test NSS.

   :ref:`mozilla_projects_nss_releases`
      This page contains information about recent releases of NSS.

   :ref:`mozilla_projects_nss_nss_releases`
      This page contains information about older releases of NSS.

   :ref:`mozilla_projects_nss_community`
      This page contains information about the community and how to reach out.


.. warning::
   References below this point are part of the deprecated documentation and will
   be ported in the future. You can contribute to refreshing this documentation
   by submitting changes directly in the NSS repository (``nss/doc/rst``).

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

   - :ref:`NSPR` - NSPR project page.
   - :ref:`NSPR API Reference` - NSPR API documentation.

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
