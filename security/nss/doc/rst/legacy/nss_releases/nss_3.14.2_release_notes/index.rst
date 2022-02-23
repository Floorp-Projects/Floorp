.. _mozilla_projects_nss_nss_3_14_2_release_notes:

NSS 3.14.2 release notes
========================

.. container::

   Network Security Services (NSS) 3.14.2 is a patch release for NSS 3.14. The bug fixes in NSS
   3.14.2 are described in the "Bugs Fixed" section below. NSS 3.14.2 should be used with NSPR 4.9.5
   or newer.

   The release is available for download from
   https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_14_2_RTM/src/

   For the primary NSS documentation pages please visit :ref:`mozilla_projects_nss`

.. _new_in_nss_3.14.2:

`New in NSS 3.14.2 <#new_in_nss_3.14.2>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  NSS will now make use of the Intel AES-NI and AVX instruction sets for hardware-accelerated
      AES-GCM on 64-bit Linux systems. Note: the new assembly code requires GNU as version 2.19 or
      newer. On Red Hat Enterprise Linux 5.x systems, install the binutils220 package and add
      /usr/libexec/binutils220 to the beginning of your PATH environment variable.
   -  Initial manual pages for some NSS command line tools have been added. They are still under
      review, and contributions are welcome. The documentation is in the docbook format and can be
      rendered as HTML and UNIX-style manual pages using an optional build target.

   .. rubric:: New Types:
      :name: new_types

   -  in certt.h

      -  ``cert_pi_useOnlyTrustAnchors``

   -  in secoidt.h

      -  ``SEC_OID_MS_EXT_KEY_USAGE_CTL_SIGNING``

.. _notable_changes_in_nss_3.14.2:

`Notable Changes in NSS 3.14.2 <#notable_changes_in_nss_3.14.2>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  Bug 805604 - Support for AES-NI and AVX accelerated AES-GCM was contributed by Shay Gueron of
      Intel. If compiled on Linux systems in 64-bit mode, NSS will include runtime detection to
      check if the platform supports AES-NI and PCLMULQDQ. If so, NSS uses the optimized code path,
      reducing the CPU cycles per byte to 1/20 of what was required before the patch
      (https://bugzilla.mozilla.org/show_bug.cgi?id=805604 and
      https://crypto.stanford.edu/RealWorldCrypto/slides/gueron.pdf). Support for other platforms,
      such as Windows, will follow in a future NSS release.
      (https://bugzilla.mozilla.org/show_bug.cgi?id=540986)
   -  SQLite has been updated to 3.7.15. Note: please apply the patch in
      https://bugzilla.mozilla.org/show_bug.cgi?id=837799 if you build NSS with the system SQLite
      library and your system SQLite library is older than 3.7.15.
   -  Bug 816853 - When using libpkix for certificate validation, applications may now supply
      additional application-defined trust anchors to be used in addition to those from loaded
      security tokens, rather than as an alternative to.
      (https://bugzilla.mozilla.org/show_bug.cgi?id=816853)
   -  Bug 772144 - Basic support for running NSS test suites on Android devices.This is currently
      limited to running tests from a Linux host machine using an SSH connection. Only the SSHDroid
      app has been tested.
   -  Bug 373108 - Fixed a bug where, under certain circumstances, when applications supplied
      invalid/out-of-bounds parameters for AES encryption, a double free may occur.
   -  Bug 813857 - Modification of certificate trust flags from multiple threads is now a
      thread-safe operation.
   -  Bug 618418 - C_Decrypt/C_DecryptFinal now correctly validate the PKCS #7 padding when present.
   -  Bug 807890 - Added support for Microsoft Trust List Signing EKU.
   -  Bug 822433 - Fixed a crash in dtls_FreeHandshakeMessages.
   -  Bug 823336 - Reject invalid LDAP AIA URIs sooner.

.. _bugs_fixed_in_nss_3.14.2:

`Bugs Fixed in NSS 3.14.2 <#bugs_fixed_in_nss_3.14.2>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  https://bugzilla.mozilla.org/buglist.cgi?list_id=5502456;resolution=FIXED;classification=Components;query_format=advanced;target_milestone=3.14.2;product=NSS

`Compatibility <#compatibility>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   NSS 3.14.2 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.14.2 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <http://bugzilla.mozilla.org/>`__ (product NSS).