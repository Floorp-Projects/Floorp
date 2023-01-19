.. _mozilla_projects_nss_nss_3_19_release_notes:

NSS 3.19 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.19, which is a minor
   security release.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_19_RTM. NSS 3.19 requires NSPR 4.10.8 or newer.

   NSS 3.19 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_19_RTM/src/

.. _security_fixes_in_nss_3.19:

`Security Fixes in NSS 3.19 <#security_fixes_in_nss_3.19>`__
------------------------------------------------------------

.. container::

   -  `Bug 1086145 <https://bugzilla.mozilla.org/show_bug.cgi?id=1086145>`__ /
      `CVE-2015-2721 <http://www.cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2015-2721>`__ - Fixed a
      bug related to the ordering of TLS handshake messages. This was also known
      as `SMACK <https://www.smacktls.com/>`__.

.. _new_in_nss_3.19:

`New in NSS 3.19 <#new_in_nss_3.19>`__
--------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  For some certificates, such as root CA certificates that don't embed any constraints, NSS
      might impose additional constraints such as name constraints. A new API
      (`CERT_GetImposedNameConstraints <http://mxr.mozilla.org/nss/ident?i=CERT_GetImposedNameConstraints>`__) has
      been added that allows one to lookup imposed constraints.
   -  It is possible to override the directory
      (`SQLITE_LIB_DIR <https://bugzilla.mozilla.org/show_bug.cgi?id=1138820>`__) in which the NSS
      build system will look for the sqlite library.

   .. rubric:: New Functions
      :name: new_functions

   -  *in cert.h*

      -  **CERT_GetImposedNameConstraints** - Check if any imposed constraints exist for the given
         certificate, and if found, return the constraints as encoded certificate extensions.

.. _notable_changes_in_nss_3.19:

`Notable Changes in NSS 3.19 <#notable_changes_in_nss_3.19>`__
--------------------------------------------------------------

.. container::

   -  The SSL 3 protocol has been disabled by default.
   -  NSS now more strictly validates TLS extensions and will fail a handshake that contains
      malformed extensions (`bug 753136 <https://bugzilla.mozilla.org/show_bug.cgi?id=753136>`__).
   -  In TLS 1.2 handshakes, NSS advertises support for the SHA512 hash algorithm in order to be
      compatible with TLS servers that use certificates with a SHA512 signature (`bug
      1155922 <https://bugzilla.mozilla.org/show_bug.cgi?id=1155922>`__).

.. _bugs_fixed_in_nss_3.19:

`Bugs fixed in NSS 3.19 <#bugs_fixed_in_nss_3.19>`__
----------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.19:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.19

`Acknowledgements <#acknowledgements>`__
----------------------------------------

.. container::

   The NSS development team would like to thank Karthikeyan Bhargavan from
   `INRIA <http://inria.fr/>`__ for responsibly disclosing the issue in `bug
   1086145 <https://bugzilla.mozilla.org/show_bug.cgi?id=1086145>`__.

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.19 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.19 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).