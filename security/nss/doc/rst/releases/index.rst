.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

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

   **NSS 3.91.0** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_91_0_release_notes`

   **NSS 3.90.0 (ESR)** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_90_0_release_notes`

.. container::

   Changes in 3.91 included in this release:

 - Bug 1837431 - Implementation of the HW support check for ADX instruction
 - Bug 1836925 - Removing the support of Curve25519
 - Bug 1839795 - Fix comment about the addition of ticketSupportsEarlyData.
 - Bug 1839327 - Adding args to enable-legacy-db build
 - Bug 1835357 dbtests.sh failure in "certutil dump keys with explicit default trust flags"
 - Bug 1837617: Initialize flags in slot structures
 - Bug 1835425: Improve the length check of RSA input to avoid heap overflow
 - Bug 1829112 - Followup Fixes
 - Bug 1784253: avoid processing unexpected inputs by checking for m_exptmod base sign
 - Bug 1826652: add a limit check on order_k to avoid infinite loop
 - Bug 1834851 - Update HACL* to commit 5f6051d2.
 - Bug 1753026 - add SHA3 to cryptohi and softoken.
 - Bug 1753026: HACL SHA3
 - Bug 1836781 - Disabling ASM C25519 for A but X86_64
