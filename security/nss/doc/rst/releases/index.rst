.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

   nss_3_80.rst
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

   **NSS 3.80** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_80_release_notes`

   **NSS 3.79** is the latest ESR version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_79_release_notes`


.. container::

   Changes in 3.80 included in this release:

   - Bug 1774720 - Fix SEC_ERROR_ALGORITHM_MISMATCH entry in SECerrs.h.
   - Bug 1617956 - Add support for asynchronous client auth hooks.
   - Bug 1497537 - nss-policy-check: make unknown keyword check optional.
   - Bug 1765383 - GatherBuffer: Reduced plaintext buffer allocations by allocating it on initialization. Replaced redundant code with assert. Debug builds: Added buffer freeing/allocation for each record.
   - Bug 1773022 - Mark 3.79 as an ESR release.
   - Bug 1764206 - Bump nssckbi version number for June.
   - Bug 1759815 - Remove Hellenic Academic 2011 Root.
   - Bug 1770267 - Add E-Tugra Roots.
   - Bug 1768970 - Add Certainly Roots.
   - Bug 1764392 - Add DigitCert Roots.
   - Bug 1759794 - Protect SFTKSlot needLogin with slotLock.
   - Bug 1366464 - Compare signature and signatureAlgorithm fields in legacy certificate verifier.
   - Bug 1771497 - Uninitialized value in cert_VerifyCertChainOld.
   - Bug 1771495 - Unchecked return code in sec_DecodeSigAlg.
   - Bug 1771498 - Uninitialized value in cert_ComputeCertType.
   - Bug 1760998 - Avoid data race on primary password change.
   - Bug 1769063 - Replace ppc64 dcbzl intrinisic.
   - Bug 1771036 - Allow LDFLAGS override in makefile builds.

