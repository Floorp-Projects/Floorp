.. _mozilla_projects_nss_nss_3_43_release_notes:

NSS 3.43 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.43 on 16 March 2019, which is a minor
   release.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_43_RTM. NSS 3.43 requires NSPR 4.21 or newer.

   NSS 3.43 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_43_RTM/src/

   Other releases are available :ref:`mozilla_projects_nss_nss_releases`.

.. _new_in_nss_3.43:

`New in NSS 3.43 <#new_in_nss_3.43>`__
--------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. rubric:: New Functions
      :name: new_functions

   -  *in sechash.h*

      -  **HASH_GetHashOidTagByHashType** - convert type HASH_HashType to type SECOidTag

   -  *in sslexp.h*

      -  **SSL_SendCertificateRequest** - allow server to request post-handshake client
         authentication. To use this both peers need to enable the
         **SSL_ENABLE_POST_HANDSHAKE_AUTH** option. Note that while the mechanism is present,
         post-handshake authentication is currently not TLS 1.3 compliant due to `Bug
         1532312 <https://bugzilla.mozilla.org/show_bug.cgi?id=1532312>`__

.. _notable_changes_in_nss_3.43:

`Notable Changes in NSS 3.43 <#notable_changes_in_nss_3.43>`__
--------------------------------------------------------------

.. container::

   -

      .. container:: field indent

         .. container::

            .. container::

               The following CA certificates were **Added**:

      -  CN = emSign Root CA - G1

         -  SHA-256 Fingerprint: 40F6AF0346A99AA1CD1D555A4E9CCE62C7F9634603EE406615833DC8C8D00367

      -  CN = emSign ECC Root CA - G3

         -  SHA-256 Fingerprint: 86A1ECBA089C4A8D3BBE2734C612BA341D813E043CF9E8A862CD5C57A36BBE6B

      -  CN = emSign Root CA - C1

         -  SHA-256 Fingerprint: 125609AA301DA0A249B97A8239CB6A34216F44DCAC9F3954B14292F2E8C8608F

      -  CN = emSign ECC Root CA - C3

         -  SHA-256 Fingerprint: BC4D809B15189D78DB3E1D8CF4F9726A795DA1643CA5F1358E1DDB0EDC0D7EB3

      -  CN = Hongkong Post Root CA 3

         -  SHA-256 Fingerprint: 5A2FC03F0C83B090BBFA40604B0988446C7636183DF9846E17101A447FB8EFD6

   -  The following CA certificates were **Removed**:

      -  None

.. _bugs_fixed_in_nss_3.43:

`Bugs fixed in NSS 3.43 <#bugs_fixed_in_nss_3.43>`__
----------------------------------------------------

.. container::

   -  `Bug 1528669 <https://bugzilla.mozilla.org/show_bug.cgi?id=1528669>`__ and `Bug
      1529308 <https://bugzilla.mozilla.org/show_bug.cgi?id=1529308>`__ - Improve Gyp build system
      handling
   -  `Bug 1529950 <https://bugzilla.mozilla.org/show_bug.cgi?id=1529950>`__ and `Bug
      1521174 <https://bugzilla.mozilla.org/show_bug.cgi?id=1521174>`__ - Improve NSS S/MIME tests
      for Thunderbird
   -  `Bug 1530134 <https://bugzilla.mozilla.org/show_bug.cgi?id=1530134>`__ - If Docker isn't
      installed, try running a local clang-format as a fallback
   -  `Bug 1531267 <https://bugzilla.mozilla.org/show_bug.cgi?id=1531267>`__ - Enable FIPS mode
      automatically if the system FIPS mode flag is set
   -  `Bug 1528262 <https://bugzilla.mozilla.org/show_bug.cgi?id=1528262>`__ - Add a -J option to
      the strsclnt command to specify sigschemes
   -  `Bug 1513909 <https://bugzilla.mozilla.org/show_bug.cgi?id=1513909>`__ - Add manual for
      nss-policy-check
   -  `Bug 1531074 <https://bugzilla.mozilla.org/show_bug.cgi?id=1531074>`__ - Fix a deref after a
      null check in SECKEY_SetPublicValue
   -  `Bug 1517714 <https://bugzilla.mozilla.org/show_bug.cgi?id=1517714>`__ - Properly handle ESNI
      with HRR
   -  `Bug 1529813 <https://bugzilla.mozilla.org/show_bug.cgi?id=1529813>`__ - Expose
      HKDF-Expand-Label with mechanism
   -  `Bug 1535122 <https://bugzilla.mozilla.org/show_bug.cgi?id=1535122>`__ - Align TLS 1.3 HKDF
      trace levels
   -  `Bug 1530102 <https://bugzilla.mozilla.org/show_bug.cgi?id=1530102>`__ - Use getentropy on
      compatible versions of FreeBSD.

   This Bugzilla query returns all the bugs fixed in NSS 3.43:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.43

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.43 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.43 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).