.. _mozilla_projects_nss_jss_jss_faq:

JSS FAQ
=======

.. _jss_frequently_asked_questions:

`JSS Frequently Asked Questions <#jss_frequently_asked_questions>`__
--------------------------------------------------------------------

.. container::

   Newsgroup: `mozilla.dev.tech.crypto <news://news.mozilla.org:119/mozilla.dev.tech.crypto>`__

   **Content:**

   -  `What versions of JDK and JCE do you suggest? <#jdkjce1>`__
   -  `Does JSS have 64 bit support? <#64bit>`__
   -  `Is JSS FIPS Compliant? <#fips>`__
   -  `Is there any sample code and documentation? <#sample>`__
   -  `If I don't call setCipherPolicy, is the DOMESTIC policy used by
      default? <#setcipherpolicy>`__
   -  `My SSL connection is hanging on Windows? <#ssl_hanging>`__
   -  `How can I tell which SSL/TLS ciphers JSS supports? <#ssltls_cipher>`__
   -  `How can I debug my SSL connection? <#ssl_debug>`__
   -  `Can you explain JSS SSL certificate approval callbacks? <#ssl_callback>`__
   -  `Can I have multiple JSS instances reading separate db's? <#jss_instance>`__
   -  `Once JSS initialized, I can't get anymore instances with
      CertificateFactory.getInstance(X.509)? <#jss_init>`__
   -  `Is it possible to sign data in Java with JSS? <#sign_date>`__
   -  `How do I convert org.mozilla.jss.crypto.X509Certificate to
      org.mozilla.jss.pkix.cert.Certificate? <#convertx509>`__
   -  `How do I convert org.mozilla.jss.pkix.cert to
      org.mozilla.jss.crypto.X509Certificate? <#convertpkix>`__
   -  `Is it possible to use JSS to access cipher functionality from pkcs11 modules? <#pkc11>`__
   -  `Can you explain token names and keys with regards to JSS? <#token_name>`__
   -  `JSS 3.2 has JCA support. When will JSS have JSSE support? <#jssjsse>`__

   **What versions of JDK and JRE do you suggest?**

   -  JSS 3.x works with JDK versions 1.2 or higher, except version 1.3.0. Most attention for future
      development and bug fixing will go to JDK 1.4 and later, so use that if you can. If you are
      using JDK 1.3.x, you will need to use at least version 1.3.1--see `bug
      113808 <http://bugzilla.mozilla.org/show_bug.cgi?id=113808>`__. JSS only supports the native
      threading model (no green threads). For JSS 3.2 and higher, if you use JDK 1.4 or higher you
      will not need to install the JCE, but if you using an earlier version of the JDK then you will
      also have to install JCE 1.2.1. See also the document `Using JSS <Using_JSS>`__.

   **Does JSS have 64 bit support?**

   -  Yes, JSS 3.2 and higher supports 64 bit. You will need JDK 1.4 or higher and all the 64 bit
      versions of NSPR, and NSS. As well you must use the java flag -d64 to specify the 64-bit data
      model.

   **Is JSS FIPS Compliant?**

   -  NSS is a FIPS-certified software library. JSS is considered a FIPS-compliant software library
      since it only uses NSS for any and all crypto routines.

   **Is there any sample code and documentation?**

   -  The `Using JSS <Using_JSS>`__ document describes how to set up your environment to run JSS.
      The only other documentation is the
      `Javadoc <ftp://ftp.mozilla.org/pub/mozilla.org/security/jss/releases/JSS_4_3_RTM/doc/JSS_4_3_RTM-doc.zip>`__.

      JSS example code is essentially developer test code; with that understanding, the best
      directory to look for sample code is in the org/mozilla/jss/tests directory:

      http://lxr.mozilla.org/mozilla/source/security/jss/org/mozilla/jss/tests

      | `org/mozilla/jss/tests/CloseDBs.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/tests/CloseDBs.java#47>`__
      | `org/mozilla/jss/tests/KeyFactoryTest.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/tests/KeyFactoryTest.java#81>`__
      | `org/mozilla/jss/tests/DigestTest.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/tests/DigestTest.java#44>`__
      | `org/mozilla/jss/tests/JCASigTest.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/tests/JCASigTest.java#50>`__
      | `org/mozilla/jss/tests/KeyWrapping.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/tests/KeyWrapping.java#45>`__
      | `org/mozilla/jss/tests/ListCerts.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/tests/ListCerts.java#40>`__
      | `org/mozilla/jss/tests/PK10Gen.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/tests/PK10Gen.java#43>`__
      | `org/mozilla/jss/tests/SDR.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/tests/SDR.java#47>`__
      | `org/mozilla/jss/tests/SelfTest.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/tests/SelfTest.java#46>`__
      | `org/mozilla/jss/tests/SetupDBs.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/tests/SetupDBs.java#42>`__
      | `org/mozilla/jss/tests/SigTest.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/tests/SigTest.java#64>`__
      | `org/mozilla/jss/tests/SymKeyGen.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/tests/SymKeyGen.java#44>`__
      | `org/mozilla/jss/tests/TestKeyGen.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/tests/TestKeyGen.java#64>`__
      | `org/mozilla/jss/tests/SSLClientAuth.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/tests/SSLClientAuth.java#99>`__
      | `org/mozilla/jss/tests/ListCACerts.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/tests/ListCACerts.java#8>`__
      | `org/mozilla/jss/tests/KeyStoreTest.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/tests/KeyStoreTest.java#68>`__
      | `org/mozilla/jss/tests/VerifyCert.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/tests/VerifyCert.java#86>`__

      SSL examples:

      | `org/mozilla/jss/tests/SSLClientAuth.java <http://lxr.mozilla.org/mozilla/source/security/jss/org/mozilla/jss/tests/SSLClientAuth.java>`__
      | `org/mozilla/jss/ssl/SSLClient.java <http://lxr.mozilla.org/mozilla/source/security/jss/org/mozilla/jss/ssl/SSLClient.java>`__
      | `org/mozilla/jss/ssl/SSLServer.java <http://lxr.mozilla.org/mozilla/source/security/jss/org/mozilla/jss/ssl/SSLServer.java>`__
      | `org/mozilla/jss/ssl/SSLTest.java <http://lxr.mozilla.org/mozilla/source/security/jss/org/mozilla/jss/ssl/SSLTest.java>`__

      Other test code that may prove useful:

      | `org/mozilla/jss/asn1/INTEGER.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/asn1/INTEGER.java#131>`__
      | `org/mozilla/jss/asn1/SEQUENCE.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/asn1/SEQUENCE.java#574>`__
      | `org/mozilla/jss/asn1/SET.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/asn1/SET.java#876>`__
      | `org/mozilla/jss/pkcs10/CertificationRequest.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/pkcs10/CertificationRequest.java#269>`__
      | `org/mozilla/jss/pkcs12/PFX.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/pkcs12/PFX.java#329>`__
      | `org/mozilla/jss/pkix/cert/Certificate.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/pkix/cert/Certificate.java#279>`__
      | `org/mozilla/jss/pkix/cmmf/CertRepContent.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/pkix/cmmf/CertRepContent.java#148>`__
      | `org/mozilla/jss/pkix/crmf/CertReqMsg.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/pkix/crmf/CertReqMsg.java#265>`__
      | `org/mozilla/jss/pkix/crmf/CertTemplate.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/pkix/crmf/CertTemplate.java#530>`__
      | `org/mozilla/jss/pkix/primitive/Name.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/pkix/primitive/Name.java#276>`__
      | `org/mozilla/jss/provider/javax/crypto/JSSSecretKeyFactorySpi.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/provider/javax/crypto/JSSSecretKeyFactorySpi.java#287>`__
      | `org/mozilla/jss/util/UTF8Converter.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/util/UTF8Converter.java#302>`__
      | `org/mozilla/jss/util/Base64InputStream.java <http://lxr.mozilla.org/security/source/security/jss/org/mozilla/jss/util/Base64InputStream.java#237>`__
      | `jss/samples/PQGGen.java <http://lxr.mozilla.org/security/source/security/jss/samples/PQGGen.java#44>`__
      | `jss/samples/pkcs12.java <http://lxr.mozilla.org/security/source/security/jss/samples/pkcs12.java#57>`__

   **If I don't call setCipherPolicy, is the DOMESTIC policy used by default?**

   -  Yes, domestic is the default because we call NSS_SetDomesticPolicy() during
      CryptoManager.initialize(). setCipherPolicy does not need to be called by a JSS app unless
      that app wants to limit itself to export-allowed cipher suites.

   **My SSL connection is hanging on Windows?**

   -  NSPR makes use of NT vs. Windows distinction and provides different NT and Windows builds.
      Many Netscape products, including NSS, have NT and Windows builds that are essentially the
      same except one difference: one is linked with the NT version of NSPR and the other is linked
      with the Windows version of NSPR. The NT fiber problem affects applications that call blocking
      system calls from the primordial thread. Either use the WIN 95 version of NSPR/NSS/JSS
      components (essentially all non-fiber builds) or set the environment variable
      NSPR_NATIVE_THREADS_ONLY=1. You can find more information in bugzilla bug
      `102251 <http://bugzilla.mozilla.org/show_bug.cgi?id=102251>`__ SSL session cache locking
      issue with NT fibers

   **How can I tell which SSL/TLS ciphers JSS supports?**

   -  Check
      http://lxr.mozilla.org/mozilla/source/security/jss/org/mozilla/jss/ssl/SSLSocket.java#730

   **How can I debug my SSL connection?**

   -  By using the NSS tool :ref:`mozilla_projects_nss_tools_ssltap`

   **Can you explain JSS SSL certificate approval callbacks?**

   -  NSS has three callbacks related to certificates. JSS has two. But JSS combines two of the NSS
      callbacks into one.

   -  NSS's three SSL cert callbacks are:

      #. SSL_AuthCertificateHook sets a callback to authenticate the peer's certificate. It is
         called instead of NSS's routine for authenticating certificates.
      #. SSL_BadCertHook sets a callback that is called when NSS's routine fails to authenticate the
         certificate.
      #. SSL_GetClientAuthDataHook sets a callback to return the local certificate for SSL client
         auth.

      JSS's two callbacks are:

      #. SSLCertificateApprovalCallback is a combination of SSL_AuthCertificateHook and
         SSL_BadCertHook. It runs NSS's cert authentication check, then calls the callback
         regardless of whether the cert passed or failed. The callback is told whether the cert
         passed, and then can do anything extra that it wants to do before making a final decision.
      #. SSLClientCertificateSelectionCallback is analogous to SSL_GetClientAuthDataHook.

   | 
   | **Can I have multiple JSS instances reading separate db's?**

   -  No, you can only have one initialized instance of JSS for each database.

   **Once JSS initialized, I can't get anymore instances with
   CertificateFactory.getInstance("X.509")?**

   -  In version previous to JSS 3.1, JSS removes the default SUN provider on startup. Upgrade to
      the latest JSS, or, in the ``CryptoManager.InitializationValues`` object you pass to
      ``CryptoManager.initialize()``, set ``removeSunProivider=true``.

   **Is it possible to sign data in Java with JSS? What I am trying to do is write a Java applet
   that will access the Netscape certificate store, retrieve a X509 certificate and then sign some
   data.**

   -  The best way to do this is with the PKCS #7 signedData type. Check out the
      `javadoc <ftp://ftp.mozilla.org/pub/mozilla.org/security/jss/releases/JSS_4_3_RTM/doc/JSS_4_3_RTM-doc.zip>`__.

   **How do I convert org.mozilla.jss.crypto.X509Certificate to
   org.mozilla.jss.pkix.cert.Certificate?**

   -  .. code:: notranslate

         import java.io.ByteArrayInputStream;

         [...]

         Certificate cert = (Certificate) ASN1Util.decode(
                 Certificate.getTemplate(),x509Cert.getEncoded() );

   **How do I convert org.mozilla.jss.pkix.cert to org.mozilla.jss.crypto.X509Certificate?**

   -  `Cryptomanager.importCertPackage() <ftp://ftp.mozilla.org/pub/mozilla.org/security/jss/releases/JSS_4_3_RTM/doc/JSS_4_3_RTM-doc.zip>`__

   **Is it possible to use JSS to acces cipher functionality from pkcs11 modules?**

   -  Yes. Before JSS 3.2 you would use CryptoManager to obtain the CryptoToken you want to use,
      then call CryptoToken.getCipherContext() to get an encryption engine. But as of JSS 3.2 you
      would use the `JSS JCA provider <JSS_Provider_Notes>`__.

   **Can you explain token names and keys with regards to JSS?**

   -  The token name is different depending on which application you are running. In JSS, the token
      is called "Internal Key Storage Token". You can look it up by name using
      CryptoManager.getTokenByName(), but a better way is to call
      CryptoManager.getInternalKeyStorageToken(), which works no matter what the token is named. In
      general, a key is a handle to an underlying object on a PKCS #11 token, not merely a Java
      object residing in memory. Symmetric Key usage:  basically encrypt/decrypt is for data and
      wrap/unwrap is for keys.

   J\ **SS 3.2 has JCA support. When will JSS have JSSE support?**

   -  Not in the near future due to pluggability is disabled in the JSSE version included in J2SE
      1.4.x for export control reasons.