.. _mozilla_projects_nss_nss_3_26_release_notes:

NSS 3.26 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The Network Security Services (NSS) team has released NSS 3.26, which is a minor release.



`Distribution information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The hg tag is NSS_3_26_RTM. NSS 3.26 requires Netscape Portable Runtime(NSPR) 4.12 or newer.

   NSS 3.26 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_26_RTM/src/

.. _new_in_nss_3.26:

`New in NSS 3.26 <#new_in_nss_3.26>`__
--------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  the selfserv test utility has been enhanced to support ALPN (HTTP/1.1) and 0-RTT
   -  added support for the System-wide crypto policy available on Fedora Linux, see
      http://fedoraproject.org/wiki/Changes/CryptoPolicy
   -  introduced build flag NSS_DISABLE_LIBPKIX which allows compilation of NSS without the libpkix
      library

.. _notable_changes_in_nss_3.26:

`Notable Changes in NSS 3.26 <#notable_changes_in_nss_3.26>`__
--------------------------------------------------------------

.. container::

   -  The following CA certificate was **Added**

      -  CN = ISRG Root X1

         -  SHA-256 Fingerprint:
            96:BC:EC:06:26:49:76:F3:74:60:77:9A:CF:28:C5:A7:CF:E8:A3:C0:AA:E1:1A:8F:FC:EE:05:C0:BD:DF:08:C6

   -  NPN is disabled, and ALPN is enabled by default
   -  the NSS test suite now completes with the experimental TLS 1.3 code enabled
   -  several test improvements and additions, including a NIST known answer test

.. _bugs_fixed_in_nss_3.26:

`Bugs fixed in NSS 3.26 <#bugs_fixed_in_nss_3.26>`__
----------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.26:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.26

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.26 shared libraries are backwards compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.26 shared libraries
   without recompiling or relinking. Applications that restrict their use of NSS APIs, to the
   functions listed in NSS Public Functions, will remain compatible with future versions of the NSS
   shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).