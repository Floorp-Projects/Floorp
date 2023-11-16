.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

   nss_3_95.rst
   nss_3_94.rst
   nss_3_93.rst
   nss_3_92.rst
   nss_3_91_0.rst
   nss_3_90_0.rst
   nss_3_89_1.rst
   nss_3_89.rst
   nss_3_88_1.rst
   nss_3_88.rst
   nss_3_87_1.rst
   nss_3_87.rst
   nss_3_86.rst
   nss_3_85.rst
   nss_3_84.rst
   nss_3_83.rst
   nss_3_82.rst
   nss_3_81.rst
   nss_3_80.rst
   nss_3_79_4.rst
   nss_3_79_3.rst
   nss_3_79_2.rst
   nss_3_79_1.rst
   nss_3_79.rst
   nss_3_78_1.rst
   nss_3_78.rst
   nss_3_77.rst
   nss_3_76_1.rst
   nss_3_76.rst
   nss_3_75.rst
   nss_3_74.rst
   nss_3_73_1.rst
   nss_3_73.rst
   nss_3_72_1.rst
   nss_3_72.rst
   nss_3_71.rst
   nss_3_70.rst
   nss_3_69_1.rst
   nss_3_69.rst
   nss_3_68_4.rst
   nss_3_68_3.rst
   nss_3_68_2.rst
   nss_3_68_1.rst
   nss_3_68.rst
   nss_3_67.rst
   nss_3_66.rst
   nss_3_65.rst
   nss_3_64.rst

.. note::

   **NSS 3.95.0** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_95_0_release_notes`

   **NSS 3.90.1 (ESR)** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_90_1_release_notes`

.. container::

   Changes in 3.95 included in this release:

  - Bug 1842932 - Bump builtins version number.
  - Bug 1851044: Remove Email trust bit from Autoridad de Certificacion Firmaprofesional CIF A62634068 root cert.
  - Bug 1855318: Remove 4 DigiCert (Symantec/Verisign) Root Certificates from NSS.
  - Bug 1851049: Remove 3 TrustCor Root Certificates from NSS.
  - Bug 1850982 - Remove Camerfirma root certificates from NSS.
  - Bug 1842935 - Remove old Autoridad de Certificacion Firmaprofesional Certificate.
  - Bug 1860670 - Add four Commscope root certificates to NSS.
  - Bug 1850598 - Add TrustAsia Global Root CA G3 and G4 root certificates.
  - Bug 1863605 - Include P-384 and P-521 Scalar Validation from HACL*
  - Bug 1861728 - Include P-256 Scalar Validation from HACL*.
  - Bug 1861265 After the HACL 256 ECC patch, NSS incorrectly encodes 256 ECC without DER wrapping at the softoken level
  - Bug 1837987:Add means to provide library parameters to C_Initialize
  - Bug 1573097 - clang format
  - Bug 1854795 - add OSXSAVE and XCR0 tests to AVX2 detection.
  - Bug 1858241 - Typo in ssl3_AppendHandshakeNumber
  - Bug 1858241 - Introducing input check of ssl3_AppendHandshakeNumber
  - Bug 1573097 - Fix Invalid casts in instance.c
