.. _mozilla_projects_nss_nss_3_63_release_notes:

NSS 3.63 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.63 was released on **18 March 2021**.



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_63_RTM. NSS 3.63 requires NSPR 4.30 or newer.

   NSS 3.63 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_63_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _bugs_fixed_in_nss_3.63:

`Bugs fixed in NSS 3.63 <#bugs_fixed_in_nss_3.63>`__
----------------------------------------------------

.. container::

   -  Bug 1688374 - Fix parallel build NSS-3.61 with make.
   -  Bug 1697380 - Make a clang-format run on top of helpful contributions.
   -  Bug 1683520 - ECCKiila P384, change syntax of nested structs initialization to prevent build
      isses with GCC 4.8.
   -  Bug 1683520 - [lib/freebl/ecl] P-384: allow zero scalars in dual scalar multiplication.
   -  Bug 1683520 - ECCKiila P521, change syntax of nested structs initialization to prevent build
      isses with GCC 4.8.
   -  Bug 1683520 - [lib/freebl/ecl] P-521: allow zero scalars in dual scalar multiplication.
   -  Bug 1696800 - HACL\* update March 2021 - c95ab70fcb2bc21025d8845281bc4bc8987ca683.
   -  Bug 1694214 - tstclnt can't enable middlebox compat mode.
   -  Bug 1694392 - NSS does not work with PKCS #11 modules not supporting profiles.
   -  Bug 1685880 - Minor fix to prevent unused variable on early return.
   -  Bug 1685880 - Fix for the gcc compiler version 7 to support setenv with nss build.
   -  Bug 1693217 - Increase nssckbi.h version number for March 2021 batch of root CA changes, CA
      list version 2.48.
   -  Bug 1692094 - Set email distrust after to 21-03-01 for Camerfirma's 'Chambers of Commerce' and
      'Global Chambersign' roots.
   -  Bug 1618407 - Symantec root certs - Set CKA_NSS_EMAIL_DISTRUST_AFTER.
   -  Bug 1693173 - Add GlobalSign R45, E45, R46, and E46 root certs to NSS.
   -  Bug 1683738 - Add AC RAIZ FNMT-RCM SERVIDORES SEGUROS root cert to NSS.
   -  Bug 1686854 - Remove GeoTrust PCA-G2 and VeriSign Universal root certs from NSS.
   -  Bug 1687822 - Turn off Websites trust bit for the “Staat der Nederlanden Root CA - G3” root
      cert in NSS.
   -  Bug 1692094 - Turn off Websites Trust Bit for 'Chambers of Commerce Root - 2008' and 'Global
      Chambersign Root - 2008’.
   -  Bug 1694291 - Tracing fixes for ECH.

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.63 shared libraries are backwards-compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.63 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report on
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).

`Notes <#notes>`__
------------------

.. container::

   This version of NSS contains a significant update to the root CAs.

   Discussions about moving the documentation are still ongoing. (See discussion in the 3.62 release
   notes.)