.. _mozilla_projects_nss_nss_3_64_release_notes:

NSS 3.64 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.64 was released on **15 April 2021**.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_64_RTM. NSS 3.64 requires NSPR 4.30 or newer.

   NSS 3.64 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_64_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _bugs_fixed_in_nss_3.64:

`Bugs fixed in NSS 3.64 <#bugs_fixed_in_nss_3.64>`__
----------------------------------------------------

.. container::

   -  Bug 1705286 - Properly detect mips64.
   -  Bug 1687164 - Introduce NSS_DISABLE_CRYPTO_VSX and disable_crypto_vsx.
   -  Bug 1698320 - replace \__builtin_cpu_supports("vsx") with ppc_crypto_support() for clang.
   -  Bug 1613235 - Add POWER ChaCha20 stream cipher vector acceleration.

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.64 shared libraries are backwards-compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.64 shared libraries
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

   This version of NSS contains a number of contributions for "unsupported platforms". We would like
   to thank the authors and the reviewers for their contributions to NSS.

   Discussions about moving the documentation are still ongoing. (See discussion in the 3.62 release
   notes.)