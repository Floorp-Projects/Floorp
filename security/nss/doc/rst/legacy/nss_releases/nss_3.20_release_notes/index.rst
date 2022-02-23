.. _mozilla_projects_nss_nss_3_20_release_notes:

NSS 3.20 release notes
======================

`Introduction <#introduction>`__
--------------------------------

.. container::

   The NSS team has released Network Security Services (NSS) 3.20, which is a minor release.

.. _distribution_information:

`Distribution Information <#distribution_information>`__
--------------------------------------------------------

.. container::

   The HG tag is NSS_3_20_RTM. NSS 3.20 requires NSPR 4.10.8 or newer.

   NSS 3.20 source distributions are available on ftp.mozilla.org for secure HTTPS download:

   -  Source tarballs:
      https://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/NSS_3_20_RTM/src/

.. _new_in_nss_3.20:

`New in NSS 3.20 <#new_in_nss_3.20>`__
--------------------------------------

.. container::

.. _new_functionality:

`New Functionality <#new_functionality>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  The TLS library has been extended to support DHE ciphersuites in server applications.

   .. rubric:: New Functions
      :name: new_functions

   -  *in ssl.h*

      -  **SSL_DHEGroupPrefSet** - Configure the set of allowed/enabled DHE group parameters that
         can be used by NSS for a server socket.
      -  **SSL_EnableWeakDHEPrimeGroup** - Enable the use of weak DHE group parameters that are
         smaller than default minimum size of the library.

   .. rubric:: New Types
      :name: new_types

   -  *in sslt.h*

      -  **SSLDHEGroupType** - Enumerates the set of DHE parameters embedded in NSS that can be used
         with function SSL_DHEGroupPrefSet

   .. rubric:: New Macros
      :name: new_macros

   -  *in ssl.h*

      -  **SSL_ENABLE_SERVER_DHE** - A socket option user to enable or disable DHE ciphersuites for
         a server socket

.. _notable_changes_in_nss_3.20:

`Notable Changes in NSS 3.20 <#notable_changes_in_nss_3.20>`__
--------------------------------------------------------------

.. container::

   -  The TLS library has been extended to support DHE ciphersuites in server applications.
   -  For backward compatibility reasons, the server side implementation of the TLS library keeps
      all DHE ciphersuites disabled by default. They can be enabled with the new socket option
      SSL_ENABLE_SERVER_DHE and the SSL_OptionSet or the SSL_OptionSetDefault API.
   -  The server side implementation of the TLS  does not support session tickets while using a DHE
      ciphersuite (see `bug 1174677 <https://bugzilla.mozilla.org/show_bug.cgi?id=1174677>`__).
   -  Support for the following ciphersuites has been added:

      -  TLS_DHE_DSS_WITH_AES_128_GCM_SHA256
      -  TLS_DHE_DSS_WITH_AES_128_CBC_SHA256
      -  TLS_DHE_DSS_WITH_AES_256_CBC_SHA256

   -  By default, the server side TLS implementation will use DHE parameters with a size of 2048
      bits when using DHE ciphersuites.
   -  NSS embeds fixed DHE parameters sized 2048, 3072, 4096, 6144 and 8192 bits, which were copied
      from version 08 of the Internet-Draft `"Negotiated Finite Field Diffie-Hellman Ephemeral
      Parameters for
      TLS" <https://datatracker.ietf.org/doc/html/draft-ietf-tls-negotiated-ff-dhe-08>`__, Appendix
      A.
   -  A new API SSL_DHEGroupPrefSet has been added to NSS, which allows a server application to
      select one or multiple of the embedded DHE parameters as the preferred parameters. The current
      implementation of NSS will always use the first entry in the array that is passed as a
      parameter to the SSL_DHEGroupPrefSet API. In future versions of the TLS implementation, a TLS
      client might show a preference for certain DHE parameters, and the NSS TLS server side
      implementation might select a matching entry from the set of parameters that have been
      configured as preferred on the server side.
   -  NSS optionally supports the use of weak DHE parameters with DHE ciphersuites in order to
      support legacy clients. To enable this support, the new API SSL_EnableWeakDHEPrimeGroup must
      be used. Each time this API is called for the first time in a process, a fresh set of weak DHE
      parameters will be randomly created, which may take a long amount of time. Please refer to the
      comments in the header file that declares the SSL_EnableWeakDHEPrimeGroup API for additional
      details.
   -  The size of the default PQG parameters used by certutil when creating DSA keys has been
      increased to use 2048 bit parameters.
   -  The selfserv utility has been enhanced to support the new DHE features.
   -  NSS no longer supports C compilers that predate the ANSI C standard (C89).

.. _bugs_fixed_in_nss_3.20:

`Bugs fixed in NSS 3.20 <#bugs_fixed_in_nss_3.20>`__
----------------------------------------------------

.. container::

   This Bugzilla query returns all the bugs fixed in NSS 3.20:

   https://bugzilla.mozilla.org/buglist.cgi?resolution=FIXED&classification=Components&query_format=advanced&product=NSS&target_milestone=3.20

`Compatibility <#compatibility>`__
----------------------------------

.. container::

   NSS 3.20 shared libraries are backward compatible with all older NSS 3.x shared libraries. A
   program linked with older NSS 3.x shared libraries will work with NSS 3.20 shared libraries
   without recompiling or relinking. Furthermore, applications that restrict their use of NSS APIs
   to the functions listed in NSS Public Functions will remain compatible with future versions of
   the NSS shared libraries.

`Feedback <#feedback>`__
------------------------

.. container::

   Bugs discovered should be reported by filing a bug report
   at ` bugzilla.mozilla.org <https://bugzilla.mozilla.org/enter_bug.cgi?product=NSS>`__ (product
   NSS).