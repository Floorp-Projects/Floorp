.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

   nss_3_70.rst
   nss_3_69_1.rst
   nss_3_69.rst
   nss_3_68.rst
   nss_3_67.rst
   nss_3_66.rst
   nss_3_65.rst
   nss_3_64.rst

.. note::

   **NSS 3.70** is the latest version of NSS.

   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_70_release_notes`

.. container::

   Changes included in this release:

   - Documentation: release notes for NSS 3.70.
   - Documentation: release notes for NSS 3.69.1.
   - Bug 1726022 - Update test case to verify fix.
   - Bug 1714579 - Explicitly disable downgrade check in TlsConnectStreamTls13.EchOuterWith12Max
   - Bug 1714579 - Explicitly disable downgrade check in TlsConnectTest.DisableFalseStartOnFallback
   - Formatting for lib/util
   - Bug 1681975 - Avoid using a lookup table in nssb64d.
   - Bug 1724629 - Use HW accelerated SHA2 on AArch64 Big Endian.
   - Bug 1714579 Change default value of enableHelloDowngradeCheck to true.
   - Formatting for gtests/pk11_gtest/pk11_hpke_unittest.cc
   - Bug 1726022 Cache additional PBE entries.
   - Bug 1709750 - Read HPKE vectors from official JSON.
   - Documentation: update for NSS 3.69 release.
