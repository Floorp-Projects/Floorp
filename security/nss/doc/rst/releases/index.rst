.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

   nss_3_74.rst
   nss_3_68_2.rst
   nss_3_73_1.rst
   nss_3_73.rst
   nss_3_72_1.rst
   nss_3_72.rst
   nss_3_71.rst
   nss_3_70.rst
   nss_3_69_1.rst
   nss_3_69.rst
   nss_3_68_2.rst
   nss_3_68_1.rst
   nss_3_68.rst
   nss_3_67.rst
   nss_3_66.rst
   nss_3_65.rst
   nss_3_64.rst

.. note::

   **NSS 3.74** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_72_release_notes`

   A new version of the Certificate Authorities Root Store is available in this release.

   **NSS 3.68.2** is the latest LTS version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_68_2_release_notes`


.. container::

   Changes in 3.74 included in this release:

   - Bug 966856 - mozilla::pkix: support SHA-2 hashes in CertIDs in OCSP responses.
   - Bug 1553612 - Ensure clients offer consistent ciphersuites after HRR.
   - Bug 1721426 - NSS does not properly restrict server keys based on policy.
   - Bug 1733003 - Set nssckbi version number to 2.54.
   - Bug 1735407 - Replace Google Trust Services LLC (GTS) R4 root certificate in NSS.
   - Bug 1735407 - Replace Google Trust Services LLC (GTS) R3 root certificate in NSS.
   - Bug 1735407 - Replace Google Trust Services LLC (GTS) R2 root certificate in NSS.
   - Bug 1735407 - Replace Google Trust Services LLC (GTS) R1 root certificate in NSS.
   - Bug 1735407 - Replace GlobalSign ECC Root CA R4 in NSS.
   - Bug 1733560 - Remove Expired Root Certificates from NSS - DST Root CA X3.
   - Bug 1740807 - Remove Expiring Cybertrust Global Root and GlobalSign root certificates from NSS.
   - Bug 1741930 - Add renewed Autoridad de Certificacion Firmaprofesional CIF A62634068 root certificate to NSS.
   - Bug 1740095 - Add iTrusChina ECC root certificate to NSS.
   - Bug 1740095 - Add iTrusChina RSA root certificate to NSS.
   - Bug 1738805 - Add ISRG Root X2 root certificate to NSS.
   - Bug 1733012 - Add Chunghwa Telecom's HiPKI Root CA - G1 root certificate to NSS.
   - Bug 1738028 - Avoid a clang 13 unused variable warning in opt build.
   - Bug 1735028 - Check for missing signedData field.
   - Bug 1737470 - Ensure DER encoded signatures are within size limits.
