.. _mozilla_projects_nss_nss_3_15_release_notes:

NSS 3.15 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.15, which is a minor release.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_15_RTM. NSS 3.15 requires NSPR 4.10 or newer.

   NSS 3.15 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_15_RTM/src/

.. _new_in_nss_3.15:

`New in NSS 3.15 <#new_in_nss_3.15>`__
--------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  Support for OCSP Stapling (`RFC 6066 <https://datatracker.ietf.org/doc/html/rfc6066>`__,
      Certificate Status Request) has been added for both client and server sockets. TLS client
      applications may enable this via a call to
      ``SSL_OptionSetDefault(SSL_ENABLE_OCSP_STAPLING, PR_TRUE);``
   -  Added function SECITEM_ReallocItemV2. It replaces function SECITEM_ReallocItem, which is now
      declared as obsolete.
   -  Support for single-operation (eg: not multi-part) symmetric key encryption and decryption, via
      *PK11_Encrypt* and *PK11_Decrypt*.
   -  certutil has been updated to support creating name constraints extensions.

   .. rubric:: New Functions
      :name: new_functions

   -  *in ssl.h*

      -  **SSL_PeerStapledOCSPResponse** - Returns the server's stapled OCSP response, when used
         with a TLS client socket that negotiated the *status_request* extension.
      -  **SSL_SetStapledOCSPResponses** - Set's a stapled OCSP response for a TLS server socket to
         return when clients send the *status_request* extension.

   -  *in ocsp.h*

      -  **CERT_PostOCSPRequest** - Primarily intended for testing, permits the sending and
         receiving of raw OCSP request/responses.

   -  *in secpkcs7.h*

      -  **SEC_PKCS7VerifyDetachedSignatureAtTime** - Verifies a PKCS#7 signature at a specific time
         other than the present time.

   -  *in xconst.h*

      -  **CERT_EncodeNameConstraintsExtension** - Matching function for
         CERT_DecodeNameConstraintsExtension, added in NSS 3.10.

   -  *in secitem.h*

      -  **SECITEM_AllocArray**
      -  **SECITEM_DupArray**
      -  **SECITEM_FreeArray**
      -  **SECITEM_ZfreeArray** - Utility functions to handle the allocation and deallocation of
         *SECItemArray*\ s
      -  **SECITEM_ReallocItemV2** - Replaces *SECITEM_ReallocItem*, which is now obsolete.
         *SECITEM_ReallocItemV2* better matches caller expectations, in that it updates
         ``item->len`` on allocation. For more details of the issues with SECITEM_ReallocItem, see
         `Bug 298649 <http://bugzil.la/298649>`__ and `Bug 298938 <http://bugzil.la/298938>`__.

   -  *in pk11pub.h*

      -  **PK11_Decrypt** - Performs decryption as a single PKCS#11 operation (eg: not multi-part).
         This is necessary for AES-GCM.
      -  **PK11_Encrypt** - Performs encryption as a single PKCS#11 operation (eg: not multi-part).
         This is necessary for AES-GCM.

   .. rubric:: New Types
      :name: new_types

   -  *in secitem.h*

      -  **SECItemArray** - Represents a variable-length array of *SECItem*\ s.

   .. rubric:: New Macros
      :name: new_macros

   -  *in ssl.h*

      -  **SSL_ENABLE_OCSP_STAPLING** - Used with *SSL_OptionSet* to configure TLS client sockets to
         request the *certificate_status* extension (eg: OCSP stapling) when set to **PR_TRUE**

.. _notable_changes_in_nss_3.15:

`Notable Changes in NSS 3.15 <#notable_changes_in_nss_3.15>`__
--------------------------------------------------------------

.. container::

   -  *SECITEM_ReallocItem* is now deprecated. Please consider using *SECITEM_ReallocItemV2* in all
      future code.

   -  NSS has migrated from CVS to the Mercurial source control management system.

      Updated build instructions are available at
      :ref:`mozilla_projects_nss_reference_building_and_installing_nss_migration_to_hg`

      As part of this migration, the source code directory layout has been re-organized.

   -  The list of root CA certificates in the *nssckbi* module has been updated.

   -  The default implementation of SSL_AuthCertificate has been updated to add certificate status
      responses stapled by the TLS server to the OCSP cache.

      Applications that use SSL_AuthCertificateHook to override the default handler should add
      appropriate calls to *SSL_PeerStapledOCSPResponse* and
      *CERT_CacheOCSPResponseFromSideChannel*.

   -  `Bug 554369 <https://bugzilla.mozilla.org/show_bug.cgi?id=554369>`__: Fixed correctness of
      CERT_CacheOCSPResponseFromSideChannel and other OCSP caching behaviour.

   -  `Bug 853285 <https://bugzilla.mozilla.org/show_bug.cgi?id=853285>`__: Fixed bugs in AES GCM.

   -  `Bug 341127 <https://bugzilla.mozilla.org/show_bug.cgi?id=341127>`__: Fix the invalid read in
      rc4_wordconv.

   -  `Faster NIST curve P-256
      implementation <https://bugzilla.mozilla.org/show_bug.cgi?id=831006>`__.

   -  Dropped (32-bit) SPARC V8 processor support on Solaris. The shared library
      ``libfreebl_32int_3.so`` is no longer produced.

.. _bugs_fixed_in_nss_3.15:

`Bugs fixed in NSS 3.15 <#bugs_fixed_in_nss_3.15>`__
----------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.15:

   https://bugzilla.mozilla.org/buglist.cgi?list_id=6278317&resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.15