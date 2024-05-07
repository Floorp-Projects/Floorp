.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

   nss_3_100.rst
   nss_3_99.rst
   nss_3_98.rst
   nss_3_97.rst
   nss_3_96_1.rst
   nss_3_96.rst
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

   **NSS 3.100** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_100_release_notes`

   **NSS 3.90.2 (ESR)** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_90_2_release_notes`

.. container::

   Changes in 3.100 included in this release:

 - Bug 1893029 - merge pk11_kyberSlotList into pk11_ecSlotList for faster Xyber operations.
 - Bug 1893752 - remove ckcapi.
 - Bug 1893162 - avoid a potential PK11GenericObject memory leak.
 - Bug 671060 - Remove incomplete ESDH code.
 - Bug 215997 - Decrypt RSA OAEP encrypted messages.
 - Bug 1887996 - Fix certutil CRLDP URI code.
 - Bug 1890069 - Don't set CKA_DERIVE for CKK_EC_EDWARDS private keys.
 - Bug 676118: Add ability to encrypt and decrypt CMS messages using ECDH.
 - Bug 676100 - Correct Templates for key agreement in smime/cmsasn.c.
 - Bug 1548723 - Moving the decodedCert allocation to NSS.
 - Bug 1885404 - Allow developers to speed up repeated local execution of NSS tests that depend on certificates.