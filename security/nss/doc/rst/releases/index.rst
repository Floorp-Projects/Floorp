.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

   nns_3_78.rst
   nns_3_77.rst
   nns_3_76_1.rst
   nns_3_76.rst
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
   nss_3_68_3.rst
   nss_3_68_2.rst
   nss_3_68_1.rst
   nss_3_68.rst
   nss_3_67.rst
   nss_3_66.rst
   nss_3_65.rst
   nss_3_64.rst

.. note::

   **NSS 3.78** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_78_release_notes`

   **NSS 3.68.3** is the latest LTS version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_68_3_release_notes`


.. container::

   Changes in 3.78 included in this release:

   - Bug 1755264 - Added TLS 1.3 zero-length inner plaintext checks and tests, zero-length record/fragment handling tests.
   - Bug 1294978 - Reworked overlong record size checks and added TLS1.3 specific boundaries.
   - Bug 1763120 - Add ECH Grease Support to tstclnt
   - Bug 1765003 - Add a strict variant of moz::pkix::CheckCertHostname.
   - Bug 1166338 - Change SSL_REUSE_SERVER_ECDHE_KEY default to false.
   - Bug 1760813 - Make SEC_PKCS12EnableCipher succeed 
   - Bug 1762489 - Update zlib in NSS to 1.2.12.

