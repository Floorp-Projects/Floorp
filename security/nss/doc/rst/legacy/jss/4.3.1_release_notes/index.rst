.. _mozilla_projects_nss_jss_4_3_1_release_notes:

4.3.1 Release Notes
===================

.. _release_date_2009-12-02:

`Release Date: 2009-12-02 <#release_date_2009-12-02>`__
-------------------------------------------------------

.. container::

`Introduction <#introduction>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Network Security Services for Java (JSS) 4.3.1 is a minor release with the following new
   features:

   -  Support for SSL3 & TLS Renegotiation Vulnerability
   -  Support to explicitly set the key usage for the generated private key

   JSS 4.3.1 is `tri-licensed <https://www.mozilla.org/MPL>`__ under MPL 1.1/GPL 2.0/LGPL 2.1.

.. _new_in_jss_4.3.1:

`New in JSS 4.3.1 <#new_in_jss_4.3.1>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

    A list of bug fixes and enhancement requests were implemented in this release can be obtained by
   running this `bugzilla
   query <http://bugzilla.mozilla.org/buglist.cgi?product=JSS&target_milestone=4.3.1&target_milestone=4.3.1&bug_status=RESOLVED&resolution=FIXED>`__

   **JSS 4.3.1 requires :ref:`mozilla_projects_nss_3_12_5_release_notes` or higher.**

   .. rubric:: SSL3 & TLS Renegotiation Vulnerability
      :name: ssl3_tls_renegotiation_vulnerability

   See `CVE-2009-3555 <http://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2009-3555>`__ and `US-CERT
   VU#120541 <http://www.kb.cert.org/vuls/id/120541>`__ for more information about this security
   vulnerability.

   All SSL/TLS renegotiation is disabled by default in NSS 3.12.5 and therefore will be disabled by
   default with JSS 4.3.1. This will cause programs that attempt to perform renegotiation to
   experience failures where they formerly experienced successes, and is necessary for them to not
   be vulnerable, until such time as a new safe renegotiation scheme is standardized by the IETF.

   If an application depends on renegotiation feature, it can be enabled by setting the environment
   variable NSS_SSL_ENABLE_RENEGOTIATION to 1. By setting this environmental variable, the fix
   provided by these patches will have no effect and the application may become vulnerable to the
   issue.

   This default setting can also be changed within the application by using the following JSS
   methods:

   -  SSLServerSocket.enableRenegotiation(int mode)
   -  SSLSocket.enableRenegotiation(int mode)
   -  SSLSocket.enableRenegotiationDefault(int mode)

   The mode of renegotiation that the peer must use can be set to the following:

   -  SSLSocket.SSL_RENEGOTIATE_NEVER - Never renegotiate at all. (Default)
   -  SSLSocket.SSL_RENEGOTIATE_UNRESTRICTED - Renegotiate without
      restriction, whether or not the peer's client hello bears the
      renegotiation info extension (like we always did in the past).
   -  SSLSocket.SSL_RENEGOTIATE_REQUIRES_XTN - NOT YET IMPLEMENTED

   .. rubric:: Explicitly set the key usage for the generated private key
      :name: explicitly_set_the_key_usage_for_the_generated_private_key

   |  In PKCS #11, each keypair can be marked with the operations it will
   |  be used to perform. Some tokens require that a key be marked for
   |  an operation before the key can be used to perform that operation;
   |  other tokens don't care. NSS/JSS provides a way to specify a set of
   |  flags and a corresponding mask for these flags.

   -  see generateECKeyPairWithOpFlags
   -  see generateRSAKeyPairWithOpFlags
   -  see generateDSAKeyPairWithOpFlags

.. _distribution_information:

`Distribution Information <#distribution_information>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  JSS is checked into ``mozilla/security/jss/``.
   -  The CVS tag for the JSS 4.3.1 release is ``JSS_4_3_1_RTM``.
   -  Source tarballs are available from
      `ftp://ftp.mozilla.org/pub/mozilla.or...-4.3.1.tar.bz2 <ftp://ftp.mozilla.org/pub/mozilla.org/security/jss/releases/JSS_4_3_1_RTM/src/jss-4.3.1.tar.bz2>`__
   -  Binary releases are no longer available on mozilla. JSS is a JNI library we provide the
      jss4.jar but expect you to build the JSS's matching JNI shared library. We provide the
      jss4.jar in case you do not want to obtain your own JCE code signing certificate. JSS is a
      JCE provider and therefore the jss4.jar must be signed.
      `ftp://ftp.mozilla.org/pub/mozilla.org/security/jss/releases/JSS_4_3_1_RTM <ftp://ftp.mozilla.org/pub/mozilla.org/security/jss/releases/JSS_4_3_1_RTM/>`__.

`Documentation <#documentation>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Documentation for JSS 4.3.1 is available as follows:

   -  `Build Instructions for JSS 4.3.1 </jss_build_4.3.1.html>`__
   -  Javadoc `[online] </javadoc>`__
      `[zipped] <ftp://ftp.mozilla.org/pub/mozilla.org/security/jss/releases/JSS_4_3_1_RTM/doc/JSS_4_3_1_RTM-doc.zip>`__
   -  Read the instructions on `using JSS </using_jss.html>`__.
   -  Source may be viewed with a browser (via the MXR tool) at
      http://mxr.mozilla.org/mozilla/source/security/jss/
   -  The RUN TIME behavior of JSS can be affected by the
      :ref:`mozilla_projects_nss_reference_nss_environment_variables`. 

.. _platform_information:

`Platform Information <#platform_information>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  You can check out the source from CVS by

      .. note::

         cvs co -r JSS_4_3_1_RTM JSS

   -  JSS 4.3.1 works with JDK versions 4 or higher we suggest the latest.

   -  JSS 4.3.1 requires :ref:`mozilla_projects_nss_3_12_5` or higher.

   -  JSS 4.3.1 requires `NSPR 4.7.1 <https://www.mozilla.org/projects/nspr/release-notes/>`__ or
      higher.

   -  JSS only supports the native threading model (no green threads).

.. _known_bugs_and_issues:

`Known Bugs and Issues <#known_bugs_and_issues>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  For a list of reported bugs that have not yet been fixed, `click
      here. <http://bugzilla.mozilla.org/buglist.cgi?bug_status=UNCONFIRMED&bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&&product=JSS>`__
      Note that some bugs may have been fixed since JSS 4.3.1 was released. 

`Compatibility <#compatibility>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  JSS 4.3.1 is backwards compatible with JSS 4.2. Applications compiled against JSS 4.2 will
      work with JSS 4.3.1.
   -  The 4.3.1 version of libjss4.so/jss4.dll must only be used with jss4.jar. In general, a JSS
      JAR file must be used with the JSS shared library from the exact same release.
   -  To obtain the version info from the jar file use,
      "System.out.println(org.mozilla.jss.CryptoManager.JAR_JSS_VERSION)" and to check the shared
      library: strings libjss4.so \| grep -i header  

`Feedback <#feedback>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  Bugs discovered should be reported by filing a bug report with
      `bugzilla <http://bugzilla.mozilla.org/enter_bug.cgi?product=JSS>`__.
   -  You can also give feedback directly to the developers on the Mozilla Cryptography forums...

      -  `Mailing list <https://lists.mozilla.org/listinfo/dev-tech-crypto>`__
      -  `Newsgroup <http://groups.google.com/group/mozilla.dev.tech.crypto>`__
      -  `RSS feed <http://groups.google.com/group/mozilla.dev.tech.crypto/feeds>`__