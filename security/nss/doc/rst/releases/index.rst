.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

   nss_3_83.rst
   nss_3_82.rst
   nss_3_81.rst
   nss_3_80.rst
   nss_3_79.rst
   nss_3_79_1.rst
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

   **NSS 3.83** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_83_release_notes`

   **NSS 3.79.1** is the latest ESR version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_79_1_release_notes`


.. container::

   Changes in 3.83 included in this release:

   - Bug 1788875 - Remove set-but-unused variables from SEC_PKCS12DecoderValidateBags
   - Bug 1563221 - remove older oses that are unused part3/ BeOS
   - Bug 1563221 - remove older unix support in NSS part 3 Irix
   - Bug 1563221 - remove support for older unix in NSS part 2 DGUX
   - Bug 1563221 - remove support for older unix in NSS part 1 OSF
   - Bug 1778413 - Set nssckbi version number to 2.58
   - Bug 1785297 - Add two SECOM root certificates to NSS
   - Bug 1787075 - Add two DigitalSign root certificates to NSS
   - Bug 1778412 - Remove Camerfirma Global Chambersign Root from NSS
   - Bug 1771100 - Added bug reference and description to disabled UnsolicitedServerNameAck bogo ECH test
   - Bug 1779361 - Removed skipping of ECH on equality of private and public SNI server name
   - Bug 1779357 - Added comment and bug reference to ECHRandomHRRExtension bogo test
   - Bug 1779370 - Added Bogo shim client HRR test support. Fixed overwriting of CHInner.random on HRR
   - Bug 1779234 - Added check for server only sending ECH extension with retry configs in EncryptedExtensions and if not accepting ECH. Changed config setting behavior to skip configs with unsupported mandatory extensions instead of failing
   - Bug 1771100 - Added ECH client support to BoGo shim. Changed CHInner creation to skip TLS 1.2 only extensions to comply with BoGo
   - Bug 1771100 - Added ECH server support to BoGo shim. Fixed NSS ECH server accept_confirmation bugs
   - Bug 1771100 - Update BoGo tests to recent BoringSSL version
   - Bug 1785846 - Bump minimum NSPR version to 4.34.1
