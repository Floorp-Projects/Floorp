.. _mozilla_projects_nss_tools_nss_tools_certutil:

NSS Tools certutil
==================

.. _using_the_certificate_database_tool:

`Using the Certificate Database Tool <#using_the_certificate_database_tool>`__
------------------------------------------------------------------------------

.. container::

   The Certificate Database Tool is a command-line utility that can create and modify the Netscape
   Communicator ``cert8.db`` and ``key3.db``\ database files. It can also list, generate, modify, or
   delete certificates within the ``cert8.db``\ file and create or change the password, generate new
   public and private key pairs, display the contents of the key database, or delete key pairs
   within the ``key3.db`` file.

   Starting from NSS 3.35, the database format was upgraded to support SQLite as described in this
   `document <https://wiki.mozilla.org/NSS_Shared_DB>`__. It means that ``cert9.db`` and ``key4.db``
   files may be targeted instead.

   The key and certificate management process generally begins with creating keys in the key
   database, then generating and managing certificates in the certificate database.

   This document discusses certificate and key database management. For information security module
   database management, see :ref:`mozilla_projects_nss_reference_nss_tools_:_modutil`

`Availability <#availability>`__
--------------------------------

.. container::

   See the release notes for the platforms this tool is available on.

`Syntax <#syntax>`__
--------------------

.. container::

   To run the Certificate Database Tool, type the command

   .. code:: notranslate

      certutil option [arguments ]

   where *options* and *arguments* are combinations of the options and arguments listed in the
   following section. Each command takes one option. Each option may take zero or more arguments. To
   see a usage string, issue the command without options, or with the ``-H`` option.

.. _options_and_arguments:

`Options and Arguments <#options_and_arguments>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Options specify an action and are uppercase. Option arguments modify an action and are lowercase.
   Certificate Database Tool command options and their arguments are defined as follows:

   +-------------------------------------------------+-------------------------------------------------+
   |  **Options**                                    |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-N``                                          | Create new certificate and key databases.       |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-S``                                          | Create an individual certificate and add it to  |
   |                                                 | a certificate database.                         |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-R``                                          | Create a certificate-request file that can be   |
   |                                                 | submitted to a Certificate Authority (CA) for   |
   |                                                 | processing into a finished certificate. Output  |
   |                                                 | defaults to standard out unless you use         |
   |                                                 | ``-o``\ *output-file* argument. Use the ``-a``  |
   |                                                 | argument to specify ASCII output.               |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-C``                                          | Create a new binary certificate file from a     |
   |                                                 | binary certificate-request file. Use the ``-i`` |
   |                                                 | argument to specify the certificate-request     |
   |                                                 | file. If this argument is not used Certificate  |
   |                                                 | Database Tool prompts for a filename.           |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-G``                                          | Generate a new public and private key pair      |
   |                                                 | within a key database. The key database should  |
   |                                                 | already exist; if one is not present, this      |
   |                                                 | option will initialize one by default. Some     |
   |                                                 | smart cards (for example, the Litronic card)    |
   |                                                 | can store only one key pair. If you create a    |
   |                                                 | new key pair for such a card, the previous pair |
   |                                                 | is overwritten.                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-F``                                          | Delete a private key from a key database.       |
   |                                                 | Specify the key to delete with the ``-n``       |
   |                                                 | argument. Specify the database from which to    |
   |                                                 | delete the key with the ``-d`` argument.        |
   |                                                 |                                                 |
   |                                                 | Use the ``-k`` argument to specify explicitly   |
   |                                                 | whether to delete a DSA or an RSA key. If you   |
   |                                                 | don't use the ``-k`` argument, the option looks |
   |                                                 | for an RSA key matching the specified nickname. |
   |                                                 |                                                 |
   |                                                 | When you delete keys, be sure to also remove    |
   |                                                 | any certificates associated with those keys     |
   |                                                 | from the certificate database, by using ``-D``. |
   |                                                 |                                                 |
   |                                                 | Some smart cards (for example, the Litronic     |
   |                                                 | card) do not let you remove a public key you    |
   |                                                 | have generated. In such a case, only the        |
   |                                                 | private key is deleted from the key pair. You   |
   |                                                 | can display the public key with the command     |
   |                                                 | ``certutil -K -h``\ *tokenname* .               |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-K``                                          | List the keyID of keys in the key database. A   |
   |                                                 | keyID is the modulus of the RSA key or the      |
   |                                                 | ``publicValue`` of the DSA key. IDs are         |
   |                                                 | displayed in hexadecimal ("0x" is not shown).   |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-A``                                          | Add an existing certificate to a certificate    |
   |                                                 | database. The certificate database should       |
   |                                                 | already exist; if one is not present, this      |
   |                                                 | option will initialize one by default.          |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-D``                                          | Delete a certificate from the certificate       |
   |                                                 | database.                                       |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-L``                                          | List all the certificates, or display           |
   |                                                 | information about a named certificate, in a     |
   |                                                 | certificate database.                           |
   |                                                 |                                                 |
   |                                                 | Use the ``-h``\ *tokenname* argument to specify |
   |                                                 | the certificate database on a particular        |
   |                                                 | hardware or software token.                     |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-V``                                          | Check the validity of a certificate and its     |
   |                                                 | attributes.                                     |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-M``                                          | Modify a certificate's trust attributes using   |
   |                                                 | the values of the ``-t`` argument.              |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-H``                                          | Display a list of the options and arguments     |
   |                                                 | used by the Certificate Database Tool.          |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-W``                                          | Change the password to a key database.          |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-U``                                          | List all available modules or print a single    |
   |                                                 | named module.                                   |
   +-------------------------------------------------+-------------------------------------------------+
   | **Arguments**                                   |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-a``                                          | Use ASCII format or allow the use of ASCII      |
   |                                                 | format for input or output. This formatting     |
   |                                                 | follows `RFC                                    |
   |                                                 | 1113 <https://tools.ietf.org/html/rfc1113>`__.  |
   |                                                 | For certificate requests, ASCII output defaults |
   |                                                 | to standard output unless redirected.           |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-b``\ *validity-time*                         | Specify a time at which a certificate is        |
   |                                                 | required to be valid. Use when checking         |
   |                                                 | certificate validity with the ``-V`` option.    |
   |                                                 | The format of the\ *validity-time* argument is  |
   |                                                 | "YYMMDDHHMMSS[+HHMM|-HHMM|Z]". Specifying       |
   |                                                 | seconds (SS) is optional. When specifying an    |
   |                                                 | explicit time, use "YYMMDDHHMMSSZ". When        |
   |                                                 | specifying an offset time, use                  |
   |                                                 | "YYMMDDHHMMSS+HHMM" or "YYMMDDHHMMSS-HHMM". If  |
   |                                                 | this option is not used, the validity check     |
   |                                                 | defaults to the current system time.            |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-c``\ *issuer*                                | Identify the certificate of the CA from which a |
   |                                                 | new certificate will derive its authenticity.   |
   |                                                 | Use the exact nickname or alias of the CA       |
   |                                                 | certificate, or use the CA's email address.     |
   |                                                 | Bracket the\ *issuer* string with quotation     |
   |                                                 | marks if it contains spaces.                    |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-d``\ *directory*                             | Specify the database directory containing the   |
   |                                                 | certificate and key database files. On Unix the |
   |                                                 | Certificate Database Tool defaults to           |
   |                                                 | ``$HOME/.netscape`` (that is, ``~/.netscape``). |
   |                                                 | On Windows NT the default is the current        |
   |                                                 | directory.                                      |
   |                                                 |                                                 |
   |                                                 | The ``cert8.db`` and ``key3.db`` database files |
   |                                                 | must reside in the same directory.              |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-P``\ *dbprefix*                              | Specify the prefix used on the ``cert8.db`` and |
   |                                                 | ``key3.db`` files (for example, ``my_cert8.db`` |
   |                                                 | and ``my_key3.db``). This option is provided as |
   |                                                 | a special case. Changing the names of the       |
   |                                                 | certificate and key databases is not            |
   |                                                 | recommended.                                    |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-e``                                          | Check a certificate's signature during the      |
   |                                                 | process of validating a certificate.            |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-f``\ *password-file*                         | Specify a file that will automatically supply   |
   |                                                 | the password to include in a certificate or to  |
   |                                                 | access a certificate database. This is a        |
   |                                                 | plain-text file containing one password. Be     |
   |                                                 | sure to prevent unauthorized access to this     |
   |                                                 | file.                                           |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-g``\ *keysize*                               | Set a key size to use when generating new       |
   |                                                 | public and private key pairs. The minimum is    |
   |                                                 | 512 bits and the maximum is 8192 bits. The      |
   |                                                 | default is 1024 bits. Any size that is a        |
   |                                                 | multiple of 8 between the minimum and maximum   |
   |                                                 | is allowed.                                     |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-h``\ *tokenname*                             | Specify the name of a token to use or act on.   |
   |                                                 | Unless specified otherwise the default token is |
   |                                                 | an internal slot (specifically, internal slot   |
   |                                                 | 2). This slot can also be explicitly named with |
   |                                                 | the string ``"internal"``. An internal slots is |
   |                                                 | a virtual slot maintained in software, rather   |
   |                                                 | than a hardware device. Internal slot 2 is used |
   |                                                 | by key and certificate services. Internal slot  |
   |                                                 | 1 is used by cryptographic services.            |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-i``\ *cert|cert-request-file*                | Specify a specific certificate, or a            |
   |                                                 | certificate-request file.                       |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-k rsa|dsa|all``                              | Specify the type of a key: RSA, DSA or both.    |
   |                                                 | The default value is ``rsa``. By specifying the |
   |                                                 | type of key you can avoid mistakes caused by    |
   |                                                 | duplicate nicknames.                            |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-l``                                          | Display detailed information when validating a  |
   |                                                 | certificate with the ``-V`` option.             |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-m``\ *serial-number*                         | Assign a unique serial number to a certificate  |
   |                                                 | being created. This operation should be         |
   |                                                 | performed by a CA. The default serial number is |
   |                                                 | 0 (zero). Serial numbers are limited to         |
   |                                                 | integers.                                       |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-n``\ *nickname*                              | Specify the nickname of a certificate or key to |
   |                                                 | list, create, add to a database, modify, or     |
   |                                                 | validate. Bracket the *nickname* string with    |
   |                                                 | quotation marks if it contains spaces.          |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-o``\ *output-file*                           | Specify the output file name for new            |
   |                                                 | certificates or binary certificate requests.    |
   |                                                 | Bracket the\ *output-file* string with          |
   |                                                 | quotation marks if it contains spaces. If this  |
   |                                                 | argument is not used the output destination     |
   |                                                 | defaults to standard output.                    |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-p``\ *phone*                                 | Specify a contact telephone number to include   |
   |                                                 | in new certificates or certificate requests.    |
   |                                                 | Bracket this string with quotation marks if it  |
   |                                                 | contains spaces.                                |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-q``\ *pqgfile*                               | Read an alternate PQG value from the specified  |
   |                                                 | file when generating DSA key pairs. If this     |
   |                                                 | argument is not used, the Key Database Tool     |
   |                                                 | generates its own PQG value. PQG files are      |
   |                                                 | created with a separate DSA utility.            |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-r``                                          | Display a certificate's binary DER encoding     |
   |                                                 | when listing information about that certificate |
   |                                                 | with the ``-L`` option.                         |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-s``\ *subject*                               | Identify a particular certificate owner for new |
   |                                                 | certificates or certificate requests. Bracket   |
   |                                                 | this string with quotation marks if it contains |
   |                                                 | spaces. The subject identification format       |
   |                                                 | follows `RFC                                    |
   |                                                 | 1485 <https://tools.ietf.org/html/rfc1485>`__.  |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-t``\ *trustargs*                             | Specify the trust attributes to modify in an    |
   |                                                 | existing certificate or to apply to a           |
   |                                                 | certificate when creating it or adding it to a  |
   |                                                 | database.                                       |
   |                                                 |                                                 |
   |                                                 | There are three available trust categories for  |
   |                                                 | each certificate, expressed in this order:      |
   |                                                 | "*SSL* ,\ *email* ,\ *object signing* ". In     |
   |                                                 | each category position use zero or more of the  |
   |                                                 | following attribute codes:                      |
   |                                                 |                                                 |
   |                                                 | | ``p``    prohibited (explicitly distrusted)   |
   |                                                 | | ``P``    Trusted peer                         |
   |                                                 | | ``c``    Valid CA                             |
   |                                                 | | ``T``    Trusted CA to issue client           |
   |                                                 |   certificates (implies ``c``)                  |
   |                                                 | | ``C``    Trusted CA to issue server           |
   |                                                 |   certificates (SSL only)                       |
   |                                                 | |       (implies ``c``)                         |
   |                                                 | | ``u``    Certificate can be used for          |
   |                                                 |   authentication or signing                     |
   |                                                 | | ``w``    Send warning (use with other         |
   |                                                 |   attributes to include a warning when the      |
   |                                                 |   certificate is used in that context)          |
   |                                                 |                                                 |
   |                                                 | The attribute codes for the categories are      |
   |                                                 | separated by commas, and the entire set of      |
   |                                                 | attributes enclosed by quotation marks. For     |
   |                                                 | example:                                        |
   |                                                 |                                                 |
   |                                                 | ``-t "TCu,Cu,Tuw"``                             |
   |                                                 |                                                 |
   |                                                 | Use the ``-L`` option to see a list of the      |
   |                                                 | current certificates and trust attributes in a  |
   |                                                 | certificate database.                           |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-u``\ *certusage*                             | Specify a usage context to apply when           |
   |                                                 | validating a certificate with the ``-V``        |
   |                                                 | option. The contexts are the following:         |
   |                                                 |                                                 |
   |                                                 | | ``C`` (as an SSL client)                      |
   |                                                 | | ``V`` (as an SSL server)                      |
   |                                                 | | ``S`` (as an email signer)                    |
   |                                                 | | ``R`` (as an email recipient)                 |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-v``\ *valid-months*                          | Set the number of months a new certificate will |
   |                                                 | be valid. The validity period begins at the     |
   |                                                 | current system time unless an offset is added   |
   |                                                 | or subtracted with the ``-w`` option. If this   |
   |                                                 | argument is not used, the default validity      |
   |                                                 | period is three months. When this argument is   |
   |                                                 | used, the default three-month period is         |
   |                                                 | automatically added to any value given in       |
   |                                                 | the\ *valid-month* argument. For example, using |
   |                                                 | this option to set a value of ``3`` would cause |
   |                                                 | 3 to be added to the three-month default,       |
   |                                                 | creating a validity period of six months. You   |
   |                                                 | can use negative values to reduce the default   |
   |                                                 | period. For example, setting a value of ``-2``  |
   |                                                 | would subtract 2 from the default and create a  |
   |                                                 | validity period of one month.                   |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-w``\ *offset-months*                         | Set an offset from the current system time, in  |
   |                                                 | months, for the beginning of a certificate's    |
   |                                                 | validity period. Use when creating the          |
   |                                                 | certificate or adding it to a database. Express |
   |                                                 | the offset in integers, using a minus sign      |
   |                                                 | (``-``) to indicate a negative offset. If this  |
   |                                                 | argument is not used, the validity period       |
   |                                                 | begins at the current system time. The length   |
   |                                                 | of the validity period is set with the ``-v``   |
   |                                                 | argument.                                       |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-x``                                          | Use the Certificate Database Tool to generate   |
   |                                                 | the signature for a certificate being created   |
   |                                                 | or added to a database, rather than obtaining a |
   |                                                 | signature from a separate CA.                   |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-y``\ *exp*                                   | Set an alternate exponent value to use in       |
   |                                                 | generating a new RSA public key for the         |
   |                                                 | database, instead of the default value of       |
   |                                                 | 65537. The available alternate values are 3 and |
   |                                                 | 17.                                             |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-z``\ *noise-file*                            | Read a seed value from the specified binary     |
   |                                                 | file to use in generating a new RSA private and |
   |                                                 | public key pair. This argument makes it         |
   |                                                 | possible to use hardware-generated seed values  |
   |                                                 | and unnecessary to manually create a value from |
   |                                                 | the keyboard. The minimum file size is 20       |
   |                                                 | bytes.                                          |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-1``                                          | Add a key usage extension to a certificate that |
   |                                                 | is being created or added to a database. This   |
   |                                                 | extension allows a certificate's key to be      |
   |                                                 | dedicated to supporting specific operations     |
   |                                                 | such as SSL server or object signing. The       |
   |                                                 | Certificate Database Tool will prompt you to    |
   |                                                 | select a particular usage for the certificate's |
   |                                                 | key. These usages are described under `Standard |
   |                                                 | X.509 v3 Certificate                            |
   |                                                 | Extensions <https://a                           |
   |                                                 | ccess.redhat.com/documentation/en-US/Red_Hat_Ce |
   |                                                 | rtificate_System/9/html/Administration_Guide/St |
   |                                                 | andard_X.509_v3_Certificate_Extensions.html>`__ |
   |                                                 | in Appendix A.3 of the\ *Red Hat Certificate    |
   |                                                 | System Administration Guide.*                   |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-2``                                          | Add a basic constraint extension to a           |
   |                                                 | certificate that is being created or added to a |
   |                                                 | database. This extension supports the           |
   |                                                 | certificate chain verification process. The     |
   |                                                 | Certificate Database Tool will prompt you to    |
   |                                                 | select the certificate constraint extension.    |
   |                                                 | Constraint extensions are described in          |
   |                                                 | `Standard X.509 v3 Certificate                  |
   |                                                 | Extensions <https://a                           |
   |                                                 | ccess.redhat.com/documentation/en-US/Red_Hat_Ce |
   |                                                 | rtificate_System/9/html/Administration_Guide/St |
   |                                                 | andard_X.509_v3_Certificate_Extensions.html>`__ |
   |                                                 | in Appendix A.3 of the\ *Red Hat Certificate    |
   |                                                 | System Administration Guide.*                   |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-3``                                          | Add an authority keyID extension to a           |
   |                                                 | certificate that is being created or added to a |
   |                                                 | database. This extension supports the           |
   |                                                 | identification of a particular certificate,     |
   |                                                 | from among multiple certificates associated     |
   |                                                 | with one subject name, as the correct issuer of |
   |                                                 | a certificate. The Certificate Database Tool    |
   |                                                 | will prompt you to select the authority keyID   |
   |                                                 | extension. Authority key ID extensions are      |
   |                                                 | described under `Standard X.509 v3 Certificate  |
   |                                                 | Extensions <http                                |
   |                                                 | s://access.redhat.com/documentation/en-us/red_h |
   |                                                 | at_certificate_system/9/html/administration_gui |
   |                                                 | de/standard_x.509_v3_certificate_extensions>`__ |
   |                                                 | in Appendix B.3 of the\ *Red Hat Certificate    |
   |                                                 | System Administration Guide.*                   |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-4``                                          | Add a CRL distribution point extension to a     |
   |                                                 | certificate that is being created or added to a |
   |                                                 | database. This extension identifies the URL of  |
   |                                                 | a certificate's associated certificate          |
   |                                                 | revocation list (CRL). The Certificate Database |
   |                                                 | Tool prompts you to enter the URL. CRL          |
   |                                                 | distribution point extensions are described in  |
   |                                                 | `Standard X.509 v3 Certificate                  |
   |                                                 | Extensions <https://a                           |
   |                                                 | ccess.redhat.com/documentation/en-US/Red_Hat_Ce |
   |                                                 | rtificate_System/9/html/Administration_Guide/St |
   |                                                 | andard_X.509_v3_Certificate_Extensions.html>`__ |
   |                                                 | in Appendix A.3 of the\ *Red Hat Certificate    |
   |                                                 | System Administration Guide.*                   |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-5``                                          | Add a Netscape certificate type extension to a  |
   |                                                 | certificate that is being created or added to   |
   |                                                 | the database. Netscape certificate type         |
   |                                                 | extensions are described in `Standard X.509 v3  |
   |                                                 | Certificate                                     |
   |                                                 | Extensions <https://a                           |
   |                                                 | ccess.redhat.com/documentation/en-US/Red_Hat_Ce |
   |                                                 | rtificate_System/9/html/Administration_Guide/St |
   |                                                 | andard_X.509_v3_Certificate_Extensions.html>`__ |
   |                                                 | in Appendix A.3 of the\ *Red Hat Certificate    |
   |                                                 | System Administration Guide.*                   |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-6``                                          | Add an extended key usage extension to a        |
   |                                                 | certificate that is being created or added to   |
   |                                                 | the database. Extended key usage extensions are |
   |                                                 | described in `Standard X.509 v3 Certificate     |
   |                                                 | Extensions <https://a                           |
   |                                                 | ccess.redhat.com/documentation/en-US/Red_Hat_Ce |
   |                                                 | rtificate_System/9/html/Administration_Guide/St |
   |                                                 | andard_X.509_v3_Certificate_Extensions.html>`__ |
   |                                                 | in Appendix A.3 of the\ *Red Hat Certificate    |
   |                                                 | System Administration Guide.*                   |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-7``\ *emailAddrs*                            | Add a comma-separated list of email addresses   |
   |                                                 | to the subject alternative name extension of a  |
   |                                                 | certificate or certificate request that is      |
   |                                                 | being created or added to the database. Subject |
   |                                                 | alternative name extensions are described in    |
   |                                                 | Section 4.2.1.7 of `RFC                         |
   |                                                 | 3                                               |
   |                                                 | 2800 <https://tools.ietf.org/html/rfc32800>`__. |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-8``\ *dns-names*                             | Add a comma-separated list of DNS names to the  |
   |                                                 | subject alternative name extension of a         |
   |                                                 | certificate or certificate request that is      |
   |                                                 | being created or added to the database. Subject |
   |                                                 | alternative name extensions are described in    |
   |                                                 | Section 4.2.1.7 of `RFC                         |
   |                                                 | 32800 <https://tools.ietf.org/html/rfc32800>`__ |
   +-------------------------------------------------+-------------------------------------------------+

`Usage <#usage>`__
------------------

.. container::

   The Certificate Database Tool's capabilities are grouped as follows, using these combinations of
   options and arguments. Options and arguments in square brackets are optional, those without
   square brackets are required.

   .. code:: notranslate

      -N [-d certdir ] 

   .. code:: notranslate

      -S -k rsa|dsa -n certname -s subject
      [-c issuer |-x] -t trustargs [-h tokenname ]
      [-m serial-number ] [-v valid-months ] [-w offset-months ]
      [-d certdir ] [-p phone ] [-f password-file ] [-1] [-2] [-3] [-4] 

   .. code:: notranslate

      -R -k rsa|dsa -s subject [-h tokenname ]
      [-d certdir ] [-p phone ] [-o output-file ] [-f password-file ] 

   .. code:: notranslate

      -C -c issuer [-f password-file ]
      [-h tokenname ] -i cert-request-file -o output-file [-m serial-number ]
      [-v valid-months ] [-w offset-months ] [-d certdir ] [-1] [-2] [-3]
      [-4] 

   .. code:: notranslate

      -A -n certname -t trustargs [-h tokenname ] [-d certdir ] [-a]
      [-i cert-request-file ] 

   .. code:: notranslate

      -L [-n certname ] [-d certdir ] [-r] [-a] 

   .. code:: notranslate

      -V -n certname -b validity-time -u certusage [-e] [-l] [-d certdir ] 

   .. code:: notranslate

      -M -n certname -t trustargs [-d certdir ] 

   .. code:: notranslate

      -H 

   -  Creating a new ``cert8.db`` file:
   -  Creating a new certificate and adding it to the database with one command:
   -  Making a separate certificate request:
   -  Creating a new binary certificate from a binary certificate request:
   -  Adding a certificate to an existing database:
   -  Listing all certificates or a named certificate:
   -  Validating a certificate:
   -  Modifying a certificate's trust attribute:
   -  Displaying a list of the options and arguments used by the Certificate Database Tool:

`Examples <#examples>`__
------------------------

.. container::

.. _creating_a_new_certificate_database:

`Creating a New Certificate Database <#creating_a_new_certificate_database>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This example creates a new certificate database (``cert8.db`` file) in the specified directory:

   .. code:: notranslate

      certutil -N -d certdir

   You must generate the associated ``key3.db`` and ``secmod.db`` files by using the Key Database
   Tool or other tools.

.. _listing_certificates_in_a_database:

`Listing Certificates in a Database <#listing_certificates_in_a_database>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This example lists all the certificates in the ``cert8.db`` file in the specified directory:

   .. code:: notranslate

      certutil -L -d certdir

   The Certificate Database Tool displays output similar to the following:

   | ``Certificate Name              Trust Attributes``
   | ``Uptime Group Plc. Class 1 CA        C,C,  VeriSign Class 1 Primary CA         ,C,  VeriSign Class 2 Primary CA         C,C,C  AT&T Certificate Services           C,C,  GTE CyberTrust Secure Server CA     C,,  Verisign/RSA Commercial CA          C,C,  AT&T Directory Services             C,C,  BelSign Secure Server CA            C,,  Verisign/RSA Secure Server CA       C,C,  GTE CyberTrust Root CA              C,C,  Uptime Group Plc. Class 4 CA        ,C,  VeriSign Class 3 Primary CA         C,C,C  Canada Post Corporation CA          C,C,  Integrion CA                        C,C,C  IBM World Registry CA               C,C,C  GTIS/PWGSC, Canada Gov. Web CA      C,C,  GTIS/PWGSC, Canada Gov. Secure CA   C,C,C  MCI Mall CA                         C,C,  VeriSign Class 4 Primary CA         C,C,C  KEYWITNESS, Canada CA               C,C,  BelSign Object Publishing CA        ,,C  BBN Certificate Services CA Root 1  C,C,  p    prohibited (explicitly distrusted)  P    Trusted peer  c    Valid CA  T    Trusted CA to issue client certs (implies c)  C    Trusted CA to issue server certs(for ssl only) (implies c)  u    User cert  w    Send warning``

.. _creating_a_certificate_request:

`Creating a Certificate Request <#creating_a_certificate_request>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This example generates a binary certificate request file named ``e95c.req`` in the specified
   directory:

   .. code:: notranslate

      certutil -R -s "CN=John Smith, O=Netscape, L=Mountain View, ST=California, C=US" -p "650-555-8888" -o mycert.req -d certdir

   Before it creates the request file, the Certificate Database Tool prompts you for a password:

   .. code:: notranslate

      Enter Password or Pin for "Communicator Certificate DB": 

.. _creating_a_certificate:

`Creating a Certificate <#creating_a_certificate>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   A valid certificate must be issued by a trusted CA. If a CA key pair is not available, you can
   create a self-signed certificate (for purposes of illustration) with the ``-x`` argument. This
   example creates a new binary, self-signed CA certificate named ``myissuer``, in the specified
   directory.

   .. code:: notranslate

      certutil -S -s "CN=My Issuer" -n myissuer -x -t "C,C,C" -1 -2 -5 -m 1234 -f password-file -d certdir

   The following example creates a new binary certificate named ``mycert.crt``, from a binary
   certificate request named ``mycert.req``, in the specified directory. It is issued by the
   self-signed certificate created above, ``myissuer``.

   .. code:: notranslate

      certutil -C -m 2345 -i mycert.req -o mycert.crt -c myissuer -d certdir

.. _adding_a_certificate_to_the_database:

`Adding a Certificate to the Database <#adding_a_certificate_to_the_database>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This example adds a certificate to the certificate database:

   .. code:: notranslate

      certutil -A -n jsmith@netscape.com -t "p,p,p" -i mycert.crt -d certdir

   You can see this certificate in the database with this command:

   .. code:: notranslate

      certutil -L -n jsmith@netscape.com -d certdir

   The Certificate Database Tool displays output similar to the following:

   | ``Certificate:    Data:      Version: 3 (0x2)      Serial Number: 0 (0x0)      Signature Algorithm: PKCS #1 MD5 With RSA Encryption      Issuer: CN=John Smith, O=Netscape, L=Mountain View, ST=California, C=US      Validity:          Not Before: Thu Mar 12 00:10:40 1998          Not After: Sat Sep 12 00:10:40 1998  Subject: CN=John Smith, O=Netscape, L=Mountain View, ST=California, C=US``
   | ``Subject Public Key Info:    Public Key Algorithm: PKCS #1 RSA Encryption    RSA Public Key:      Modulus:          00:da:53:23:58:00:91:6a:d1:a2:39:26:2f:06:3a:          38:eb:d4:c1:54:a3:62:00:b9:f0:7f:d6:00:76:aa:          18:da:6b:79:71:5b:d9:8a:82:24:07:ed:49:5b:33:          bf:c5:79:7c:f6:22:a7:18:66:9f:ab:2d:33:03:ec:          63:eb:9d:0d:02:1b:da:32:ae:6c:d4:40:95:9f:b3:          44:8b:8e:8e:a3:ae:ad:08:38:4f:2e:53:e9:e1:3f:          8e:43:7f:51:61:b9:0f:f3:a6:25:1e:0b:93:74:8f:          c6:13:a3:cd:51:40:84:0e:79:ea:b7:6b:d1:cc:6b:          78:d0:5d:da:be:2b:57:c2:6f      Exponent: 65537 (0x10001)  Signature Algorithm: PKCS #1 MD5 With RSA Encryption  Signature:    44:15:e5:ae:c4:30:2c:cd:60:89:f1:1d:22:ed:5e:5b:10:c8:    7e:5f:56:8c:b4:00:12:ed:5f:a4:6a:12:c3:0d:01:03:09:f2:    2f:e7:fd:95:25:47:80:ea:c1:25:5a:33:98:16:52:78:24:80:    c9:53:11:40:99:f5:bd:b8:e9:35:0e:5d:3e:38:6a:5c:10:d1:    c6:f9:54:af:28:56:62:f4:2f:b3:9b:50:e1:c3:a2:ba:27:ee:    07:9f:89:2e:78:5c:6d:46:b6:5e:99:de:e6:9d:eb:d9:ff:b2:    5f:c6:f6:c6:52:4a:d4:67:be:8d:fc:dd:52:51:8e:a2:d7:15:    71:3e``
   | ``Certificate Trust Flags:    SSL Flags:      Valid CA      Trusted CA    Email Flags:      Valid CA      Trusted CA    Object Signing Flags:      Valid CA      Trusted CA``

.. _validating_a_certificate:

`Validating a Certificate <#validating_a_certificate>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This example validates a certificate:

   .. code:: notranslate

      certutil -V -n jsmith@netscape.com -b 9803201212Z -u SR -e -l -d certdir

   The Certificate Database Tool shows results similar to

   .. code:: notranslate

      Certificate:'jsmith@netscape.com' is valid.

   or

   .. code:: notranslate

      UID=jsmith, E=jsmith@netscape.com, CN=John Smith, O=Netscape Communications Corp., C=US : Expired certificate

   or

   .. code:: notranslate

      UID=jsmith, E=jsmith@netscape.com, CN=John Smith, O=Netscape Communications Corp., C=US : Certificate not approved for this operation