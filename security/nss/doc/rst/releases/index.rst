.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

   nss_3_75.rst
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

   **NSS 3.75** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_75_release_notes`

   **NSS 3.68.2** is the latest LTS version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_68_2_release_notes`


.. container::

   Changes in 3.75 included in this release:

   - Bug 1749030 - This patch adds gcc-9 and gcc-10 to the CI.
   - Bug 1749794 - Make DottedOIDToCode.py compatible with python3.
   - Bug 1749475 - Avoid undefined shift in SSL_CERT_IS while fuzzing.
   - Bug 1748386 - Remove redundant key type check.
   - Bug 1749869 - Update ABI expectations to match ECH changes.
   - Bug 1748386 - Enable CKM_CHACHA20.
   - Bug 1747327 - check return on NSS_NoDB_Init and NSS_Shutdown.
   - Bug 1747310 - real move assignment operator.
   - Bug 1748245 - Run ECDSA test vectors from bltest as part of the CI tests.
   - Bug 1743302 - Add ECDSA test vectors to the bltest command line tool.
   - Bug 1747772 - Allow to build using clang's integrated assembler.
   - Bug 1321398 - Allow to override python for the build.
   - Bug 1747317 - test HKDF output rather than input.
   - Bug 1747316 - Use ASSERT macros to end failed tests early.
   - Bug 1747310 - move assignment operator for DataBuffer.
   - Bug 1712879 - Add test cases for ECH compression and unexpected extensions in SH.
   - Bug 1725938 - Update tests for ECH-13.
   - Bug 1725938 - Tidy up error handling.
   - Bug 1728281 - Add tests for ECH HRR Changes.
   - Bug 1728281 - Server only sends GREASE HRR extension if enabled by preference.
   - Bug 1725938 - Update generation of the Associated Data for ECH-13.
   - Bug 1712879 - When ECH is accepted, reject extensions which were only advertised in the Outer Client Hello.
   - Bug 1712879 - Allow for compressed, non-contiguous, extensions.
   - Bug 1712879 - Scramble the PSK extension in CHOuter.
   - Bug 1712647 - Split custom extension handling for ECH.
   - Bug 1728281 - Add ECH-13 HRR Handling.
   - Bug 1677181 - Client side ECH padding.
   - Bug 1725938 - Stricter ClientHelloInner Decompression.
   - Bug 1725938 - Remove ECH_inner extension, use new enum format.
   - Bug 1725938 - Update the version number for ECH-13 and adjust the ECHConfig size.

