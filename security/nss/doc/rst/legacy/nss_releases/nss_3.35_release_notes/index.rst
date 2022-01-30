.. _mozilla_projects_nss_nss_3_35_release_notes:

NSS 3.35 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.35, which is a minor release.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_35_RTM. NSS 3.35 requires NSPR 4.18, or newer.

   NSS 3.35 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_35_RTM/src/

.. _new_in_nss_3.35:

`New in NSS 3.35 <#new_in_nss_3.35>`__
--------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  TLS 1.3 support has been updated to draft -23. This includes a large number of changes since
      3.34, which supported only draft -18. See below for details.

   .. rubric:: New Types
      :name: new_types

   -  *in sslt.h*

      -  **SSLHandshakeType** - The type of a TLS handshake message.
      -  For the **SSLSignatureScheme** enum, the enumerated values ssl_sig_rsa_pss_sha\* are
         deprecated in response to a change in TLS 1.3.  Please use the equivalent
         ssl_sig_rsa_pss_rsae_sha\* for rsaEncryption keys, or ssl_sig_rsa_pss_pss_sha\* for PSS
         keys. Note that this release does not include support for the latter.

.. _notable_changes_in_nss_3.35:

`Notable Changes in NSS 3.35 <#notable_changes_in_nss_3.35>`__
--------------------------------------------------------------

.. container::

   -  Previously, NSS used the DBM file format by default. Starting with version 3.35, NSS uses the
      SQL file format by default. Below, are explanations that could be helpful for environments
      that need to adopt to the new default.

      -  If NSS is initialized, in read-write mode with a database directory provided, it uses
         database files to store certificates, key, trust, and other information. NSS supports two
         different database file formats:

         -  DBM: The legacy file format, based on Berkeley DB, using filenames cert8.db, key3.db and
            secmod.db. Parallel database access, by multiple applications, is forbidden as it will
            likely result in data corruption.
         -  SQL: The newer file format, based on SQLite, using filenames cert9.db, key4.db and
            pkcs11.txt. Parallel database access, by multiple applications, is supported.

      -  Applications using NSS may explicitly request to use a specific database format, by adding
         a type prefix to the database directory, provided at NSS initialization time. Without a
         prefix, the default database type will be used (DBM in versions prior to 3.35, and SQL in
         version 3.35 and later.)
      -  When using the SQL type (either explicitly, or because of the new default), with a database
         directory which already contains a DBM type database, NSS will automatically perform a one
         time migration of the information contained in the DBM files to the newer SQL files. If a
         master password was set on the DBM database, then the initial migration may be partial, and
         migration of keys from DBM to SQL will be delayed, until this master password is provided
         to NSS. (Conversely, NSS will never synchronize data from SQL to DBM format.)
      -  Additional information can be found on this Fedora Linux project page:
         https://fedoraproject.org/wiki/Changes/NSSDefaultFileFormatSql

   -  Added formally verified implementations of non-vectorized Chacha20 and non-vectorized Poly1305
      64-bit.
   -  For stronger security, when creating encrypted PKCS#7 or PKCS#12 data, the iteration count for
      the password based encryption algorithm has been increased to one million iterations. Note
      that debug builds will use a lower count, for better performance in test environments. As a
      reminder, debug builds should not be used for production purposes.
   -  NSS 3.30 had introduced a regression, preventing NSS from reading some AES encrypted data,
      produced by older versions of NSS. NSS 3.35 fixes this regression and restores the ability to
      read affected data.
   -  The following CA certificates were **Removed**:

      -  OU = Security Communication EV RootCA1

         -  SHA-256 Fingerprint:
            A2:2D:BA:68:1E:97:37:6E:2D:39:7D:72:8A:AE:3A:9B:62:96:B9:FD:BA:60:BC:2E:11:F6:47:F2:C6:75:FB:37

      -  CN = CA Disig Root R1

         -  SHA-256 Fingerprint:
            F9:6F:23:F4:C3:E7:9C:07:7A:46:98:8D:5A:F5:90:06:76:A0:F0:39:CB:64:5D:D1:75:49:B2:16:C8:24:40:CE

      -  CN = DST ACES CA X6

         -  SHA-256 Fingerprint:
            76:7C:95:5A:76:41:2C:89:AF:68:8E:90:A1:C7:0F:55:6C:FD:6B:60:25:DB:EA:10:41:6D:7E:B6:83:1F:8C:40

      -  Subject CN = VeriSign Class 3 Secure Server CA - G2

         -  SHA-256 Fingerprint:
            0A:41:51:D5:E5:8B:84:B8:AC:E5:3A:5C:12:12:2A:C9:59:CD:69:91:FB:B3:8E:99:B5:76:C0:AB:DA:C3:58:14
         -  This intermediate cert had been directly included to help with transition from 1024-bit
            roots per `Bug #1045189 <https://bugzilla.mozilla.org/show_bug.cgi?id=1045189>`__.

   -  The Websites (TLS/SSL) trust bit was turned **off** for the following CA certificates:

      -  CN = Chambers of Commerce Root

         -  SHA-256 Fingerprint:
            0C:25:8A:12:A5:67:4A:EF:25:F2:8B:A7:DC:FA:EC:EE:A3:48:E5:41:E6:F5:CC:4E:E6:3B:71:B3:61:60:6A:C3

      -  CN = Global Chambersign Root

         -  SHA-256 Fingerprint:
            EF:3C:B4:17:FC:8E:BF:6F:97:87:6C:9E:4E:CE:39:DE:1E:A5:FE:64:91:41:D1:02:8B:7D:11:C0:B2:29:8C:ED

   -  Significant changes to TLS 1.3 were made, along with the update from draft -18 to draft -23:

      -  Support for KeyUpdate was added.  KeyUpdate will be used automatically, if a cipher is used
         for a sufficient number of records.
      -  SSL_KEYLOGFILE support was updated for TLS 1.3.
      -  An option to enable TLS 1.3 compatibility mode, SSL_ENABLE_TLS13_COMPAT_MODE, was added.
      -  Note: In this release, support for new rsa_pss_pss_shaX signature schemes have been
         disabled; end-entity certificates with RSA-PSS keys will still be used to produce
         signatures, but they will use the rsa_pss_rsae_shaX codepoints.
      -  Note: The value of ssl_tls13_key_share_xtn value, from the SSLExtensionType, has been
         renumbered to match changes in TLS 1.3. This is not expected to cause problems; code
         compiled against previous versions of TLS will now refer to an unsupported codepoint, if
         this value was used.  Recompilation should correct any mismatches.
      -  Note: DTLS support is promoted in draft -23, but this is currently not compliant with the
         DTLS 1.3 draft -23 specification.

   -  TLS servers are able to handle a ClientHello statelessly, if the client supports TLS 1.3.  If
      the server sends a HelloRetryRequest, it is possible to discard the server socket, and make a
      new socket to handle any subsequent ClientHello.  This better enables stateless server
      operation.  (This feature is added in support of QUIC, but it also has utility for DTLS 1.3
      servers.)
   -  The tstclnt utility now supports DTLS, using the -P option.  Note that a DTLS server is also
      provided in tstclnt.
   -  TLS compression is no longer possible with NSS.  The option can be enabled, but NSS will no
      longer negotiate compression.
   -  The signatures of functions SSL_OptionSet, SSL_OptionGet, SSL_OptionSetDefault and
      SSL_OptionGetDefault have been modified, to take a PRIntn argument rather than PRBool.  This
      makes it clearer, that options can have values other than 0 or 1.  Note this does not affect
      ABI compatibility, because PRBool is a typedef for PRIntn.

.. _experimental_apis_and_functionality:

`Experimental APIs and Functionality <#experimental_apis_and_functionality>`__
------------------------------------------------------------------------------

.. container::

   The functionality and the APIs listed in this section are experimental. Any of these APIs may be
   removed from future NSS versions. Applications *must not* rely on these APIs to be present. If an
   application is linked at runtime to a later version of NSS, which no longer provides any of these
   APIs, the application *must* handle the scenario gracefully.

   In order to ease transitions, experimental functions return SECFailure and set the
   SSL_ERROR_UNSUPPORTED_EXPERIMENTAL_API code if the selected API is not available. Experimental
   functions will always return this result if they are disabled or removed from a later NSS
   release. If these experimental functions are made permanent in a later NSS release, no change to
   code is necessary.

   (Only APIs exported in \*.def files are stable APIs.)

.. _new_experimental_functionality_provided:

`New experimental functionality provided <#new_experimental_functionality_provided>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Below are descriptions of experimental functionality, which might not be available in future
   releases of NSS.

   -  Users of TLS are now able to provide implementations of TLS extensions, through an
      experimental custom extension API. See the documentation in sslexp.h for
      SSL_InstallExtensionHooks for more information on this feature.
   -  Several experimental APIs were added in support of TLS 1.3 features:

      -  TLS servers are able to send session tickets to clients on demand, using the experimental
         SSL_SendSessionTicket function.  This ticket can include arbitrary application-chosen
         content.
      -  An anti-replay mechanism was added for 0-RTT, through the experimental SSL_SetupAntiReplay
         function.  *This mechanism must be enabled for 0-RTT to be accepted when NSS is being used
         as a server.*
      -  KeyUpdate can be triggered by the experimental SSL_KeyUpdate() function.
      -  TLS servers can screen new TLS 1.3 connections, as they are made using the experimental
         SSL_HelloRetryRequestCallback function.  This function allows for callbacks to be
         installed, which are called when a server receives a new TLS ClientHello.  The application
         is then able to examine application-chosen content from the session tickets, or
         HelloRetryRequest cookie, and decide whether to proceed with the connection.  For an
         initial ClientHello, an application can control whether NSS sends a HelloRetryRequest, and
         include application-chosen content in the cookie.

.. _new_experimental_apis:

`New experimental APIs <#new_experimental_apis>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Below is a list of experimental functions, which might not be available in future releases of
   NSS.

   -  *in sslexp.h*

      -  *experimental:* **SSL_KeyUpdate** - prompt NSS to update traffic keys (TLS 1.3 only).
      -  *experimental:* **SSL_GetExtensionSupport** - query NSS support for a TLS extension.
      -  *experimental:* **SSL_InstallExtensionHooks** - install custom handlers for a TLS
         extension.
      -  *experimental:* **SSL_SetupAntiReplay** - configure a TLS server for 0-RTT anti-replay (TLS
         1.3 server only).
      -  *experimental:* **SSL_SendSessionTicket** - send a session ticket (TLS 1.3 server only).

.. _removed_experimental_apis:

`Removed experimental APIs <#removed_experimental_apis>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Note that experimental APIs might get removed from NSS without announcing removals in the release
   notes. This section might be incomplete.

   -  The experimental API SSL_UseAltServerHelloType has been disabled.

.. _bugs_fixed_in_nss_3.35:

`Bugs fixed in NSS 3.35 <#bugs_fixed_in_nss_3.35>`__
----------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.35:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.35

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.35 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.35 shared libraries,
   without recompiling, or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (select product
   'NSS').