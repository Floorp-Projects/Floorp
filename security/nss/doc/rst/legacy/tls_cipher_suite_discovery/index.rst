.. _mozilla_projects_nss_tls_cipher_suite_discovery:

TLS Cipher Suite Discovery
==========================

.. container::

   |
   | In order to communicate securely, an TLS client and TLS server must agree on the cryptographic
     algorithms and keys that they will both use on the secured connection. They must agree on these
     items:

   -  Key Establishment Algorithm (such as RSA, DH, or ECDH)
   -  Peer Authentication Algorithm (such as RSA, DSA, ECDSA)
   -  Bulk Data Encryption Algorithm (such as RC4, DES, AES) and key size
   -  Digest Algorithm for Message Authentication Checking (SHA1, SHA256)

   There are numerous available choices for each of those categories, and the number of possible
   combinations of all those choices is large. TLS does not allow all possible combinations of
   choices from those categories to be used. Instead, TLS allows only certain well-defined
   combinations of those choices, known as Cipher Suites, defined in the IETF RFC standards.

   Each Cipher Suite is represented by a 16-bit number. The number of well-defined cipher suites
   grows with time, and no TLS implementation offers all known cipher suites at all times. An
   implementation that claimed to offer all defined Cipher Suites would only be able to make that
   claim for a short time until another new Cipher Suite was defined. At any time, any real
   implementation implements some subset of the complete set of well-defined cipher suites.

   Each new release of a TLS implementation may contain support for new Cipher Suites not supported
   in previous versions. When a new version of a TLS Implementation is made available for use by
   applications, those applications may wish to immediately use the newly supported Cipher Suites
   found in the new version, without the application needing to be modified and re-released to know
   about these new cipher suites. To that end, NSS's libSSL offers a way for applications to
   discover at run time the set of Cipher Suites supported by that version of libSSL. libSSL
   provides enough information about each of the supported cipher suites that the application can
   construct a display of that information from which the user can choose which cipher suites his
   application will attempt to use.

   Here are the details of how an NSS-based application learns what cipher suites are supported and
   obtains the information to display to the user.

   libSSL offers a public table of well defined cipher suite numbers. The cipher suites are listed
   in the table in order of preference, from the most preferred cipher suite to the least preferred.
   The size of this table varies from release to release, and so libSSL makes the number of entries
   in that table publicly available too. The table and the number of entries are declared in
   "ssl.h", as follows:

   .. code::

        /* constant table enumerating all implemented SSL 2 and 3 cipher suites. */
        SSL_IMPORT const PRUint16 SSL_ImplementedCiphers[];

        /* number of entries in the above table. */
        SSL_IMPORT const PRUint16 SSL_NumImplementedCiphers;

   Of course, the raw integer numbers of the cipher suites are not likely to be known to most users,
   so libSSL provides a function by which the application can obtain a wealth of information about
   any supported cipher suite, by its number. This function is declared in "ssl.h" as follows:

   .. code::

       SSL_IMPORT SECStatus
       SSL_GetCipherSuiteInfo(
             PRUint16 cipherSuite,
             SSLCipherSuiteInfo *info,
             PRUintn len);

   The application provides

   -  the cipher suite number for which it wants information,
   -  the address of a block of memory allocated to receive that information, and
   -  the size in bytes of that block of memory.

   ``SSL_GetCipherSuiteInfo`` fills that caller-supplied memory with information from the
   ``SSLCipherSuiteInfo`` structure for that cipher suite. The ``SSLCipherSuiteInfo`` structure
   contains this information, declared in "sslt.h":

   .. code::

       typedef struct SSLCipherSuiteInfoStr {
           PRUint16             length;
           PRUint16             cipherSuite;

           /* Cipher Suite Name */
           const char *         cipherSuiteName;

           /* server authentication info */
           const char *         authAlgorithmName;
           SSLAuthType          authAlgorithm;

           /* key exchange algorithm info */
           const char *         keaTypeName;
           SSLKEAType           keaType;

           /* symmetric encryption info */
           const char *         symCipherName;
           SSLCipherAlgorithm   symCipher;
           PRUint16             symKeyBits;
           PRUint16             symKeySpace;
           PRUint16             effectiveKeyBits;

           /* MAC info */
           const char *         macAlgorithmName;
           SSLMACAlgorithm      macAlgorithm;
           PRUint16             macBits;

           PRUintn              isFIPS       : 1;
           PRUintn              isExportable : 1;
           PRUintn              nonStandard  : 1;
           PRUintn              reservedBits :29;

       } SSLCipherSuiteInfo;

   (Unfinished, To be completed here)