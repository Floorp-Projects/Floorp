.. _mozilla_projects_nss_reference_nss_tools_:_modutil:

NSS tools : modutil
===================

.. container::

   Name

   | modutil - Manage PKCS #11 module information within the security module
   | database.

   Synopsis

   modutil [options] [[arguments]]

   STATUS

   This documentation is still work in progress. Please contribute to the initial review in Mozilla
   NSS bug 836477[1]

   Description

   | The Security Module Database Tool, modutil, is a command-line utility
   | for managing PKCS #11 module information both within secmod.db files and
   | within hardware tokens. modutil can add and delete PKCS #11 modules,
   | change passwords on security databases, set defaults, list module
   | contents, enable or disable slots, enable or disable FIPS 140-2
   | compliance, and assign default providers for cryptographic operations.
   | This tool can also create certificate, key, and module security database
   | files.

   | The tasks associated with security module database management are part of
   | a process that typically also involves managing key databases and
   | certificate databases.

   Options

   | Running modutil always requires one (and only one) option to specify the
   | type of module operation. Each option may take arguments, anywhere from
   | none to multiple arguments.

   Options

   -add modulename

   | Add the named PKCS #11 module to the database. Use this option
   | with the -libfile, -ciphers, and -mechanisms arguments.

   -changepw tokenname

   | Change the password on the named token. If the token has not been
   | initialized, this option initializes the password. Use this option
   | with the -pwfile and -newpwfile arguments. A password is
   | equivalent to a personal identification number (PIN).

   -chkfips

   | Verify whether the module is in the given FIPS mode. true means to
   | verify that the module is in FIPS mode, while false means to
   | verify that the module is not in FIPS mode.

   -create

   | Create new certificate, key, and module databases. Use the -dbdir
   | directory argument to specify a directory. If any of these
   | databases already exist in a specified directory, modutil returns
   | an error message.

   -default modulename

   | Specify the security mechanisms for which the named module will be
   | a default provider. The security mechanisms are specified with the
   | -mechanisms argument.

   -delete modulename

   | Delete the named module. The default NSS PKCS #11 module cannot be
   | deleted.

   -disable modulename

   | Disable all slots on the named module. Use the -slot argument to
   | disable a specific slot.

   The internal NSS PKCS #11 module cannot be disabled.

   -enable modulename

   | Enable all slots on the named module. Use the -slot argument to
   | enable a specific slot.

   -fips [true \| false]

   | Enable (true) or disable (false) FIPS 140-2 compliance for the
   | default NSS module.

   -force

   | Disable modutil's interactive prompts so it can be run from a
   | script. Use this option only after manually testing each planned
   | operation to check for warnings and to ensure that bypassing the
   | prompts will cause no security lapses or loss of
   | database integrity.

   -jar JAR-file

   | Add a new PKCS #11 module to the database using the named JAR
   | file. Use this command with the -installdir and -tempdir
   | arguments. The JAR file uses the NSS PKCS #11 JAR format to
   | identify all the files to be installed, the module's name, the
   | mechanism flags, and the cipher flags, as well as any files to be
   | installed on the target machine, including the PKCS #11 module
   | library file and other files such as documentation. This is
   | covered in the JAR installation file section in the man page,
   | which details the special script needed to perform an installation
   | through a server or with modutil.

   -list [modulename]

   | Display basic information about the contents of the secmod.db
   | file. Specifying a modulename displays detailed information about
   | a particular module and its slots and tokens.

   -rawadd

   Add the module spec string to the secmod.db database.

   -rawlist

   | Display the module specs for a specified module or for all
   | loadable modules.

   -undefault modulename

   | Specify the security mechanisms for which the named module will
   | not be a default provider. The security mechanisms are specified
   | with the -mechanisms argument.

   Arguments

   MODULE

   Give the security module to access.

   MODULESPEC

   Give the security module spec to load into the security database.

   -ciphers cipher-enable-list

   | Enable specific ciphers in a module that is being added to the
   | database. The cipher-enable-list is a colon-delimited list of
   | cipher names. Enclose this list in quotation marks if it contains
   | spaces.

   -dbdir [sql:]directory

   | Specify the database directory in which to access or create
   | security module database files.

   | modutil supports two types of databases: the legacy security
   | databases (cert8.db, key3.db, and secmod.db) and new SQLite
   | databases (cert9.db, key4.db, and pkcs11.txt). If the prefix sql:
   | is not used, then the tool assumes that the given databases are in
   | the old format.

   --dbprefix prefix

   | Specify the prefix used on the database files, such as my\_ for
   | my_cert8.db. This option is provided as a special case. Changing
   | the names of the certificate and key databases is not recommended.

   -installdir root-installation-directory

   | Specify the root installation directory relative to which files
   | will be installed by the -jar option. This directory should be one
   | below which it is appropriate to store dynamic library files, such
   | as a server's root directory.

   -libfile library-file

   | Specify a path to a library file containing the implementation of
   | the PKCS #11 interface module that is being added to the database.

   -mechanisms mechanism-list

   | Specify the security mechanisms for which a particular module will
   | be flagged as a default provider. The mechanism-list is a
   | colon-delimited list of mechanism names. Enclose this list in
   | quotation marks if it contains spaces.

   | The module becomes a default provider for the listed mechanisms
   | when those mechanisms are enabled. If more than one module claims
   | to be a particular mechanism's default provider, that mechanism's
   | default provider is undefined.

   | modutil supports several mechanisms: RSA, DSA, RC2, RC4, RC5, AES,
   | DES, DH, SHA1, SHA256, SHA512, SSL, TLS, MD5, MD2, RANDOM (for
   | random number generation), and FRIENDLY (meaning certificates are
   | publicly readable).

   -newpwfile new-password-file

   | Specify a text file containing a token's new or replacement
   | password so that a password can be entered automatically with the
   | -changepw option.

   -nocertdb

   | Do not open the certificate or key databases. This has several
   | effects:

   | o With the -create command, only a module security file is
   | created; certificate and key databases are not created.

   | o With the -jar command, signatures on the JAR file are not
   | checked.

   | o With the -changepw command, the password on the NSS internal
   | module cannot be set or changed, since this password is
   | stored in the key database.

   -pwfile old-password-file

   | Specify a text file containing a token's existing password so that
   | a password can be entered automatically when the -changepw option
   | is used to change passwords.

   -secmod secmodname

   | Give the name of the security module database (like secmod.db) to
   | load.

   -slot slotname

   | Specify a particular slot to be enabled or disabled with the
   | -enable or -disable options.

   -string CONFIG_STRING

   | Pass a configuration string for the module being added to the
   | database.

   -tempdir temporary-directory

   | Give a directory location where temporary files are created during
   | the installation by the -jar option. If no temporary directory is
   | specified, the current directory is used.

   Usage and Examples

   Creating Database Files

   | Before any operations can be performed, there must be a set of security
   | databases available. modutil can be used to create these files. The only
   | required argument is the database that where the databases will be
   | located.

   modutil -create -dbdir [sql:]directory

   Adding a Cryptographic Module

   | Adding a PKCS #11 module means submitting a supporting library file,
   | enabling its ciphers, and setting default provider status for various
   | security mechanisms. This can be done by supplying all of the information
   | through modutil directly or by running a JAR file and install script. For
   | the most basic case, simply upload the library:

   modutil -add modulename -libfile library-file [-ciphers cipher-enable-list] [-mechanisms
   mechanism-list]

   For example:

   modutil -dbdir sql:/home/my/sharednssdb -add "Example PKCS #11 Module" -libfile "/tmp/crypto.so"
   -mechanisms RSA:DSA:RC2:RANDOM

   | Using database directory ...
   | Module "Example PKCS #11 Module" added to database.

   Installing a Cryptographic Module from a JAR File

   | PKCS #11 modules can also be loaded using a JAR file, which contains all
   | of the required libraries and an installation script that describes how to
   | install the module. The JAR install script is described in more detail in
   | [1]the section called “JAR Installation File Format”.

   | The JAR installation script defines the setup information for each
   | platform that the module can be installed on. For example:

   | Platforms {
   | Linux:5.4.08:x86 {
   | ModuleName { "Example PKCS #11 Module" }
   | ModuleFile { crypto.so }
   | DefaultMechanismFlags{0x0000}
   | CipherEnableFlags{0x0000}
   | Files {
   | crypto.so {
   | Path{ /tmp/crypto.so }
   | }
   | setup.sh {
   | Executable
   | Path{ /tmp/setup.sh }
   | }
   | }
   | }
   | Linux:6.0.0:x86 {
   | EquivalentPlatform { Linux:5.4.08:x86 }
   | }
   | }

   | Both the install script and the required libraries must be bundled in a
   | JAR file, which is specified with the -jar argument.

   modutil -dbdir sql:/home/mt"jar-install-filey/sharednssdb -jar install.jar -installdir
   sql:/home/my/sharednssdb

   | This installation JAR file was signed by:
   | ----------------------------------------------

   \**SUBJECT NAME*\*

   | C=US, ST=California, L=Mountain View, CN=Cryptorific Inc., OU=Digital ID
   | Class 3 - Netscape Object Signing, OU="www.verisign.com/repository/CPS
   | Incorp. by Ref.,LIAB.LTD(c)9 6", OU=www.verisign.com/CPS Incorp.by Ref
   | . LIABILITY LTD.(c)97 VeriSign, OU=VeriSign Object Signing CA - Class 3
   | Organization, OU="VeriSign, Inc.", O=VeriSign Trust Network \**ISSUER
   | NAME**, OU=www.verisign.com/CPS Incorp.by Ref. LIABILITY LTD.(c)97
   | VeriSign, OU=VeriSign Object Signing CA - Class 3 Organization,
   | OU="VeriSign, Inc.", O=VeriSign Trust Network
   | ----------------------------------------------

   | Do you wish to continue this installation? (y/n) y
   | Using installer script "installer_script"
   | Successfully parsed installation script
   | Current platform is Linux:5.4.08:x86
   | Using installation parameters for platform Linux:5.4.08:x86
   | Installed file crypto.so to /tmp/crypto.so
   | Installed file setup.sh to ./pk11inst.dir/setup.sh
   | Executing "./pk11inst.dir/setup.sh"...
   | "./pk11inst.dir/setup.sh" executed successfully
   | Installed module "Example PKCS #11 Module" into module database

   Installation completed successfully

   Adding Module Spec

   | Each module has information stored in the security database about its
   | configuration and parameters. These can be added or edited using the
   | -rawadd command. For the current settings or to see the format of the
   | module spec in the database, use the -rawlist option.

   modutil -rawadd modulespec

   Deleting a Module

   A specific PKCS #11 module can be deleted from the secmod.db database:

   modutil -delete modulename -dbdir [sql:]directory

   Displaying Module Information

   | The secmod.db database contains information about the PKCS #11 modules
   | that are available to an application or server to use. The list of all
   | modules, information about specific modules, and database configuration
   | specs for modules can all be viewed.

   To simply get a list of modules in the database, use the -list command.

   modutil -list [modulename] -dbdir [sql:]directory

   | Listing the modules shows the module name, their status, and other
   | associated security databases for certificates and keys. For example:

   modutil -list -dbdir sql:/home/my/sharednssdb

   | Listing of PKCS #11 Modules
   | -----------------------------------------------------------
   | 1. NSS Internal PKCS #11 Module
   | slots: 2 slots attached
   | status: loaded

   | slot: NSS Internal Cryptographic Services
   | token: NSS Generic Crypto Services

   | slot: NSS User Private Key and Certificate Services
   | token: NSS Certificate DB
   | -----------------------------------------------------------

   | Passing a specific module name with the -list returns details information
   | about the module itself, like supported cipher mechanisms, version
   | numbers, serial numbers, and other information about the module and the
   | token it is loaded on. For example:

   modutil -list "NSS Internal PKCS #11 Module" -dbdir sql:/home/my/sharednssdb

   | -----------------------------------------------------------
   | Name: NSS Internal PKCS #11 Module
   | Library file: \**Internal ONLY module*\*
   | Manufacturer: Mozilla Foundation
   | Description: NSS Internal Crypto Services
   | PKCS #11 Version 2.20
   | Library Version: 3.11
   | Cipher Enable Flags: None
   | Default Mechanism Flags: RSA:RC2:RC4:DES:DH:SHA1:MD5:MD2:SSL:TLS:AES

   | Slot: NSS Internal Cryptographic Services
   | Slot Mechanism Flags: RSA:RC2:RC4:DES:DH:SHA1:MD5:MD2:SSL:TLS:AES
   | Manufacturer: Mozilla Foundation
   | Type: Software
   | Version Number: 3.11
   | Firmware Version: 0.0
   | Status: Enabled
   | Token Name: NSS Generic Crypto Services
   | Token Manufacturer: Mozilla Foundation
   | Token Model: NSS 3
   | Token Serial Number: 0000000000000000
   | Token Version: 4.0
   | Token Firmware Version: 0.0
   | Access: Write Protected
   | Login Type: Public (no login required)
   | User Pin: NOT Initialized

   | Slot: NSS User Private Key and Certificate Services
   | Slot Mechanism Flags: None
   | Manufacturer: Mozilla Foundation
   | Type: Software
   | Version Number: 3.11
   | Firmware Version: 0.0
   | Status: Enabled
   | Token Name: NSS Certificate DB
   | Token Manufacturer: Mozilla Foundation
   | Token Model: NSS 3
   | Token Serial Number: 0000000000000000
   | Token Version: 8.3
   | Token Firmware Version: 0.0
   | Access: NOT Write Protected
   | Login Type: Login required
   | User Pin: Initialized

   | A related command, -rawlist returns information about the database
   | configuration for the modules. (This information can be edited by loading
   | new specs using the -rawadd command.)

   | modutil -rawlist -dbdir sql:/home/my/sharednssdb
   | name="NSS Internal PKCS #11 Module" parameters="configdir=. certPrefix= keyPrefix=
     secmod=secmod.db flags=readOnly " NSS="trustOrder=75 cipherOrder=100
     slotParams={0x00000001=[slotFlags=RSA,RC4,RC2,DES,DH,SHA1,MD5,MD2,SSL,TLS,AES,RANDOM askpw=any
     timeout=30 ] } Flags=internal,critical"

   Setting a Default Provider for Security Mechanisms

   | Multiple security modules may provide support for the same security
   | mechanisms. It is possible to set a specific security module as the
   | default provider for a specific security mechanism (or, conversely, to
   | prohibit a provider from supplying those mechanisms).

   modutil -default modulename -mechanisms mechanism-list

   | To set a module as the default provider for mechanisms, use the -default
   | command with a colon-separated list of mechanisms. The available
   | mechanisms depend on the module; NSS supplies almost all common
   | mechanisms. For example:

   modutil -default "NSS Internal PKCS #11 Module" -dbdir -mechanisms RSA:DSA:RC2

   Using database directory c:\databases...

   Successfully changed defaults.

   Clearing the default provider has the same format:

   modutil -undefault "NSS Internal PKCS #11 Module" -dbdir -mechanisms MD2:MD5

   Enabling and Disabling Modules and Slots

   | Modules, and specific slots on modules, can be selectively enabled or
   | disabled using modutil. Both commands have the same format:

   modutil -enable|-disable modulename [-slot slotname]

   For example:

   modutil -enable "NSS Internal PKCS #11 Module" -slot "NSS Internal Cryptographic Services "
   -dbdir .

   Slot "NSS Internal Cryptographic Services " enabled.

   | Be sure that the appropriate amount of trailing whitespace is after the
   | slot name. Some slot names have a significant amount of whitespace that
   | must be included, or the operation will fail.

   Enabling and Verifying FIPS Compliance

   | The NSS modules can have FIPS 140-2 compliance enabled or disabled using
   | modutil with the -fips option. For example:

   modutil -fips true -dbdir sql:/home/my/sharednssdb/

   FIPS mode enabled.

   | To verify that status of FIPS mode, run the -chkfips command with either a
   | true or false flag (it doesn't matter which). The tool returns the current
   | FIPS setting.

   modutil -chkfips false -dbdir sql:/home/my/sharednssdb/

   FIPS mode enabled.

   Changing the Password on a Token

   Initializing or changing a token's password:

   modutil -changepw tokenname [-pwfile old-password-file] [-newpwfile new-password-file]

   modutil -dbdir sql:/home/my/sharednssdb -changepw "NSS Certificate DB"

   | Enter old password:
   | Incorrect password, try again...
   | Enter old password:
   | Enter new password:
   | Re-enter new password:
   | Token "Communicator Certificate DB" password changed successfully.

   JAR Installation File Format

   | When a JAR file is run by a server, by modutil, or by any program that
   | does not interpret JavaScript, a special information file must be included
   | to install the libraries. There are several things to keep in mind with
   | this file:

   o It must be declared in the JAR archive's manifest file.

   o The script can have any name.

   | o The metainfo tag for this is Pkcs11_install_script. To declare
   | meta-information in the manifest file, put it in a file that is passed
   | to signtool.

   Sample Script

   | For example, the PKCS #11 installer script could be in the file
   | pk11install. If so, the metainfo file for signtool includes a line such as
   | this:

   + Pkcs11_install_script: pk11install

   | The script must define the platform and version number, the module name
   | and file, and any optional information like supported ciphers and
   | mechanisms. Multiple platforms can be defined in a single install file.

   | ForwardCompatible { IRIX:6.2:mips SUNOS:5.5.1:sparc }
   | Platforms {
   | WINNT::x86 {
   | ModuleName { "Example Module" }
   | ModuleFile { win32/fort32.dll }
   | DefaultMechanismFlags{0x0001}
   | DefaultCipherFlags{0x0001}
   | Files {
   | win32/setup.exe {
   | Executable
   | RelativePath { %temp%/setup.exe }
   | }
   | win32/setup.hlp {
   | RelativePath { %temp%/setup.hlp }
   | }
   | win32/setup.cab {
   | RelativePath { %temp%/setup.cab }
   | }
   | }
   | }
   | WIN95::x86 {
   | EquivalentPlatform {WINNT::x86}
   | }
   | SUNOS:5.5.1:sparc {
   | ModuleName { "Example UNIX Module" }
   | ModuleFile { unix/fort.so }
   | DefaultMechanismFlags{0x0001}
   | CipherEnableFlags{0x0001}
   | Files {
   | unix/fort.so {
   | RelativePath{%root%/lib/fort.so}
   | AbsolutePath{/usr/local/netscape/lib/fort.so}
   | FilePermissions{555}
   | }
   | xplat/instr.html {
   | RelativePath{%root%/docs/inst.html}
   | AbsolutePath{/usr/local/netscape/docs/inst.html}
   | FilePermissions{555}
   | }
   | }
   | }
   | IRIX:6.2:mips {
   | EquivalentPlatform { SUNOS:5.5.1:sparc }
   | }
   | }

   Script Grammar

   | The script is basic Java, allowing lists, key-value pairs, strings, and
   | combinations of all of them.

   --> valuelist

   | valuelist --> value valuelist
   | <null>

   | value ---> key_value_pair
   | string

   key_value_pair --> key { valuelist }

   key --> string

   | string --> simple_string
   | "complex_string"

   simple_string --> [^ \\t\n\""{""}"]+

   complex_string --> ([^\"\\\r\n]|(\\\")|(\\\\))+

   | Quotes and backslashes must be escaped with a backslash. A complex string
   | must not include newlines or carriage returns.Outside of complex strings,
   | all white space (for example, spaces, tabs, and carriage returns) is
   | considered equal and is used only to delimit tokens.

   Keys

   | The Java install file uses keys to define the platform and module
   | information.

   | ForwardCompatible gives a list of platforms that are forward compatible.
   | If the current platform cannot be found in the list of supported
   | platforms, then the ForwardCompatible list is checked for any platforms
   | that have the same OS and architecture in an earlier version. If one is
   | found, its attributes are used for the current platform.

   | Platforms (required) Gives a list of platforms. Each entry in the list is
   | itself a key-value pair: the key is the name of the platform and the value
   | list contains various attributes of the platform. The platform string is
   | in the format system name:OS release:architecture. The installer obtains
   | these values from NSPR. OS release is an empty string on non-Unix
   | operating systems. NSPR supports these platforms:

   o AIX (rs6000)

   o BSDI (x86)

   o FREEBSD (x86)

   o HPUX (hppa1.1)

   o IRIX (mips)

   o LINUX (ppc, alpha, x86)

   o MacOS (PowerPC)

   o NCR (x86)

   o NEC (mips)

   o OS2 (x86)

   o OSF (alpha)

   o ReliantUNIX (mips)

   o SCO (x86)

   o SOLARIS (sparc)

   o SONY (mips)

   o SUNOS (sparc)

   o UnixWare (x86)

   o WIN16 (x86)

   o WIN95 (x86)

   o WINNT (x86)

   For example:

   | IRIX:6.2:mips
   | SUNOS:5.5.1:sparc
   | Linux:2.0.32:x86
   | WIN95::x86

   | The module information is defined independently for each platform in the
   | ModuleName, ModuleFile, and Files attributes. These attributes must be
   | given unless an EquivalentPlatform attribute is specified.

   Per-Platform Keys

   | Per-platform keys have meaning only within the value list of an entry in
   | the Platforms list.

   | ModuleName (required) gives the common name for the module. This name is
   | used to reference the module by servers and by the modutil tool.

   | ModuleFile (required) names the PKCS #11 module file for this platform.
   | The name is given as the relative path of the file within the JAR archive.

   | Files (required) lists the files that need to be installed for this
   | module. Each entry in the file list is a key-value pair. The key is the
   | path of the file in the JAR archive, and the value list contains
   | attributes of the file. At least RelativePath or AbsolutePath must be
   | specified for each file.

   | DefaultMechanismFlags specifies mechanisms for which this module is the
   | default provider; this is equivalent to the -mechanism option with the
   | -add command. This key-value pair is a bitstring specified in hexadecimal
   | (0x) format. It is constructed as a bitwise OR. If the
   | DefaultMechanismFlags entry is omitted, the value defaults to 0x0.

   | RSA: 0x00000001
   | DSA: 0x00000002
   | RC2: 0x00000004
   | RC4: 0x00000008
   | DES: 0x00000010
   | DH: 0x00000020
   | FORTEZZA: 0x00000040
   | RC5: 0x00000080
   | SHA1: 0x00000100
   | MD5: 0x00000200
   | MD2: 0x00000400
   | RANDOM: 0x08000000
   | FRIENDLY: 0x10000000
   | OWN_PW_DEFAULTS: 0x20000000
   | DISABLE: 0x40000000

   | CipherEnableFlags specifies ciphers that this module provides that NSS
   | does not provide (so that the module enables those ciphers for NSS). This
   | is equivalent to the -cipher argument with the -add command. This key is a
   | bitstring specified in hexadecimal (0x) format. It is constructed as a
   | bitwise OR. If the CipherEnableFlags entry is omitted, the value defaults
   | to 0x0.

   | EquivalentPlatform specifies that the attributes of the named platform
   | should also be used for the current platform. This makes it easier when
   | more than one platform uses the same settings.

   Per-File Keys

   | Some keys have meaning only within the value list of an entry in a Files
   | list.

   | Each file requires a path key the identifies where the file is. Either
   | RelativePath or AbsolutePath must be specified. If both are specified, the
   | relative path is tried first, and the absolute path is used only if no
   | relative root directory is provided by the installer program.

   | RelativePath specifies the destination directory of the file, relative to
   | some directory decided at install time. Two variables can be used in the
   | relative path: %root% and %temp%. %root% is replaced at run time with the
   | directory relative to which files should be installed; for example, it may
   | be the server's root directory. The %temp% directory is created at the
   | beginning of the installation and destroyed at the end. The purpose of
   | %temp% is to hold executable files (such as setup programs) or files that
   | are used by these programs. Files destined for the temporary directory are
   | guaranteed to be in place before any executable file is run; they are not
   | deleted until all executable files have finished.

   | AbsolutePath specifies the destination directory of the file as an
   | absolute path.

   | Executable specifies that the file is to be executed during the course of
   | the installation. Typically, this string is used for a setup program
   | provided by a module vendor, such as a self-extracting setup executable.
   | More than one file can be specified as executable, in which case the files
   | are run in the order in which they are specified in the script file.

   | FilePermissions sets permissions on any referenced files in a string of
   | octal digits, according to the standard Unix format. This string is a
   | bitwise OR.

   | user read: 0400
   | user write: 0200
   | user execute: 0100
   | group read: 0040
   | group write: 0020
   | group execute: 0010
   | other read: 0004
   | other write: 0002
   | other execute: 0001

   | Some platforms may not understand these permissions. They are applied only
   | insofar as they make sense for the current platform. If this attribute is
   | omitted, a default of 777 is assumed.

   NSS Database Types

   | NSS originally used BerkeleyDB databases to store security information.
   | The last versions of these legacy databases are:

   o cert8.db for certificates

   o key3.db for keys

   o secmod.db for PKCS #11 module information

   | BerkeleyDB has performance limitations, though, which prevent it from
   | being easily used by multiple applications simultaneously. NSS has some
   | flexibility that allows applications to use their own, independent
   | database engine while keeping a shared database and working around the
   | access issues. Still, NSS requires more flexibility to provide a truly
   | shared security database.

   | In 2009, NSS introduced a new set of databases that are SQLite databases
   | rather than BerkleyDB. These new databases provide more accessibility and
   | performance:

   o cert9.db for certificates

   o key4.db for keys

   | o pkcs11.txt, which is listing of all of the PKCS #11 modules contained
   | in a new subdirectory in the security databases directory

   | Because the SQLite databases are designed to be shared, these are the
   | shared database type. The shared database type is preferred; the legacy
   | format is included for backward compatibility.

   | By default, the tools (certutil, pk12util, modutil) assume that the given
   | security databases follow the more common legacy type. Using the SQLite
   | databases must be manually specified by using the sql: prefix with the
   | given security directory. For example:

   modutil -create -dbdir sql:/home/my/sharednssdb

   | To set the shared database type as the default type for the tools, set the
   | NSS_DEFAULT_DB_TYPE environment variable to sql:

   export NSS_DEFAULT_DB_TYPE="sql"

   | This line can be added to the ~/.bashrc file to make the change
   | permanent.

   | Most applications do not use the shared database by default, but they can
   | be configured to use them. For example, this how-to article covers how to
   | configure Firefox and Thunderbird to use the new shared NSS databases:

   o https://wiki.mozilla.org/NSS_Shared_DB_Howto

   | For an engineering draft on the changes in the shared NSS databases, see
   | the NSS project wiki:

   o https://wiki.mozilla.org/NSS_Shared_DB

   See Also

   certutil (1)

   pk12util (1)

   signtool (1)

   | The NSS wiki has information on the new database design and how to
   | configure applications to use it.

   o https://wiki.mozilla.org/NSS_Shared_DB_Howto

   o https://wiki.mozilla.org/NSS_Shared_DB

   Additional Resources

   | For information about NSS and other tools related to NSS (like JSS), check
   | out the NSS project wiki at
   | [2]http://www.mozilla.org/projects/security/pki/nss/. The NSS site relates
   | directly to NSS code changes and releases.

   Mailing lists: https://lists.mozilla.org/listinfo/dev-tech-crypto

   IRC: Freenode at #dogtag-pki

   Authors

   | The NSS tools were written and maintained by developers with Netscape, Red
   | Hat, Sun, Oracle, Mozilla, and Google.

   | Authors: Elio Maldonado <emaldona@redhat.com>, Deon Lackey
   | <dlackey@redhat.com>.

   License

   | Licensed under the Mozilla Public License, v. 2.0.
   | If a copy of the MPL was not distributed with this file,
   | You can obtain one at https://mozilla.org/MPL/2.0/.

   References

   | 1. Mozilla NSS bug 836477
   | https://bugzilla.mozilla.org/show_bug.cgi?id=836477

   | Visible links
   | 1. JAR Installation File Format
   | file:///tmp/xmlto.eUWOJ0/modutil.pro...r-install-file
   | 2. http://www.mozilla.org/projects/security/pki/nss/