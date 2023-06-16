.. _mozilla_projects_nss_nss_3_31_release_notes:

NSS 3.31 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The Network Security Services (NSS) team has released NSS 3.31, which is a minor release.

.. _distribution_information:

`Distribution information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The hg tag is NSS_3_31_RTM. NSS 3.31 requires Netscape Portable Runtime (NSPR) 4.15 or newer.

   NSS 3.31 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_31_RTM/src/

.. _new_in_nss_3.31:

`New in NSS 3.31 <#new_in_nss_3.31>`__
--------------------------------------

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  Allow certificates to be specified by RFC7512 PKCS#11 URIs.
   -  Allow querying a certificate object for its temporary or permanent storage status in a thread
      safe way.

   .. rubric:: New Functions
      :name: new_functions

   -  *in cert.h*

      -  **CERT_GetCertIsPerm** - retrieve the permanent storage status attribute of a certificate
         in a thread safe way.
      -  **CERT_GetCertIsTemp** - retrieve the temporary storage status attribute of a certificate
         in a thread safe way.

   -  *in pk11pub.h*

      -  **PK11_FindCertFromURI** - find a certificate identified by the given URI.
      -  **PK11_FindCertsFromURI** - find a list of certificates identified by the given URI.
      -  **PK11_GetModuleURI** - retrieve the URI of the given module.
      -  **PK11_GetTokenURI** - retrieve the URI of a token based on the given slot information.

   -  *in pkcs11uri.h*

      -  **PK11URI_CreateURI** - create a new PK11URI object from a set of attributes.
      -  **PK11URI_DestroyURI** - destroy a PK11URI object.
      -  **PK11URI_FormatURI** - format a PK11URI object to a string.
      -  **PK11URI_GetPathAttribute** - retrieve a path attribute with the given name.
      -  **PK11URI_GetQueryAttribute** - retrieve a query attribute with the given name.
      -  **PK11URI_ParseURI** - parse PKCS#11 URI and return a new PK11URI object.

   .. rubric:: New Macros
      :name: new_macros

   -  *in pkcs11uri.h*

      -  Several new macros that start with **PK11URI_PATTR\_** for path attributes defined in
         RFC7512.
      -  Several new macros that start with **PK11URI_QATTR\_** for query attributes defined in
         RFC7512.

.. _notable_changes_in_nss_3.31:

`Notable Changes in NSS 3.31 <#notable_changes_in_nss_3.31>`__
--------------------------------------------------------------

.. container::

   -  The APIs that set a TLS version range have been changed to trim the requested range to the
      overlap with a systemwide crypto policy, if configured. **SSL_VersionRangeGetSupported** can
      be used to query the overlap between the library's supported range of TLS versions and the
      systemwide policy.
   -  Previously, **SSL_VersionRangeSet** and **SSL_VersionRangeSetDefault** returned a failure if
      the requested version range wasn't fully allowed by the systemwide crypto policy. They have
      been changed to return success, if at least one TLS version overlaps between the requested
      range and the systemwide policy. An application may call **SSL_VersionRangeGet**
      and **SSL_VersionRangeGetDefault** to query the TLS version range that was effectively
      activated.
   -  Corrected the encoding of Domain Name Constraints extensions created by certutil
   -  NSS supports a clean seeding mechanism for \*NIX systems now using only /dev/urandom. This is
      used only when SEED_ONLY_DEV_URANDOM is set at compile time.
   -  CERT_AsciiToName can handle OIDs in dotted decimal form now.

.. _bugs_fixed_in_nss_3.31:

`Bugs fixed in NSS 3.31 <#bugs_fixed_in_nss_3.31>`__
----------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.31:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.31

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.31 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.31 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report with
   `bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product NSS).