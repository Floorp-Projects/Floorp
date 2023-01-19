.. _mozilla_projects_nss_nss_3_18_1_release_notes:

NSS 3.18.1 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.18.1 is a patch release for NSS 3.18. The bug fixes in NSS
   3.18.1 are described in the "Bugs Fixed" section below.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_18_1_RTM. NSS 3.18.1 requires NSPR 4.10.8 or newer.

   NSS 3.18.1 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_18_1_RTM/src/

.. _new_in_nss_3.18.1:

`New in NSS 3.18.1 <#new_in_nss_3.18.1>`__
------------------------------------------

.. container::

   No new functionality is introduced in this release. This is a patch release to update the list of
   root CA certificates.

.. _notable_changes_in_nss_3.18.1:

`Notable Changes in NSS 3.18.1 <#notable_changes_in_nss_3.18.1>`__
------------------------------------------------------------------

.. container::

   -  The following CA certificate had the Websites and Code Signing trust **bits restored to their
      original state** to allow more time to develop a better transition strategy for affected
      sites. The Websites and Code Signing trust bits were turned off in
      :ref:`mozilla_projects_nss_nss_3_18_release_notes`. But when Firefox 38 went into Beta, there
      was a huge spike in the number of certificate verification errors attributed to this change.
      So, to give website administrators more time to update their web servers, we reverted the
      trust bits back to being enabled.

      -  OU = Equifax Secure Certificate Authority

         -  SHA1 Fingerprint: D2:32:09:AD:23:D3:14:23:21:74:E4:0D:7F:9D:62:13:97:86:63:3A

   -  The following CA certificate was **removed** after `discussion about
      it <https://groups.google.com/d/msg/mozilla.dev.security.policy/LKJO9W5dkSY/9VjSJhRfraIJ>`__
      in the mozilla.dev.security.policy forum\ **.**

      -  CN = e-Guven Kok Elektronik Sertifika Hizmet Saglayicisi

         -  SHA1 Fingerprint: DD:E1:D2:A9:01:80:2E:1D:87:5E:84:B3:80:7E:4B:B1:FD:99:41:34

   -  The following intermediate CA certificate has been added as `actively
      distrusted <https://wiki.mozilla.org/CA:MaintenanceAndEnforcement#Actively_Distrusting_a_Certificate>`__
      because it was
      `misused <https://blog.mozilla.org/security/2015/04/02/distrusting-new-cnnic-certificates/>`__ to
      issue certificates for domain names the holder did not own or control.

      -  CN=MCSHOLDING TEST, O=MCSHOLDING, C=EG

         -  SHA1 Fingerprint: E1:F3:59:1E:76:98:65:C4:E4:47:AC:C3:7E:AF:C9:E2:BF:E4:C5:76

   -  The version number of the updated root CA list has been set to 2.4

.. _bugs_fixed_in_nss_3.18.1:

`Bugs fixed in NSS 3.18.1 <#bugs_fixed_in_nss_3.18.1>`__
--------------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.18.1:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.18.1

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.18.1 shared libraries are backward compatible with all older NSS 3.18 shared libraries. A
   program linked with older NSS 3.18 shared libraries will work with NSS 3.18.1 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).