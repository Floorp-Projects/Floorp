.. _mozilla_projects_nss_nss_3_16_release_notes:

NSS 3.16 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.16, which is a minor release.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_16_RTM. NSS 3.16 requires NSPR 4.10.3 or newer.

   NSS 3.16 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_16_RTM/src/

.. _new_in_nss_3.16:

`New in NSS 3.16 <#new_in_nss_3.16>`__
--------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  Supports the Linux x32 ABI. (This requires NSPR 4.10.4.) To build for the Linux x32 target,
      set the environment variable USE_X32=1 when building NSS.

   .. rubric:: New Functions
      :name: new_functions

   -  *in cms.h*

      -  **NSS_CMSSignerInfo_Verify** - verify the signature of a single SignerInfo. It just
         verifies the signature, assuming that the certificate has been verified already.

   .. rubric:: New Macros
      :name: new_macros

   -  *in sslproto.h*

      -  **TLS_RSA_WITH_RC4_128_SHA, TLS_RSA_WITH_3DES_EDE_CBC_SHA, etc.** - cipher suites that were
         first defined in SSL 3.0 can now be referred to with their official IANA names in TLS, with
         the TLS\_ prefix. Previously, they had to be referred to with their names in SSL 3.0, with
         the SSL\_ prefix.

.. _notable_changes_in_nss_3.16:

`Notable Changes in NSS 3.16 <#notable_changes_in_nss_3.16>`__
--------------------------------------------------------------

.. container::

   -  ECC is enabled by default. It is no longer necessary to set the environment variable
      NSS_ENABLE_ECC=1 when building NSS. To disable ECC, set the environment variable
      NSS_DISABLE_ECC=1 when building NSS.
   -  `Bug 903885 <https://bugzilla.mozilla.org/show_bug.cgi?id=903885>`__: (CVE-2014-1492) In a
      wildcard certificate, the wildcard character should not be embedded within the U-label of an
      internationalized domain name. See the last bullet point in `RFC 6125, Section
      7.2 <https://datatracker.ietf.org/doc/html/rfc6125#section-7.2>`__.
   -  `Bug 962760 <https://bugzilla.mozilla.org/show_bug.cgi?id=962760>`__: libpkix should not
      include the common name of CA as DNS names when evaluating name constraints.
   -  `Bug 981170 <https://bugzilla.mozilla.org/show_bug.cgi?id=981170>`__: AESKeyWrap_Decrypt
      should not return SECSuccess for invalid keys.
   -  `Bug 974693 <https://bugzilla.mozilla.org/show_bug.cgi?id=974693>`__: Fix a memory corruption
      in sec_pkcs12_new_asafe.
   -  `Bug 956082 <https://bugzilla.mozilla.org/show_bug.cgi?id=956082>`__: If the NSS_SDB_USE_CACHE
      environment variable is set, skip the runtime test sdb_measureAccess.
   -  The built-in roots module has been updated to version 1.97, which adds, removes, and distrusts
      several certificates.
   -  The atob utility has been improved to automatically ignore lines of text that aren't in base64
      format.
   -  The certutil utility has been improved to support creation of version 1 and version 2
      certificates, in addition to the existing version 3 support.

.. _bugs_fixed_in_nss_3.16:

`Bugs fixed in NSS 3.16 <#bugs_fixed_in_nss_3.16>`__
----------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.16:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.16