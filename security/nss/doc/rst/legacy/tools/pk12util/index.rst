.. _mozilla_projects_nss_tools_pk12util:

NSS tools : pk12util
====================

.. container::

   | Name
   |    pk12util — Export and import keys and certificate to or from a PKCS #12
   |    file and the NSS database
   | Synopsis
   |    pk12util [-i p12File [-h tokenname] [-v] [common-options] ] [ -l p12File
   |    [-h tokenname] [-r] [common-options] ] [ -o p12File -n certname [-c
   |    keyCipher] [-C certCipher] [-m|--key_len keyLen] [-n|--cert_key_len
   |    certKeyLen] [common-options] ] [ common-options are: [-d [sql:]directory]
   |    [-P dbprefix] [-k slotPasswordFile|-K slotPassword] [-w
   |    p12filePasswordFile|-W p12filePassword] ]
   | Description
   |    The PKCS #12 utility, pk12util, enables sharing certificates among any
   |    server that supports PKCS#12. The tool can import certificates and keys
   |    from PKCS#12 files into security databases, export certificates, and list
   |    certificates and keys.
   | Options and Arguments
   |    Options
   |    -i p12file
   |            Import keys and certificates from a PKCS#12 file into a security
   |            database.
   |    -l p12file
   |            List the keys and certificates in PKCS#12 file.
   |    -o p12file
   |            Export keys and certificates from the security database to a
   |            PKCS#12 file.
   |    Arguments
   |    -n certname
   |            Specify the nickname of the cert and private key to export.
   |    -d [sql:]directory
   |            Specify the database directory into which to import to or export
   |            from certificates and keys.
   |            pk12util supports two types of databases: the legacy security
   |            databases (cert8.db, key3.db, and secmod.db) and new SQLite
   |            databases (cert9.db, key4.db, and pkcs11.txt). If the prefix sql:
   |            is not used, then the tool assumes that the given databases are in
   |            the old format.
   |    -P prefix
   |            Specify the prefix used on the certificate and key databases. This
   |            option is provided as a special case. Changing the names of the
   |            certificate and key databases is not recommended.
   |    -h tokenname
   |            Specify the name of the token to import into or export from.
   |    -v
   |            Enable debug logging when importing.
   |    -k slotPasswordFile
   |            Specify the text file containing the slot's password.
   |    -K slotPassword
   |            Specify the slot's password.
   |    -w p12filePasswordFile
   |            Specify the text file containing the pkcs #12 file password.
   |    -W p12filePassword
   |            Specify the pkcs #12 file password.
   |    -c keyCipher
   |            Specify the key encryption algorithm.
   |    -C certCipher
   |            Specify the key cert (overall package) encryption algorithm.
   |    -m \| --key-len keyLength
   |            Specify the desired length of the symmetric key to be used to
   |            encrypt the private key.
   |    -n \| --cert-key-len certKeyLength
   |            Specify the desired length of the symmetric key to be used to
   |            encrypt the certificates and other meta-data.
   |    -r
   |            Dumps all of the data in raw (binary) form. This must be saved as
   |            a DER file. The default is to return information in a pretty-print
   |            ASCII format, which displays the information about the
   |            certificates and public keys in the p12 file.
   | Return Codes
   |      o 0 - No error
   |      o 1 - User Cancelled
   |      o 2 - Usage error
   |      o 6 - NLS init error
   |      o 8 - Certificate DB open error
   |      o 9 - Key DB open error
   |      o 10 - File initialization error
   |      o 11 - Unicode conversion error
   |      o 12 - Temporary file creation error
   |      o 13 - PKCS11 get slot error
   |      o 14 - PKCS12 decoder start error
   |      o 15 - error read from import file
   |      o 16 - pkcs12 decode error
   |      o 17 - pkcs12 decoder verify error
   |      o 18 - pkcs12 decoder validate bags error
   |      o 19 - pkcs12 decoder import bags error
   |      o 20 - key db conversion version 3 to version 2 error
   |      o 21 - cert db conversion version 7 to version 5 error
   |      o 22 - cert and key dbs patch error
   |      o 23 - get default cert db error
   |      o 24 - find cert by nickname error
   |      o 25 - create export context error
   |      o 26 - PKCS12 add password itegrity error
   |      o 27 - cert and key Safes creation error
   |      o 28 - PKCS12 add cert and key error
   |      o 29 - PKCS12 encode error
   | Examples
   |    Importing Keys and Certificates
   |    The most basic usage of pk12util for importing a certificate or key is the
   |    PKCS#12 input file (-i) and some way to specify the security database
   |    being accessed (either -d for a directory or -h for a token).
   |  pk12util -i p12File [-h tokenname] [-v] [-d [sql:]directory] [-P dbprefix] [-k
     slotPasswordFile|-K slotPassword] [-w p12filePasswordFile|-W p12filePassword]
   |    For example:
   |  # pk12util -i /tmp/cert-files/users.p12 -d sql:/home/my/sharednssdb
   |  Enter a password which will be used to encrypt your keys.
   |  The password should be at least 8 characters long,
   |  and should contain at least one non-alphabetic character.
   |  Enter new password:
   |  Re-enter password:
   |  Enter password for PKCS12 file:
   |  pk12util: PKCS12 IMPORT SUCCESSFUL
   |    Exporting Keys and Certificates
   |    Using the pk12util command to export certificates and keys requires both
   |    the name of the certificate to extract from the database (-n) and the
   |    PKCS#12-formatted output file to write to. There are optional parameters
   |    that can be used to encrypt the file to protect the certificate material.
   |  pk12util -o p12File -n certname [-c keyCipher] [-C certCipher] [-m|--key_len keyLen]
     [-n|--cert_key_len certKeyLen] [-d [sql:]directory] [-P dbprefix] [-k slotPasswordFile|-K
     slotPassword] [-w p12filePasswordFile|-W p12filePassword]
   |    For example:
   |  # pk12util -o certs.p12 -n Server-Cert -d sql:/home/my/sharednssdb
   |  Enter password for PKCS12 file:
   |  Re-enter password:
   |    Listing Keys and Certificates
   |    The information in a .p12 file are not human-readable. The certificates
   |    and keys in the file can be printed (listed) in a human-readable
   |    pretty-print format that shows information for every certificate and any
   |    public keys in the .p12 file.
   |  pk12util -l p12File [-h tokenname] [-r] [-d [sql:]directory] [-P dbprefix] [-k
     slotPasswordFile|-K slotPassword] [-w p12filePasswordFile|-W p12filePassword]
   |    For example, this prints the default ASCII output:
   |  # pk12util -l certs.p12
   |  Enter password for PKCS12 file:
   |  Key(shrouded):
   |      Friendly Name: Thawte Freemail Member's Thawte Consulting (Pty) Ltd. ID
   |      Encryption algorithm: PKCS #12 V2 PBE With SHA-1 And 3KEY Triple DES-CBC
   |          Parameters:
   |              Salt:
   |                  45:2e:6a:a0:03:4d:7b:a1:63:3c:15:ea:67:37:62:1f
   |              Iteration Count: 1 (0x1)
   |  Certificate:
   |      Data:
   |          Version: 3 (0x2)
   |          Serial Number: 13 (0xd)
   |          Signature Algorithm: PKCS #1 SHA-1 With RSA Encryption
   |          Issuer: "E=personal-freemail@thawte.com,CN=Thawte Personal Freemail C
   |              A,OU=Certification Services Division,O=Thawte Consulting,L=Cape T
   |              own,ST=Western Cape,C=ZA"
   |  ....
   |    Alternatively, the -r prints the certificates and then exports them into
   |    separate DER binary files. This allows the certificates to be fed to
   |    another application that supports .p12 files. Each certificate is written
   |    to a sequentially-number file, beginning with file0001.der and continuing
   |    through file000N.der, incrementing the number for every certificate:
   |  # pk12util -l test.p12 -r
   |  Enter password for PKCS12 file:
   |  Key(shrouded):
   |      Friendly Name: Thawte Freemail Member's Thawte Consulting (Pty) Ltd. ID
   |      Encryption algorithm: PKCS #12 V2 PBE With SHA-1 And 3KEY Triple DES-CBC
   |          Parameters:
   |              Salt:
   |                  45:2e:6a:a0:03:4d:7b:a1:63:3c:15:ea:67:37:62:1f
   |              Iteration Count: 1 (0x1)
   |  Certificate    Friendly Name: Thawte Personal Freemail Issuing CA - Thawte Consulting
   |  Certificate    Friendly Name: Thawte Freemail Member's Thawte Consulting (Pty) Ltd. ID
   | Password Encryption
   |    PKCS#12 provides for not only the protection of the private keys but also
   |    the certificate and meta-data associated with the keys. Password-based
   |    encryption is used to protect private keys on export to a PKCS#12 file
   |    and, optionally, the entire package. If no algorithm is specified, the
   |    tool defaults to using PKCS12 V2 PBE with SHA1 and 3KEY Triple DES-cbc for
   |    private key encryption. PKCS12 V2 PBE with SHA1 and 40 Bit RC4 is the
   |    default for the overall package encryption when not in FIPS mode. When in
   |    FIPS mode, there is no package encryption.
   |    The private key is always protected with strong encryption by default.
   |    Several types of ciphers are supported.
   |    Symmetric CBC ciphers for PKCS#5 V2
   |            DES_CBC
   |               o RC2-CBC
   |               o RC5-CBCPad
   |               o DES-EDE3-CBC (the default for key encryption)
   |               o AES-128-CBC
   |               o AES-192-CBC
   |               o AES-256-CBC
   |               o CAMELLIA-128-CBC
   |               o CAMELLIA-192-CBC
   |               o CAMELLIA-256-CBC
   |    PKCS#12 PBE ciphers
   |            PKCS #12 PBE with Sha1 and 128 Bit RC4
   |               o PKCS #12 PBE with Sha1 and 40 Bit RC4
   |               o PKCS #12 PBE with Sha1 and Triple DES CBC
   |               o PKCS #12 PBE with Sha1 and 128 Bit RC2 CBC
   |               o PKCS #12 PBE with Sha1 and 40 Bit RC2 CBC
   |               o PKCS12 V2 PBE with SHA1 and 128 Bit RC4
   |               o PKCS12 V2 PBE with SHA1 and 40 Bit RC4 (the default for
   |                 non-FIPS mode)
   |               o PKCS12 V2 PBE with SHA1 and 3KEY Triple DES-cbc
   |               o PKCS12 V2 PBE with SHA1 and 2KEY Triple DES-cbc
   |               o PKCS12 V2 PBE with SHA1 and 128 Bit RC2 CBC
   |               o PKCS12 V2 PBE with SHA1 and 40 Bit RC2 CBC
   |    PKCS#5 PBE ciphers
   |            PKCS #5 Password Based Encryption with MD2 and DES CBC
   |               o PKCS #5 Password Based Encryption with MD5 and DES CBC
   |               o PKCS #5 Password Based Encryption with SHA1 and DES CBC
   |    With PKCS#12, the crypto provider may be the soft token module or an
   |    external hardware module. If the cryptographic module does not support the
   |    requested algorithm, then the next best fit will be selected (usually the
   |    default). If no suitable replacement for the desired algorithm can be
   |    found, the tool returns the error no security module can perform the
   |    requested operation.
   | NSS Database Types
   |    NSS originally used BerkeleyDB databases to store security information.
   |    The last versions of these legacy databases are:
   |      o cert8.db for certificates
   |      o key3.db for keys
   |      o secmod.db for PKCS #11 module information
   |    BerkeleyDB has performance limitations, though, which prevent it from
   |    being easily used by multiple applications simultaneously. NSS has some
   |    flexibility that allows applications to use their own, independent
   |    database engine while keeping a shared database and working around the
   |    access issues. Still, NSS requires more flexibility to provide a truly
   |    shared security database.
   |    In 2009, NSS introduced a new set of databases that are SQLite databases
   |    rather than BerkleyDB. These new databases provide more accessibility and
   |    performance:
   |      o cert9.db for certificates
   |      o key4.db for keys
   |      o pkcs11.txt, which is listing of all of the PKCS #11 modules contained
   |        in a new subdirectory in the security databases directory
   |    Because the SQLite databases are designed to be shared, these are the
   |    shared database type. The shared database type is preferred; the legacy
   |    format is included for backward compatibility.
   |    By default, the tools (certutil, pk12util, modutil) assume that the given
   |    security databases follow the more common legacy type. Using the SQLite
   |    databases must be manually specified by using the sql: prefix with the
   |    given security directory. For example:
   |  # pk12util -i /tmp/cert-files/users.p12 -d sql:/home/my/sharednssdb
   |    To set the shared database type as the default type for the tools, set the
   |    NSS_DEFAULT_DB_TYPE environment variable to sql:
   |  export NSS_DEFAULT_DB_TYPE="sql"
   |    This line can be set added to the ~/.bashrc file to make the change
   |    permanent.
   |    Most applications do not use the shared database by default, but they can
   |    be configured to use them. For example, this how-to article covers how to
   |    configure Firefox and Thunderbird to use the new shared NSS databases:
   |      o https://wiki.mozilla.org/NSS_Shared_DB_Howto
   |    For an engineering draft on the changes in the shared NSS databases, see
   |    the NSS project wiki:
   |      o https://wiki.mozilla.org/NSS_Shared_DB
   | See Also
   |    certutil (1)
   |    modutil (1)
   |    The NSS wiki has information on the new database design and how to
   |    configure applications to use it.
   |      o https://wiki.mozilla.org/NSS_Shared_DB_Howto
   |      o https://wiki.mozilla.org/NSS_Shared_DB
   | Additional Resources
   |    For information about NSS and other tools related to NSS (like JSS), check
   |    out the NSS project wiki at
   |   
     [1]\ `http://www.mozilla.org/projects/security/pki/nss/ <https://www.mozilla.org/projects/security/pki/nss/>`__.
     The NSS site relates
   |    directly to NSS code changes and releases.
   |    Mailing lists: https://lists.mozilla.org/listinfo/dev-tech-crypto
   |    IRC: Freenode at #dogtag-pki
   | Authors
   |    The NSS tools were written and maintained by developers with Netscape, Red
   |    Hat, and Sun.
   |    Authors: Elio Maldonado <emaldona@redhat.com>, Deon Lackey
   |    <dlackey@redhat.com>.
   | Copyright
   |    (c) 2010, Red Hat, Inc. Licensed under the GNU Public License version 2.
   | References
   |    Visible links
   |    1.
     `http://www.mozilla.org/projects/security/pki/nss/ <https://www.mozilla.org/projects/security/pki/nss/>`__