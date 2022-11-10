.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

   nss_3_85.rst
   nss_3_84.rst
   nss_3_83.rst
   nss_3_82.rst
   nss_3_81.rst
   nss_3_80.rst
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

   **NSS 3.85** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_85_release_notes`

   **NSS 3.79.2** is the latest ESR version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_79_2_release_notes`


.. container::

   Changes in 3.85 included in this release:

   - Bug 1792821 - Modification of the primes.c and dhe-params.c in order to have better looking tables.
   - Bug 1796815 - Update zlib in NSS to 1.2.13.
   - Bug 1796504 - Skip building modutil and shlibsign when building in Firefox.
   - Bug 1796504 - Use __STDC_VERSION__ rather than __STDC__ as a guard. 
   - Bug 1796407 - Fix -Wunused-but-set-variable warning from clang 15.
   - Bug 1796308 - Fix -Wtautological-constant-out-of-range-compare and -Wtype-limits warnings. 
   - Bug 1796281 - Followup: add missing stdint.h include.
   - Bug 1796281 - Fix -Wint-to-void-pointer-cast warnings.
   - Bug 1796280 - Fix -Wunused-{function,variable,but-set-variable} warnings on Windows.
   - Bug 1796079 - Fix -Wstring-conversion warnings.
   - Bug 1796075 - Fix -Wempty-body warnings.
   - Bug 1795242 - Fix unused-but-set-parameter warning.
   - Bug 1795241 - Fix unreachable-code warnings.
   - Bug 1795222 - Mark _nss_version_c unused on clang-cl.
   - Bug 1795668 - Remove redundant variable definitions in lowhashtest.
   - No bug - Add note about python executable to build instructions.