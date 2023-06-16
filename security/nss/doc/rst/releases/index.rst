.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

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

   **NSS 3.90.0 (ESR)** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_90_0_release_notes`


.. container::

   Changes in 3.90.0 included in this release:

   - Bug 1623338 - ride along: remove a duplicated doc page
   - Bug 1623338 - remove a reference to IRC
   - Bug 1831983 - clang-format lib/freebl/stubs.c
   - Bug 1831983 - Add a constant time select function
   - Bug 1774657 - Updating an old dbm with lots of certs with keys to sql results in a database that is slow to access.
   - Bug 1830973 - output early build errors by default
   - Bug 1804505 - Update the technical constraints for KamuSM
   - Bug 1822921 - Add BJCA Global Root CA1 and CA2 root certificates
   - Bug 1790763 - Enable default UBSan Checks
   - Bug 1786018 - Add explicit handling of zero length records
   - Bug 1829391 - Tidy up DTLS ACK Error Handling Path
   - Bug 1786018 - Refactor zero length record tests
   - Bug 1829112 - Fix compiler warning via correct assert
   - Bug 1755267 - run linux tests on nss-t/t-linux-xlarge-gcp
   - Bug 1806496 - In FIPS mode, nss should reject RSASSA-PSS salt lengths larger than the output size of the hash function used, or provide an indicator
   - Bug 1784163 - Fix reading raw negative numbers
   - Bug 1748237 - Repairing unreachable code in clang built with gyp
   - Bug 1783647 - Integrate Vale Curve25519
   - Bug 1799468 - Removing unused flags for Hacl*
   - Bug 1748237 - Adding a better error message
   - Bug 1727555 - Update HACL* till 51a72a953a4ee6f91e63b2816ae5c4e62edf35d6
   - Bug 1782980 - Fall back to the softokn when writing certificate trust
   - Bug 1806010 - FIPS-104-3 requires we restart post programmatically
   - Bug 1826650 - cmd/ecperf: fix dangling pointer warning on gcc 13
   - Bug 1818766 - Update ACVP dockerfile for compatibility with debian package changes
   - Bug 1815796 - Add a CI task for tracking ECCKiila code status, update whitespace in ECCKiila files
   - Bug 1819958 - Removed deprecated sprintf function and replaced with snprintf
   - Bug 1822076 - fix rst warnings in nss doc
   - Bug 1821997 - Fix incorrect pygment style
   - Bug 1821292 - Change GYP directive to apply across platforms
   - Add libsmime3 abi-check exception for NSS_CMSSignerInfo_GetDigestAlgTag
