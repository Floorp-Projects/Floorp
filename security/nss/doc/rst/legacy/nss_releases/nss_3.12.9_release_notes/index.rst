.. _:

NSS 3.12.9 release notes
========================

.. _removed_functions:

`Removed functions <#removed_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   2010-09-23
   *Newsgroup:*\ `mozilla.dev.tech.crypto <news://news.mozilla.org/mozilla.dev.tech.crypto>`__

   .. container::
      :name: section_1

      .. rubric:: Introduction
         :name: Introduction_2

      Network Security Services (NSS) 3.12.9 is a patch release for NSS 3.12. The bug fixes in NSS
      3.12.9 are described in the "\ `Bugs Fixed <#bugsfixed>`__" section below.

      NSS 3.12.9 is tri-licensed under the MPL 1.1/GPL 2.0/LGPL 2.1.

   .. container::
      :name: section_2

      .. rubric:: Distribution Information
         :name: Distribution_Information

      | The CVS tag for the NSS 3.12.9 release is ``NSS_3.12.9_RTM``.  NSS 3.12.9 requires `NSPR
        4.8.7 <https://www.mozilla.org/projects/nspr/release-notes/nspr486.html>`__.
      | See the `Documentation <#docs>`__ section for the build instructions.

      NSS 3.12.9 source distribution is also available on ``ftp.mozilla.org`` for secure HTTPS
      download:

      -  Source tarballs:
         https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3.12.9_RTM/src/.

      You also need to download the NSPR 4.8.7 binary distributions to get the NSPR 4.8.7 header
      files and shared libraries, which NSS 3.12.9 requires. NSPR 4.8.7 binary distributions are in
      https://ftp.mozilla.org/pub/mozilla.org/nspr/releases/v4.8.7/.

   .. container::
      :name: section_3

      .. rubric:: New in NSS 3.12.9
         :name: New_in_NSS_3.12.9

      .. container::
         :name: section_5

      .. container::
         :name: section_6

         .. rubric:: New SSL options
            :name: New_SSL_options

         .. container::
            :name: section_7

            .. rubric:: New error codes
               :name: New_error_codes

   .. container::
      :name: section_8

      .. rubric:: Bugs Fixed
         :name: Bugs_Fixed

      The following bugs have been fixed in NSS 3.12.9.

      -  `Bug 609068 <https://bugzilla.mozilla.org/show_bug.cgi?id=609068>`__: Implement J-PAKE in
         FreeBL
      -  `Bug 607058 <https://bugzilla.mozilla.org/show_bug.cgi?id=607058>`__: crash [@
         nss_cms_decoder_work_data]
      -  `Bug 613394 <https://bugzilla.mozilla.org/show_bug.cgi?id=613394>`__: November/December
         2010 batch of NSS root CA changes
      -  `Bug 610843 <https://bugzilla.mozilla.org/show_bug.cgi?id=610843>`__: Need way to recover
         softoken in child after fork()
      -  `Bug 617492 <https://bugzilla.mozilla.org/show_bug.cgi?id=617492>`__: Add
         PK11_KeyGenWithTemplate function to pk11wrap (for Firefox Sync)
      -  `Bug 610162 <https://bugzilla.mozilla.org/show_bug.cgi?id=610162>`__: SHA-512 and SHA-384
         hashes are incorrect for inputs of 512MB or larger when running under Windows and other
         32-bit platforms (Fx 3.6.12 and 4.0b6)
      -  `Bug 518551 <https://bugzilla.mozilla.org/show_bug.cgi?id=518551>`__: Vfychain crashes in
         PKITS tests.
      -  `Bug 536485 <https://bugzilla.mozilla.org/show_bug.cgi?id=536485>`__: crash during ssl
         handshake in [@ intel_aes_decrypt_cbc_256]
      -  `Bug 444367 <https://bugzilla.mozilla.org/show_bug.cgi?id=444367>`__: NSS 3.12 softoken
         returns the certificate type of a certificate object as CKC_X_509_ATTR_CERT.
      -  `Bug 620908 <https://bugzilla.mozilla.org/show_bug.cgi?id=620908>`__: certutil -T -d
         "sql:." dumps core
      -  `Bug 584257 <https://bugzilla.mozilla.org/show_bug.cgi?id=584257>`__: Need a way to expand
         partial private keys.
      -  `Bug 596798 <https://bugzilla.mozilla.org/show_bug.cgi?id=596798>`__: win_rand.c (among
         others) uses unsafe \_snwprintf
      -  `Bug 597622 <https://bugzilla.mozilla.org/show_bug.cgi?id=597622>`__: Do not use the
         SEC_ERROR_BAD_INFO_ACCESS_LOCATION error code for bad CRL distribution point URLs
      -  `Bug 619268 <https://bugzilla.mozilla.org/show_bug.cgi?id=619268>`__: Memory leaks in
         CERT_ChangeCertTrust and CERT_SaveSMimeProfile
      -  `Bug 585518 <https://bugzilla.mozilla.org/show_bug.cgi?id=585518>`__: AddTrust Qualified CA
         Root serial wrong in certdata.txt trust entry
      -  `Bug 337433 <https://bugzilla.mozilla.org/show_bug.cgi?id=337433>`__: Need
         CERT_FindCertByNicknameOrEmailAddrByUsage
      -  `Bug 592939 <https://bugzilla.mozilla.org/show_bug.cgi?id=592939>`__: Expired CAs in
         certdata.txt

   .. container::
      :name: section_9

      .. rubric:: Documentation
         :name: Documentation

      NSS Documentation. New and revised documents available since the release of NSS 3.11 include
      the following:

      -  `Build Instructions for NSS 3.11.4 and
         above <https://www-archive.mozilla.org/projects/security/pki/nss/nss-3.11.4/nss-3.11.4-build>`__
      -  `NSS Shared DB <http://wiki.mozilla.org/NSS_Shared_DB>`__

   .. container::
      :name: section_10

      .. rubric:: Compatibility
         :name: Compatibility

      NSS 3.12.9 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
      program linked with older NSS 3.x shared libraries will work with NSS 3.12.9 shared libraries
      without recompiling or relinking.  Furthermore, applications that restrict their use of NSS
      APIs to the functions listed in `NSS Public Functions </en-US/ref/nssfunctions.html>`__ will
      remain compatible with future versions of the NSS shared libraries.

   .. container::
      :name: section_11

      .. rubric:: Feedback
         :name: Feedback

      Bugs discovered should be reported by filing a bug report with `mozilla.org
      Bugzilla <https://bugzilla.mozilla.org/>`__ (product NSS).