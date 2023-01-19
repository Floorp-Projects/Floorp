.. _mozilla_projects_nss_nss_tech_notes_nss_tech_note7:

nss tech note7
==============

.. _rsa_signing_and_encryption_with_nss:

`RSA Signing and Encryption with NSS <#rsa_signing_and_encryption_with_nss>`__
------------------------------------------------------------------------------

.. container::

.. _nss_technical_note_7:

`NSS Technical Note: 7 <#nss_technical_note_7>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This technical note explains how to use NSS to perform RSA signing and encryption. The industry
   standard for RSA signing and encryption is `PKCS
   #1 <http://www.rsasecurity.com/rsalabs/node.asp?id=2125>`__. NSS supports PKCS #1 v1.5. NSS
   doesn't yet support PKCS #1 v2.0 and v2.1, in particular OAEP, but OAEP support is on our `to-do
   list <https://bugzilla.mozilla.org/show_bug.cgi?id=158747>`__. Your contribution is welcome.

.. _data_types:

`Data Types <#data_types>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   NSS uses the following data types to represent keys:

   -  ``SECKEYPublicKey``: a public key, defined in "keythi.h".
   -  ``SECKEYPrivateKey``: a private key, defined in "keythi.h".
   -  ``PK11SymKey``: a symmetric key (often called a session key), defined in "secmodt.h".

   | These data types should be used as if they were opaque structures, that is, they should only be
     created by some NSS functions and you always pass pointers to these data types to NSS functions
     and never examine the members of these structures.
   | The strength of an RSA key pair is measured by the size of its modulus because given the
     modulus and public exponent, the best known algorithm for computing the private exponent is to
     factor the modulus. At present 1024 bit and 2048 bit RSA keys are the most common and
     recommended. To prevent denial-of-service attacks with huge public keys, NSS disallows modulus
     size greater than 8192 bits.
   | How are these keys created in NSS? There are a few possibilities.

   -  RSA key pairs may be generated inside a crypto module (also known as a token). Use
      ``PK11_GenerateKeyPair()`` to generate a key pair in a crypto module.

   -  Key pairs may be generated elsewhere, exported in encrypted form, and imported into a crypto
      module.

   -  | Public keys may be imported into NSS. Call ``SECKEY_ImportDERPublicKey()`` with
        ``type=CKK_RSA`` to import a DER-encoded RSA public key. If you have the modulus and public
        exponent, you need to first encode them into an RSA public key and then import the public
        key into NSS.
      | PKCS #1 defines an RSA public key as a ``SEQUENCE`` of modulus and public exponent, both of
        which are ``INTEGER``\ s. Here is the ASN.1 type definition:

      .. code::

         RSAPublicKey ::= SEQUENCE {
           modulus INTEGER, -- n
           publicExponent INTEGER -- e }

      The following sample code (error handling omitted for brevity) encodes a ``RSAPublicKey`` from
      a modulus and a public exponent and imports the public key into NSS.

      .. code::

         struct MyRSAPublicKey {
             SECItem m_modulus;
             SECItem m_exponent;
         } inPubKey;

         SECItem derPubKey;

         SECKEYPublicKey *pubKey;

         const SEC_ASN1Template MyRSAPublicKeyTemplate[] = {
             { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(MyRSAPublicKey) },
             { SEC_ASN1_INTEGER, offsetof(MyRSAPublicKey,m_modulus), },
             { SEC_ASN1_INTEGER, offsetof(MyRSAPublicKey,m_exponent), },
             { 0, }
         };

         PRArenaPool *arena;

         /*
          * Point inPubKey.m_modulus and m_exponent at the data, and
          * then set their types to unsigned integers.
          */
         inPubKey.m_modulus.type = siUnsignedInteger;
         inPubKey.m_exponent.type = siUnsignedInteger;

         arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
         SEC_ASN1EncodeItem(arena, &derPubKey, &inPubKey,
                            MyRSAPublicKeyTemplate);
         pubKey = SECKEY_ImportDERPublicKey(&derPubKey, CKK_RSA);
         PORT_FreeArena(arena, PR_FALSE);

   -  Public keys may be extracted from certificates. Given a certficate (``CERTCertificate *``),
      use ``CERT_ExtractPublicKey()`` to extract its public key. The returned public key may be used
      after the certificate is destroyed.

   When the keys are no longer needed, they need to be destroyed.

   -  Use ``SECKEY_DestroyPublicKey()`` to destroy a public key (``SECKEYPublicKey *``).
   -  Use ``SECKEY_DestroyPrivateKey()`` to destroy a private key (``SECKEYPrivateKey *``).
   -  Unlike ``SECKEYPublicKey`` and ``SECKEYPrivateKey``, ``PK11SymKey`` objects are reference
      counted. Use ``PK11_ReferenceSymKey()`` to acquire a reference to a symmetric key
      (``PK11SymKey *``). Use ``PK11_FreeSymKey()`` to release a reference to a symmetric key
      (``PK11SymKey *``); the symmetric key is destroyed when its reference count becomes zero.

`Functions <#functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   | RSA signing and encryption functions are provided by two layers of NSS function: the ``SGN_``
     and ``VFY_`` functions in cryptohi.h, and the ``PK11_`` functions in pk11pub.h. As a general
     principle, you should use the highest layer of NSS you can possibly use for what you are trying
     to accomplish.
   | For example, if you just need to generate or verify a signature, you can use the ``SGN_`` and
     ``VFY_`` functions in cryptohi.h.
   | If you need to interoperate with a protocol that isn't implemented by NSS, then you may need to
     use the ``PK11_`` functions. (This API pretty much consists of what was needed to implement SSL
     and S/MIME, plus a few enhancements over the years to support JSS.) When using the ``PK11_``
     interfaces, the same principal applies: use the highest available function.
   | If you are really trying to send a key, you should use ``PK11_PubWrapSymKey()``. For a low
     level signature, use ``PK11_Sign()``. Both of these functions do the PKCS #1 wrapping of the
     data. ``PK11_Sign`` does not do the BER encoding of the hash (as is done in ``SGN_``
     functions).
   | If you are trying to just send data, use ``PK11_PubEncryptPKCS1``.
   | ``PK11_PubEncryptRaw`` is the lowest level function. It takes a modulus size data and does a
     raw RSA operation on the data. It's used to support SSL2, which modifies the key encoding to
     include the SSL version number.

.. _pkcs_1_v1.5_block_formatting:

`PKCS #1 v1.5 Block Formatting <#pkcs_1_v1.5_block_formatting>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   | Question:
   | In PKCS #1 v1.5 (Section 8.1 Encryption-block formatting) and v2.1 (Section 7.2.1 Encryption
     operation), PKCS1 v1.5 padding is described like this:
   | ``00 || 02 || PS || 00 || M``
   | but in PKCS #1 v2.0 (Section 9.1.2.1 Encoding operation, Step 3) and on the W3C web site
     (http://www.w3.org/TR/xmlenc-core/#rsa-1_5), PKCS1 v1.5 padding is described like this:
   | ``02 || PS || 00 || M``
   | 00 at the beginning is missing. Why?
   | Answer:
   | The version without the initial 00 says :

   .. container::

      "PS is a string of strong pseudo-random octets [RANDOM] [...] long enough that the value of
      the quantity being CRYPTed is one octet shorter than the RSA modulus"

   |
   | The version with the initial 00 instead says to pad to the same length as the RSA modulus.
   | "The same length as the RSA modulus with an initial octet of 0" and "one octet shorter without
     that initial octet" are exactly the same thing because the formatted block is treated as a
     big-endian big integer by the RSA algorithm. The leading 00 octet is simply eight most
     significant 0 bits. For example, 0x00123456 is equal to 0x123456.
   | Perhaps this change made in PKCS #1 v2.0 confused many people, so it was reversed in v2.1.

.. _sample_code:

`Sample Code <#sample_code>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  :ref:`mozilla_projects_nss_nss_sample_code_nss_sample_code_sample4`

`References <#references>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `RSA Labs PKCS #1 web site <http://www.rsasecurity.com/rsalabs/node.asp?id=2125>`__
   -  `RFC 3447 <http://www.ietf.org/rfc/rfc3447.txt>`__: RSA PKCS #1 v2.1
   -  `Poupou's Blog: Common question: How to encrypt using
      RSA <http://www.dotnet247.com/247reference/a.aspx?u=http://pages.infinit.net/ctech/20031101-0151.html>`__