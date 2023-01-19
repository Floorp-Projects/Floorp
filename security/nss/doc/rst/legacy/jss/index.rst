.. _mozilla_projects_nss_jss:

JSS
===

`Documentation <#documentation>`__
----------------------------------

.. container::

   .. warning::

      **The JSS project has been relocated!**

      As of April 6, 2018, JSS has been migrated from Mercurial on Mozilla to Git on Github.

      JSS source should now be checked out from the Github:

      -  git clone git@github.com:dogtagpki/jss.git
         -- OR --
      -  git clone https://github.com/dogtagpki/jss.git

      All future upstream enquiries to JSS should now use the Pagure Issue Tracker system:

      -  https://pagure.io/jss/issues

      Documentation regarding the JSS project should now be viewed at:

      -  http://www.dogtagpki.org/wiki/JSS

      **NOTE:  As much of the JSS documentation is sorely out-of-date, updated information will be a
      work in progress, and many portions of any legacy documentation will be re-written over the
      course of time.  Stay tuned!**

      Legacy JSS information can still be found at:

      -  SOURCE: https://hg.mozilla.org/projects/jss
      -  ISSUES:   https://bugzilla.mozilla.org/buglist.cgi?product=JSS
      -  WIKI:        :ref:`mozilla_projects_nss_jss`

   Network Security Services for Java (JSS) is a Java interface to
   `NSS <https://developer.mozilla.org/en-US/docs/NSS>`__. JSS supports most of the security
   standards and encryption technologies supported by :ref:`mozilla_projects_nss_reference`. JSS
   also provides a pure Java interface for ASN.1 types and BER/DER encoding.

   JSS offers a implementation of Java SSL sockets that uses NSS's SSL/TLS implementation rather
   than Sun's JSSE implementation. You might want to use JSS's own `SSL
   classes <ftp://ftp.mozilla.org/pub/mozilla.org/security/jss/releases>`__ if you want to use some
   of the capabilities found in NSS's SSL/TLS library but not found in JSSE.

   NSS is the cryptographic module where all cryptographic operations are performed. JSS essentially
   provides a Java JNI bridge to NSS C shared libraries. When NSS is put in FIPS mode, JSS ensures
   FIPS compliance by ensuring that all cryptographic operations are performed by the NSS
   cryptographic module.

   JSS offers a JCE provider, `"Mozilla-JSS" JCA Provider notes <JSS/JSS_Provider_Notes>`__.

   JSS, jss4.jar, is still built with JDK 1.4.2. While JDK 1.4.2 is EOL'd and all new product
   development should be using the latest
   `JavaSE <http://java.sun.com/javase/downloads/index.jsp>`__, legacy business products that must
   use JDK 1.4 or 1.5 can continue to add NSS/JSS security fixes/enhancements.

   JSS is used by Red Hat and Sun products that do crypto in Java. JSS is available under the
   Mozilla Public License, the GNU General Public License, and the GNU Lesser General Public
   License. JSS requires `NSPR <https://developer.mozilla.org/en-US/docs/NSPR>`__ and
   `NSS <https://developer.mozilla.org/en-US/docs/NSS>`__.

   Java provides a JCE provider called SunPKCS11 (see `Java PKCS#11 Reference
   Guide <http://download.java.net/jdk7/docs/technotes/guides/security/p11guide.html>`__.) SunPKCS11
   can be configured to use the NSS module as the crytographic provider. If you are planning to just
   use JSS JCE provider as a bridge to NSS's FIPS validated PKCS#11 module, then the SunPKCS11 JCE
   provider may do all that you need. Note that Java 1.5 claimed no FIPS compliance, and `Java
   1.6 <http://java.sun.com/javase/6/docs/technotes/guides/security/enhancements.html>`__ or higher
   needs to be used. A current limitation to the configured SunPKCS11-NSS bridge configuration is if
   you add a PKCS#11 module to the NSS database such as for a smartcard, you won't be able to access
   that smartcard through the SunPKCS11-NSS bridge. If you use JSS, you can easily get lists of
   modules and tokens that are configured in the NSS DB and freely access all of it.

   +-------------------------------------------------+-------------------------------------------------+
   | Before you use JSS, you should have a good      | .. rubric:: Community                           |
   | understanding of the crypto technologies it     |    :name: Community                             |
   | uses. You might want to read these documents:   |                                                 |
   |                                                 | -  View Mozilla Cryptography forums...          |
   | -  `Introduction to Public-Key                  |                                                 |
   |    Crypt                                        |    -  `Mailing                                  |
   | ography <https://developer.mozilla.org/en-US/do |       list <https:/                             |
   | cs/Introduction_to_Public-Key_Cryptography>`__. | /lists.mozilla.org/listinfo/dev-tech-crypto>`__ |
   |    Explains the basic concepts of public-key    |    -  `Newsgroup <http://grou                   |
   |    cryptography that underlie NSS and JSS.      | ps.google.com/group/mozilla.dev.tech.crypto>`__ |
   | -  `Introduction to                             |    -  `RSS                                      |
   |    SSL <https://developer.                      |       feed <http://groups.goo                   |
   | mozilla.org/en-US/docs/Introduction_to_SSL>`__. | gle.com/group/mozilla.dev.tech.crypto/feeds>`__ |
   |    Introduces the SSL protocol, including       |                                                 |
   |    information about cryptographic ciphers      | .. rubric:: Related Topics                      |
   |    supported by SSL and the steps involved in   |    :name: Related_Topics                        |
   |    the SSL handshake.                           |                                                 |
   |                                                 | -  `Security <https:                            |
   | For information on downloading NSS releases,    | //developer.mozilla.org/en-US/docs/Security>`__ |
   | see `NSS sources building                       |                                                 |
   | testing <NSS_Sources_Building_Te                |                                                 |
   | sting>`__\ `. <NSS_Sources_Building_Testing>`__ |                                                 |
   |                                                 |                                                 |
   | Read `Using JSS <JSS/Using_JSS>`__ to get you   |                                                 |
   | started with development after you've built and |                                                 |
   | downloaded it.                                  |                                                 |
   |                                                 |                                                 |
   | .. rubric:: Release Notes                       |                                                 |
   |    :name: Release_Notes                         |                                                 |
   |                                                 |                                                 |
   | -  `4.3.1 Release                               |                                                 |
   |    Notes </4.3.1_Release_Notes>`__              |                                                 |
   | -  `4.3 Release                                 |                                                 |
   |    Notes <https://developer.                    |                                                 |
   | mozilla.org/en-US/docs/JSS/4_3_ReleaseNotes>`__ |                                                 |
   | -  `Older Release                               |                                                 |
   |    Notes <http://www-archive.mozil              |                                                 |
   | la.org/projects/security/pki/jss/index.html>`__ |                                                 |
   |                                                 |                                                 |
   | .. rubric:: Build Instructions                  |                                                 |
   |    :name: Build_Instructions                    |                                                 |
   |                                                 |                                                 |
   | -  :re                                          |                                                 |
   | f:`mozilla_projects_nss_jss_build_instructions_ |                                                 |
   | for_jss_4_4_x#build_instructions_for_jss_4_4_x` |                                                 |
   | -  `Building JSS                                |                                                 |
   |    4.3.x <https://developer.mozilla.org/en-U    |                                                 |
   | S/docs/JSS/Build_instructions_for_JSS_4.3.x>`__ |                                                 |
   | -  `Older Build                                 |                                                 |
   |    Instructions <http://www-archive.mozil       |                                                 |
   | la.org/projects/security/pki/jss/index.html>`__ |                                                 |
   |                                                 |                                                 |
   | .. rubric:: Download or View Source             |                                                 |
   |    :name: Download_or_View_Source               |                                                 |
   |                                                 |                                                 |
   | -  `Download binaries, source, and              |                                                 |
   |    javadoc <ftp://ftp.mozilla                   |                                                 |
   | .org/pub/mozilla.org/security/jss/releases/>`__ |                                                 |
   | -  `View the source                             |                                                 |
   |    online <http://m                             |                                                 |
   | xr.mozilla.org/mozilla/source/security/jss/>`__ |                                                 |
   |                                                 |                                                 |
   | .. rubric:: Testing                             |                                                 |
   |    :name: Testing                               |                                                 |
   |                                                 |                                                 |
   | -  `JSS                                         |                                                 |
   |    tests <https://                              |                                                 |
   | hg.mozilla.org/projects/jss/file/tip/README>`__ |                                                 |
   |                                                 |                                                 |
   | .. rubric:: Frequently Asked Questions          |                                                 |
   |    :name: Frequently_Asked_Questions            |                                                 |
   |                                                 |                                                 |
   | -  `JSS FAQ <JSS/JSS_FAQ>`__                    |                                                 |
   |                                                 |                                                 |
   | Information on JSS planning can be found at     |                                                 |
   | `wik                                            |                                                 |
   | i.mozilla.org <http://wiki.mozilla.org/NSS>`__, |                                                 |
   | including:                                      |                                                 |
   |                                                 |                                                 |
   | -  `NSS FIPS                                    |                                                 |
   |    Validati                                     |                                                 |
   | on <http://wiki.mozilla.org/FIPS_Validation>`__ |                                                 |
   | -  `NSS Roadmap                                 |                                                 |
   |                                                 |                                                 |
   |   page <http://wiki.mozilla.org/NSS:Roadmap>`__ |                                                 |
   +-------------------------------------------------+-------------------------------------------------+