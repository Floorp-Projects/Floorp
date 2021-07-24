.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

   nss_3_68.rst
   nss_3_67.rst
   nss_3_66.rst
   nss_3_65.rst
   nss_3_64.rst

.. note::

   **NSS 3.68** is the latest version of NSS.

   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_68_release_notes`

.. container::

   Changes included in this release:

   -  Bug 1709654 - Update for NetBSD configuration.
   -  Bug 1709750 - Disable HPKE test when fuzzing.
   -  Bug 1566124 - Optimize AES-GCM for ppc64le.
   -  Bug 1699021 - Add AES-256-GCM to HPKE.
   -  Bug 1698419 - ECH -10 updates.
   -  Bug 1692930 - Update HPKE to final version.
   -  Bug 1707130 - NSS should use modern algorithms in PKCS#12 files by default.
   -  Bug 1703936 - New coverity/cpp scanner errors.
   -  Bug 1697303 - NSS needs to update it's csp clearing to FIPS 180-3 standards.
   -  Bug 1702663 - Need to support RSA PSS with Hashing PKCS #11 Mechanisms.
   -  Bug 1705119 - Deadlock when using GCM and non-thread safe tokens.
