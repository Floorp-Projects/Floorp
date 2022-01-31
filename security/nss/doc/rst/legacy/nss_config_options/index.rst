.. _mozilla_projects_nss_nss_config_options:

NSS Config Options
==================

.. _nss_config_options_format:

` NSS Config Options Format <#nss_config_options_format>`__
-----------------------------------------------------------

.. container::

   The specified ciphers will be allowed by policy, but an application may allow more by policy
   explicitly:

   .. code:: notranslate

      config="allow=curve1:curve2:hash1:hash2:rsa-1024..."

   Only the specified hashes and curves will be allowed:

   .. code:: notranslate

      config="disallow=all allow=sha1:sha256:secp256r1:secp384r1"

   Only the specified hashes and curves will be allowed, and RSA keys of 2048 or more will be
   accepted, and DH key exchange with 1024-bit primes or more:

   .. code:: notranslate

      config="disallow=all allow=sha1:sha256:secp256r1:secp384r1:min-rsa=2048:min-dh=1024"

   A policy that enables the AES ciphersuites and the SECP256/384 curves:

   .. code:: notranslate

      config="allow=aes128-cbc:aes128-gcm::HMAC-SHA1:SHA1:SHA256:SHA384:RSA:ECDHE-RSA:SECP256R1:SECP384R1"

   Turn off md5

   .. code:: notranslate

      config="disallow=MD5"

   Turn off md5 and sha1 only for SSL

   .. code:: notranslate

      config="disallow=MD5(SSL):SHA1(SSL)"

   Disallow values are parsed first, and then allow values, independent of the order in which they
   appear.

   .. code:: notranslate

      Future key words (not yet implemented):
      enable: turn on ciphersuites by default.
      disable: turn off ciphersuites by default without disallowing them by policy.
      flags: turn on the following flags:
           ssl-lock: turn off the ability for applications to change policy with
                     the SSL_SetCipherPolicy (or SSL_SetPolicy).
           policy-lock: turn off the ability for applications to change policy with
                     the call NSS_SetAlgorithmPolicy.
           ssl-default-lock: turn off the ability for applications to change cipher
                     suite states with SSL_EnableCipher, SSL_DisableCipher.

   .. rubric::  ECC Curves
      :name: ecc_curves

   | 
   | PRIME192V1
   | PRIME192V2
   | PRIME192V3
   | PRIME239V1
   | PRIME239V2
   | PRIME239V3
   | PRIME256V1
   | SECP112R1
   | SECP112R2
   | SECP128R1
   | SECP128R2
   | SECP160K1
   | SECP160R1
   | SECP160R2
   | SECP192K1
   | SECP192R1
   | SECP224K1
   | SECP256K1
   | SECP256R1
   | SECP384R1
   | SECP521R1
   | C2PNB163V1
   | C2PNB163V2
   | C2PNB163V3
   | C2PNB176V1
   | C2TNB191V1
   | C2TNB191V2
   | C2TNB191V3
   | C2ONB191V4
   | C2ONB191V5
   | C2PNB208W1
   | C2TNB239V1
   | C2TNB239V2
   | C2TNB239V3
   | C2ONB239V4
   | C2ONB239V5
   | C2PNB272W1
   | C2PNB304W1
   | C2TNB359V1
   | C2PNB368W1
   | C2TNB431R1
   | SECT113R1
   | SECT131R1
   | SECT131R1
   | SECT131R2
   | SECT163K1
   | SECT163R1
   | SECT163R2
   | SECT193R1
   | SECT193R2
   | SECT233K1
   | SECT233R1
   | SECT239K1
   | SECT283K1
   | SECT283R1
   | SECT409K1
   | SECT409R1
   | SECT571K1
   | SECT571R1

   .. rubric:: Hashes
      :name: hashes

   | 
   | MD2
   | MD4
   | MD5
   | SHA1
   | SHA224
   | SHA256
   | SHA384
   | SHA512

   .. rubric:: MACS
      :name: macs

   | HMAC-SHA1
   | HMAC-SHA224
   | HMAC-SHA256
   | HMAC-SHA384
   | HMAC-SHA512
   | HMAC-MD5

   .. rubric:: Ciphers
      :name: ciphers

   | AES128-CBC
   | AES192-CBC
   | AES256-CBC
   | AES128-GCM
   | AES192-GCM
   | AES256-GCM
   | CAMELLIA128-CBC
   | CAMELLIA192-CBC
   | CAMELLIA256-CBC
   | SEED-CBC
   | DES-EDE3-CBC
   | DES-40-CBC
   | DES-CBC
   | NULL-CIPHER
   | RC2
   | RC4
   | IDEA

   .. rubric:: SSL Key exchanges
      :name: ssl_key_exchanges

   | RSA
   | RSA-EXPORT
   | DHE-RSA
   | DHE-DSS
   | DH-RSA
   | DH-DSS
   | ECDHE-ECDSA
   | ECDHE-RSA
   | ECDH-ECDSA
   | ECDH-RSA

   .. rubric:: Restrictions for asymmetric keys (integers)
      :name: restrictions_for_asymmetric_keys_(integers)

   | RSA-MIN
   | DH-MIN
   | DSA-MIN

   .. rubric:: Constraints on SSL Protocols Versions (integers)
      :name: constraints_on_ssl_protocols_versions_(integers)

   | TLS-VERSION-MIN
   | TLS-VERSION-MAX

   .. rubric:: Constraints on DTLS Protocols Versions (integers)
      :name: constraints_on_dtls_protocols_versions_(integers)

   | DTLS-VERSION-MIN
   | DTLS-VERSION-MAX

   .. rubric:: Policy flags for algorithms
      :name: policy_flags_for_algorithms

   | SSL
   | SSL-KEY-EXCHANGE
   | KEY-EXCHANGE
   | CERT-SIGNATURE
   | SIGNATURE
   | ALL
   | NONE