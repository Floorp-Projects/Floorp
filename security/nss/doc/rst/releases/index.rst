.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

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

   **NSS 3.97** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_97_release_notes`

   **NSS 3.90.1 (ESR)** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_90_1_release_notes`

.. container::

   Changes in 3.97 included in this release:

  - Bug 1875506 - make Xyber768d00 opt-in by policy.
  - Bug 1871631 - add libssl support for xyber768d00.
  - Bug 1871630 - add PK11_ConcatSymKeys.
  - Bug 1775046 - add Kyber and a PKCS#11 KEM interface to softoken.
  - Bug 1871152 - add a FreeBL API for Kyber.
  - Bug 1826451 - part 2: vendor github.com/pq-crystals/kyber/commit/e0d1c6ff.
  - Bug 1826451 - part 1: add a script for vendoring kyber from pq-crystals repo.
  - Bug 1835828 - Removing the calls to RSA Blind from loader.*
  - Bug 1874111 - fix worker type for level3 mac tasks.
  - Bug 1835828 - RSA Blind implementation.
  - Bug 1869642 - Remove DSA selftests.
  - Bug 1873296 - read KWP testvectors from JSON.
  - Bug 1822450 - Backed out changeset dcb174139e4f
  - Bug 1822450 - Fix CKM_PBE_SHA1_DES2_EDE_CBC derivation.
  - Bug 1871219 - Wrap CC shell commands in gyp expansions.

