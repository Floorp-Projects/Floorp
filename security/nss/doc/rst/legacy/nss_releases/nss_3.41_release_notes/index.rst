.. _mozilla_projects_nss_nss_3_41_release_notes:

NSS 3.41 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.41 on 7 December 2018, which is a
   minor release.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_41_RTM. NSS 3.41 requires NSPR 4.20 or newer.

   NSS 3.41 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_41_RTM/src/

.. _new_in_nss_3.41:

`New in NSS 3.41 <#new_in_nss_3.41>`__
--------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `Bug 1252891 <https://bugzilla.mozilla.org/show_bug.cgi?id=1252891>`__ - Implemented EKU
      handling for IPsec IKE.
   -  `Bug 1423043 <https://bugzilla.mozilla.org/show_bug.cgi?id=1423043>`__ - Enable half-closed
      states for TLS.
   -  `Bug 1493215 <https://bugzilla.mozilla.org/show_bug.cgi?id=1493215>`__ - Enabled the following
      ciphersuites by default:

      -  TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384
      -  TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
      -  TLS_DHE_RSA_WITH_AES_256_GCM_SHA384
      -  TLS_RSA_WITH_AES_256_GCM_SHA384

   .. rubric:: New Functions
      :name: new_functions

   -  none

.. _notable_changes_in_nss_3.41:

`Notable Changes in NSS 3.41 <#notable_changes_in_nss_3.41>`__
--------------------------------------------------------------

.. container::

   -  The following CA certificates were **Added**:

      -  CN = Certigna Root CA

         -  SHA-256 Fingerprint: D48D3D23EEDB50A459E55197601C27774B9D7B18C94D5A059511A10250B93168

      -  CN = GTS Root R1

         -  SHA-256 Fingerprint: 2A575471E31340BC21581CBD2CF13E158463203ECE94BCF9D3CC196BF09A5472

      -  CN = GTS Root R2

         -  SHA-256 Fingerprint: C45D7BB08E6D67E62E4235110B564E5F78FD92EF058C840AEA4E6455D7585C60

      -  CN = GTS Root R3

         -  SHA-256 Fingerprint: 15D5B8774619EA7D54CE1CA6D0B0C403E037A917F131E8A04E1E6B7A71BABCE5

      -  CN = GTS Root R4

         -  SHA-256 Fingerprint: 71CCA5391F9E794B04802530B363E121DA8A3043BB26662FEA4DCA7FC951A4BD

      -  CN = UCA Global G2 Root

         -  SHA-256 Fingerprint: 9BEA11C976FE014764C1BE56A6F914B5A560317ABD9988393382E5161AA0493C

      -  CN = UCA Extended Validation Root

         -  SHA-256 Fingerprint: D43AF9B35473755C9684FC06D7D8CB70EE5C28E773FB294EB41EE71722924D24

   -  The following CA certificates were **Removed**:

      -  CN = AC Raíz Certicámara S.A.

         -  SHA-256 Fingerprint: A6C51E0DA5CA0A9309D2E4C0E40C2AF9107AAE8203857FE198E3E769E343085C

      -  CN = Certplus Root CA G1

         -  SHA-256 Fingerprint: 152A402BFCDF2CD548054D2275B39C7FCA3EC0978078B0F0EA76E561A6C7433E

      -  CN = Certplus Root CA G2

         -  SHA-256 Fingerprint: 6CC05041E6445E74696C4CFBC9F80F543B7EABBB44B4CE6F787C6A9971C42F17

      -  CN = OpenTrust Root CA G1

         -  SHA-256 Fingerprint: 56C77128D98C18D91B4CFDFFBC25EE9103D4758EA2ABAD826A90F3457D460EB4

      -  CN = OpenTrust Root CA G2

         -  SHA-256 Fingerprint: 27995829FE6A7515C1BFE848F9C4761DB16C225929257BF40D0894F29EA8BAF2

      -  CN = OpenTrust Root CA G3

         -  SHA-256 Fingerprint: B7C36231706E81078C367CB896198F1E3208DD926949DD8F5709A410F75B6292

.. _bugs_fixed_in_nss_3.41:

`Bugs fixed in NSS 3.41 <#bugs_fixed_in_nss_3.41>`__
----------------------------------------------------

.. container::

   -  `Bug 1412829 <https://bugzilla.mozilla.org/show_bug.cgi?id=1412829>`__, Reject empty
      supported_signature_algorithms in Certificate Request in TLS 1.2

   -  `Bug 1485864 <https://bugzilla.mozilla.org/show_bug.cgi?id=1485864>`__ - Cache side-channel
      variant of the Bleichenbacher attack (CVE-2018-12404)

   -  `Bug 1481271 <https://bugzilla.mozilla.org/show_bug.cgi?id=1481271>`__ - Resend the same
      ticket in ClientHello after HelloRetryRequest

   -  `Bug 1493769 <https://bugzilla.mozilla.org/show_bug.cgi?id=1493769>`__ - Set session_id for
      external resumption tokens

   -  `Bug 1507179 <https://bugzilla.mozilla.org/show_bug.cgi?id=1507179>`__ - Reject CCS after
      handshake is complete in TLS 1.3

   This Bugzilla query returns all the bugs fixed in NSS 3.41:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.41

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.41 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.41 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).