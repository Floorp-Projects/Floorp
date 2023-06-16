.. _mozilla_projects_nss_nss_3_14_3_release_notes:

NSS 3.14.3 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.14.3 is a patch release for NSS 3.14. The bug fixes in NSS
   3.14.3 are described in the "Bugs Fixed" section below.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The CVS tag is NSS_3_14_3_RTM. NSS 3.14.3 requires NSPR 4.9.5 or newer.

   NSS 3.14.3 source distributions are also available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_14_3_RTM/src/

.. _new_in_nss_3.14.3:

`New in NSS 3.14.3 <#new_in_nss_3.14.3>`__
------------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  No new major functionality is introduced in this release. This release is a patch release to
      address `CVE-2013-1620 <http://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2013-1620>`__.

   .. rubric:: New Functions
      :name: new_functions

   -  *in pk11pub.h*

      -  **PK11_SignWithSymKey** - Similar to PK11_Sign, performs a signing operation in a single
         operation. However, unlike PK11_Sign, which uses a *SECKEYPrivateKey*, PK11_SignWithSymKey
         performs the signature using a symmetric key, such as commonly used for generating MACs.

   .. rubric:: New Types
      :name: new_types

   -  *CK_NSS_MAC_CONSTANT_TIME_PARAMS* - Parameters for use with *CKM_NSS_HMAC_CONSTANT_TIME* and
      *CKM_NSS_SSL3_MAC_CONSTANT_TIME*.

   .. rubric:: New PKCS #11 Mechanisms
      :name: new_pkcs_11_mechanisms

   -  *CKM_NSS_HMAC_CONSTANT_TIME* - Constant-time HMAC operation for use when verifying a padded,
      MAC-then-encrypted block of data.
   -  *CKM_NSS_SSL3_MAC_CONSTANT_TIME* - Constant-time MAC operation for use when verifying a
      padded, MAC-then-encrypted block of data using the SSLv3 MAC.

.. _notable_changes_in_nss_3.14.3:

`Notable Changes in NSS 3.14.3 <#notable_changes_in_nss_3.14.3>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `CVE-2013-1620 <http://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2013-1620>`__

      Recent research by Nadhem AlFardan and Kenny Patterson has highlighted a weakness in the
      handling of CBC padding as used in SSL, TLS, and DTLS that allows an attacker to exploit
      timing differences in MAC processing. The details of their research and the attack can be
      found at http://www.isg.rhul.ac.uk/tls/, and has been referred to as "Lucky Thirteen".

      NSS 3.14.3 includes changes to the *softoken* and *ssl* libraries to address and mitigate
      these attacks, contributed by Adam Langley of Google. This attack is mitigated when using NSS
      3.14.3 with an NSS Cryptographic Module ("softoken") version 3.14.3 or later. However, this
      attack is only partially mitigated if NSS 3.14.3 is used with the current FIPS validated `NSS
      Cryptographic
      Module <http://csrc.nist.gov/groups/STM/cmvp/documents/140-1/1401val2012.htm#1837>`__, version
      3.12.9.1.

   -  `Bug 840714 <https://bugzilla.mozilla.org/show_bug.cgi?id=840714>`__ - "certutil -a" was not
      correctly producing ASCII output as requested.

   -  `Bug 837799 <https://bugzilla.mozilla.org/show_bug.cgi?id=837799>`__ - NSS 3.14.2 broke
      compilation with older versions of sqlite that lacked the SQLITE_FCNTL_TEMPFILENAME file
      control. NSS 3.14.3 now properly compiles when used with older versions of sqlite.

`Acknowledgements <#acknowledgements>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The NSS development team would like to thank Nadhem AlFardan and Kenny Patterson (Royal Holloway,
   University of London) for responsibly disclosing the issue by providing advance copies of their
   research. In addition, thanks to Adam Langley (Google) for the development of a mitigation for
   the issues raised in the paper, along with Emilia Kasper and Bodo Möller (Google) for assisting
   in the review and improvements to the initial patches.

.. _bugs_fixed_in_nss_3.14.3:

`Bugs fixed in NSS 3.14.3 <#bugs_fixed_in_nss_3.14.3>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  https://bugzilla.mozilla.org/buglist.cgi?list_id=5689256;resolution=FIXED;classification=Components;query_format=advanced;target_milestone=3.14.3;product=NSS

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.14.3 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.14.3 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).