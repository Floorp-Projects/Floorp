.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

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

   **NSS 3.86** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_86_release_notes`

   **NSS 3.79.2** is the latest ESR version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_79_2_release_notes`


.. container::

   Changes in 3.86 included in this release:

   - Bug 1803190 - conscious language removal in NSS.
   - Bug 1794506 - Set nssckbi version number to 2.60.
   - Bug 1803453 - Set CKA_NSS_SERVER_DISTRUST_AFTER and CKA_NSS_EMAIL_DISTRUST_AFTER for 3 TrustCor Root Certificates.
   - Bug 1799038 - Remove Staat der Nederlanden EV Root CA from NSS.
   - Bug 1797559 - Remove EC-ACC root cert from NSS.
   - Bug 1794507 - Remove SwissSign Platinum CA - G2 from NSS.
   - Bug 1794495 - Remove Network Solutions Certificate Authority.
   - Bug 1802331 - compress docker image artifact with zstd.
   - Bug 1799315 - Migrate nss from AWS to GCP.
   - Bug 1800989 - Enable static builds in the CI.
   - Bug 1765759 - Removing SAW docker from the NSS build system.
   - Bug 1783231 - Initialising variables in the rsa blinding code.
   - Bug 320582 - Implementation of the double-signing of the message for ECDSA.
   - Bug 1783231 - Adding exponent blinding for RSA.

