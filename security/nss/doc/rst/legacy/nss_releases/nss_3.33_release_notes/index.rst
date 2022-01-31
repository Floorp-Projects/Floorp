.. _mozilla_projects_nss_nss_3_33_release_notes:

NSS 3.33 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The Network Security Services (NSS) team has released NSS 3.33, which is a minor release.

.. _distribution_information:

`Distribution information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The hg tag is NSS_3_33_RTM. NSS 3.33 requires Netscape Portable Runtime (NSPR) 4.17, or newer.

   NSS 3.33 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_33_RTM/src/

.. _notable_changes_in_nss_3.33:

`Notable Changes in NSS 3.33 <#notable_changes_in_nss_3.33>`__
--------------------------------------------------------------

.. container::

   -  TLS compression is no longer supported. API calls that attempt to enable compression are
      accepted without failure. However, TLS compression will remain disabled.
   -  This version of NSS uses a `formally verified
      implementation <https://blog.mozilla.org/security/2017/09/13/verified-cryptography-firefox-57/>`__
      of Curve25519 on 64-bit systems.
   -  The compile time flag DISABLE_ECC has been removed.
   -  When NSS is compiled without NSS_FORCE_FIPS=1 startup checks are no longer performed.
   -  Fixes CVE-2017-7805, a potential use-after-free in TLS 1.2 server, when verifying client
      authentication.
   -  Various minor improvements and correctness fixes.

.. _new_in_nss_3.33:

`New in NSS 3.33 <#new_in_nss_3.33>`__
--------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  When listing an NSS database, using certutil -L, and the database hasn't yet been initialized
      with any non-empty or empty password, the text "Database needs user init" will be included in
      the listing.
   -  When using certutil to set an inacceptable password in FIPS mode, a correct explanation of
      acceptable passwords will be printed.

   .. rubric:: New Functions
      :name: new_functions

   -  *in cert.h*

      -  **CERT_FindCertByIssuerAndSNCX** - a variation of existing function
         CERT_FindCertByIssuerAndSN that accepts an additional password context parameter.
      -  **CERT_FindCertByNicknameOrEmailAddrCX** - a variation of existing function
         CERT_FindCertByNicknameOrEmailAddr that accepts an additional password context parameter.
      -  **CERT_FindCertByNicknameOrEmailAddrForUsageCX** - a variation of existing function
         CERT_FindCertByNicknameOrEmailAddrForUsage that accepts an additional password context
         parameter.

   -  *in secport.h*

      -  **NSS_SecureMemcmpZero** - check if a memory region is all zero in constant time.
      -  **PORT_ZAllocAligned** - allocate aligned memory.
      -  **PORT_ZAllocAlignedOffset** - allocate aligned memory for structs.

   -  *in ssl.h*

      -  **SSL_GetExperimentalAPI** - access experimental APIs in libssl.

.. _bugs_fixed_in_nss_3.33:

`Bugs fixed in NSS 3.33 <#bugs_fixed_in_nss_3.33>`__
----------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.33:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.33

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.33 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.33 shared libraries,
   without recompiling, or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (select product
   'NSS').