.. _mozilla_projects_nss_tools_nss_tools_modutil:

NSS Tools modutil
=================

.. _using_the_security_module_database_(modutil):

`Using the Security Module Database (modutil) <#using_the_security_module_database_(modutil)>`__
------------------------------------------------------------------------------------------------

.. container::

   Newsgroup: `mozilla.dev.tech.crypto <news://news.mozilla.org/mozilla.dev.tech.crypto>`__
   The Security Module Database Tool is a command-line utility for managing PKCS #11 module
   information within ``secmod.db`` files or within hardware tokens. You can use the tool to add and
   delete PKCS #11 modules, change passwords, set defaults, list module contents, enable or disable
   slots, enable or disable FIPS 140-2 compliance, and assign default providers for cryptographic
   operations. This tool can also create ``key3.db``, ``cert8.db``, and ``secmod.db`` security
   database files.

   The tasks associated with security module database management are part of a process that
   typically also involves managing key databases (``key3.db`` files) and certificate databases
   (``cert8.db`` files). The key, certificate, and PKCS #11 module management process generally
   begins with creating the keys and key database necessary to generate and manage certificates and
   the certificate database. This document discusses security module database management. For
   information on certificate database and key database management, see `Using the Certificate
   Database Tool <certutil.html>`__.

.. _availability_2:

` <#availability_2>`__ Availability
-----------------------------------

.. container::

   This tool is known to build on Solaris 2.5.1 (SunOS 5.5.1) and Windows NT 4.0.

.. _syntax_2:

` <#syntax_2>`__ Syntax
-----------------------

.. container::

   To run the Security Module Database Tool, type the command
   ``modutil``\ *option*\ ``[``\ *arguments*\ ``]`` where *option* and *arguments* are combinations
   of the options and arguments listed in the following section. Each command takes one option. Each
   option may take zero or more arguments. To see a usage string, issue the command without options.

.. _options_and_arguments:

`Options and Arguments <#options_and_arguments>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Options specify an action. Option arguments modify an action. The options and arguments for the
   ``modutil`` command are defined as follows:

   +-------------------------------------------------+-------------------------------------------------+
   | **Options**                                     |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-create``                                     | Create new ``secmod.db``, ``key3.db``, and      |
   |                                                 | ``cert8.db`` files. Use the ``-dbdir``          |
   |                                                 | *directory* argument to specify a directory. If |
   |                                                 | any of these databases already exist in a       |
   |                                                 | specified directory, the Security Module        |
   |                                                 | Database Tool displays an error message.        |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-list [``\ *modulename*\ ``]``                | Display basic information about the contents of |
   |                                                 | the ``secmod.db`` file. Use *modulename* to     |
   |                                                 | display detailed information about a particular |
   |                                                 | module and its slots and tokens.                |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-add``\ *modulename*                          | Add the named PKCS #11 module to the database.  |
   |                                                 | Use this option with the ``-libfile``,          |
   |                                                 | ``-ciphers``, and ``-mechanisms`` arguments.    |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-jar``\ *JAR-file*                            | Add a new PKCS #11 module to the database using |
   |                                                 | the named JAR file. Use this option with the    |
   |                                                 | ``-installdir`` and ``-tempdir`` arguments. The |
   |                                                 | JAR file uses the Netscape Server PKCS #11 JAR  |
   |                                                 | format to identify all the files to be          |
   |                                                 | installed, the module's name, the mechanism     |
   |                                                 | flags, and the cipher flags. The JAR file       |
   |                                                 | should also contain any files to be installed   |
   |                                                 | on the target machine, including the PKCS #11   |
   |                                                 | module library file and other files such as     |
   |                                                 | documentation. See the section `JAR             |
   |                                                 | Installation File <modutil.html#1043224>`__ for |
   |                                                 | information on creating the special script      |
   |                                                 | needed to perform an installation through a     |
   |                                                 | server or with the Security Module Database     |
   |                                                 | Tool (that is, in environments without          |
   |                                                 | JavaScript support). For general installation   |
   |                                                 | instructions and to install a module in         |
   |                                                 | environments where JavaScript support is        |
   |                                                 | available (as in Netscape Communicator), see    |
   |                                                 | the document `Using the JAR Installation        |
   |                                                 | Manager to Install a PKCS #11 Cryptographic     |
   |                                                 | Module <http://developer.netscape.co            |
   |                                                 | m/docs/manuals/security/jmpkcs/jimpkcs.htm>`__. |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-delete``\ *modulename*                       | Delete the named module. Note that you cannot   |
   |                                                 | delete the Netscape Communicator internal PKCS  |
   |                                                 | #11 module.                                     |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-changepw``\ *tokenname*                      | Change the password on the named token. If the  |
   |                                                 | token has not been initialized, this option     |
   |                                                 | initializes the password. Use this option with  |
   |                                                 | the ``-pwfile`` and ``-newpwfile`` arguments.   |
   |                                                 | In this context, the term "password" is         |
   |                                                 | equivalent to a personal identification number  |
   |                                                 | (PIN).                                          |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-default``\ *modulename*                      | Specify the security mechanisms for which the   |
   |                                                 | named module will be a default provider. The    |
   |                                                 | security mechanisms are specified with the      |
   |                                                 | ``-mechanisms`` *mechanism-list* argument.      |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-undefault``\ *modulename*                    | Specify the security mechanisms for which the   |
   |                                                 | named module will *not* be a default provider.  |
   |                                                 | The security mechanisms are specified with      |
   |                                                 | the\ ``-mechanisms`` *mechanism-list* argument. |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-enable``\ *modulename*                       | Enable all slots on the named module. Use the   |
   |                                                 | ``[-slot``\ *slotname*\ ``]``\ argument to      |
   |                                                 | enable a specific slot.                         |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-disable``\ *modulename*                      | Disable all slots on the named module. Use the  |
   |                                                 | ``[-slot``\ *slotname*\ ``]``\ argument to      |
   |                                                 | disable a specific slot.                        |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-fips [true | false]``                        | Enable (``true``) or disable (``false``) FIPS   |
   |                                                 | 140-2 compliance for the Netscape Communicator  |
   |                                                 | internal module.                                |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-force``                                      | Disable the Security Module Database Tool's     |
   |                                                 | interactive prompts so it can be run from a     |
   |                                                 | script. Use this option only after manually     |
   |                                                 | testing each planned operation to check for     |
   |                                                 | warnings and to ensure that bypassing the       |
   |                                                 | prompts will cause no security lapses or loss   |
   |                                                 | of database integrity.                          |
   +-------------------------------------------------+-------------------------------------------------+
   | **Arguments**                                   |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-dbdir``\ *directory*                         | Specify the database directory in which to      |
   |                                                 | access or create security module database       |
   |                                                 | files. On Unix, the Security Module Database    |
   |                                                 | Tool defaults to the user's Netscape directory. |
   |                                                 | Windows NT has no default directory, so         |
   |                                                 | ``-dbdir`` must be used to specify a directory. |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-dbprefix`` *prefix*                          | Specify the prefix used on the ``cert8.db`` and |
   |                                                 | ``key3.db`` files (for example, ``my_cert8.db`` |
   |                                                 | and ``my_key3.db``). This option is provided as |
   |                                                 | a special case. Changing the names of the       |
   |                                                 | certificate and key databases is not            |
   |                                                 | recommended.                                    |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-libfile``\ *library-file*                    | Specify a path to the DLL or other library file |
   |                                                 | containing the implementation of the PKCS #11   |
   |                                                 | interface module that is being added to the     |
   |                                                 | database.                                       |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-ciphers``\ *cipher-enable-list*              | Enable specific ciphers in a module that is     |
   |                                                 | being added to the database. The                |
   |                                                 | *cipher-enable-list* is a colon-delimited list  |
   |                                                 | of cipher names. Enclose this list in quotation |
   |                                                 | marks if it contains spaces. The following      |
   |                                                 | cipher is currently available: ``FORTEZZA``.    |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-mechanisms``\ *mechanism-list*               | Specify the security mechanisms for which a     |
   |                                                 | particular module will be flagged as a default  |
   |                                                 | provider. The *mechanism-list* is a             |
   |                                                 | colon-delimited list of mechanism names.        |
   |                                                 | Enclose this list in quotation marks if it      |
   |                                                 | contains spaces. The module becomes a default   |
   |                                                 | provider for the listed mechanisms when those   |
   |                                                 | mechanisms are enabled. If more than one module |
   |                                                 | claims to be a particular mechanism's default   |
   |                                                 | provider, that mechanism's default provider is  |
   |                                                 | undefined. The following mechanisms are         |
   |                                                 | currently available: ``RSA``, ``DSA``, ``RC2``, |
   |                                                 | ``RC4``, ``RC5``, ``DES``, ``DH``,              |
   |                                                 | ``FORTEZZA``, ``SHA1``, ``MD5``, ``MD2``,       |
   |                                                 | ``RANDOM`` (for random number generation), and  |
   |                                                 | ``FRIENDLY`` (meaning certificates are publicly |
   |                                                 | readable).                                      |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-installdir``\ *root-installation-directory*  | Specify the root installation directory         |
   |                                                 | relative to which files will be installed by    |
   |                                                 | the ``-jar`` *JAR-file* option. This directory  |
   |                                                 | should be one below which it is appropriate to  |
   |                                                 | store dynamic library files (for example, a     |
   |                                                 | server's root directory or the Netscape         |
   |                                                 | Communicator root directory).                   |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-tempdir``\ *temporary-directory*             | The temporary directory is the location where   |
   |                                                 | temporary files will be created in the course   |
   |                                                 | of installation by the ``-jar`` *JAR-file*      |
   |                                                 | option. If no temporary directory is specified, |
   |                                                 | the current directory will be used.             |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-pwfile``\ *old-password-file*                | Specify a text file containing a token's        |
   |                                                 | existing password so that a password can be     |
   |                                                 | entered automatically when the ``-changepw``    |
   |                                                 | *tokenname* option is used to change passwords. |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-newpwfile``\ *new-password-file*             | Specify a text file containing a token's new or |
   |                                                 | replacement password so that a password can be  |
   |                                                 | entered automatically with the ``-changepw``    |
   |                                                 | *tokenname* option.                             |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-slot``\ *slotname*                           | Specify a particular slot to be enabled or      |
   |                                                 | disabled with the ``-enable`` *modulename* or   |
   |                                                 | ``-disable`` *modulename* options.              |
   +-------------------------------------------------+-------------------------------------------------+
   | -nocertdb                                       | Do not open the certificate or key databases.   |
   |                                                 | This has several effects:                       |
   |                                                 |                                                 |
   |                                                 | -  With the ``-create`` command, only a         |
   |                                                 |    ``secmod.db`` file will be created;          |
   |                                                 |    ``cert8.db`` and ``key3.db`` will not be     |
   |                                                 |    created.                                     |
   |                                                 | -  With the ``-jar`` command, signatures on the |
   |                                                 |    JAR file will not be checked.                |
   |                                                 | -  With the ``-changepw`` command, the password |
   |                                                 |    on the Netscape internal module cannot be    |
   |                                                 |    set or changed, since this password is       |
   |                                                 |    stored in ``key3.db``.                       |
   +-------------------------------------------------+-------------------------------------------------+

.. _usage_2:

` <#usage_2>`__ Usage
---------------------

.. container::

   The Security Module Database Tool's capabilities are grouped as follows, using these combinations
   of options and arguments. The options and arguments in square brackets are optional, those
   without square brackets are required.

   -  Creating a set of security management database files (``key3.db``, ``cert8.db``, and
      ``secmod.db``):

         ``-create``

   -  Displaying basic module information or detailed information about the contents of a given
      module:

         ``-list [``\ *modulename*\ ``]``

   -  Adding a PKCS #11 module, which includes setting a supporting library file, enabling ciphers,
      and setting default provider status for various security mechanisms:

         ``-add``\ *modulename*\ ``-libfile``\ *library-file*
         ``[-ciphers``\ *cipher-enable-list*\ ``] [-mechanisms``\ *mechanism-list*\ ``]``

   -  Adding a PKCS #11 module from an existing JAR file:

         ``-jar``\ *JAR-file* ``-installdir``\ *root-installation-directory*
         ``[-tempdir``\ *temporary-directory*\ ``]``

   -  Deleting a specific PKCS #11 module from a security module database:

         ``-delete``\ *modulename*

   -  Initializing or changing a token's password:

         ``-changepw``\ *tokenname*
         ``[-pwfile``\ *old-password-file*\ ``]  [-newpwfile``\ *new-password-file*\ ``]``

   -  Setting the default provider status of various security mechanisms in an existing PKCS #11
      module:

         ``-default``\ *modulename* ``-mechanisms``\ *mechanism-list*

   -  Clearing the default provider status of various security mechanisms in an existing PKCS #11
      module:

         ``-undefault``\ *modulename* ``-mechanisms``\ *mechanism-list*

   -  Enabling a specific slot or all slots within a module:

         ``-enable``\ *modulename* ``[-slot``\ *slotname*\ ``]``

   -  Disabling a specific slot or all slots within a module:

         ``-disable``\ *modulename* ``[-slot``\ *slotname*\ ``]``

   -  Enabling or disabling FIPS 140-2 compliance within the Netscape Communicator internal module:

         ``-fips [true | false]``

   -  Disabling interactive prompts for the Security Module Database Tool, to support scripted
      operation:

         ``-force``

.. _jar_installation_file:

`JAR Installation File <#jar_installation_file>`__
--------------------------------------------------

.. container::

   When a JAR file is run by a server, by the Security Module Database Tool, or by any program that
   does not interpret JavaScript, a special information file must be included in the format
   described below. This information file contains special scripting and must be declared in the JAR
   archive's manifest file. The script can have any name. The metainfo tag for this is
   ``Pkcs11_install_script``. To declare meta-information in the manifest file, put it in a file
   that is passed to the `Netscape Signing
   Tool <http://developer.netscape.com/docs/manuals/signedobj/signtool/index.htm>`__.

.. _sample_script:

`Sample Script <#sample_script>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   For example, the PKCS #11 installer script could be in the file ``pk11install.`` If so, the
   metainfo file for the `Netscape Signing
   Tool <http://developer.netscape.com/docs/manuals/signedobj/signtool/index.htm>`__ would include a
   line such as this:
   .. code::

      + Pkcs11_install_script: pk11install

   The sample script file could contain the following:
   .. code::

      ForwardCompatible { IRIX:6.2:mips SUNOS:5.5.1:sparc }
      Platforms {
         WINNT::x86 {
            ModuleName { "Fortezza Module" }
            ModuleFile { win32/fort32.dll }
            DefaultMechanismFlags{0x0001}
            DefaultCipherFlags{0x0001}
            Files {
               win32/setup.exe {
                  Executable
                  RelativePath { %temp%/setup.exe }
               }
               win32/setup.hlp {
                  RelativePath { %temp%/setup.hlp }
               }
               win32/setup.cab {
                  RelativePath { %temp%/setup.cab }
               }
            }
         }
         WIN95::x86 {
            EquivalentPlatform {WINNT::x86}
         }
         SUNOS:5.5.1:sparc {
            ModuleName { "Fortezza UNIX Module" }
            ModuleFile { unix/fort.so }
            DefaultMechanismFlags{0x0001}
            CipherEnableFlags{0x0001}
            Files {
               unix/fort.so {
                  RelativePath{%root%/lib/fort.so}
                  AbsolutePath{/usr/local/netscape/lib/fort.so}
                  FilePermissions{555}
               }
               xplat/instr.html {
                  RelativePath{%root%/docs/inst.html}
                  AbsolutePath{/usr/local/netscape/docs/inst.html}
                  FilePermissions{555}
               }
            }
         }
         IRIX:6.2:mips {
            EquivalentPlatform { SUNOS:5.5.1:sparc }
         }
      }

.. _script_grammar:

`Script Grammar <#script_grammar>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The script file grammar is as follows:
   .. code::

      --> valuelist

   .. code::

      valuelist --> value valuelist
                     <null>

   .. code::

      value ---> key_value_pair
                  string

   .. code::

      key_value_pair --> key { valuelist }

   .. code::

      key --> string

   .. code::

      string --> simple_string
                  "complex_string"

   .. code::

      simple_string --> [^ \t\n\""{""}"]+
      (No whitespace, quotes, or braces.)

   .. code::

      complex_string --> ([^\"\\\r\n]|(\\\")|(\\\\))+ (Quotes and
      backslashes must be escaped with a backslash. A complex string must not
      include newlines or carriage returns.)

   Outside of complex strings, all white space (for example, spaces, tabs, and carriage returns) is
   considered equal and is used only to delimit tokens.

`Keys <#keys>`__
~~~~~~~~~~~~~~~~

.. container::

   Keys are case-insensitive. This section discusses the following keys: `Global
   Keys <modutil.html#1042778>`__
   `Per-Platform Keys <modutil.html#1040459>`__
   `Per-File Keys <modutil.html#1040510>`__
   .. rubric:: Global Keys
      :name: global_keys

   ``ForwardCompatible`` Gives a list of platforms that are forward compatible. If the current
   platform cannot be found in the list of supported platforms, then the ``ForwardCompatible`` list
   is checked for any platforms that have the same OS and architecture in an earlier version. If one
   is found, its attributes are used for the current platform. ``Platforms`` (required) Gives a list
   of platforms. Each entry in the list is itself a key-value pair: the key is the name of the
   platform and the value list contains various attributes of the platform. The ``ModuleName``,
   ``ModuleFile``, and ``Files`` attributes must be specified for each platform unless an
   ``EquivalentPlatform`` attribute is specified. The platform string is in the following format:
   *system name*\ ``:``\ *OS release*\ ``:``\ *architecture*. The installer obtains these values
   from NSPR. *OS release* is an empty string on non-Unix operating systems. The following system
   names and platforms are currently defined by NSPR:

   -  AIX (rs6000)
   -  BSDI (x86)
   -  FREEBSD (x86)
   -  HPUX (hppa1.1)
   -  IRIX (mips)
   -  LINUX (ppc, alpha, x86)
   -  MacOS (PowerPC)
   -  NCR (x86)
   -  NEC (mips)
   -  OS2 (x86)
   -  OSF (alpha)
   -  ReliantUNIX (mips)
   -  SCO (x86)
   -  SOLARIS (sparc)
   -  SONY (mips)
   -  SUNOS (sparc)
   -  UnixWare (x86)
   -  WIN16 (x86)
   -  WIN95 (x86)
   -  WINNT (x86)

   Here are some examples of valid platform strings:
   .. code::

      IRIX:6.2:mips
      SUNOS:5.5.1:sparc
      Linux:2.0.32:x86
      WIN95::x86.

   .. rubric:: Per-Platform Keys
      :name: per-platform_keys

   These keys have meaning only within the value list of an entry in the ``Platforms`` list.
   ``ModuleName`` (required) Gives the common name for the module. This name will be used to
   reference the module from Netscape Communicator, the Security Module Database tool (``modutil``),
   servers, or any other program that uses the Netscape security module database. ``ModuleFile``
   (required) Names the PKCS #11 module file (DLL or ``.so``) for this platform. The name is given
   as the relative path of the file within the JAR archive. ``Files`` (required) Lists the files
   that need to be installed for this module. Each entry in the file list is a key-value pair: the
   key is the path of the file in the JAR archive, and the value list contains attributes of the
   file. At least ``RelativePath`` or ``AbsolutePath`` must be specified for each file.
   ``DefaultMechanismFlags`` Specifies mechanisms for which this module will be a default provider.
   This key-value pair is a bitstring specified in hexadecimal (0x) format. It is constructed as a
   bitwise OR of the following constants. If the ``DefaultMechanismFlags`` entry is omitted, the
   value defaults to 0x0.
   .. code::

         RSA:                   0x00000001
         DSA:                   0x00000002
         RC2:                   0x00000004
         RC4:                   0x00000008
         DES:                   0x00000010
         DH:                    0x00000020
         FORTEZZA:              0x00000040
         RC5:                   0x00000080
         SHA1:                  0x00000100
         MD5:                   0x00000200
         MD2:                   0x00000400
         RANDOM:                0x08000000
         FRIENDLY:              0x10000000
         OWN_PW_DEFAULTS:       0x20000000
         DISABLE:               0x40000000

   ``CipherEnableFlags`` Specifies ciphers that this module provides but Netscape Communicator does
   not, so that Communicator can enable them. This key is a bitstring specified in hexadecimal (0x)
   format. It is constructed as a bitwise OR of the following constants. If the
   ``CipherEnableFlags`` entry is omitted, the value defaults to 0x0.
   .. code::

         FORTEZZA:               0x0000 0001

   ``EquivalentPlatform`` Specifies that the attributes of the named platform should also be used
   for the current platform. Saves typing when there is more than one platform using the same
   settings.
   .. rubric:: Per-File Keys
      :name: per-file_keys

   These keys have meaning only within the value list of an entry in a ``Files`` list. At least one
   of ``RelativePath`` and ``AbsolutePath`` must be specified. If both are specified, the relative
   path is tried first, and the absolute path is used only if no relative root directory is provided
   by the installer program. ``RelativePath`` Specifies the destination directory of the file,
   relative to some directory decided at install time. Two variables can be used in the relative
   path: "``%root%``" and "``%temp%``". "``%root%``" is replaced at run time with the directory
   relative to which files should be installed; for example, it may be the server's root directory
   or the Netscape Communicator root directory. The "``%temp%``" directory is created at the
   beginning of the installation and destroyed at the end. The purpose of "``%temp%``" is to hold
   executable files (such as setup programs) or files that are used by these programs. For example,
   a Windows installation might consist of a ``setup.exe`` installation program, a help file, and a
   ``.cab`` file containing compressed information. All these files could be installed in the
   temporary directory. Files destined for the temporary directory are guaranteed to be in place
   before any executable file is run; they are not deleted until all executable files have finished.
   ``AbsolutePath`` Specifies the destination directory of the file as an absolute path. If both
   ``RelativePath`` and ``AbsolutePath`` are specified, the installer attempts to use the relative
   path; if it is unable to determine a relative path, it uses the absolute path. ``Executable``
   Specifies that the file is to be executed during the course of the installation. Typically this
   string would be used for a setup program provided by a module vendor, such as a self-extracting
   ``setup.exe``. More than one file can be specified as executable, in which case the files are run
   in the order in which they are specified in the script file. ``FilePermissions`` Interpreted as a
   string of octal digits, according to the standard Unix format. This string is a bitwise OR of the
   following constants:
   .. code::

         user read:                0400
         user write:               0200
         user execute:             0100
         group read:               0040
         group write:              0020
         group execute:            0010
         other read:               0004
         other write:              0002
         other execute:       0001

   Some platforms may not understand these permissions. They are applied only insofar as they make
   sense for the current platform. If this attribute is omitted, a default of 777 is assumed.

.. _examples_2:

` <#examples_2>`__ Examples
---------------------------

.. container::

   `Creating Database Files <modutil.html#1028724>`__
   `Displaying Module Information <modutil.html#1034026>`__
   `Setting a Default Provider <modutil.html#1028731>`__
   `Enabling a Slot <modutil.html#1034020>`__
   `Enabling FIPS Compliance <modutil.html#1034010>`__
   `Adding a Cryptographic Module <modutil.html#1042489>`__
   `Installing a Cryptographic Module from a JAR File <modutil.html#1042502>`__
   `Changing the Password on a Token <modutil.html#1043961>`__

.. _creating_database_files:

`Creating Database Files <#creating_database_files>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This example creates a set of security management database files in the specified directory:
   .. code::

      modutil -create -dbdir c:\databases

   The Security Module Database Tool displays a warning:
   .. code::

      WARNING: Performing this operation while Communicator is running could
      cause corruption of your security databases. If Communicator is
      currently running, you should exit Communicator before continuing this
      operation. Type 'q <enter>' to abort, or <enter> to continue:

   After you press Enter, the tool displays the following:
   .. code::

      Creating "c:\databases\key3.db"...done.
      Creating "c:\databases\cert8.db"...done.
      Creating "c:\databases\secmod.db"...done.

.. _displaying_module_information:

`Displaying Module Information <#displaying_module_information>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This example gives detailed information about the specified module:
   .. code::

      modutil -list "Netscape Internal PKCS #11 Module" -dbdir c:\databases

   The Security Module Database Tool displays information similar to this:
   .. code::

      Using database directory c:\databases...
      --------------------------------------------------------
      Name: Netscape Internal PKCS #11 Module
      Library file: **Internal ONLY module**
      Manufacturer: Netscape Communications Corp
      Description: Communicator Internal Crypto Svc
      PKCS #11 Version 2.0
      Library Version: 4.0
      Cipher Enable Flags: None
      Default Mechanism Flags: RSA:DSA:RC2:RC4:DES:SHA1:MD5:MD2

   .. code::

      Slot: Communicator Internal Cryptographic Services Version 4.0
      Manufacturer: Netscape Communications Corp
      Type: Software
      Version Number: 4.1
      Firmware Version: 0.0
      Status: Enabled
      Token Name: Communicator Generic Crypto Svcs
      Token Manufacturer: Netscape Communications Corp
      Token Model: Libsec 4.0
      Token Serial Number: 0000000000000000
      Token Version: 4.0
      Token Firmware Version: 0.0
      Access: Write Protected
      Login Type: Public (no login required)
      User Pin: NOT Initialized

   .. code::

      Slot: Communicator User Private Key and Certificate Services
      Manufacturer: Netscape Communications Corp
      Type: Software
      Version Number: 3.0
      Firmware Version: 0.0
      Status: Enabled
      Token Name: Communicator Certificate DB
      Token Manufacturer: Netscape Communications Corp
      Token Model: Libsec 4.0
      Token Serial Number: 0000000000000000
      Token Version: 7.0
      Token Firmware Version: 0.0
      Access: NOT Write Protected
      Login Type: Login required
      User Pin: NOT Initialized

.. _setting_a_default_provider:

`Setting a Default Provider <#setting_a_default_provider>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This example makes the specified module a default provider for the RSA, DSA, and RC2 security
   mechanisms:
   .. code::

      modutil -default "Cryptographic Module" -dbdir
      c:\databases -mechanisms RSA:DSA:RC2

   The Security Module Database Tool displays a warning:
   .. code::

      WARNING: Performing this operation while Communicator is running could
      cause corruption of your security databases. If Communicator is
      currently running, you should exit Communicator before continuing this
      operation. Type 'q <enter>' to abort, or <enter> to continue:

   After you press Enter, the tool displays the following:
   .. code::

      Using database directory c:\databases...

   .. code::

      Successfully changed defaults.

.. _enabling_a_slot:

`Enabling a Slot <#enabling_a_slot>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This example enables a particular slot in the specified module:
   .. code::

      modutil -enable "Cryptographic Module" -slot
      "Cryptographic Reader" -dbdir c:\databases

   The Security Module Database Tool displays a warning:
   .. code::

      WARNING: Performing this operation while Communicator is running could
      cause corruption of your security databases. If Communicator is
      currently running, you should exit Communicator before continuing this
      operation. Type 'q <enter>' to abort, or <enter> to continue:

   After you press Enter, the tool displays the following:
   .. code::

      Using database directory c:\databases...

   .. code::

      Slot "Cryptographic Reader" enabled.

.. _enabling_fips_compliance:

`Enabling FIPS Compliance <#enabling_fips_compliance>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This example enables FIPS 140-2 compliance in Communicator's internal module:
   .. code::

      modutil -dbdir "C:\databases" -fips true

   The Security Module Database Tool displays a warning:
   .. code::

      WARNING: Performing this operation while the browser is running could cause
      corruption of your security databases. If the browser is currently running,
      you should exit browser before continuing this operation. Type
      'q <enter>' to abort, or <enter> to continue:

   After you press Enter, the tool displays the following:
   .. code::

      FIPS mode enabled.

.. _adding_a_cryptographic_module:

`Adding a Cryptographic Module <#adding_a_cryptographic_module>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This example adds a new cryptographic module to the database:
   .. code::

      C:\modutil> modutil -dbdir "C:\databases" -add "Cryptorific Module" -
      libfile "C:\winnt\system32\crypto.dll" -mechanisms RSA:DSA:RC2:RANDOM

   The Security Module Database Tool displays a warning:
   .. code::

      WARNING: Performing this operation while Communicator is running could
      cause corruption of your security databases. If Communicator is
      currently running, you should exit Communicator before continuing this
      operation. Type 'q <enter>' to abort, or <enter> to continue:

   After you press Enter, the tool displays the following:
   .. code::

      Using database directory C:\databases...
      Module "Cryptorific Module" added to database.
      C:\modutil>

.. _installing_a_cryptographic_module_from_a_jar_file:

`Installing a Cryptographic Module from a JAR File <#installing_a_cryptographic_module_from_a_jar_file>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This example installs a cryptographic module from the following sample installation script.
   .. code::

      Platforms {
         WinNT::x86 {
            ModuleName { "Cryptorific Module" }
            ModuleFile { crypto.dll }
            DefaultMechanismFlags{0x0000}
            CipherEnableFlags{0x0000}
            Files {
               crypto.dll {
                  RelativePath{ %root%/system32/crypto.dll }
               }
               setup.exe {
                  Executable
                  RelativePath{ %temp%/setup.exe }
               }
            }
         }
         Win95::x86 {
            EquivalentPlatform { Winnt::x86 }
         }
      }

   To install from the script, use the following command. The root directory should be the Windows
   root directory (for example, ``c:\\windows``, or ``c:\\winnt``).
   .. code::

      C:\modutil> modutil -dbdir "c:\databases" -jar
      install.jar -installdir "C:/winnt"

   The Security Module Database Tool displays a warning:
   .. code::

      WARNING: Performing this operation while Communicator is running could
      cause corruption of your security databases. If Communicator is
      currently running, you should exit Communicator before continuing this
      operation. Type 'q <enter>' to abort, or <enter> to continue:

   After you press Enter, the tool displays the following:
   .. code::

      Using database directory c:\databases...

   .. code::

      This installation JAR file was signed by:
      ----------------------------------------------

   .. code::

      **SUBJECT NAME**

   .. code::

      C=US, ST=California, L=Mountain View, CN=Cryptorific Inc., OU=Digital ID
      Class 3 - Netscape Object Signing, OU="www.verisign.com/repository/CPS
      Incorp. by Ref.,LIAB.LTD(c)9 6", OU=www.verisign.com/CPS Incorp.by Ref
      . LIABILITY LTD.(c)97 VeriSign, OU=VeriSign Object Signing CA - Class 3
      Organization, OU="VeriSign, Inc.", O=VeriSign Trust Network **ISSUER
      NAME**, OU=www.verisign.com/CPS Incorp.by Ref. LIABILITY LTD.(c)97
      VeriSign, OU=VeriSign Object Signing CA - Class 3 Organization,
      OU="VeriSign, Inc.", O=VeriSign Trust Network
      ----------------------------------------------

   .. code::

      Do you wish to continue this installation? (y/n) y
      Using installer script "installer_script"
      Successfully parsed installation script
      Current platform is WINNT::x86
      Using installation parameters for platform WinNT::x86
      Installed file crypto.dll to C:/winnt/system32/crypto.dll
      Installed file setup.exe to ./pk11inst.dir/setup.exe
      Executing "./pk11inst.dir/setup.exe"...
      "./pk11inst.dir/setup.exe" executed successfully
      Installed module "Cryptorific Module" into module database

   .. code::

      Installation completed successfully
      C:\modutil>

.. _changing_the_password_on_a_token:

`Changing the Password on a Token <#changing_the_password_on_a_token>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This example changes the password for a token on an existing module.
   .. code::

      C:\modutil> modutil -dbdir "c:\databases" -changepw
      "Communicator Certificate DB"

   The Security Module Database Tool displays a warning:
   .. code::

      WARNING: Performing this operation while Communicator is running could
      cause corruption of your security databases. If Communicator is
      currently running, you should exit Communicator before continuing this
      operation. Type 'q <enter>' to abort, or <enter> to continue:

   After you press Enter, the tool displays the following:
   .. code::

      Using database directory c:\databases...
      Enter old password:
      Incorrect password, try again...
      Enter old password:
      Enter new password:
      Re-enter new password:
      Token "Communicator Certificate DB" password changed successfully.
      C:\modutil>

   --------------