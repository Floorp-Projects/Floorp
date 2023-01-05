.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

   nss_3_87.rst
   nss_3_86.rst
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

   **NSS 3.87** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_87_release_notes`

   **NSS 3.79.2** is the latest ESR version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_79_2_release_notes`


.. container::

   Changes in 3.87 included in this release:

   - Bug 1803226 - NULL password encoding incorrect.
   - Bug 1804071 - Fix rng stub signature for fuzzing builds.
   - Bug 1803595 - Updating the compiler parsing for build.
   - Bug 1749030 - Modification of supported compilers.
   - Bug 1774654 tstclnt crashes when accessing gnutls server without a user cert in the database.
   - Bug 1751707 - Add configuration option to enable source-based coverage sanitizer.
   - Bug 1751705 - Update ECCKiila generated files.
   - Bug 1730353 - Add support for the LoongArch 64-bit architecture.
   - Bug 1798823 - add checks for zero-length RSA modulus to avoid memory errors and failed assertions later.
   - Bug 1798823 - Additional zero-length RSA modulus checks.