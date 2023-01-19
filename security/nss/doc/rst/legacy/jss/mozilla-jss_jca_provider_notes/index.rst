.. _mozilla_projects_nss_jss_mozilla-jss_jca_provider_notes:

Mozilla-JSS JCA Provider notes
==============================

.. _the_mozilla-jss_jca_provider:

`The Mozilla-JSS JCA Provider <#the_mozilla-jss_jca_provider>`__
----------------------------------------------------------------

.. container::

   *Newsgroup:*\ `mozilla.dev.tech.crypto <news://news.mozilla.org/mozilla.dev.tech.crypto>`__

`Overview <#overview>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This document describes the JCA Provider shipped with JSS. The provider's name is "Mozilla-JSS".
   It implements cryptographic operations in native code using the
   `NSS <https://www.mozilla.org/projects/security/pki/nss>`__ libraries.

`Contents <#contents>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `Signed JAR
      file <https://www.mozilla.org/projects/security/pki/jss/provider_notes.html#signed-jar>`__
   -  `Installing the
      Provider <https://www.mozilla.org/projects/security/pki/jss/provider_notes.html#installing-provider>`__
   -  `Specifying the
      CryptoToken <https://www.mozilla.org/projects/security/pki/jss/provider_notes.html#specifying-token>`__
   -  `Supported
      Classes <https://www.mozilla.org/projects/security/pki/jss/provider_notes.html#supported-classes>`__
   -  `What's Not
      Supported <https://www.mozilla.org/projects/security/pki/jss/provider_notes.html#not-supported>`__

.. _signed_jar_file:

`Signed JAR file <#signed_jar_file>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   JSS implements several JCE (Java Cryptography Extension) algorithms. These algorithms have at
   various times been export-controlled by the US government. JRE therefore requires that JAR files
   implementing JCE algorithms be digitally signed by an approved organization. The maintainers of
   JSS, Sun, Red Hat, and Mozilla, have this approval and signs the official builds of ``jss4.jar``.
   At runtime, the JRE automatically verifies this signature whenever a JSS class is loaded that
   implements a JCE algorithm. The verification is transparent to the application (unless it fails
   and throws an exception). If you are curious, you can verify the signature on the JAR file using
   the ``jarsigner`` tool, which is distributed with the JDK.

   If you build JSS yourself from source instead of using binaries downloaded from mozilla.org, your
   JAR file will not have a valid signature. This means you will not be able to use the JSS provider
   for JCE algorithms. You have two choices.

   #. Use the binary release of JSS from mozilla.org.
   #. Apply for your own JCE code-signing certificate following the procedure at `How to Implement a
      Provider for the Java\ TM Cryptography
      Extension <http://java.sun.com/javase/6/docs/technotes/guides/security/crypto/HowToImplAProvider.html#Step61>`__.
      Then you can sign your own JSS JAR file.

.. _installing_the_provider:

`Installing the Provider <#installing_the_provider>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   In order to use any part of JSS, including the JCA provider, you must first call
   ``CryptoManager.initialize()``. By default, the JCA provider will be installed in the list of
   providers maintained by the ``java.security.Security`` class. If you do not wish the provider to
   be installed, create a
   ```CryptoManager.InitializationValues`` <https://www.mozilla.org/projects/security/pki/jss/javadoc/org/mozilla/jss/CryptoManager.InitializationValues.html>`__
   object, set its ``installJSSProvider`` field to ``false``, and pass the ``InitializationValues``
   object to ``CryptoManager.initialize()``.

.. _specifying_the_cryptotoken:

`Specifying the CryptoToken <#specifying_the_cryptotoken>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   All cryptographic operations in JSS and NSS occur on a particular PKCS #11 token, implemented in
   software or hardware. There is no clean way to specify this token through the JCA API. By
   default, the JSS provider carries out all operations except MessageDigest on the Internal Key
   Storage Token, a software token included in JSS/NSS. MessageDigest operations take place by
   default on the Internal Crypto Token, another internal software token in JSS/NSS. There is no
   good design reason for this difference, but it is necessitated by a quirk in the NSS
   implementation.

   In order to use a different token, use ``CryptoManager.setThreadToken()``. This sets the token to
   be used by the JSS JCA provider in the current thread. When you call ``getInstance()`` on a JCA
   class, the JSS provider checks the current per-thread default token (by calling
   ``CryptoManager.getThreadToken()``) and instructs the new object to use that token for
   cryptographic operations. The per-thread default token setting is only consulted inside
   ``getInstance()``. Once a JCA object has been created it will continue to use the same token,
   even if the application later changes the per-thread default token.

   Whenever a new thread is created, its token is initialized to the default, the Internal Key
   Storage Token. Thus, the thread token is not inherited from the parent thread.

   The following example shows how you can specify which token is used for various JCA operations:

   .. code::

      // Lookup PKCS #11 tokens
      CryptoManager manager = CryptoManager.getInstance();
      CryptoToken tokenA = manager.getTokenByName("TokenA");
      CryptoToken tokenB = manager.getTokenByName("TokenB");

      // Create an RSA KeyPairGenerator using TokenA
      manager.setThreadToken(tokenA);
      KeyPairGenerator rsaKpg = KeyPairGenerator.getInstance("Mozilla-JSS", "RSA");

      // Create a DSA KeyPairGenerator using TokenB
      manager.setThreadToken(tokenB);
      KeyPairGenerator dsaKpg  = KeyPairGenerator.getInstance("Mozilla-JSS", "DSA");

      // Generate an RSA KeyPair. This will happen on TokenA because TokenA
      // was the per-thread default token when rsaKpg was created.
      rsaKpg.initialize(1024);
      KeyPair rsaPair = rsaKpg.generateKeyPair();

      // Generate a DSA KeyPair. This will happen on TokenB because TokenB
      // was the per-thread default token when dsaKpg was created.
      dsaKpg.initialize(1024);
      KeyPair dsaPair = dsaKpg.generateKeyPair();

.. _supported_classes:

`Supported Classes <#supported_classes>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `Cipher <https://www.mozilla.org/projects/security/pki/jss/provider_notes.html#Cipher>`__
   -  `DSAPrivateKey <https://www.mozilla.org/projects/security/pki/jss/provider_notes.html#DSAPrivateKey>`__
   -  DSAPublicKey
   -  `KeyFactory <https://www.mozilla.org/projects/security/pki/jss/provider_notes.html#KeyFactory>`__
   -  `KeyGenerator <https://www.mozilla.org/projects/security/pki/jss/provider_notes.html#KeyGenerator>`__
   -  `KeyPairGenerator <https://www.mozilla.org/projects/security/pki/jss/provider_notes.html#KeyPairGenerator>`__
   -  `Mac <https://www.mozilla.org/projects/security/pki/jss/provider_notes.html#Mac>`__
   -  `MessageDigest <https://www.mozilla.org/projects/security/pki/jss/provider_notes.html#MessageDigest>`__
   -  `RSAPrivateKey <https://www.mozilla.org/projects/security/pki/jss/provider_notes.html#RSAPrivateKey>`__
   -  RSAPublicKey
   -  `SecretKeyFactory <https://www.mozilla.org/projects/security/pki/jss/provider_notes.html#SecretKeyFactory>`__
   -  `SecretKey <https://www.mozilla.org/projects/security/pki/jss/provider_notes.html#SecretKey>`__
   -  `SecureRandom <https://www.mozilla.org/projects/security/pki/jss/provider_notes.html#SecureRandom>`__
   -  `Signature <https://www.mozilla.org/projects/security/pki/jss/provider_notes.html#Signature>`__

`Cipher <#cipher>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. rubric:: Supported Algorithms
      :name: supported_algorithms

   .. rubric:: Notes
      :name: notes

   -  AES
   -  DES
   -  DESede (*DES3*)
   -  RC2
   -  RC4
   -  RSA

      -  The following modes and padding schemes are supported:

         +--------------------------------+--------------------------------+--------------------------------+
         | Algorithm                      | Mode                           | Padding                        |
         +--------------------------------+--------------------------------+--------------------------------+
         | DES                            | ECB                            | NoPadding                      |
         +--------------------------------+--------------------------------+--------------------------------+
         |                                | CBC                            | NoPadding                      |
         +--------------------------------+--------------------------------+--------------------------------+
         |                                |                                | PKCS5 Padding                  |
         +--------------------------------+--------------------------------+--------------------------------+
         | DESede                         | ECB                            | NoPadding                      |
         | *DES3*                         |                                |                                |
         +--------------------------------+--------------------------------+--------------------------------+
         |                                | CBC                            | NoPadding                      |
         +--------------------------------+--------------------------------+--------------------------------+
         |                                |                                | PKCS5 Padding                  |
         +--------------------------------+--------------------------------+--------------------------------+
         | AES                            | ECB                            | NoPadding                      |
         +--------------------------------+--------------------------------+--------------------------------+
         |                                | CBC                            | NoPadding                      |
         +--------------------------------+--------------------------------+--------------------------------+
         |                                |                                | PKCS5 Padding                  |
         +--------------------------------+--------------------------------+--------------------------------+
         | RC4                            | *None*                         | *None*                         |
         +--------------------------------+--------------------------------+--------------------------------+
         | RC2                            | CBC                            | NoPadding                      |
         +--------------------------------+--------------------------------+--------------------------------+
         |                                |                                | PKCS5Padding                   |
         +--------------------------------+--------------------------------+--------------------------------+

      -  The SecureRandom argument passed to ``initSign()`` and ``initVerify()`` is ignored, because
         NSS does not support specifying an external source of randomness.

`DSAPrivateKey <#dsaprivatekey>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  ``getX()`` is not supported because NSS does not support extracting data from private keys.

`KeyFactory <#keyfactory>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. rubric:: Supported Algorithms
      :name: supported_algorithms_2

   .. rubric:: Notes
      :name: notes_2

   -  DSA
   -  RSA
   -  The following transformations are supported for ``generatePublic()`` and
      ``generatePrivate()``:

      +-------------------------------------------------+-------------------------------------------------+
      | From                                            | To                                              |
      +-------------------------------------------------+-------------------------------------------------+
      | ``RSAPublicKeySpec``                            | ``RSAPublicKey``                                |
      +-------------------------------------------------+-------------------------------------------------+
      | ``DSAPublicKeySpec``                            | ``DSAPublicKey``                                |
      +-------------------------------------------------+-------------------------------------------------+
      | ``X509EncodedKeySpec``                          | ``RSAPublicKey``                                |
      |                                                 | ``DSAPublicKey``                                |
      +-------------------------------------------------+-------------------------------------------------+
      | ``RSAPrivateCrtKeySpec``                        | ``RSAPrivateKey``                               |
      +-------------------------------------------------+-------------------------------------------------+
      | ``DSAPrivateKeySpec``                           | ``DSAPrivateKey``                               |
      +-------------------------------------------------+-------------------------------------------------+
      | ``PKCS8EncodedKeySpec``                         | ``RSAPrivateKey``                               |
      |                                                 | ``DSAPrivateKey``                               |
      +-------------------------------------------------+-------------------------------------------------+

   -  ``getKeySpec()`` is not supported. This method exports key material in plaintext and is
      therefore insecure. Note that a public key's data can be accessed directly from the key.
   -  ``translateKey()`` simply gets the encoded form of the given key and then tries to import it
      by calling ``generatePublic()`` or ``generatePrivate()``. Only ``X509EncodedKeySpec`` is
      supported for public keys, and only ``PKCS8EncodedKeySpec`` is supported for private keys.

`KeyGenerator <#keygenerator>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. rubric:: Supported Algorithms
      :name: supported_algorithms_3

   .. rubric:: Notes
      :name: notes_3

   -  AES
   -  DES
   -  DESede (*DES3*)
   -  RC4
   -  The SecureRandom argument passed to ``init()`` is ignored, because NSS does not support
      specifying an external source of randomness.
   -  None of the key generation algorithms accepts an ``AlgorithmParameterSpec``.

`KeyPairGenerator <#keypairgenerator>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. rubric:: Supported Algorithms
      :name: supported_algorithms_4

   .. rubric:: Notes
      :name: notes_4

   -  DSA
   -  RSA

   -  The SecureRandom argument passed to initialize() is ignored, because NSS does not support
      specifying an external source of randomness.

`Mac <#mac>`__
~~~~~~~~~~~~~~

.. container::

   .. rubric:: Supported Algorithms
      :name: supported_algorithms_5

   .. rubric:: Notes
      :name: notes_5

   -  HmacSHA1 (*Hmac-SHA1*)

   -  Any secret key type (AES, DES, etc.) can be used as the MAC key, but it must be a JSS key.
      That is, it must be an ``instanceof org.mozilla.jss.crypto.SecretKeyFacade``.
   -  The params passed to ``init()`` are ignored.

`MessageDigest <#messagedigest>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. rubric:: Supported Algorithms
      :name: supported_algorithms_6

   -  MD5
   -  MD2
   -  SHA-1 (*SHA1, SHA*)

`RSAPrivateKey <#rsaprivatekey>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. rubric:: Notes
      :name: notes_6

   -  ``getModulus()`` is not supported because NSS does not support extracting data from private
      keys.
   -  ``getPrivateExponent()`` is not supported because NSS does not support extracting data from
      private keys.

`SecretKeyFactory <#secretkeyfactory>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. rubric:: Supported Algorithms
      :name: supported_algorithms_7

   .. rubric:: Notes
      :name: notes_7

   -  AES
   -  DES
   -  DESede (*DES3*)
   -  PBAHmacSHA1
   -  PBEWithMD5AndDES
   -  PBEWithSHA1AndDES
   -  PBEWithSHA1AndDESede (*PBEWithSHA1AndDES3*)
   -  PBEWithSHA1And128RC4
   -  RC4

   -  ``generateSecret`` supports the following transformations:

      +-------------------------------------------------+-------------------------------------------------+
      | KeySpec Class                                   | Key Algorithm                                   |
      +-------------------------------------------------+-------------------------------------------------+
      | PBEKeySpec                                      | *Using the appropriate PBE algorithm:*          |
      | org.mozilla.jss.crypto.PBEKeyGenParams          | DES                                             |
      |                                                 | DESede                                          |
      |                                                 | RC4                                             |
      +-------------------------------------------------+-------------------------------------------------+
      | DESedeKeySpec                                   | DESede                                          |
      +-------------------------------------------------+-------------------------------------------------+
      | DESKeySpec                                      | DES                                             |
      +-------------------------------------------------+-------------------------------------------------+
      | SecretKeySpec                                   | AES                                             |
      |                                                 | DES                                             |
      |                                                 | DESede                                          |
      |                                                 | RC4                                             |
      +-------------------------------------------------+-------------------------------------------------+

   -  ``getKeySpec`` supports the following transformations:

      +-------------------------------------------------+-------------------------------------------------+
      | Key Algorithm                                   | KeySpec Class                                   |
      +-------------------------------------------------+-------------------------------------------------+
      | DESede                                          | DESedeKeySpec                                   |
      +-------------------------------------------------+-------------------------------------------------+
      | DES                                             | DESKeySpec                                      |
      +-------------------------------------------------+-------------------------------------------------+
      | DESede                                          | SecretKeySpec                                   |
      | DES                                             |                                                 |
      | AES                                             |                                                 |
      | RC4                                             |                                                 |
      +-------------------------------------------------+-------------------------------------------------+

   -  For increased security, some SecretKeys may not be extractable from their PKCS #11 token. In
      this case, the key should be wrapped (encrypted with another key), and then the encrypted key
      might be extractable from the token. This policy varies across PKCS #11 tokens.
   -  ``translateKey`` tries two approaches to copying keys. First, it tries to copy the key
      material directly using NSS calls to PKCS #11. If that fails, it calls ``getEncoded()`` on the
      source key, and then tries to create a new key on the target token from the encoded bits. Both
      of these operations will fail if the source key is not extractable.
   -  The class ``java.security.spec.PBEKeySpec`` in JDK versions earlier than 1.4 does not contain
      the salt and iteration fields, which are necessary for PBE key generation. These fields were
      added in JDK 1.4. If you are using a JDK (or JRE) version earlier than 1.4, you cannot use
      class ``java.security.spec.PBEKeySpec``. Instead, you can use
      ``org.mozilla.jss.crypto.PBEKeyGenParams``. If you are using JDK (or JRE) 1.4 or later, you
      can use ``java.security.spec.PBEKeySpec`` or ``org.mozilla.jss.crypto.PBEKeyGenParams``.

`SecretKey <#secretkey>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. rubric:: Supported Algorithms
      :name: supported_algorithms_8

   .. rubric:: Notes
      :name: notes_8

   -  AES
   -  DES
   -  DESede (*DES3*)
   -  HmacSHA1
   -  RC2
   -  RC4

   -  ``SecretKey`` is implemented by the class ``org.mozilla.jss.crypto.SecretKeyFacade``, which
      acts as a wrapper around the JSS class ``SymmetricKey``. Any ``SecretKeys`` handled by JSS
      will actually be ``SecretKeyFacades``. This should usually be transparent.

`SecureRandom <#securerandom>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. rubric:: Supported Algorithms
      :name: supported_algorithms_9

   .. rubric:: Notes
      :name: notes_9

   -  pkcs11prng

   -  This invokes the NSS internal pseudorandom number generator.

`Signature <#signature>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. rubric:: Supported Algorithms
      :name: supported_algorithms_10

   .. rubric:: Notes
      :name: notes_10

   -  SHA1withDSA (*DSA, DSS, SHA/DSA, SHA-1/DSA, SHA1/DSA, DSAWithSHA1, SHAwithDSA*)
   -  SHA-1/RSA (*SHA1/RSA, SHA1withRSA*)
   -  MD5/RSA (*MD5withRSA*)
   -  MD2/RSA

   -  The SecureRandom argument passed to ``initSign()`` and ``initVerify()`` is ignored, because
      NSS does not support specifying an external source of randomness.

.. _what's_not_supported:

`What's Not Supported <#what's_not_supported>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The following classes don't work very well:

   -  **KeyStore:** There are many serious problems mapping the JCA keystore interface onto NSS's
      model of PKCS #11 modules. The current implementation is almost useless. Since these problems
      lie deep in the NSS design and implementation, there is no clear timeframe for fixing them.
      Meanwhile, the ``org.mozilla.jss.crypto.CryptoStore`` class can be used for some of this
      functionality.