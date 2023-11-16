.. _mozilla_projects_nss_nss_3_16_4_release_notes:

NSS 3.16.4 release notes
========================

`Introduction <#introduction>`__
--------------------------------

.. container::

   Network Security Services (NSS) 3.16.4 is a patch release for NSS 3.16. The bug fixes in NSS
   3.16.4 are described in the "Bugs Fixed" section below.



`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_16_4_RTM. NSS 3.16.4 requires NSPR 4.10.6 or newer.

   NSS 3.16.4 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_16_4_RTM/src/

.. _new_in_nss_3.16.4:

`New in NSS 3.16.4 <#new_in_nss_3.16.4>`__
------------------------------------------

.. container::

   This release consists primarily of CA certificate changes as listed below, and includes a small
   number of bug fixes.

.. _notable_changes_in_nss_3.16.4:

`Notable Changes in NSS 3.16.4 <#notable_changes_in_nss_3.16.4>`__
------------------------------------------------------------------

.. container::

   -  The following **1024-bit** root CA certificate was **restored** to allow more time to develop
      a better transition strategy for affected sites. It was removed in
      :ref:`mozilla_projects_nss_nss_3_16_3_release_notes`, but discussion in the
      mozilla.dev.security.policy forum led to the decision to keep this root included longer in
      order to give website administrators more time to update their web servers.

      -  CN = GTE CyberTrust Global Root

         -  SHA1 Fingerprint: 97:81:79:50:D8:1C:96:70:CC:34:D8:09:CF:79:44:31:36:7E:F4:74

   -  In :ref:`mozilla_projects_nss_nss_3_16_3_release_notes`, the **1024-bit** "Entrust.net Secure
      Server Certification Authority" root CA certificate (SHA1 Fingerprint:
      99:A6:9B:E6:1A:FE:88:6B:4D:2B:82:00:7C:B8:54:FC:31:7E:15:39) was removed. In NSS 3.16.4, a
      **2048-bit** intermediate CA certificate has been included, without explicit trust. The
      intention is to mitigate the effects of the previous removal of the 1024-bit Entrust.net root
      certificate, because many public Internet sites still use the "USERTrust Legacy Secure Server
      CA" intermediate certificate that is signed by the 1024-bit Entrust.net root certificate. The
      inclusion of the intermediate certificate is a temporary measure to allow those sites to
      function, by allowing them to find a trust path to another **2048-bit** root CA certificate.
      The temporarily included intermediate certificate expires November 1, 2015.

.. _bugs_fixed_in_nss_3.16.4:

`Bugs fixed in NSS 3.16.4 <#bugs_fixed_in_nss_3.16.4>`__
--------------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.16.4:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.16.4