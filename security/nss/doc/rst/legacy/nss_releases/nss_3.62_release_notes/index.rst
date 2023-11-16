.. _mozilla_projects_nss_nss_3_62_release_notes:

NSS 3.62 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team released Network Security Services (NSS) 3.62 on **19 February 2021**, which is a
   minor release.



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_62_RTM. NSS 3.62 requires NSPR 4.29 or newer.

   NSS 3.62 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_62_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _bugs_fixed_in_nss_3.62:

`Bugs fixed in NSS 3.62 <#bugs_fixed_in_nss_3.62>`__
----------------------------------------------------

.. container::

   -  Bug 1688374 - Fix parallel build NSS-3.61 with make.
   -  Bug 1682044 - pkix_Build_GatherCerts() + pkix_CacheCert_Add() can corrupt "cachedCertTable".
   -  Bug 1690583 - Fix CH padding extension size calculation.
   -  Bug 1690421 - Adjust 3.62 ABI report formatting for new libabigail.
   -  Bug 1690421 - Install packaged libabigail in docker-builds image.
   -  Bug 1689228 - Minor ECH -09 fixes for interop testing, fuzzing.
   -  Bug 1674819 - Fixup a51fae403328, enum type may be signed.
   -  Bug 1681585 - Add ECH support to selfserv.
   -  Bug 1681585 - Update ECH to Draft-09.
   -  Bug 1678398 - Add Export/Import functions for HPKE context.
   -  Bug 1678398 - Update HPKE to draft-07.

   This Bugzilla query returns all the bugs fixed in NSS 3.62:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.62

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.62 shared libraries are backwards-compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.62 shared libraries
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

   Due to changes to MDN, we have been notified that the NSS documentation will have to move off of
   MDN. It is not fully clear yet, but the proposed solution is to move the documentation in-tree
   (nss/docs), to the md/sphinx format, and have it either rendered as a sub-section of the Firefox
   source docs or as a standalone website. More information will follow in the NSS 3.63 notes.

   Regarding the Release day, in order to organize release process better and avoid issues, we will
   likely move the release day to Thursdays. Please take a look at the release calendar for the
   exact dates.