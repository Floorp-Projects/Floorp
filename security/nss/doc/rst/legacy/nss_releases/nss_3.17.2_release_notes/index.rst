.. _mozilla_projects_nss_nss_3_17_2_release_notes:

NSS 3.17.2 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.17.2 is a patch release for NSS 3.17. The bug fixes in NSS
   3.17.2 are described in the "Bugs Fixed" section below.



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_17_2_RTM. NSS 3.17.2 requires NSPR 4.10.7 or newer.

   NSS 3.17.2 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_17_2_RTM/src/

.. _new_in_nss_3.17.2:

`New in NSS 3.17.2 <#new_in_nss_3.17.2>`__
------------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   No new functionality is introduced in this release. This is a patch release to fix a regression
   and other bugs.

.. _notable_changes_in_nss_3.17.2:

`Notable Changes in NSS 3.17.2 <#notable_changes_in_nss_3.17.2>`__
------------------------------------------------------------------

.. container::

   -  `Bug 1049435 <https://bugzilla.mozilla.org/show_bug.cgi?id=1049435>`__: Change
      RSA_PrivateKeyCheck to not require p > q. This fixes a regression introduced in NSS 3.16.2
      that prevented NSS from importing some RSA private keys (such as in PKCS #12 files) generated
      by other crypto libraries.
   -  `Bug 1057161 <https://bugzilla.mozilla.org/show_bug.cgi?id=1057161>`__: Check that an imported
      elliptic curve public key is valid. Previously NSS would only validate the peer's public key
      before performing ECDH key agreement. Now EC public keys are validated at import time.
   -  `Bug 1078669 <https://bugzilla.mozilla.org/show_bug.cgi?id=1078669>`__: certutil crashes when
      an argument is passed to the --certVersion option.

.. _bugs_fixed_in_nss_3.17.2:

`Bugs fixed in NSS 3.17.2 <#bugs_fixed_in_nss_3.17.2>`__
--------------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.17.2:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.17.2

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.17.2 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.17.2 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).