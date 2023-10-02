.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

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

   **NSS 3.94.0** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_94_0_release_notes`

   **NSS 3.90.0 (ESR)** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_90_0_release_notes`

.. container::

   Changes in 3.94 included in this release:

   - Bug 1853737 - Updated code and commit ID for HACL*. 
   - Bug 1840510 - update ACVP fuzzed test vector: refuzzed with current NSS
   - Bug 1827303 - Softoken C_ calls should use system FIPS setting to select NSC_ or FC_ variants.
   - Bug 1774659 - NSS needs a database tool that can dump the low level representation of the database. 
   - Bug 1852179 - declare string literals using char in pkixnames_tests.cpp. 
   - Bug 1852179 - avoid implicit conversion for ByteString.
   - Bug 1818766 - update rust version for acvp docker.
   - Bug 1852011 - Moving the init function of the mpi_ints before clean-up in ec.c 
   - Bug 1615555 - P-256 ECDH and ECDSA from HACL*. 
   - Bug 1840510 - Add ACVP test vectors to the repository 
   - Bug 1849077 - Stop relying on std::basic_string<uint8_t>.
   - Bug 1847845 - Transpose the PPC_ABI check from Makefile to gyp.
