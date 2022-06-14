.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

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

   **NSS 3.79** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_79_release_notes`

   **NSS 3.79** is the latest ESR version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_79_release_notes`


.. container::

   Changes in 3.79 included in this release:

   - Bug 205717 - Use PK11_GetSlotInfo instead of raw C_GetSlotInfo calls.
   - Bug 1766907 - Update mercurial in clang-format docker image.
   - Bug 1454072 - Use of uninitialized pointer in lg_init after alloc fail.
   - Bug 1769295 - selfserv and tstclnt should use PR_GetPrefLoopbackAddrInfo.
   - Bug 1753315 - Add SECMOD_LockedModuleHasRemovableSlots.
   - Bug 1387919 - Fix secasn1d parsing of indefinite SEQUENCE inside indefinite GROUP.
   - Bug 1765753 - Added RFC8422 compliant TLS <= 1.2 undefined/compressed ECPointFormat extension alerts.
   - Bug 1765753 - TLS 1.3 Server: Send protocol_version alert on unsupported ClientHello.legacy_version.
   - Bug 1764788 - Correct invalid record inner and outer content type alerts.
   - Bug 1757075 - NSS does not properly import or export pkcs12 files with large passwords and pkcs5v2 encoding.
   - Bug 1766978 - improve error handling after nssCKFWInstance_CreateObjectHandle.
   - Bug 1767590 - Initialize pointers passed to NSS_CMSDigestContext_FinishMultiple.
   - Bug 1769302 - NSS 3.79 should depend on NSPR 4.34   


