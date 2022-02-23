.. _mozilla_projects_nss_nss_3_29_1_release_notes:

NSS 3.29.1 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.29.1 is a patch release for NSS 3.29. The bug fixes in NSS
   3.29.1 are described in the "Bugs Fixed" section below.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_29_1_RTM. NSS 3.29.1 requires Netscape Portable Runtime(NSPR) 4.13.1 or
   newer.

   NSS 3.29.1 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/security/nss/releases/NSS_3_29_1_RTM/src/

.. _new_in_nss_3.29.1:

`New in NSS 3.29.1 <#new_in_nss_3.29.1>`__
------------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   No new functionality is introduced in this release. This is a patch release to fix binary
   compatibility issues.

.. _bugs_fixed_in_nss_3.29.1:

`Bugs fixed in NSS 3.29.1 <#bugs_fixed_in_nss_3.29.1>`__
--------------------------------------------------------

.. container::

   NSS version 3.28, 3.28.1, 3.28.2 and 3.29 contained changes that were in violation with the NSS
   compatibility promise.

   ECParams, which is part of the public API of the freebl/softokn parts of NSS, had been changed to
   include an additional attribute. That size increase caused crashes or malfunctioning with
   applications that use that data structure directly, or indirectly through ECPublicKey,
   ECPrivateKey, NSSLOWKEYPublicKey, NSSLOWKEYPrivateKey, or potentially other data structures that
   reference ECParams. The change has been reverted to the original state in `bug
   1334108 <https://bugzilla.mozilla.org/show_bug.cgi?id=1334108>`__.

   SECKEYECPublicKey had been extended with a new attribute, named "encoding". If an application
   passed type SECKEYECPublicKey to NSS (as part of SECKEYPublicKey), the NSS library read the
   uninitialized attribute. With this NSS release SECKEYECPublicKey.encoding is deprecated. NSS no
   longer reads the attribute, and will always set it to ECPoint_Undefined. See `bug
   1340103 <https://bugzilla.mozilla.org/show_bug.cgi?id=1340103>`__.

   Note that NSS 3.28.3 from the older NSS 3.28.x branch
   :ref:`mozilla_projects_nss_nss_3_28_3_release_notes` with the identical fixes.

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.29.1 shared libraries are backward compatible with most older NSS 3.x shared libraries, but
   depending on your application, may be incompatible, if you application has been compiled against
   header files of versions 3.28, 3.28.1, 3.28.2 NSS 3.29.1.

   A program linked with most older NSS 3.x shared libraries (excluding the exceptions mentioned
   above), will work with NSS 3.29.1 shared libraries without recompiling or relinking. Furthermore,
   applications that restrict their use of NSS APIs to the functions listed in NSS Public Functions
   will remain compatible with future versions of the NSS shared libraries.

   If you had compiled your application against header files of NSS 3.28, NSS 3.28.1, NSS 3.28.2 or
   NSS 3.29, it is recommended that you recompile your application against NSS 3.29.1 (or NSS
   3.28.3), at the time you upgrade to NSS 3.29.1 (or NSS 3.28.3).

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).