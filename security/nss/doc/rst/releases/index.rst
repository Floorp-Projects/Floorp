.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

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

   **NSS 3.77** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_77_release_notes`

   **NSS 3.68.3** is the latest LTS version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_68_3_release_notes`


.. container::

   Changes in 3.77 included in this release:

   - Bug 1762244 - resolve mpitests build failure on Windows.
   - Bug 1761779 - Fix link to TLS page on wireshark wiki
   - Bug 1754890 - Add two D-TRUST 2020 root certificates.
   - Bug 1751298 - Add Telia Root CA v2 root certificate.
   - Bug 1751305 - Remove expired explicitly distrusted certificates from certdata.txt.
   - Bug 1005084 - support specific RSA-PSS parameters in mozilla::pkix
   - Bug 1753535 - Remove obsolete stateEnd check in SEC_ASN1DecoderUpdate.
   - Bug 1756271 - Remove token member from NSSSlot struct.
   - Bug 1602379 - Provide secure variants of mpp_pprime and mpp_make_prime.
   - Bug 1757279 - Support UTF-8 library path in the module spec string.
   - Bug 1396616 - Update nssUTF8_Length to RFC 3629 and fix buffer overrun.
   - Bug 1760827 - Add a CI Target for gcc-11.
   - Bug 1760828 - Change to makefiles for gcc-4.8.
   - Bug 1741688 - Update googletest to 1.11.0
   - Bug 1759525 - Add SetTls13GreaseEchSize to experimental API.
   - Bug 1755264 - TLS 1.3 Illegal legacy_version handling/alerts.
   - Bug 1755904 - Fix calculation of ECH HRR Transcript.
   - Bug 1758741 - Allow ld path to be set as environment variable.
   - Bug 1760653 - Ensure we don't read uninitialized memory in ssl gtests.
   - Bug 1758478 - Fix DataBuffer Move Assignment.
   - Bug 1552254 - internal_error alert on Certificate Request with sha1+ecdsa in TLS 1.3
   - Bug 1755092 - rework signature verification in mozilla::pkix

