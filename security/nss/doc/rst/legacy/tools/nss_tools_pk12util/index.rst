.. _mozilla_projects_nss_tools_nss_tools_pk12util:

NSS Tools pk12util
==================

.. _using_the_pkcs_12_tool_(pk12util):

`Using the PKCS #12 Tool (pk12util) <#using_the_pkcs_12_tool_(pk12util)>`__
---------------------------------------------------------------------------

.. container::

   Newsgroup: `mozilla.dev.tech.crypto <news://news.mozilla.org/mozilla.dev.tech.crypto>`__
   The PKCS #12 utility makes sharing of certificates among Enterprise server 3.x and any server
   (Netscape products or non-Netscape products) that supports PKCS#12 possible. The tool allows you
   to import certificates and keys from pkcs #12 files into NSS or export them and also list
   certificates and keys in such files.

.. _availability_2:

` <#availability_2>`__ Availability
-----------------------------------

.. container::

   See the `release notes <../release_notes.html>`__ for the platforms this tool is available on.

`Synopsis <#synopsis>`__
------------------------

.. container::

   **pk12util** ``-i p12File [-h tokenname] [-v] [common-options]``
     or
   **pk12util**
   ``-o p12File -n certname [-c keyCipher] [-C certCipher] [-m | --key_len keyLen] [-n | --cert_key_len certKeyLen] [common-options]``
     or
   **pk12util** ``-l p12File [-h tokenname] [-r] [common-options]``
     where
   **[common-options]** =
   ``[-d dir] [-P dbprefix] [-k slotPasswordFile | -K slotPassword] [-w p12filePasswordFile | -W p12filePassword]``

`Syntax <#syntax>`__
--------------------

.. container::

   To run the PKCS #12 Tool, type the command ``pk12util`` *option*\ ``[``\ *arguments*\ ``]`` where
   *option* and *arguments* are combinations of the options and arguments listed in the following
   section. Three of the options, -i, -o, and -l, should be considered commands of the pk12util
   invocation. Each command takes several options. Options may take zero or more arguments. To see a
   usage string, issue the pkcs12util command without any options.

.. _options_and_arguments:

`Options and Arguments <#options_and_arguments>`__
--------------------------------------------------

.. container::

   Options specify an action. Option arguments modify an action. The options and arguments for the
   ``pk12util`` command are defined as follows:

   +-------------------------------------------------+-------------------------------------------------+
   | **Options**                                     |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-i`` *p12file*                                | Import a certificate and private key from the   |
   |                                                 | p12file into the database.                      |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-o`` *p12file*                                | Export certificate and private key, specified   |
   |                                                 | by the -n option, from the database to the p12  |
   |                                                 | file.                                           |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-l`` *p12file*                                | List certificate and private key from the       |
   |                                                 | ``p12file`` file.                               |
   +-------------------------------------------------+-------------------------------------------------+
   | **Arguments**                                   |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-n`` *certname*                               | Specify the nickname of the cert and private    |
   |                                                 | key to export.                                  |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-d`` *dir*                                    | Specify the database directory into which to    |
   |                                                 | import to or export from certificates and keys. |
   |                                                 | If not specified the directory defaults to      |
   |                                                 | $HOME/.netscape (when $HOME exists in the       |
   |                                                 | environment), or to ./.netscape (when $HOME     |
   |                                                 | does not exist in the environment).             |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-P`` *prefix*                                 | Specify the prefix used on the ``cert8.db`` and |
   |                                                 | ``key3.db`` files (for example, ``my_cert8.db`` |
   |                                                 | and ``my_key3.db``). This option is provided as |
   |                                                 | a special case. Changing the names of the       |
   |                                                 | certificate and key databases is not            |
   |                                                 | recommended.                                    |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-h`` *tokenname*                              | Specify the name of the token to import into or |
   |                                                 | export from                                     |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-v``                                          | Enable debug logging when importing             |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-k`` *slotPasswordFile*                       | Specify the text file containing the slot's     |
   |                                                 | password                                        |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-K`` *slotPassword*                           | Specify a slot's password                       |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-w`` *p12filePasswordFile*                    | Specify the text file containing the pkcs 12    |
   |                                                 | file's password                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-W`` *p12filePassword*                        | Specify the pkcs 12 file's password             |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-c`` *key-cipher*                             | Specify the key encryption algorithm            |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-C`` *certCipher*                             | Specify the PFX encryption algorithm            |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-m | --key_len`` *                            | Specify the desired length of the symmetric key |
   | keyLen*                                         | to be used to encrypt the private key           |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-n | --cert_key_len`` *                       | Specify the desired length of the symmetric key |
   | certLeyLen*                                     | to be used to encrypt the top level protocol    |
   |                                                 | data unit                                       |
   +-------------------------------------------------+-------------------------------------------------+

   +---+
   |   |
   +---+

.. _password_based_encryption:

` <#password_based_encryption>`__ Password Based Encryption
-----------------------------------------------------------

.. container::

   PKCS #12 provides for not only the protection of the private keys but also the certificate and
   meta-data associated with the keys. Password based encryption is used to protect private keys on
   export to a PKCS #12 file and also the entire package when allowed. If no algorithm is specified,
   the tool defaults to using "PKCS12 V2 PBE With SHA1 And 3KEY Triple DES-cbc" for private key
   encryption. For historical export control reasons "PKCS12 V2 PBE With SHA1 And 40 Bit RC4" is the
   default for the overall package encryption when not in FIPS mode and no package encryption when
   in FIPS mode. The private key is always protected with strong encryption by default. A list of
   ciphers follows.

   -  symmetric CBC ciphers for PKCS #5 V2:

      -  "DES_CBC"
      -  "RC2-CBC"
      -  "RC5-CBCPad"
      -  "DES-EDE3-CBC"
         --- default for key encryption
      -  "AES-128-CBC"
      -  "AES-192-CBC"
      -  "AES-256-CBC"
      -  "CAMELLIA-128-CBC"
      -  "CAMELLIA-192-CBC"
      -  "CAMELLIA-256-CBC"

   -  PKCS #12 PBE Ciphers:

      -  "PKCS #12 PBE With Sha1 and 128 Bit RC4"
      -  "PKCS #12 PBE With Sha1 and 40 Bit RC4"
      -  "PKCS #12 PBE With Sha1 and Triple DES CBC"
      -  "PKCS #12 PBE With Sha1 and 128 Bit RC2 CBC"
      -  "PKCS #12 PBE With Sha1 and 40 Bit RC2 CBC"
      -  "PKCS12 V2 PBE With SHA1 And 128 Bit RC4"
      -  "PKCS12 V2 PBE With SHA1 And 40 Bit RC4"
         --- default for PFX encryption in non-fips mode, no encryption on fips mode
      -  "PKCS12 V2 PBE With SHA1 And 3KEY Triple DES-cbc"
      -  "PKCS12 V2 PBE With SHA1 And 2KEY Triple DES-cbc"
      -  "PKCS12 V2 PBE With SHA1 And 128 Bit RC2 CBC"
      -  "PKCS12 V2 PBE With SHA1 And 40 Bit RC2 CBC"

   -  PKCS #5 PBE Ciphers:

      -  "PKCS #5 Password Based Encryption with MD2 and DES CBC"
      -  "PKCS #5 Password Based Encryption with MD5 and DES CBC"
      -  "PKCS #5 Password Based Encryption with SHA1 and DES CBC"

   It should be noted that the crypto provider may be the softtoken module or an external hardware
   module. It may be the case that the cryptographic module does not support the requested algorithm
   and a best fit will be selected, likely to be the default. If no suitable replacement for the
   desired algorithm can be found a "no security module can perform the requested operation" will
   appear on the error message.

.. _error_codes:

` <#error_codes>`__ Error Codes
-------------------------------

.. container::

   **pk12util** can return the following values:
   | **0** - No error
   | **1** - User Cancelled
   | **2** - Usage error
   | **6** - NLS init error
   | **8** - Certificate DB open error
   | **9** - Key DB open error
   | **10** - File initialization error
   | **11** - Unicode conversion error
   | **12** - Temporary file creation error
   | **13** - PKCS11 get slot error
   | **14** - PKCS12 decoder start error
   | **15** - error read from import file
   | **16** - pkcs12 decode error
   | **17** - pkcs12 decoder verify error
   | **18** - pkcs12 decoder validate bags error
   | **19** - pkcs12 decoder import bags error
   | **20** - key db conversion version 3 to version 2 error
   | **21** - cert db conversion version 7 to version 5 error
   | **22** - cert and key dbs patch error
   | **23** - get default cert db error
   | **24** - find cert by nickname error
   | **25** - create export context error
   | **26** - PKCS12 add password itegrity error
   | **27** - cert and key Safes creation error
   | **28** - PKCS12 add cert and key error
   | **29** - PKCS12 encode error