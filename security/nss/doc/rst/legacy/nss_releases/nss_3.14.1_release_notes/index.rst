.. _mozilla_projects_nss_nss_3_14_1_release_notes:

NSS 3.14.1 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.14.1 is a patch release for NSS 3.14. The bug fixes in NSS
   3.14.1 are described in the "Bugs Fixed" section below.

   NSS 3.14.1 is licensed under the MPL 2.0.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The CVS tag is NSS_3_14_1_RTM. NSS 3.14.1 requires NSPR 4.9.4 or newer.

   NSS 3.14.1 source distributions are also available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_14_1_RTM/src/

.. _new_in_nss_3.14.1:

`New in NSS 3.14.1 <#new_in_nss_3.14.1>`__
------------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  NSS now has the ability to create signed OCSP responses.

      -  The ability to create signed OCSP responses has been added in NSS 3.14.1. Note that this
         code is used primarily for purposes of testing.

   .. rubric:: New Functions
      :name: new_functions

   -  *in ocspt.h*

      -  CERT_CreateOCSPSingleResponseGood
      -  CERT_CreateOCSPSingleResponseUnknown
      -  CERT_CreateOCSPSingleResponseRevoked
      -  CERT_CreateEncodedOCSPSuccessResponse
      -  CERT_CreateEncodedOCSPErrorResponse

   .. rubric:: New Types
      :name: new_types

   -  *in ocspt.h*

      -  CERTOCSPResponderIDType

.. _notable_changes_in_nss_3.14.1:

`Notable Changes in NSS 3.14.1 <#notable_changes_in_nss_3.14.1>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  Windows CE support has been removed from the code base.
   -  `Bug 812399 <https://bugzilla.mozilla.org/show_bug.cgi?id=812399>`__ - In NSS 3.14, a
      regression caused `Bug 641052 <https://bugzilla.mozilla.org/show_bug.cgi?id=641052>`__ /
      CVE-2011-3640 to be re-introduced under certain situations. This regression only affected
      applications that initialize NSS via the NSS_NoDB_Init function. NSS 3.14.1 includes the
      complete fix for this issue.
   -  `Bug 357025 <https://bugzilla.mozilla.org/show_bug.cgi?id=357025>`__ - NSS 3.14 added support
      for tokens that make use of CKA_ALWAYS_AUTHENTICATE. However, when authenticating with such
      tokens, it was possible for an internal lock to be acquired twice, causing a hang. This hang
      has been fixed in NSS 3.14.1.
   -  `Bug 802429 <https://bugzilla.mozilla.org/show_bug.cgi?id=802429>`__ - In previous versions of
      NSS, the "cipherOrder" slot configuration flag was not respected, causing the most recently
      added slot that supported the requested PKCS#11 mechanism to be used instead. NSS now
      correctly respects the supplied cipherOrder.
      Applications which use multiple PKCS#11 modules, which do not indicate which tokens should be
      used by default for particular algorithms, and which do make use of cipherOrder may now find
      that cryptographic operations occur on a different PKCS#11 token.
   -  `Bug 802429 <https://bugzilla.mozilla.org/show_bug.cgi?id=802429>`__ - The NSS softoken is now
      the default token for SHA-256 and SHA-512. In previous versions of NSS, these algorithms would
      be handled by the most recently added PKCS#11 token that supported them.
   -  `Bug 611451 <https://bugzilla.mozilla.org/show_bug.cgi?id=611451>`__ - When built with the
      current version of Apple XCode on Mac OS X, the NSS shared libraries will now only export the
      public NSS functions.
   -  `Bug 810582 <https://bugzilla.mozilla.org/show_bug.cgi?id=810582>`__ - TLS False Start is now
      only used with servers that negotiate a cipher suite that supports forward secrecy.
      **Note**: The criteria for False Start may change again in future NSS releases.

.. _bugs_fixed_in_nss_3.14.1:

`Bugs fixed in NSS 3.14.1 <#bugs_fixed_in_nss_3.14.1>`__
--------------------------------------------------------

.. container::

   The following Bugzilla query returns all of the bugs fixed in NSS 3.14.1:

   https://bugzilla.mozilla.org/buglist.cgi?list_id=5216669;resolution=FIXED;query_format=advanced;bug_status=RESOLVED;bug_status=VERIFIED;target_milestone=3.14.1;product=NSS

`Compatability <#compatability>`__
----------------------------------

.. container::

   NSS 3.14.1 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.14.1 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered in this release should be reported by filing a bug report at
   https://bugzilla.mozilla.org with the Product of NSS.