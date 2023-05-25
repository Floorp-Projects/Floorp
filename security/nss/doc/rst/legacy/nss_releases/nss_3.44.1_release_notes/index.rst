.. _mozilla_projects_nss_nss_3_44_1_release_notes:

NSS 3.44.1 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.44.1 is a patch release for NSS 3.44. The bug fixes in NSS
   3.44.1 are described in the "Bugs Fixed" section below. It was released on 21 June 2019.

   The NSS team would like to recognize first-time contributors: Greg Rubin

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_44_1_RTM. NSS 3.44.1 requires NSPR 4.21 or newer.

   NSS 3.44.1 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_44_1_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _new_in_nss_3.44.1:

`New in NSS 3.44.1 <#new_in_nss_3.44.1>`__
------------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -

      .. container::

         `1546229 <https://bugzilla.mozilla.org/show_bug.cgi?id=1546229>`__ - Add IPSEC IKE support
         to softoken

   -

      .. container::

         Many new FIPS test cases (Note: This has increased the source archive by approximately 50
         megabytes for this release.)

.. _bugs_fixed_in_nss_3.44.1:

`Bugs fixed in NSS 3.44.1 <#bugs_fixed_in_nss_3.44.1>`__
--------------------------------------------------------

.. container::

   -

      .. container::

         `1554336 <https://bugzilla.mozilla.org/show_bug.cgi?id=1554336>`__ - Optimize away unneeded
         loop in mpi.c

   -

      .. container::

         `1515342 <https://bugzilla.mozilla.org/show_bug.cgi?id=1515342>`__ - More thorough input
         checking (`CVE-2019-11729) <https://bugzilla.mozilla.org/show_bug.cgi?id=CVE-2019-11729>`__

   -

      .. container::

         `1540541 <https://bugzilla.mozilla.org/show_bug.cgi?id=1540541>`__ - Don't unnecessarily
         strip leading 0's from key material during PKCS11 import
         (`CVE-2019-11719 <https://bugzilla.mozilla.org/show_bug.cgi?id=CVE-2019-11719>`__)

   -

      .. container::

         `1515236 <https://bugzilla.mozilla.org/show_bug.cgi?id=1515236>`__ - Add a SSLKEYLOGFILE
         enable/disable flag at `build.sh <http://build.sh>`__

   -

      .. container::

         `1473806 <https://bugzilla.mozilla.org/show_bug.cgi?id=1473806>`__ - Fix
         SECKEY_ConvertToPublicKey handling of non-RSA keys

   -

      .. container::

         `1546477 <https://bugzilla.mozilla.org/show_bug.cgi?id=1546477>`__ - Updates to testing for
         FIPS validation

   -

      .. container::

         `1552208 <https://bugzilla.mozilla.org/show_bug.cgi?id=1552208>`__ - Prohibit use of
         RSASSA-PKCS1-v1_5 algorithms in TLS 1.3
         (`CVE-2019-11727 <https://bugzilla.mozilla.org/show_bug.cgi?id=CVE-2019-11727>`__)

   -

      .. container::

         `1551041 <https://bugzilla.mozilla.org/show_bug.cgi?id=1551041>`__ - Unbreak build on GCC <
         4.3 big-endian

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.44.1 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.44.1 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).