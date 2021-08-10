.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

   nss_3_69.rst
   nss_3_68.rst
   nss_3_67.rst
   nss_3_66.rst
   nss_3_65.rst
   nss_3_64.rst

.. note::

   **NSS 3.69** is the latest version of NSS.

   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_69_release_notes`

.. container::

   Changes included in this release:

   - Bug 1722613 - Disable DTLS 1.0 and 1.1 by default
   - Bug 1720226 - integrity checks in key4.db not happening on private components with AES_CBC
   - Bug 1720235 - SSL handling of signature algorithms ignores environmental invalid algorithms.
   - Bug 1721476 - sqlite 3.34 changed it's open semantics, causing nss failures.
   - Bug 1720230 - Gtest update changed the gtest reports, losing gtest details in all.sh reports.
   - Bug 1720228 - NSS incorrectly accepting 1536 bit DH primes in FIPS mode
   - Bug 1720232 - SQLite calls could timeout in starvation situations.
   - Bug 1720225 - Coverity/cpp scanner errors found in nss 3.67
   - Bug 1709817 - Import the NSS documentation from MDN in nss/doc.
   - Bug 1720227 - NSS using a tempdir to measure sql performance not active
