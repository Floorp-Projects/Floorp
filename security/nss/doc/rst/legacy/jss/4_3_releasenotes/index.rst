.. _mozilla_projects_nss_jss_4_3_releasenotes:

4.3 Release Notes
=================

.. _release_date_01_april_2009:

`Release Date: 01 April 2009 <#release_date_01_april_2009>`__
-------------------------------------------------------------

.. container::

`Introduction <#introduction>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Network Security Services for Java (JSS) 4.3 is a minor release with the following new features:

   -  SQLite-Based Shareable Certificate and Key Databases
   -  libpkix: an RFC 3280 Compliant Certificate Path Validation Library
   -  PKCS11 needsLogin method
   -  support HmacSHA256, HmacSHA384, and HmacSHA512
   -  support for all NSS 3.12 initialization options

   JSS 4.3 is `tri-licensed <https://www.mozilla.org/MPL>`__ under MPL 1.1/GPL 2.0/LGPL 2.1.

.. _new_in_jss_4.3:

`New in JSS 4.3 <#new_in_jss_4.3>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

    A list of bug fixes and enhancement requests were implemented in this release can be obtained by
   running this `bugzilla
   query <http://bugzilla.mozilla.org/buglist.cgi?product=JSS&target_milestone=4.2.5&target_milestone=4.3&bug_status=RESOLVED&resolution=FIXED>`__

   **JSS 4.3 requires**\ `NSS
   3.12 <https://www.mozilla.org/projects/security/pki/nss/nss-3.12/nss-3.12-release-notes.html>`__\ **or
   higher.**

   -  New `SQLite-Based Shareable Certificate and Key
      Databases <https://wiki.mozilla.org/NSS_Shared_DB>`__ by prepending the string "sql:" to the
      directory path passed to configdir parameter for Crypomanager.initialize method or using the
      NSS environment variable :ref:`mozilla_projects_nss_reference_nss_environment_variables`.
   -  Libpkix: an RFC 3280 Compliant Certificate Path Validation Library (see
      `PKIXVerify <http://mxr.mozilla.org/mozilla/ident?i=PKIXVerify>`__)
   -  PK11Token.needsLogin method (see needsLogin)
   -  support HmacSHA256, HmacSHA384, and HmacSHA512 (see
      `HMACTest.java <http://mxr.mozilla.org/mozilla/source/security/jss/org/mozilla/jss/tests/HMACTest.java>`__)
   -  support for all NSS 3.12 initialization options (see InitializationValues)
   -  New SSL error codes (see https://mxr.mozilla.org/security/sour...util/SSLerrs.h)

      -  SSL_ERROR_UNSUPPORTED_EXTENSION_ALERT
         SSL_ERROR_CERTIFICATE_UNOBTAINABLE_ALERT
         SSL_ERROR_UNRECOGNIZED_NAME_ALERT
         SSL_ERROR_BAD_CERT_STATUS_RESPONSE_ALERT
         SSL_ERROR_BAD_CERT_HASH_VALUE_ALERT

   -  New TLS cipher suites (see https://mxr.mozilla.org/security/sour...SSLSocket.java):

      -  TLS_RSA_WITH_CAMELLIA_128_CBC_SHA
         TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA
         TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA
         TLS_RSA_WITH_CAMELLIA_256_CBC_SHA
         TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA
         TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA

   -  Note: the following TLS cipher suites are declared but are not yet implemented:

      -  TLS_DH_DSS_WITH_CAMELLIA_128_CBC_SHA
         TLS_DH_RSA_WITH_CAMELLIA_128_CBC_SHA
         TLS_DH_ANON_WITH_CAMELLIA_128_CBC_SHA
         TLS_DH_DSS_WITH_CAMELLIA_256_CBC_SHA
         TLS_DH_RSA_WITH_CAMELLIA_256_CBC_SHA
         TLS_DH_ANON_WITH_CAMELLIA_256_CBC_SHA
         TLS_ECDH_anon_WITH_NULL_SHA
         TLS_ECDH_anon_WITH_RC4_128_SHA
         TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA
         TLS_ECDH_anon_WITH_AES_128_CBC_SHA
         TLS_ECDH_anon_WITH_AES_256_CBC_SHA

.. _distribution_information:

`Distribution Information <#distribution_information>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  JSS is checked into ``mozilla/security/jss/``.
   -  The CVS tag for the JSS 4.3 release is ``JSS_4_3_RTM``.
   -  Source tarballs are available from
      https://archive.mozilla.org/pub/security/jss/releases/JSS_4_3_RTM/src/jss-4.3.tar.bz2
   -  Binary releases are no longer available on mozilla. JSS is a JNI library we provide the
      jss4.jar but expect you to build the JSS's matching JNI shared library. We provide the
      jss4.jar in case you do not want to obtain your own JCE code signing certificate. JSS is a
      JCE provider and therefore the jss4.jar must be signed.
      https://archive.mozilla.org/pub/security/jss/releases/JSS_4_3_RTM/

   --------------

`Documentation <#documentation>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Documentation for JSS 4.3 is available as follows:

   -  `Build Instructions for JSS 4.3 </jss_build_4.3.html>`__
   -  Javadoc `[online] </javadoc>`__
      `[zipped] <ftp://ftp.mozilla.org/pub/mozilla.org/security/jss/releases/JSS_4_3_RTM/doc/JSS_4_3_RTM-doc.zip>`__
   -  Read the instructions on `using JSS </using_jss.html>`__.
   -  Source may be viewed with a browser (via the MXR tool) at
      http://mxr.mozilla.org/mozilla/source/security/jss/
   -  The RUN TIME behavior of JSS can be affected by the
      :ref:`mozilla_projects_nss_reference_nss_environment_variables`. 

.. _platform_information:

`Platform Information <#platform_information>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  JSS 4.3 works with JDK versions 4 or higher we suggest the latest.
   -  JSS 4.3 requires `NSS
      3.12 <https://www.mozilla.org/projects/security/pki/nss/nss-3.12/nss-3.12-release-notes.html>`__
      or higher.
   -  JSS 4.3 requires `NSPR 4.7.1 <https://www.mozilla.org/projects/nspr/release-notes/>`__ or
      higher.
   -  JSS only supports the native threading model (no green threads).

   --------------

.. _known_bugs_and_issues:

`Known Bugs and Issues <#known_bugs_and_issues>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  For a list of reported bugs that have not yet been fixed, `click
      here. <http://bugzilla.mozilla.org/buglist.cgi?bug_status=UNCONFIRMED&bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&&product=JSS>`__
      Note that some bugs may have been fixed since JSS 4.3 was released. 

   --------------

`Compatibility <#compatibility>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  JSS 4.3 is backwards compatible with JSS 4.2. Applications compiled against JSS 4.2 will work
      with JSS 4.3.
   -  The 4.3 version of libjss4.so/jss4.dll must only be used with jss4.jar. In general, a JSS JAR
      file must be used with the JSS shared library from the exact same release.
   -  To obtain the version info from the jar file use,
      "System.out.println(org.mozilla.jss.CryptoManager.JAR_JSS_VERSION)" and to check the shared
      library: strings libjss4.so \| grep -i header  

   --------------

`Feedback <#feedback>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  Bugs discovered should be reported by filing a bug report with
      `bugzilla <http://bugzilla.mozilla.org/enter_bug.cgi?product=JSS>`__.
   -  You can also give feedback directly to the developers on the Mozilla Cryptography forums...

      -  `Mailing list <https://lists.mozilla.org/listinfo/dev-tech-crypto>`__
      -  `Newsgroup <http://groups.google.com/group/mozilla.dev.tech.crypto>`__
      -  `RSS feed <http://groups.google.com/group/mozilla.dev.tech.crypto/feeds>`__