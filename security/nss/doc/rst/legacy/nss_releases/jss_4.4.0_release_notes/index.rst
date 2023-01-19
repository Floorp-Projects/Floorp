.. _mozilla_projects_nss_jss_4_4_0_release_notes:

JSS 4.4.0 Release Notes
=======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The Java Security Services (JSS) team has released JSS 4.4.0, which is a minor release.

.. _distribution_information:

`Distribution information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The hg tag is JSS_4_4\ **\_20170313**. JSS 4.4.0 requires Netswork Security Services (NSS) 3.29.1
   and Netscape Portable Runtime (NSPR) 4.13.1 or newer.

   JSS 4.4.0 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      `https://ftp.mozilla.org/pub/mozilla.org/security/jss/releases/JSS_4_4_0_RTM/src/ <https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_30_RTM/src/>`__

.. _new_in_jss_4.40:

`New in JSS 4.40 <#new_in_jss_4.40>`__
--------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. rubric:: New Functions
      :name: new_functions

   New Macros

.. _notable_changes_in_jss_4.40:

`Notable Changes in JSS 4.40 <#notable_changes_in_jss_4.40>`__
--------------------------------------------------------------

.. container::

   -  Picks up work done downstream for Fedora and RHEL and used by various Linux distributions with
      includes:.
   -  Support for IPv6.
   -  Support for TLS v1.1 and TLS v1.2 via NSS though JSS.

.. _bugs_fixed_in_jss_4.4.0:

`Bugs fixed in JSS 4.4.0 <#bugs_fixed_in_jss_4.4.0>`__
------------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 4.4.0:

   https://bugzilla.mozilla.org/buglist.cgi?product=JSS&target_milestone=4.4&target_milestone=4.4&bug_status=RESOLVED&resolution=FIXED

`Documentation <#documentation>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Build instructions for JSS at https://hg.mozilla.org/projects/jss/file/tip/README

.. _platform_information:

`Platform Information <#platform_information>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  You can check out the source from mercurial via hg clone -r  055aa3ce8a61
      https://hg.mozilla.org/projects/jss

   -  JSS 4.4.0 works with OpenJDK versions 1.7 or higher we suggest the latest - OpenJDK 1.8.
   -  JSS 4.4.0 requires :ref:`mozilla_projects_nss_nss_3_12_5_release_notes` or higher though NSS
      3.28.3 is recommended.
   -  JSS 4.3.1 requires `NSPR 4.7.1 <https://www.mozilla.org/projects/nspr/release-notes/>`__ or
      higher though NSPR 3.13 is recommended.
   -  JSS only supports the native threading model (no green threads).

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   JSS 3.30 shared libraries are not backward compatible with all older JSS 4.3.2 shared libraries.
   A program linked with older jSS 4.3.2 shared libraries will not work with JSS 4.4.0 shared
   libraries without recompiling or relinking. Furthermore, applications that restrict their use of
   jSS APIs to the functions listed in JSS Public Functions will remain compatible with future
   versions of the JSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product JSS).