.. _mozilla_projects_nss_reference_nss_tools_:_certutil:

NSS tools : certutil
====================

.. container::

   | Name
   |    certutil — Manage keys and certificate in both NSS databases and other NSS tokens
   | Synopsis
   |    certutil [options] [[arguments]]
   | Description
   |    The Certificate Database Tool, certutil, is a command-line utility
   |    that can create and modify certificate and key databases.
   |    It can specifically list, generate, modify, or delete certificates, create or
   |    change the password, generate new public and private key pairs,
   |    display the contents of the key database, or delete key pairs within  the key database.
   |    Certificate issuance, part of the key and certificate management process, requires that
   |    keys and certificates be created in the key database. This document discusses certificate
   |    and key database management. For information on the  security module database management,
   |    see the modutil manpage.
   | Options and Arguments
   |    Running certutil always requires one and only one command option to
   |    specify the type of certificate operation. Each option may take arguments,
   |    anywhere from none to multiple arguments. The command option -H will list
   |    all the command options available and their relevant arguments.
   |    Command Options
   |    -A
   |           Add an existing certificate to a certificate database.
   |           The certificate database should already exist; if one is
   |           not present, this command option will initialize one by default.
   |    -B
   |           Run a series of commands from the specified batch file.
   |           This requires the -i argument.
   |    -C
   |           Create a new binary certificate file from a binary
   |           certificate request file. Use the -i argument to specify
   |           the certificate request file. If this argument is not
   |           used, certutil prompts for a filename.
   |    -D
   |           Delete a certificate from the certificate database.

   |   --rename
   |          Change the database nickname of a certificate.

   |
   |    -E
   |           Add an email certificate to the certificate database.
   |    -F
   |           Delete a private key from a key database. Specify the
   |           key to delete with the -n argument. Specify the database
   |           from which to delete the key with the -d argument. Use
   |           the -k argument to specify explicitly whether to delete
   |           a DSA, RSA, or ECC key. If you don't use the -k
   |           argument, the option looks for an RSA key matching the
   |           specified nickname.
   |           When you delete keys, be sure to also remove any
   |           certificates associated with those keys from the
   |           certificate database, by using -D. Some smart cards (for
   |           example, the Litronic card) do not let you remove a
   |           public key you have generated. In such a case, only the
   |           private key is deleted from the key pair. You can
   |           display the public key with the command certutil -K -h
   |           tokenname.
   |    -G
   |           Generate a new public and private key pair within a key
   |           database. The key database should already exist; if one
   |           is not present, this option will initialize one by
   |           default. Some smart cards (for example, the Litronic
   |           card) can store only one key pair. If you create a new
   |           key pair for such a card, the previous pair is
   |           overwritten.
   |    -H
   |           Display a list of the options and arguments used by the
   |           Certificate Database Tool.
   |    -K
   |           List the key ID of keys in the key database. A key ID is
   |           the modulus of the RSA key or the publicValue of the DSA
   |           key. IDs are displayed in hexadecimal ("0x" is not
   |           shown).
   |    -L
   |           List all the certificates, or display information about
   |           a named certificate, in a certificate database. Use the
   |           -h tokenname argument to specify the certificate
   |           database on a particular hardware or software token.
   |    -M
   |           Modify a certificate's trust attributes using the values
   |           of the -t argument.
   |    -N
   |           Create new certificate and key databases.
   |    -O
   |           Print the certificate chain.
   |    -R
   |           Create a certificate request file that can be submitted
   |           to a Certificate Authority (CA) for processing into a
   |           finished certificate. Output defaults to standard out
   |           unless you use -o output-file argument. Use the -a
   |           argument to specify ASCII output.
   |    -S
   |           Create an individual certificate and add it to a
   |           certificate database.
   |    -T
   |           Reset the key database or token.
   |    -U
   |           List all available modules or print a single named
   |           module.
   |    -V
   |           Check the validity of a certificate and its attributes.
   |    -W
   |           Change the password to a key database.
   |    --merge
   |           Merge two databases into one.
   |    --upgrade-merge
   |           Upgrade an old database and merge it into a new
   |           database. This is used to migrate legacy NSS databases
   |           (cert8.db and key3.db) into the newer SQLite databases
   |           (cert9.db and key4.db).
   |    Arguments
   |    Arguments modify a command option and are usually lower case, numbers, or symbols.
   |    -a
   |           Use ASCII format or allow the use of ASCII format for
   |           input or output. This formatting follows RFC 1113. For
   |           certificate requests, ASCII output defaults to standard
   |           output unless redirected.
   |    -b validity-time
   |           Specify a time at which a certificate is required to be
   |           valid. Use when checking certificate validity with the
   |           -V option. The format of the validity-time argument is
   |           YYMMDDHHMMSS[+HHMM|-HHMM|Z], which allows offsets to be
   |           set relative to the validity end time. Specifying
   |           seconds (SS) is optional. When specifying an explicit
   |           time, use a Z at the end of the term, YYMMDDHHMMSSZ, to
   |           close it. When specifying an offset time, use
   |           YYMMDDHHMMSS+HHMM or YYMMDDHHMMSS-HHMM for adding or
   |           subtracting time, respectively.
   |           If this option is not used, the validity check defaults
   |           to the current system time.
   |    -c issuer
   |           Identify the certificate of the CA from which a new
   |           certificate will derive its authenticity. Use the exact
   |           nickname or alias of the CA certificate, or use the CA's
   |           email address. Bracket the issuer string with quotation
   |           marks if it contains spaces.
   |    -d [prefix]directory
   |           Specify the database directory containing the
   |           certificate and key database files.
   |           certutil supports two types of databases: the legacy
   |           security databases (cert8.db, key3.db, and secmod.db)
   |           and new SQLite databases (cert9.db, key4.db, and
   |           pkcs11.txt).

              NSS recognizes the following prefixes:

              ·   sql: requests the newer database

              ·   dbm: requests the legacy database

   |            If no prefix is specified the default type is retrieved from NSS_DEFAULT_DB_TYPE. If
     NSS_DEFAULT_DB_TYPE is not set
   |            then dbm: is the default.

   |     --dump-ext-val OID
   |            For single cert, print binary DER encoding of extension OID.
   |    -e
   |           Check a certificate's signature during the process of
   |           validating a certificate.

   |        --email email-address
   |            Specify the email address of a certificate to list. Used with the -L command option.

   |        --extGeneric OID:critical-flag:filename[,OID:critical-flag:filename]...
   |            Add one or multiple extensions that certutil cannot encode yet, by loading their
     encodings from external files.

              ·   OID (example): 1.2.3.4

              ·   critical-flag: critical or not-critical

              ·   filename: full path to a file containing an encoded extension

   |
   |    -f password-file
   |           Specify a file that will automatically supply the
   |           password to include in a certificate or to access a
   |           certificate database. This is a plain-text file
   |           containing one password. Be sure to prevent unauthorized
   |           access to this file.
   |    -g keysize
   |           Set a key size to use when generating new public and
   |           private key pairs. The minimum is 512 bits and the
   |           maximum is 16384 bits. The default is 2048 bits. Any size
   |           between the minimum and maximum is allowed.
   |    -h tokenname
   |           Specify the name of a token to use or act on. Unless
   |           specified otherwise the default token is an internal
   |           slot.
   |    -i input_file
   |           Pass an input file to the command. Depending on the
   |           command option, an input file can be a specific
   |           certificate, a certificate request file, or a batch file
   |           of commands.
   |    -k rsa|dsa|ec|all
   |           Specify the type of a key. The valid options are RSA,
   |           DSA, ECC, or all. The default value is rsa. Specifying
   |           the type of key can avoid mistakes caused by duplicate
   |           nicknames.
   |    -k key-type-or-id
   |            Specify the type or specific ID of a key.

   |            The valid key type options are rsa, dsa, ec, or all. The default value is rsa.
     Specifying the type of key can avoid
   |            mistakes caused by duplicate nicknames. Giving a key type generates a new key pair;
     giving the ID of an existing key
   |            reuses that key pair (which is required to renew certificates).
   |    -l
   |           Display detailed information when validating a
   |           certificate with the -V option.
   |    -m serial-number
   |           Assign a unique serial number to a certificate being created. This operation should
     be performed by a CA. If no
   |            serial number is provided a default serial number is made from the current time.
     Serial numbers are limited to
   |            integers.
   |    -n nickname
   |           Specify the nickname of a certificate or key to list,
   |           create, add to a database, modify, or validate. Bracket
   |           the nickname string with quotation marks if it contains
   |           spaces.
   |    -o output-file
   |           Specify the output file name for new certificates or
   |           binary certificate requests. Bracket the output-file
   |           string with quotation marks if it contains spaces. If
   |           this argument is not used the output destination
   |           defaults to standard output.
   |    -P dbPrefix
   |           Specify the prefix used on the certificate and key
   |           database file. This argument is provided to support
   |           legacy servers. Most applications do not use a database prefix.
   |    -p phone
   |           Specify a contact telephone number to include in new
   |           certificates or certificate requests. Bracket this
   |           string with quotation marks if it contains spaces.
   |    -q pqgfile or curve-name
   |            Read an alternate PQG value from the specified file when generating DSA key pairs.
   |            If this argument is not used,certutil generates its own PQG value. PQG files are
     created with a separate DSA utility.

              Elliptic curve name is one of the ones from SUITE B: nistp256, nistp384, nistp521

   |            If NSS has been compiled with support curves outside of SUITE B: sect163k1,
     nistk163, sect163r1, sect163r2, nistb163,
   |            sect193r1, sect193r2, sect233k1, nistk233, sect233r1, nistb233, sect239k1,
     sect283k1, nistk283, sect283r1, nistb283,
   |            sect409k1, nistk409, sect409r1, nistb409, sect571k1, nistk571, sect571r1, nistb571,
     secp160k1, secp160r1, secp160r2,
   |            secp192k1, secp192r1, nistp192, secp224k1, secp224r1, nistp224, secp256k1,
     secp256r1, secp384r1, secp521r1,
   |            prime192v1, prime192v2, prime192v3, prime239v1, prime239v2, prime239v3, c2pnb163v1,
     c2pnb163v2, c2pnb163v3,
   |            c2pnb176v1, c2tnb191v1, c2tnb191v2, c2tnb191v3, c2pnb208w1, c2tnb239v1, c2tnb239v2,
     c2tnb239v3, c2pnb272w1,
   |            c2pnb304w1, c2tnb359w1, c2pnb368w1, c2tnb431r1, secp112r1, secp112r2, secp128r1,
     secp128r2, sect113r1, sect113r2
   |            sect131r1, sect131r2

   |
   |    -r
   |           Display a certificate's binary DER encoding when listing
   |           information about that certificate with the -L option.
   |    -s subject
   |           Identify a particular certificate owner for new
   |           certificates or certificate requests. Bracket this
   |           string with quotation marks if it contains spaces. The
   |           subject identification format follows RFC #1485.
   |    -t trustargs
   |           Specify the trust attributes to modify in an existing
   |           certificate or to apply to a certificate when creating
   |           it or adding it to a database. There are three available
   |           trust categories for each certificate, expressed in the
   |           order SSL, email, object signing for each trust setting.
   |           In each category position, use none, any, or all of the
   |           attribute codes:
   |           + p - Valid peer
   |           + P - Trusted peer (implies p)
   |           + c - Valid CA
   |           + T - Trusted CA to issue client certificates (implies
   |             c)
   |           + C - Trusted CA to issue server certificates (SSL only)
   |             (implies c)
   |           + u - Certificate can be used for authentication or
   |             signing
   |           + w - Send warning (use with other attributes to include
   |             a warning when the certificate is used in that
   |             context)
   |           The attribute codes for the categories are separated by
   |           commas, and the entire set of attributes enclosed by
   |           quotation marks. For example:
   |           -t "TC,C,T"
   |           Use the -L option to see a list of the current
   |           certificates and trust attributes in a certificate
   |           database.

   |            Note that the output of the -L option may include "u" flag, which means that there
     is a private key associated with
   |            the certificate. It is a dynamic flag and you cannot set it with certutil.
   |    -u certusage
   |           Specify a usage context to apply when validating a
   |           certificate with the -V option.
   |           The contexts are the following:

              ·   C (as an SSL client)

              ·   V (as an SSL server)

              ·   L (as an SSL CA)

              ·   A (as Any CA)

              ·   Y (Verify CA)

              ·   S (as an email signer)

              ·   R (as an email recipient)

              ·   O (as an OCSP status responder)

              ·   J (as an object signer)

   |
   |    -v valid-months
   |           Set the number of months a new certificate will be
   |           valid. The validity period begins at the current system
   |           time unless an offset is added or subtracted with the -w
   |           option. If this argument is not used, the default
   |           validity period is three months. When this argument is
   |           used, the default three-month period is automatically
   |           added to any value given in the valid-month argument.
   |           For example, using this option to set a value of 3 would
   |           cause 3 to be added to the three-month default, creating
   |           a validity period of six months. You can use negative
   |           values to reduce the default period. For example,
   |           setting a value of -2 would subtract 2 from the default
   |           and create a validity period of one month.
   |    -w offset-months
   |           Set an offset from the current system time, in months,
   |           for the beginning of a certificate's validity period.
   |           Use when creating the certificate or adding it to a
   |           database. Express the offset in integers, using a minus
   |           sign (-) to indicate a negative offset. If this argument
   |           is not used, the validity period begins at the current
   |           system time. The length of the validity period is set
   |           with the -v argument.
   |    -X
   |           Force the key and certificate database to open in
   |           read-write mode. This is used with the -U and -L command
   |           options.
   |    -x
   |           Use certutil to generate the signature for a certificate
   |           being created or added to a database, rather than
   |           obtaining a signature from a separate CA.
   |    -y exp
   |           Set an alternate exponent value to use in generating a
   |           new RSA public key for the database, instead of the
   |           default value of 65537. The available alternate values
   |           are 3 and 17.
   |    -z noise-file
   |           Read a seed value from the specified file to generate a
   |           new private and public key pair. This argument makes it
   |           possible to use hardware-generated seed values or
   |           manually create a value from the keyboard. The minimum
   |           file size is 20 bytes.
   |    -0 SSO_password
   |           Set a site security officer password on a token.
   |    -1 \| --keyUsage keyword,keyword
   |           Set a Netscape Certificate Type Extension in the
   |           certificate. There are several available keywords:
   |           + digital signature
   |           + nonRepudiation
   |           + keyEncipherment
   |           + dataEncipherment
   |           + keyAgreement
   |           + certSigning
   |           + crlSigning
   |           + critical
   |    -2
   |           Add a basic constraint extension to a certificate that
   |           is being created or added to a database. This extension
   |           supports the certificate chain verification process.
   |           certutil prompts for the certificate constraint
   |           extension to select.
   |           X.509 certificate extensions are described in RFC 5280.
   |    -3
   |           Add an authority key ID extension to a certificate that
   |           is being created or added to a database. This extension
   |           supports the identification of a particular certificate,
   |           from among multiple certificates associated with one
   |           subject name, as the correct issuer of a certificate.
   |           The Certificate Database Tool will prompt you to select
   |           the authority key ID extension.
   |           X.509 certificate extensions are described in RFC 5280.
   |    -4
   |           Add a CRL distribution point extension to a certificate
   |           that is being created or added to a database. This
   |           extension identifies the URL of a certificate's
   |           associated certificate revocation list (CRL). certutil
   |           prompts for the URL.
   |           X.509 certificate extensions are described in RFC 5280.
   |    -5 \| --nsCertType keyword,keyword
   |           Add a Netscape certificate type extension to a
   |           certificate that is being created or added to the
   |           database. There are several available keywords:
   |           + sslClient
   |           + sslServer
   |           + smime
   |           + objectSigning
   |           + sslCA
   |           + smimeCA
   |           + objectSigningCA
   |           + critical
   |           X.509 certificate extensions are described in RFC 5280.
   |    -6 \| --extKeyUsage keyword,keyword
   |           Add an extended key usage extension to a certificate
   |           that is being created or added to the database. Several
   |           keywords are available:
   |           + serverAuth
   |           + clientAuth
   |           + codeSigning
   |           + emailProtection
   |           + timeStamp
   |           + ocspResponder
   |           + stepUp
   |           + critical
   |           X.509 certificate extensions are described in RFC 5280.
   |    -7 emailAddrs
   |           Add a comma-separated list of email addresses to the
   |           subject alternative name extension of a certificate or
   |           certificate request that is being created or added to
   |           the database. Subject alternative name extensions are
   |           described in Section 4.2.1.7 of RFC 3280.
   |    -8 dns-names
   |           Add a comma-separated list of DNS names to the subject
   |           alternative name extension of a certificate or
   |           certificate request that is being created or added to
   |           the database. Subject alternative name extensions are
   |           described in Section 4.2.1.7 of RFC 3280.
   |    --extAIA
   |           Add the Authority Information Access extension to the
   |           certificate. X.509 certificate extensions are described
   |           in RFC 5280.
   |    --extSIA
   |           Add the Subject Information Access extension to the
   |           certificate. X.509 certificate extensions are described
   |           in RFC 5280.
   |    --extCP
   |           Add the Certificate Policies extension to the
   |           certificate. X.509 certificate extensions are described
   |           in RFC 5280.
   |    --extPM
   |           Add the Policy Mappings extension to the certificate.
   |           X.509 certificate extensions are described in RFC 5280.
   |    --extPC
   |           Add the Policy Constraints extension to the certificate.
   |           X.509 certificate extensions are described in RFC 5280.
   |    --extIA
   |           Add the Inhibit Any Policy Access extension to the
   |           certificate. X.509 certificate extensions are described
   |           in RFC 5280.
   |    --extSKID
   |           Add the Subject Key ID extension to the certificate.
   |           X.509 certificate extensions are described in RFC 5280.
   |    --source-dir certdir
   |           Identify the certificate database directory to upgrade.
   |    --source-prefix certdir
   |           Give the prefix of the certificate and key databases to
   |           upgrade.
   |    --upgrade-id uniqueID
   |           Give the unique ID of the database to upgrade.
   |    --upgrade-token-name name
   |           Set the name of the token to use while it is being
   |           upgraded.
   |    -@ pwfile
   |           Give the name of a password file to use for the database
   |           being upgraded.
   | Usage and Examples
   |    Most of the command options in the examples listed here have
   |    more arguments available. The arguments included in these
   |    examples are the most common ones or are used to illustrate a
   |    specific scenario. Use the -H option to show the complete list
   |    of arguments for each command option.
   |    Creating New Security Databases
   |    Certificates, keys, and security modules related to managing
   |    certificates are stored in three related databases:
   |      \* cert8.db or cert9.db
   |      \* key3.db or key4.db
   |      \* secmod.db or pkcs11.txt
   |    These databases must be created before certificates or keys can
   |    be generated.
   | certutil -N -d [sql:]directory
   |    Creating a Certificate Request
   |    A certificate request contains most or all of the information
   |    that is used to generate the final certificate. This request is
   |    submitted separately to a certificate authority and is then
   |    approved by some mechanism (automatically or by human review).
   |    Once the request is approved, then the certificate is
   |    generated.
   | $ certutil -R -k key-type-or-id [-q pqgfile|curve-name] -g key-size -s s
   | ubject [-h tokenname] -d [sql:]directory [-p phone] [-o output-file] [-a
   | ]
   |    The -R command options requires four arguments:
   |      \* -k to specify either the key type to generate or, when
   |        renewing a certificate, the existing key pair to use
   |      \* -g to set the keysize of the key to generate
   |      \* -s to set the subject name of the certificate
   |      \* -d to give the security database directory
   |    The new certificate request can be output in ASCII format (-a)
   |    or can be written to a specified file (-o).
   |    For example:
   | $ certutil -R -k ec -q nistb409 -g 512 -s "CN=John Smith,O=Example Corp,
   | L=Mountain View,ST=California,C=US" -d sql:/home/my/sharednssdb -p 650-5
   | 55-0123 -a -o cert.cer
   | Generating key.  This may take a few moments...
   | Certificate request generated by Netscape
   | Phone: 650-555-0123
   | Common Name: John Smith
   | Email: (not ed)
   | Organization: Example Corp
   | State: California
   | Country: US
   | -----BEGIN NEW CERTIFICATE REQUEST-----
   | MIIBIDCBywIBADBmMQswCQYDVQQGEwJVUzETMBEGA1UECBMKQ2FsaWZvcm5pYTEW
   | MBQGA1UEBxMNTW91bnRhaW4gVmlldzEVMBMGA1UEChMMRXhhbXBsZSBDb3JwMRMw
   | EQYDVQQDEwpKb2huIFNtaXRoMFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBAMVUpDOZ
   | KmHnOx7reP8Cc0Lk+fFWEuYIDX9W5K/BioQOKvEjXyQZhit9aThzBVMoSf1Y1S8J
   | CzdUbCg1+IbnXaECAwEAAaAAMA0GCSqGSIb3DQEBBQUAA0EAryqZvpYrUtQ486Ny
   | qmtyQNjIi1F8c1Z+TL4uFYlMg8z6LG/J/u1E5t1QqB5e9Q4+BhRbrQjRR1JZx3tB
   | 1hP9Gg==
   | -----END NEW CERTIFICATE REQUEST-----
   |    Creating a Certificate
   |    A valid certificate must be issued by a trusted CA. This can be
   |    done by specifying a CA certificate (-c) that is stored in the
   |    certificate database. If a CA key pair is not available, you
   |    can create a self-signed certificate using the -x argument with
   |    the -S command option.
   | $ certutil -S -k rsa|dsa|ec -n certname -s subject [-c issuer \|-x] -t tr
   | ustargs -d [sql:]directory [-m serial-number] [-v valid-months] [-w offs
   | et-months] [-p phone] [-1] [-2] [-3] [-4] [-5 keyword] [-6 keyword] [-7
   | emailAddress] [-8 dns-names] [--extAIA] [--extSIA] [--extCP] [--extPM] [
   | --extPC] [--extIA] [--extSKID]
   |    The series of numbers and --ext\* options set certificate
   |    extensions that can be added to the certificate when it is
   |    generated by the CA.
   |    For example, this creates a self-signed certificate:
   | $ certutil -S -s "CN=Example CA" -n my-ca-cert -x -t "C,C,C" -1 -2 -5 -m
   |  3650
   |    From there, new certificates can reference the self-signed
   |    certificate:
   | $ certutil -S -s "CN=My Server Cert" -n my-server-cert -c "my-ca-cert" -
   | t "u,u,u" -1 -5 -6 -8 -m 730
   |    Generating a Certificate from a Certificate Request
   |    When a certificate request is created, a certificate can be
   |    generated by using the request and then referencing a
   |    certificate authority signing certificate (the issuer specified
   |    in the -c argument). The issuing certificate must be in the
   |    certificate database in the specified directory.
   | certutil -C -c issuer -i cert-request-file -o output-file [-m serial-num
   | ber] [-v valid-months] [-w offset-months] -d [sql:]directory [-1] [-2] [
   | -3] [-4] [-5 keyword] [-6 keyword] [-7 emailAddress] [-8 dns-names]
   |    For example:
   | $ certutil -C -c "my-ca-cert" -i /home/certs/cert.req -o cert.cer -m 010
   |  -v 12 -w 1 -d sql:/home/my/sharednssdb -1 nonRepudiation,dataEncipherme
   | nt -5 sslClient -6 clientAuth -7 jsmith@example.com
   |    Generating Key Pairs
   |    Key pairs are generated automatically with a certificate
   |    request or certificate, but they can also be generated
   |    independently using the -G command option.
   | certutil -G -d [sql:]directory \| -h tokenname -k key-type -g key-size [-
   | y exponent-value] -q pqgfile|curve-name
   |    For example:
   | $ certutil -G -h lunasa -k ec -g 256 -q sect193r2
   |    Listing Certificates
   |    The -L command option lists all of the certificates listed in
   |    the certificate database. The path to the directory (-d) is
   |    required.
   | $ certutil -L -d sql:/home/my/sharednssdb
   | Certificate Nickname                                         Trust Attri
   | butes
   |                                                              SSL,S/MIME,
   | JAR/XPI
   | CA Administrator of Instance pki-ca1's Example Domain ID     u,u,u
   | TPS Administrator's Example Domain ID                        u,u,u
   | Google Internet Authority                                    ,,
   | Certificate Authority - Example Domain                       CT,C,C
   |    Using additional arguments with -L can return and print the
   |    information for a single, specific certificate. For example,
   |    the -n argument passes the certificate name, while the -a
   |    argument prints the certificate in ASCII format:
   | $ certutil -L -d sql:/home/my/sharednssdb -a -n "Certificate Authority -
   |  Example Domain"
   | -----BEGIN CERTIFICATE-----
   | MIIDmTCCAoGgAwIBAgIBATANBgkqhkiG9w0BAQUFADA5MRcwFQYDVQQKEw5FeGFt
   | cGxlIERvbWFpbjEeMBwGA1UEAxMVQ2VydGlmaWNhdGUgQXV0aG9yaXR5MB4XDTEw
   | MDQyOTIxNTY1OFoXDTEyMDQxODIxNTY1OFowOTEXMBUGA1UEChMORXhhbXBsZSBE
   | b21haW4xHjAcBgNVBAMTFUNlcnRpZmljYXRlIEF1dGhvcml0eTCCASIwDQYJKoZI
   | hvcNAQEBBQADggEPADCCAQoCggEBAO/bqUli2KwqXFKmMMG93KN1SANzNTXA/Vlf
   | Tmrih3hQgjvR1ktIY9aG6cB7DSKWmtHp/+p4PUCMqL4ZrSGt901qxkePyZ2dYmM2
   | RnelK+SEUIPiUtoZaDhNdiYsE/yuDE8vQWj0vHCVL0w72qFUcSQ/WZT7FCrnUIUI
   | udeWnoPSUn70gLhcj/lvxl7K9BHyD4Sq5CzktwYtFWLiiwV+ZY/Fl6JgbGaQyQB2
   | bP4iRMfloGqsxGuB1evWVDF1haGpFDSPgMnEPSLg3/3dXn+HDJbZ29EU8/xKzQEb
   | 3V0AHKbu80zGllLEt2Zx/WDIrgJEN9yMfgKFpcmL+BvIRsmh0VsCAwEAAaOBqzCB
   | qDAfBgNVHSMEGDAWgBQATgxHQyRUfKIZtdp55bZlFr+tFzAPBgNVHRMBAf8EBTAD
   | AQH/MA4GA1UdDwEB/wQEAwIBxjAdBgNVHQ4EFgQUAE4MR0MkVHyiGbXaeeW2ZRa/
   | rRcwRQYIKwYBBQUHAQEEOTA3MDUGCCsGAQUFBzABhilodHRwOi8vbG9jYWxob3N0
   | LmxvY2FsZG9tYWluOjkxODAvY2Evb2NzcDANBgkqhkiG9w0BAQUFAAOCAQEAi8Gk
   | L3XO43u7/TDOeEsWPmq+jZsDZ3GZ85Ajt3KROLWeKVZZZa2E2Hnsvf2uXbk5amKe
   | lRxdSeRH9g85pv4KY7Z8xZ71NrI3+K3uwmnqkc6t0hhYb1mw/gx8OAAoluQx3biX
   | JBDxjI73Cf7XUopplHBjjiwyGIJUO8BEZJ5L+TF4P38MJz1snLtzZpEAX5bl0U76
   | bfu/tZFWBbE8YAWYtkCtMcalBPj6jn2WD3M01kGozW4mmbvsj1cRB9HnsGsqyHCu
   | U0ujlL1H/RWcjn607+CTeKH9jLMUqCIqPJNOa+kq/6F7NhNRRiuzASIbZc30BZ5a
   | nI7q5n1USM3eWQlVXw==
   | -----END CERTIFICATE-----
   |    Listing Keys
   |    Keys are the original material used to encrypt certificate
   |    data. The keys generated for certificates are stored
   |    separately, in the key database.
   |    To list all keys in the database, use the -K command option and
   |    the (required) -d argument to give the path to the directory.
   | $ certutil -K -d sql:/home/my/sharednssdb
   | certutil: Checking token "NSS Certificate DB" in slot "NSS User Private
   | Key and Certificate Services                  "
   | < 0> rsa      455a6673bde9375c2887ec8bf8016b3f9f35861d   Thawte Freemail
   |  Member's Thawte Consulting (Pty) Ltd. ID
   | < 1> rsa      40defeeb522ade11090eacebaaf1196a172127df   Example Domain
   | Administrator Cert
   | < 2> rsa      1d0b06f44f6c03842f7d4f4a1dc78b3bcd1b85a5   John Smith user
   |  cert
   |    There are ways to narrow the keys listed in the search results:
   |      \* To return a specific key, use the -n name argument with the
   |        name of the key.
   |      \* If there are multiple security devices loaded, then the -h
   |        tokenname argument can search a specific token or all
   |        tokens.
   |      \* If there are multiple key types available, then the -k
   |        key-type argument can search a specific type of key, like
   |        RSA, DSA, or ECC.
   |    Listing Security Modules
   |    The devices that can be used to store certificates -- both
   |    internal databases and external devices like smart cards -- are
   |    recognized and used by loading security modules. The -U command
   |    option lists all of the security modules listed in the
   |    secmod.db database. The path to the directory (-d) is required.
   | $ certutil -U -d sql:/home/my/sharednssdb
   |     slot: NSS User Private Key and Certificate Services
   |    token: NSS Certificate DB
   |     slot: NSS Internal Cryptographic Services
   |    token: NSS Generic Crypto Services
   |    Adding Certificates to the Database
   |    Existing certificates or certificate requests can be added
   |    manually to the certificate database, even if they were
   |    generated elsewhere. This uses the -A command option.
   | certutil -A -n certname -t trustargs -d [sql:]directory [-a] [-i input-f
   | ile]
   |    For example:
   | $ certutil -A -n "CN=My SSL Certificate" -t "u,u,u" -d sql:/home/my/shar
   | ednssdb -i /home/example-certs/cert.cer
   |    A related command option, -E, is used specifically to add email
   |    certificates to the certificate database. The -E command has
   |    the same arguments as the -A command. The trust arguments for
   |    certificates have the format SSL,S/MIME,Code-signing, so the
   |    middle trust settings relate most to email certificates (though
   |    the others can be set). For example:
   | $ certutil -E -n "CN=John Smith Email Cert" -t ",Pu," -d sql:/home/my/sh
   | arednssdb -i /home/example-certs/email.cer
   |    Deleting Certificates to the Database
   |    Certificates can be deleted from a database using the -D
   |    option. The only required options are to give the security
   |    database directory and to identify the certificate nickname.
   | certutil -D -d [sql:]directory -n "nickname"
   |    For example:
   | $ certutil -D -d sql:/home/my/sharednssdb -n "my-ssl-cert"
   |    Validating Certificates
   |    A certificate contains an expiration date in itself, and
   |    expired certificates are easily rejected. However, certificates
   |    can also be revoked before they hit their expiration date.
   |    Checking whether a certificate has been revoked requires
   |    validating the certificate. Validation can also be used to
   |    ensure that the certificate is only used for the purposes it
   |    was initially issued for. Validation is carried out by the -V
   |    command option.
   | certutil -V -n certificate-name [-b time] [-e] [-u cert-usage] -d [sql:]
   | directory
   |    For example, to validate an email certificate:
   | $ certutil -V -n "John Smith's Email Cert" -e -u S,R -d sql:/home/my/sha
   | rednssdb
   |    Modifying Certificate Trust Settings
   |    The trust settings (which relate to the operations that a
   |    certificate is allowed to be used for) can be changed after a
   |    certificate is created or added to the database. This is
   |    especially useful for CA certificates, but it can be performed
   |    for any type of certificate.
   | certutil -M -n certificate-name -t trust-args -d [sql:]directory
   |    For example:
   | $ certutil -M -n "My CA Certificate" -d sql:/home/my/sharednssdb -t "CTu
   | ,CTu,CTu"
   |    Printing the Certificate Chain
   |    Certificates can be issued in chains because every certificate
   |    authority itself has a certificate; when a CA issues a
   |    certificate, it essentially stamps that certificate with its
   |    own fingerprint. The -O prints the full chain of a certificate,
   |    going from the initial CA (the root CA) through ever
   |    intermediary CA to the actual certificate. For example, for an
   |    email certificate with two CAs in the chain:
   | $ certutil -d sql:/home/my/sharednssdb -O -n "jsmith@example.com"
   | "Builtin Object Token:Thawte Personal Freemail CA" [E=personal-freemail@
   | thawte.com,CN=Thawte Personal Freemail CA,OU=Certification Services Divi
   | sion,O=Thawte Consulting,L=Cape Town,ST=Western Cape,C=ZA]
   |   "Thawte Personal Freemail Issuing CA - Thawte Consulting" [CN=Thawte P
   | ersonal Freemail Issuing CA,O=Thawte Consulting (Pty) Ltd.,C=ZA]
   |     "(null)" [E=jsmith@example.com,CN=Thawte Freemail Member]
   |    Resetting a Token
   |    The device which stores certificates -- both external hardware
   |    devices and internal software databases -- can be blanked and
   |    reused. This operation is performed on the device which stores
   |    the data, not directly on the security databases, so the
   |    location must be referenced through the token name (-h) as well
   |    as any directory path. If there is no external token used, the
   |    default value is internal.
   | certutil -T -d [sql:]directory -h token-name -0 security-officer-passwor
   | d
   |    Many networks have dedicated personnel who handle changes to
   |    security tokens (the security officer). This person must supply
   |    the password to access the specified token. For example:
   | $ certutil -T -d sql:/home/my/sharednssdb -h nethsm -0 secret
   |    Upgrading or Merging the Security Databases
   |    Many networks or applications may be using older BerkeleyDB
   |    versions of the certificate database (cert8.db). Databases can
   |    be upgraded to the new SQLite version of the database
   |    (cert9.db) using the --upgrade-merge command option or existing
   |    databases can be merged with the new cert9.db databases using
   |    the ---merge command.
   |    The --upgrade-merge command must give information about the
   |    original database and then use the standard arguments (like -d)
   |    to give the information about the new databases. The command
   |    also requires information that the tool uses for the process to
   |    upgrade and write over the original database.
   | certutil --upgrade-merge -d [sql:]directory [-P dbprefix] --source-dir d
   | irectory --source-prefix dbprefix --upgrade-id id --upgrade-token-name n
   | ame [-@ password-file]
   |    For example:
   | $ certutil --upgrade-merge -d sql:/home/my/sharednssdb --source-dir /opt
   | /my-app/alias/ --source-prefix serverapp- --upgrade-id 1 --upgrade-token
   | -name internal
   |    The --merge command only requires information about the
   |    location of the original database; since it doesn't change the
   |    format of the database, it can write over information without
   |    performing interim step.
   | certutil --merge -d [sql:]directory [-P dbprefix] --source-dir directory
   |  --source-prefix dbprefix [-@ password-file]
   |    For example:
   | $ certutil --merge -d sql:/home/my/sharednssdb --source-dir /opt/my-app/
   | alias/ --source-prefix serverapp-
   |    Running certutil Commands from a Batch File
   |    A series of commands can be run sequentially from a text file
   |    with the -B command option. The only argument for this
   |    specifies the input file.
   | $ certutil -B -i /path/to/batch-file
   | NSS Database Types
   |    NSS originally used BerkeleyDB databases to store security
   |    information. The last versions of these legacy databases are:
   |      \* cert8.db for certificates
   |      \* key3.db for keys
   |      \* secmod.db for PKCS #11 module information
   |    BerkeleyDB has performance limitations, though, which prevent
   |    it from being easily used by multiple applications
   |    simultaneously. NSS has some flexibility that allows
   |    applications to use their own, independent database engine
   |    while keeping a shared database and working around the access
   |    issues. Still, NSS requires more flexibility to provide a truly
   |    shared security database.
   |    In 2009, NSS introduced a new set of databases that are SQLite
   |    databases rather than BerkleyDB. These new databases provide
   |    more accessibility and performance:
   |      \* cert9.db for certificates
   |      \* key4.db for keys
   |      \* pkcs11.txt, which is listing of all of the PKCS #11 modules
   |        contained in a new subdirectory in the security databases
   |        directory
   |    Because the SQLite databases are designed to be shared, these
   |    are the shared database type. The shared database type is
   |    preferred; the legacy format is included for backward
   |    compatibility.
   |    By default, the tools (certutil, pk12util, modutil) assume that
   |    the given security databases follow the more common legacy
   |    type. Using the SQLite databases must be manually specified by
   |    using the sql: prefix with the given security directory. For
   |    example:
   | $ certutil -L -d sql:/home/my/sharednssdb
   |    To set the shared database type as the default type for the
   |    tools, set the NSS_DEFAULT_DB_TYPE environment variable to sql:
   | export NSS_DEFAULT_DB_TYPE="sql"
   |    This line can be set added to the ~/.bashrc file to make the
   |    change permanent.
   |    Most applications do not use the shared database by default,
   |    but they can be configured to use them. For example, this
   |    how-to article covers how to configure Firefox and Thunderbird
   |    to use the new shared NSS databases:
   |      \* https://wiki.mozilla.org/NSS_Shared_DB_Howto
   |    For an engineering draft on the changes in the shared NSS
   |    databases, see the NSS project wiki:
   |      \* https://wiki.mozilla.org/NSS_Shared_DB
   | See Also
   |    pk12util (1)
   |    modutil (1)
   |    certutil has arguments or operations that use features defined
   |    in several IETF RFCs.
   |      \* `http://tools.ietf.org/html/rfc5280 <https://datatracker.ietf.org/doc/html/rfc5280>`__
   |      \* `http://tools.ietf.org/html/rfc1113 <https://datatracker.ietf.org/doc/html/rfc1113>`__
   |      \* `http://tools.ietf.org/html/rfc1485 <https://datatracker.ietf.org/doc/html/rfc1485>`__
   |    The NSS wiki has information on the new database design and how
   |    to configure applications to use it.
   |      \* https://wiki.mozilla.org/NSS_Shared_DB_Howto
   |      \* https://wiki.mozilla.org/NSS_Shared_DB
   | Additional Resources
   |    For information about NSS and other tools related to NSS (like
   |    JSS), check out the NSS project wiki at
   |   
     `http://www.mozilla.org/projects/security/pki/nss/ <https://www.mozilla.org/projects/security/pki/nss/>`__.
     The NSS site
   |    relates directly to NSS code changes and releases.
   |    Mailing lists:
   |    https://lists.mozilla.org/listinfo/dev-tech-crypto
   |    IRC: Freenode at #dogtag-pki
   | Authors
   |    The NSS tools were written and maintained by developers with
   |    Netscape, Red Hat, Sun, Oracle, Mozilla, and Google.
   |    Authors: Elio Maldonado <emaldona@redhat.com>, Deon Lackey
   |    <dlackey@redhat.com>.

   | LICENSE
   |        Licensed under the Mozilla Public License, v. 2.0. If a copy of the MPL was not
     distributed with this file, You can
   |        obtain one at https://mozilla.org/MPL/2.0/.

   | NOTES
   |         1. Mozilla NSS bug 836477
   |            https://bugzilla.mozilla.org/show_bug.cgi?id=836477