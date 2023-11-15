.. _mozilla_projects_nss_nss_3_34_1_release_notes:

NSS 3.34.1 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The Network Security Services (NSS) team has released NSS 3.34.1, which is a minor release.



`Distribution information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The hg tag is NSS_3_34.1_RTM. NSS 3.34.1 requires Netscape Portable Runtime (NSPR) 4.17, or
   newer.

   NSS 3.34.1 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_34_1_RTM/src/

.. _notable_changes_in_nss_3.34.1:

`Notable Changes in NSS 3.34.1 <#notable_changes_in_nss_3.34.1>`__
------------------------------------------------------------------

.. container::

   -  The following CA certificate was **Re-Added**. It was previously removed in NSS 3.34, but now
      re-added with only the Email trust bit set. (`bug
      1418678 <https://bugzilla.mozilla.org/show_bug.cgi?id=1418678>`__)

      -  CN = Certum CA, O=Unizeto Sp. z o.o.

         -  SHA-256 Fingerprint:
            D8:E0:FE:BC:1D:B2:E3:8D:00:94:0F:37:D2:7D:41:34:4D:99:3E:73:4B:99:D5:65:6D:97:78:D4:D8:14:36:24

   -  Removed entries from certdata.txt for actively distrusted certificates that have expired (`bug
      1409872 <https://bugzilla.mozilla.org/show_bug.cgi?id=1409872>`__).
   -  The version of the CA list was set to 2.20.

.. _new_in_nss_3.34:

`New in NSS 3.34 <#new_in_nss_3.34>`__
--------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  None

   .. rubric:: New Functions
      :name: new_functions

.. _bugs_fixed_in_nss_3.34.1:

`Bugs fixed in NSS 3.34.1 <#bugs_fixed_in_nss_3.34.1>`__
--------------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.34.1:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.34.1

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.34.1 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.34 shared libraries,
   without recompiling, or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (select product
   'NSS').