.. _mozilla_projects_nss_tools_signtool:

NSS tools : signtool
====================

.. container::

   | Name
   |    signtool — Digitally sign objects and files.
   | Synopsis
   |    signtool [-k keyName] `-h <-h>`__ `-H <-H>`__ `-l <-l>`__ `-L <-L>`__ `-M <-M>`__
     `-v <-v>`__ `-w <-w>`__
   |    `-G nickname <-G_nickname>`__ `-s size <--keysize>`__ `-b basename <-b_basename>`__ [[-c
     Compression
   |    Level] ] [[-d cert-dir] ] [[-i installer script] ] [[-m metafile] ] [[-x
   |    name] ] [[-f filename] ] [[-t|--token tokenname] ] [[-e extension] ] [[-o]
   |    ] [[-z] ] [[-X] ] [[--outfile] ] [[--verbose value] ] [[--norecurse] ]
   |    [[--leavearc] ] [[-j directory] ] [[-Z jarfile] ] [[-O] ] [[-p password] ]
   |    [directory-tree] [archive]
   | Description
   |    The Signing Tool, signtool, creates digital signatures and uses a Java
   |    Archive (JAR) file to associate the signatures with files in a directory.
   |    Electronic software distribution over any network involves potential
   |    security problems. To help address some of these problems, you can
   |    associate digital signatures with the files in a JAR archive. Digital
   |    signatures allow SSL-enabled clients to perform two important operations:
   |    \* Confirm the identity of the individual, company, or other entity whose
   |    digital signature is associated with the files
   |    \* Check whether the files have been tampered with since being signed
   |    If you have a signing certificate, you can use Netscape Signing Tool to
   |    digitally sign files and package them as a JAR file. An object-signing
   |    certificate is a special kind of certificate that allows you to associate
   |    your digital signature with one or more files.
   |    An individual file can potentially be signed with multiple digital
   |    signatures. For example, a commercial software developer might sign the
   |    files that constitute a software product to prove that the files are
   |    indeed from a particular company. A network administrator manager might
   |    sign the same files with an additional digital signature based on a
   |    company-generated certificate to indicate that the product is approved for
   |    use within the company.
   |    The significance of a digital signature is comparable to the significance
   |    of a handwritten signature. Once you have signed a file, it is difficult
   |    to claim later that you didn't sign it. In some situations, a digital
   |    signature may be considered as legally binding as a handwritten signature.
   |    Therefore, you should take great care to ensure that you can stand behind
   |    any file you sign and distribute.
   |    For example, if you are a software developer, you should test your code to
   |    make sure it is virus-free before signing it. Similarly, if you are a
   |    network administrator, you should make sure, before signing any code, that
   |    it comes from a reliable source and will run correctly with the software
   |    installed on the machines to which you are distributing it.
   |    Before you can use Netscape Signing Tool to sign files, you must have an
   |    object-signing certificate, which is a special certificate whose
   |    associated private key is used to create digital signatures. For testing
   |    purposes only, you can create an object-signing certificate with Netscape
   |    Signing Tool 1.3. When testing is finished and you are ready to
   |    disitribute your software, you should obtain an object-signing certificate
   |    from one of two kinds of sources:
   |    \* An independent certificate authority (CA) that authenticates your
   |    identity and charges you a fee. You typically get a certificate from an
   |    independent CA if you want to sign software that will be distributed over
   |    the Internet.
   |    \* CA server software running on your corporate intranet or extranet.
   |    Netscape Certificate Management System provides a complete management
   |    solution for creating, deploying, and managing certificates, including CAs
   |    that issue object-signing certificates.
   |    You must also have a certificate for the CA that issues your signing
   |    certificate before you can sign files. If the certificate authority's
   |    certificate isn't already installed in your copy of Communicator, you
   |    typically install it by clicking the appropriate link on the certificate
   |    authority's web site, for example on the page from which you initiated
   |    enrollment for your signing certificate. This is the case for some test
   |    certificates, as well as certificates issued by Netscape Certificate
   |    Management System: you must download the CA certificate in addition to
   |    obtaining your own signing certificate. CA certificates for several
   |    certificate authorities are preinstalled in the Communicator certificate
   |    database.
   |    When you receive an object-signing certificate for your own use, it is
   |    automatically installed in your copy of the Communicator client software.
   |    Communicator supports the public-key cryptography standard known as PKCS
   |    #12, which governs key portability. You can, for example, move an
   |    object-signing certificate and its associated private key from one
   |    computer to another on a credit-card-sized device called a smart card.
   | Options
   |    -b basename
   |            Specifies the base filename for the .rsa and .sf files in the
   |            META-INF directory to conform with the JAR format. For example, -b
   |            signatures causes the files to be named signatures.rsa and
   |            signatures.sf. The default is signtool.
   |    -c#
   |            Specifies the compression level for the -J or -Z option. The
   |            symbol # represents a number from 0 to 9, where 0 means no
   |            compression and 9 means maximum compression. The higher the level
   |            of compression, the smaller the output but the longer the
   |            operation takes. If the -c# option is not used with either the -J
   |            or the -Z option, the default compression value used by both the
   |            -J and -Z options is 6.
   |    -d certdir
   |            Specifies your certificate database directory; that is, the
   |            directory in which you placed your key3.db and cert7.db files. To
   |            specify the current directory, use "-d." (including the period).
   |            The Unix version of signtool assumes ~/.netscape unless told
   |            otherwise. The NT version of signtool always requires the use of
   |            the -d option to specify where the database files are located.
   |    -e extension
   |            Tells signtool to sign only files with the given extension; for
   |            example, use -e".class" to sign only Java class files. Note that
   |            with Netscape Signing Tool version 1.1 and later this option can
   |            appear multiple times on one command line, making it possible to
   |            specify multiple file types or classes to include.
   |    -f commandfile
   |            Specifies a text file containing Netscape Signing Tool options and
   |            arguments in keyword=value format. All options and arguments can
   |            be expressed through this file. For more information about the
   |            syntax used with this file, see "Tips and Techniques".
   |    -i scriptname
   |            Specifies the name of an installer script for SmartUpdate. This
   |            script installs files from the JAR archive in the local system
   |            after SmartUpdate has validated the digital signature. For more
   |            details, see the description of -m that follows. The -i option
   |            provides a straightforward way to provide this information if you
   |            don't need to specify any metadata other than an installer script.
   |    -j directory
   |            Specifies a special JavaScript directory. This option causes the
   |            specified directory to be signed and tags its entries as inline
   |            JavaScript. This special type of entry does not have to appear in
   |            the JAR file itself. Instead, it is located in the HTML page
   |            containing the inline scripts. When you use signtool -v, these
   |            entries are displayed with the string NOT PRESENT.
   |    -k key ... directory
   |            Specifies the nickname (key) of the certificate you want to sign
   |            with and signs the files in the specified directory. The directory
   |            to sign is always specified as the last command-line argument.
   |            Thus, it is possible to write signtool -k MyCert -d . signdir You
   |            may have trouble if the nickname contains a single quotation mark.
   |            To avoid problems, escape the quotation mark using the escape
   |            conventions for your platform. It's also possible to use the -k
   |            option without signing any files or specifying a directory. For
   |            example, you can use it with the -l option to get detailed
   |            information about a particular signing certificate.
   |    -G nickname
   |            Generates a new private-public key pair and corresponding
   |            object-signing certificate with the given nickname. The newly
   |            generated keys and certificate are installed into the key and
   |            certificate databases in the directory specified by the -d option.
   |            With the NT version of Netscape Signing Tool, you must use the -d
   |            option with the -G option. With the Unix version of Netscape
   |            Signing Tool, omitting the -d option causes the tool to install
   |            the keys and certificate in the Communicator key and certificate
   |            databases. If you are installing the keys and certificate in the
   |            Communicator databases, you must exit Communicator before using
   |            this option; otherwise, you risk corrupting the databases. In all
   |            cases, the certificate is also output to a file named x509.cacert,
   |            which has the MIME-type application/x-x509-ca-cert. Unlike
   |            certificates normally used to sign finished code to be distributed
   |            over a network, a test certificate created with -G is not signed
   |            by a recognized certificate authority. Instead, it is self-signed.
   |            In addition, a single test signing certificate functions as both
   |            an object-signing certificate and a CA. When you are using it to
   |            sign objects, it behaves like an object-signing certificate. When
   |            it is imported into browser software such as Communicator, it
   |            behaves like an object-signing CA and cannot be used to sign
   |            objects. The -G option is available in Netscape Signing Tool 1.0
   |            and later versions only. By default, it produces only RSA
   |            certificates with 1024-byte keys in the internal token. However,
   |            you can use the -s option specify the required key size and the -t
   |            option to specify the token. For more information about the use of
   |            the -G option, see "Generating Test Object-Signing
   |            Certificates""Generating Test Object-Signing Certificates" on page
   |            1241.
   |    -l
   |            Lists signing certificates, including issuing CAs. If any of your
   |            certificates are expired or invalid, the list will so specify.
   |            This option can be used with the -k option to list detailed
   |            information about a particular signing certificate. The -l option
   |            is available in Netscape Signing Tool 1.0 and later versions only.
   |    -J
   |            Signs a directory of HTML files containing JavaScript and creates
   |            as many archive files as are specified in the HTML tags. Even if
   |            signtool creates more than one archive file, you need to supply
   |            the key database password only once. The -J option is available
   |            only in Netscape Signing Tool 1.0 and later versions. The -J
   |            option cannot be used at the same time as the -Z option. If the
   |            -c# option is not used with the -J option, the default compression
   |            value is 6. Note that versions 1.1 and later of Netscape Signing
   |            Tool correctly recognizes the CODEBASE attribute, allows paths to
   |            be expressed for the CLASS and SRC attributes instead of filenames
   |            only, processes LINK tags and parses HTML correctly, and offers
   |            clearer error messages.
   |    -L
   |            Lists the certificates in your database. An asterisk appears to
   |            the left of the nickname for any certificate that can be used to
   |            sign objects with signtool.
   |    --leavearc
   |            Retains the temporary .arc (archive) directories that the -J
   |            option creates. These directories are automatically erased by
   |            default. Retaining the temporary directories can be an aid to
   |            debugging.
   |    -m metafile
   |            Specifies the name of a metadata control file. Metadata is signed
   |            information attached either to the JAR archive itself or to files
   |            within the archive. This metadata can be any ASCII string, but is
   |            used mainly for specifying an installer script. The metadata file
   |            contains one entry per line, each with three fields: field #1:
   |            file specification, or + if you want to specify global metadata
   |            (that is, metadata about the JAR archive itself or all entries in
   |            the archive) field #2: the name of the data you are specifying;
   |            for example: Install-Script field #3: data corresponding to the
   |            name in field #2 For example, the -i option uses the equivalent of
   |            this line: + Install-Script: script.js This example associates a
   |            MIME type with a file: movie.qt MIME-Type: video/quicktime For
   |            information about the way installer script information appears in
   |            the manifest file for a JAR archive, see The JAR Format on
   |            Netscape DevEdge.
   |    -M
   |            Lists the PKCS #11 modules available to signtool, including smart
   |            cards. The -M option is available in Netscape Signing Tool 1.0 and
   |            later versions only. For information on using Netscape Signing
   |            Tool with smart cards, see "Using Netscape Signing Tool with Smart
   |            Cards". For information on using the -M option to verify
   |            FIPS-140-1 validated mode, see "Netscape Signing Tool and
   |            FIPS-140-1".
   |    --norecurse
   |            Blocks recursion into subdirectories when signing a directory's
   |            contents or when parsing HTML.
   |    -o
   |            Optimizes the archive for size. Use this only if you are signing
   |            very large archives containing hundreds of files. This option
   |            makes the manifest files (required by the JAR format) considerably
   |            smaller, but they contain slightly less information.
   |    --outfile outputfile
   |            Specifies a file to receive redirected output from Netscape
   |            Signing Tool.
   |    -p password
   |            Specifies a password for the private-key database. Note that the
   |            password entered on the command line is displayed as plain text.
   |    -s keysize
   |            Specifies the size of the key for generated certificate. Use the
   |            -M option to find out what tokens are available. The -s option can
   |            be used with the -G option only.
   |    -t token
   |            Specifies which available token should generate the key and
   |            receive the certificate. Use the -M option to find out what tokens
   |            are available. The -t option can be used with the -G option only.
   |    -v archive
   |            Displays the contents of an archive and verifies the cryptographic
   |            integrity of the digital signatures it contains and the files with
   |            which they are associated. This includes checking that the
   |            certificate for the issuer of the object-signing certificate is
   |            listed in the certificate database, that the CA's digital
   |            signature on the object-signing certificate is valid, that the
   |            relevant certificates have not expired, and so on.
   |    --verbosity value
   |            Sets the quantity of information Netscape Signing Tool generates
   |            in operation. A value of 0 (zero) is the default and gives full
   |            information. A value of -1 suppresses most messages, but not error
   |            messages.
   |    -w archive
   |            Displays the names of signers of any files in the archive.
   |    -x directory
   |            Excludes the specified directory from signing. Note that with
   |            Netscape Signing Tool version 1.1 and later this option can appear
   |            multiple times on one command line, making it possible to specify
   |            several particular directories to exclude.
   |    -z
   |            Tells signtool not to store the signing time in the digital
   |            signature. This option is useful if you want the expiration date
   |            of the signature checked against the current date and time rather
   |            than the time the files were signed.
   |    -Z jarfile
   |            Creates a JAR file with the specified name. You must specify this
   |            option if you want signtool to create the JAR file; it does not do
   |            so automatically. If you don't specify -Z, you must use an
   |            external ZIP tool to create the JAR file. The -Z option cannot be
   |            used at the same time as the -J option. If the -c# option is not
   |            used with the -Z option, the default compression value is 6.
   | The Command File Format
   |    Entries in a Netscape Signing Tool command file have this general format:
   |    keyword=value Everything before the = sign on a single line is a keyword,
   |    and everything from the = sign to the end of line is a value. The value
   |    may include = signs; only the first = sign on a line is interpreted. Blank
   |    lines are ignored, but white space on a line with keywords and values is
   |    assumed to be part of the keyword (if it comes before the equal sign) or
   |    part of the value (if it comes after the first equal sign). Keywords are
   |    case insensitive, values are generally case sensitive. Since the = sign
   |    and newline delimit the value, it should not be quoted.
   |    Subsection
   |    basename
   |            Same as -b option.
   |    compression
   |            Same as -c option.
   |    certdir
   |            Same as -d option.
   |    extension
   |            Same as -e option.
   |    generate
   |            Same as -G option.
   |    installscript
   |            Same as -i option.
   |    javascriptdir
   |            Same as -j option.
   |    htmldir
   |            Same as -J option.
   |    certname
   |            Nickname of certificate, as with -k and -l -k options.
   |    signdir
   |            The directory to be signed, as with -k option.
   |    list
   |            Same as -l option. Value is ignored, but = sign must be present.
   |    listall
   |            Same as -L option. Value is ignored, but = sign must be present.
   |    metafile
   |            Same as -m option.
   |    modules
   |            Same as -M option. Value is ignored, but = sign must be present.
   |    optimize
   |            Same as -o option. Value is ignored, but = sign must be present.
   |    password
   |            Same as -p option.
   |    keysize
   |            Same as -s option.
   |    token
   |            Same as -t option.
   |    verify
   |            Same as -v option.
   |    who
   |            Same as -w option.
   |    exclude
   |            Same as -x option.
   |    notime
   |            Same as -z option. value is ignored, but = sign must be present.
   |    jarfile
   |            Same as -Z option.
   |    outfile
   |            Name of a file to which output and error messages will be
   |            redirected. This option has no command-line equivalent.
   | Extended Examples
   |    The following example will do this and that
   |    Listing Available Signing Certificates
   |    You use the -L option to list the nicknames for all available certificates
   |    and check which ones are signing certificates.
   |  signtool -L
   |  using certificate directory: /u/jsmith/.netscape
   |  S Certificates
   |  - ------------
   |    BBN Certificate Services CA Root 1
   |    IBM World Registry CA
   |    VeriSign Class 1 CA - Individual Subscriber - VeriSign, Inc.
   |    GTE CyberTrust Root CA
   |    Uptime Group Plc. Class 4 CA
   |  \* Verisign Object Signing Cert
   |    Integrion CA
   |    GTE CyberTrust Secure Server CA
   |    AT&T Directory Services
   |  \* test object signing cert
   |    Uptime Group Plc. Class 1 CA
   |    VeriSign Class 1 Primary CA
   |  - ------------
   |  Certificates that can be used to sign objects have \*'s to their left.
   |    Two signing certificates are displayed: Verisign Object Signing Cert and
   |    test object signing cert.
   |    You use the -l option to get a list of signing certificates only,
   |    including the signing CA for each.
   |  signtool -l
   |  using certificate directory: /u/jsmith/.netscape
   |  Object signing certificates
   |  ---------------------------------------
   |  Verisign Object Signing Cert
   |      Issued by: VeriSign, Inc. - Verisign, Inc.
   |      Expires: Tue May 19, 1998
   |  test object signing cert
   |      Issued by: test object signing cert (Signtool 1.0 Testing
   |  Certificate (960187691))
   |      Expires: Sun May 17, 1998
   |  ---------------------------------------
   |    For a list including CAs, use the -L option.
   |    Signing a File
   |    1. Create an empty directory.
   |  mkdir signdir
   |    2. Put some file into it.
   |  echo boo > signdir/test.f
   |    3. Specify the name of your object-signing certificate and sign the
   |    directory.
   |  signtool -k MySignCert -Z testjar.jar signdir
   |  using key "MySignCert"
   |  using certificate directory: /u/jsmith/.netscape
   |  Generating signdir/META-INF/manifest.mf file..
   |  --> test.f
   |  adding signdir/test.f to testjar.jar
   |  Generating signtool.sf file..
   |  Enter Password or Pin for "Communicator Certificate DB":
   |  adding signdir/META-INF/manifest.mf to testjar.jar
   |  adding signdir/META-INF/signtool.sf to testjar.jar
   |  adding signdir/META-INF/signtool.rsa to testjar.jar
   |  tree "signdir" signed successfully
   |    4. Test the archive you just created.
   |  signtool -v testjar.jar
   |  using certificate directory: /u/jsmith/.netscape
   |  archive "testjar.jar" has passed crypto verification.
   |             status   path
   |       ------------   -------------------
   |           verified   test.f
   |    Using Netscape Signing Tool with a ZIP Utility
   |    To use Netscape Signing Tool with a ZIP utility, you must have the utility
   |    in your path environment variable. You should use the zip.exe utility
   |    rather than pkzip.exe, which cannot handle long filenames. You can use a
   |    ZIP utility instead of the -Z option to package a signed archive into a
   |    JAR file after you have signed it:
   |  cd signdir
   |    zip -r ../myjar.jar \*
   |    adding: META-INF/ (stored 0%)
   |    adding: META-INF/manifest.mf (deflated 15%)
   |    adding: META-INF/signtool.sf (deflated 28%)
   |    adding: META-INF/signtool.rsa (stored 0%)
   |    adding: text.txt (stored 0%)
   |    Generating the Keys and Certificate
   |    The signtool option -G generates a new public-private key pair and
   |    certificate. It takes the nickname of the new certificate as an argument.
   |    The newly generated keys and certificate are installed into the key and
   |    certificate databases in the directory specified by the -d option. With
   |    the NT version of Netscape Signing Tool, you must use the -d option with
   |    the -G option. With the Unix version of Netscape Signing Tool, omitting
   |    the -d option causes the tool to install the keys and certificate in the
   |    Communicator key and certificate databases. In all cases, the certificate
   |    is also output to a file named x509.cacert, which has the MIME-type
   |    application/x-x509-ca-cert.
   |    Certificates contain standard information about the entity they identify,
   |    such as the common name and organization name. Netscape Signing Tool
   |    prompts you for this information when you run the command with the -G
   |    option. However, all of the requested fields are optional for test
   |    certificates. If you do not enter a common name, the tool provides a
   |    default name. In the following example, the user input is in boldface:
   |  signtool -G MyTestCert
   |  using certificate directory: /u/someuser/.netscape
   |  Enter certificate information. All fields are optional. Acceptable
   |  characters are numbers, letters, spaces, and apostrophes.
   |  certificate common name: Test Object Signing Certificate
   |  organization: Netscape Communications Corp.
   |  organization unit: Server Products Division
   |  state or province: California
   |  country (must be exactly 2 characters): US
   |  username: someuser
   |  email address: someuser@netscape.com
   |  Enter Password or Pin for "Communicator Certificate DB": [Password will not echo]
   |  generated public/private key pair
   |  certificate request generated
   |  certificate has been signed
   |  certificate "MyTestCert" added to database
   |  Exported certificate to x509.raw and x509.cacert.
   |    The certificate information is read from standard input. Therefore, the
   |    information can be read from a file using the redirection operator (<) in
   |    some operating systems. To create a file for this purpose, enter each of
   |    the seven input fields, in order, on a separate line. Make sure there is a
   |    newline character at the end of the last line. Then run signtool with
   |    standard input redirected from your file as follows:
   |  signtool -G MyTestCert inputfile
   |    The prompts show up on the screen, but the responses will be automatically
   |    read from the file. The password will still be read from the console
   |    unless you use the -p option to give the password on the command line.
   |    Using the -M Option to List Smart Cards
   |    You can use the -M option to list the PKCS #11 modules, including smart
   |    cards, that are available to signtool:
   |  signtool -d "c:\netscape\users\jsmith" -M
   |  using certificate directory: c:\netscape\users\username
   |  Listing of PKCS11 modules
   |  -----------------------------------------------
   |          1. Netscape Internal PKCS #11 Module
   |                            (this module is internally loaded)
   |                            slots: 2 slots attached
   |                            status: loaded
   |            slot: Communicator Internal Cryptographic Services Version 4.0
   |           token: Communicator Generic Crypto Svcs
   |            slot: Communicator User Private Key and Certificate Services
   |           token: Communicator Certificate DB
   |          2. CryptOS
   |                            (this is an external module)
   |   DLL name: core32
   |           slots: 1 slots attached
   |          status: loaded
   |            slot: Litronic 210
   |           token:
   |          -----------------------------------------------
   |    Using Netscape Signing Tool and a Smart Card to Sign Files
   |    The signtool command normally takes an argument of the -k option to
   |    specify a signing certificate. To sign with a smart card, you supply only
   |    the fully qualified name of the certificate.
   |    To see fully qualified certificate names when you run Communicator, click
   |    the Security button in Navigator, then click Yours under Certificates in
   |    the left frame. Fully qualified names are of the format smart
   |    card:certificate, for example "MyCard:My Signing Cert". You use this name
   |    with the -k argument as follows:
   |  signtool -k "MyCard:My Signing Cert" directory
   |    Verifying FIPS Mode
   |    Use the -M option to verify that you are using the FIPS-140-1 module.
   |  signtool -d "c:\netscape\users\jsmith" -M
   |  using certificate directory: c:\netscape\users\jsmith
   |  Listing of PKCS11 modules
   |  -----------------------------------------------
   |    1. Netscape Internal PKCS #11 Module
   |            (this module is internally loaded)
   |            slots: 2 slots attached
   |            status: loaded
   |      slot: Communicator Internal Cryptographic Services Version 4.0
   |     token: Communicator Generic Crypto Svcs
   |      slot: Communicator User Private Key and Certificate Services
   |     token: Communicator Certificate DB
   |  -----------------------------------------------
   |    This Unix example shows that Netscape Signing Tool is using a FIPS-140-1
   |    module:
   |  signtool -d "c:\netscape\users\jsmith" -M
   |  using certificate directory: c:\netscape\users\jsmith
   |  Enter Password or Pin for "Communicator Certificate DB": [password will not echo]
   |  Listing of PKCS11 modules
   |  -----------------------------------------------
   |  1. Netscape Internal FIPS PKCS #11 Module
   |  (this module is internally loaded)
   |  slots: 1 slots attached
   |  status: loaded
   |  slot: Netscape Internal FIPS-140-1 Cryptographic Services
   |  token: Communicator Certificate DB
   |  -----------------------------------------------
   | See Also
   |    signver (1)
   |    The NSS wiki has information on the new database design and how to
   |    configure applications to use it.
   |      o https://wiki.mozilla.org/NSS_Shared_DB_Howto
   |      o https://wiki.mozilla.org/NSS_Shared_DB
   | Additional Resources
   |    For information about NSS and other tools related to NSS (like JSS), check
   |    out the NSS project wiki at
   |   
     [1]\ `http://www.mozilla.org/projects/security/pki/nss/ <https://www.mozilla.org/projects/security/pki/nss/>`__.
     The NSS site relates
   |    directly to NSS code changes and releases.
   |    Mailing lists: https://lists.mozilla.org/listinfo/dev-tech-crypto
   |    IRC: Freenode at #dogtag-pki
   | Authors
   |    The NSS tools were written and maintained by developers with Netscape, Red
   |    Hat, and Sun.
   |    Authors: Elio Maldonado <emaldona@redhat.com>, Deon Lackey
   |    <dlackey@redhat.com>.
   | Copyright
   |    (c) 2010, Red Hat, Inc. Licensed under the GNU Public License version 2.
   | References
   |    Visible links
   |    1.
     `http://www.mozilla.org/projects/security/pki/nss/ <https://www.mozilla.org/projects/security/pki/nss/>`__