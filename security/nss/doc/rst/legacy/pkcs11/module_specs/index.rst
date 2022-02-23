.. _mozilla_projects_nss_pkcs11_module_specs:

PKCS #11 Module Specs
=====================

.. _pkcs_.2311_module_specs:

`PKCS #11 Module Specs <#pkcs_.2311_module_specs>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The following is a proposal to the `PKCS <https://en.wikipedia.org/wiki/PKCS>`__ #11 working
   group made in August 2001 for configuring PKCS #11 modules. NSS currently implements this
   proposal internally.

   The file format consists of name/value pairs of the form ``name=value``. Each name/value pair is
   separated by a blank value. A single line, terminated by a '\n', '\r\n', or '\r' represents a
   single pkcs #11 library.

   Names can be any alpha/numeric combination, and are parsed case-insensitive.

   Values can contain any printable ASCII value, including UTF8 characters. Values can contain
   embedded blanks either through quoting the entire value, or by escaping the embedded blanks with
   '\'. The value is considered quoted if the first character after the '=' is ', ", {, [, or <. If
   the value is quoted, then the value is terminated with and ending quote of the form ', ", ), ],
   }, or > matching the respective starting quote. Ending quotes can be escaped. Embedded '\'
   characters are considered escape characters for the next character in the stream. Note that case
   must be preserved in the values.

   These modules specs can be passed by the application directly to NSS via the
   ``SECMOD_LoadUserModule()`` call. To initialize a PKCS #11 module 'on-the-fly'.

   .. rubric:: Recognized Names
      :name: recognized_names

   All applications/libraries must be able recognize the following name values:

   library 
      This specifies the path to the pkcs #11 library.
   name 
      This specifies the name of the pkcs #11 library.
   parameter 
      This specifies a pkcs #11 library parameter with the application must pass to the pkcs #11
      library at ``C_Initialize()`` time (see below).

   In additions applications/libraries should be able to ignore additional name value pairs which
   are used to specify configuration for other applications. Of course these application/libraries
   should be able to parse their own name/value pairs.

   Each of these name/value pairs are optional.

   If the library is not specified, the line represents some application specific meta configuration
   data. Other applications and libraries can safely ignore this line.

   If the name is not specified, the application can use the library path to describe the PKCS #11
   library in any UI it may have.

   If the parameter is not specified, no parameters are passed to the PKCS #11 module.

   If the application/library does not find its application/library specific data, it should use
   it's defaults for this pkcs #11 library.

   .. rubric:: Parameter Passing
      :name: parameter_passing

   If the parameter is specified, the application/library will strip the value out, processing any
   outter quotes and escapes appropriately, and pass the parameter to the pkcs #11 library when it
   calls ``C_Initialize()``.

   A new ``CK_C_INITIALIZE_ARGS`` structure is defined as

   .. code:: notranslate

      typedef struct CK_C_INITIALIZE_ARGS {
        CK_CREATEMUTEX CreateMutex;
        CK_DESTROYMUTEX DestroyMutex;
        CK_LOCKMUTEX LockMutex;
        CK_UNLOCKMUTEX UnlockMutex;
        CK_FLAGS flags;
        CK_VOID_PTR LibraryParameters;
        CK_VOID_PTR pReserved;
      } CK_C_INITIALIZE_ARGS;

   Applications/libraries must set LibraryParameters to ``NULL`` if no parameter value is specified.
   PKCS #11 libraries which accept parameters must check if the 'new' ``pReserved`` field is
   ``NULL`` if and only if ``LibraryParameters`` field is not ``NULL``.

.. _nss_specific_parameters_in_module_specs:

`NSS Specific Parameters in Module Specs <#nss_specific_parameters_in_module_specs>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Here are the NSS Application specific parameters in use. This data is currently stored in
   ``secmod.db`` or pkcs11.txt. This isn't part of the generic spec (that is other applications need
   not parse it, nor pkcs #11 modules need supply them or use them).

   .. code:: eval

      NSS="nss_params"

   ``nss_params`` are themselves name/value pairs, parsed with the same rules described above. Valid
   names inside ``nss_params`` are:

   flags
      comma separated list of flag values, parsed case-insensitive.
      Valid flag values are:

      internal
         this library is actually the Netscape internal library
      fips
         this library is the Netscape internal fips library.
      critical
         if this library cannot be loaded, completely fail initialization.
      moduleDB
         this library includes NSS specific functions to supply additional module specs for loading.
         **moduleDBOnly** - this library has no PKCS #11 functions and is only used for loading
         additional modules.
   trustOrder
      integer value specifying the order in which the trust information for certificates specified
      by tokens on this PKCS #11 library should be rolled up. A value of 0 means that tokens on this
      library should not supply trust information. The default trust order value is 50. The relative
      order of two pkcs#11 libraries which have the same trustOrder value is undefined.
   cipherOrder
      integer value specifiying the order in which tokens are searched when looking for a token to
      do a generic operation (DES/Hashing, etc).
   ciphers
      comma separated list of ciphers this token will enable that isn't already enabled by the
      library (currently only **FORTEZZA** is defined) (case-insensitive).
   slotParams
      space separated list of name/value pairs where the name is a slotID and the value is a space
      separated list of parameters related to that slotID. Valid slotParams values are:

      slotFlags
         comma separated list of cipher groups which this slot is expected to be the default
         implementation for (case-insensitive).
         Valid flags are:

         RSA
            This token should be used for all RSA operations (other than Private key operations
            where the key lives in another token).
         DSA
            This token should be used for all DSA operations (other than Private key operations
            where the key lives in another token).
         RC4
            This token should be used for all RC4 operations which are not constrained by an
            existing key in another token.
         RC2
            This token should be used for all RC2 operations which are not constrained by an
            existing key in another token.
         DES
            This token should be used for all DES, DES2, and DES3 operations which are not
            constrained by an existing key in another token.
         DH
            This token should be used for all DH operations (other than Private key operations where
            the key lives in another token).
         FORTEZZA
            This token should be used for all KEA operations (other than Private key operations
            where the key lives in another token), as well as SKIPJACK operations which are not
            constrained by an existing key in another token.
         RC5
            This token should be used for all RC5 operations which are not constrained by an
            existing key in another token.
         SHA1
            This token should be used for all basic SHA1 hashing.
         MD5
            This token should be used for all basic MD5 hashing.
         MD2
            This token should be used for all basic MD2 hashing.
         SSL
            This token should be used for SSL key derivation which are not constrained by an
            existing key in another token.
         TLS
            This token should be used for TLS key derivation which are not constrained by an
            existing key in another token.
         AES
            This token should be used for all AES operations which are not constrained by an
            existing key in another token.
         RANDOM
            This token should be used to generate random numbers when the application call
            'PK11_GenerateRandom'.
         PublicCerts
            The certificates on this token can be read without authenticating to this token, and any
            user certs on this token have a matching public key which is also readable without
            authenticating. Setting this flags means NSS will not try to authenticate to the token
            when searching for Certificates. This removes spurious password prompts, but if
            incorrectly set it can also cause NSS to miss certificates in a token until that token
            is explicitly logged in.
      rootFlags
         comma separated of flags describing any root certs that may be stored (case-insensitive).
         Valid flags are:

         hasRootCerts
            claims that this token has the default root certs and trust values. At init time NSS,
            will try to look for a default root cert device if one has not already been loaded.
         hasRootTrust
            parsed but ignored.
      timeout
         time in minutes before the current authentication should be rechecked. This value is only
         used if askpwd is set to 'timeout'. (default = 0).
      askpwd
         case-insensitive flag describing how password prompts should be manages. Only one of the
         following can be specified.

         every
            prompt whenever the a private key on this token needs to be access (this is on the
            entire token, not on a key-by-key basis.
         timeout
            whenever the last explicit login was longer than 'timeout' minutes ago.
         only
            authenticate to the token only when necessary (default).

   Sample file:

   .. code:: notranslate

      library= name="Netscape Internal Crypto Module"   parameters="configdir=/u/relyea/.netscape certprefix= secmod=secmod.db" NSS="Flags=internal,pkcs11module TrustOrder=1 CipherOrder=-1 ciphers= slotParams={0x1=[slotFlags='RSA,DSA,DH,RC4,RC2,DES,MD2,MD5,SHA1,SSL,TLS,PublicCerts,Random'] 0x2=[slotFlags='RSA' askpw=only]}"
      library=dkck32.dll name="DataKey SignaSURE 3600" NSS="TrustOrder=50 ciphers= "
      library=swft32.dll name="Netscape Software Fortezza" parameters="keyfile=/u/relyea/keyfile" NSS="TrustOrder=50 ciphers=FORTEZZA slotParams=0x1=[slotFlags='FORTEZZA']"
      library=core32.dll name="Litronic Netsign"

.. _softoken_specific_parameters:

`Softoken Specific Parameters <#softoken_specific_parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   The internal NSS PKCS #11 implementation (softoken) requires Applications parameters. It will not
   initialize if the **parameters**\ = is not specified. If another application wishes to load the
   softoken, that application must supply a non-``NULL`` ``libraryParameters`` value in the
   ``CK_C_INITIALIZE_ARGS`` structure passed at ``C_INITIALIZE`` time. The parameter passed to
   softoken is a space separated list of name/value pairs exactly like those specified in the PKCS
   #11 module spec.

   Valid values are:

   configDir 
      Configuration Directory where NSS can store persistant state information (typically
      databases).
   secmod 
      Name of the secmod database (default = secmod.db).
   certPrefix 
      Prefix for the cert database.
   keyPrefix 
      Prefix for the key database.
   minPWLen 
      Minimum password length in bytes.
   manufacturerID 
      Override the default ``manufactureID`` value for the module returned in the ``CK_INFO``,
      ``CK_SLOT_INFO``, and ``CK_TOKEN_INFO`` structures with an internationalize string (UTF8).
      This value will be truncated at 32 bytes (no NULL, partial UTF8 characters dropped).
   libraryDescription 
      Override the default ``libraryDescription`` value for the module returned in the ``CK_INFO``
      structure with an internationalize string (UTF8). This value will be truncated at 32 bytes (no
      ``NULL``, partial UTF8 characters dropped).
   cryptoTokenDescription 
      Override the default label value for the internal crypto token returned in the
      ``CK_TOKEN_INFO`` structure with an internationalize string (UTF8). This value will be
      truncated at 32 bytes (no NULL, partial UTF8 characters dropped).
   dbTokenDescription 
      Override the default label value for the internal DB token returned in the ``CK_TOKEN_INFO``
      structure with an internationalize string (UTF8). This value will be truncated at 32 bytes (no
      NULL, partial UTF8 characters dropped).
   FIPSTokenDescription 
      Override the default label value for the internal FIPS token returned in the ``CK_TOKEN_INFO``
      structure with an internationalize string (UTF8). This value will be truncated at 32 bytes (no
      NULL, partial UTF8 characters dropped).
   cryptoSlotDescription 
      Override the default ``slotDescription`` value for the internal crypto token returned in the
      ``CK_SLOT_INFO`` structure with an internationalize string (UTF8). This value will be
      truncated at 64 bytes (no NULL, partial UTF8 characters dropped).
   dbSlotDescription 
      Override the default ``slotDescription`` value for the internal DB token returned in the
      ``CK_SLOT_INFO`` structure with an internationalize string (UTF8). This value will be
      truncated at 64 bytes (no NULL, partial UTF8 characters dropped).
   FIPSSlotDescription 
      Override the default ``slotDescription`` value for the internal FIPS token returned in the
      ``CK_SLOT_INFO`` structure with an internationalize string (UTF8). This value will be
      truncated at 64 bytes (no NULL, partial UTF8 characters dropped).
   flags 
      comma separated list of flag values, parsed case-insensitive.

   .. rubric:: Flags
      :name: flags

   Valid flags are:

   noModDB 
      Don't open ``secmod.db`` and try to supply the strings. The MOD DB function is not through
      standard PKCS #11 interfaces.
   readOnly 
      Databases should be opened read only.
   noCertDB 
      Don't try to open a certificate database.
   noKeyDB 
      Don't try to open a key database.
   forceOpen 
      Don't fail to initialize the token if the databases could not be opened.
   passwordRequired 
      Zero length passwords are not acceptable (valid only if there is a keyDB).
   optimizeSpace 
      allocate smaller hash tables and lock tables. When this flag is not specified, Softoken will
      allocate large tables to prevent lock contention.
   tokens 
      configure 'tokens' by hand. The tokens parameter specifies a space separated list of slotIDS,
      each of which specify their own set of parameters affecting that token. Typically 'tokens'
      would not be specified unless additional databases are to be opened as additional tokens. If
      tokens is specified, then all tokens (including the default tokens) need to be specified. If
      tokens is not specified, then softoken would default to the following specs:

   In non-FIPS mode:

   .. code:: eval

      tokens=<0x01=[configDir=configDir tokenDescription=cryptoTokenDescription slotDescription=cryptoSlotDescription flags=noCertDB,noKeyDB,optimizeSpace] 0x02=[configDir=configDir tokenDescription=dbTokenDescription slotDescription=dbSlotDescription certPrefix=certPrefix keyPrefix=keyPrefix flags=flags minPWLen=minPWLen]>

   In FIPS mode:

   .. code:: eval

      tokens=<0x03=[configDir=configDir tokenDescription=FIPSTokenDescription slotDescription=FIPSSlotDescription certPrefix=certPrefix keyPrefix=keyPrefix flags=flags minPWLen=minPWLen]>

   where *configDir*, *cryptoTokenDescription*, *cryptoSlotDescription*, *dbTokenDescription*,
   *dbSlotDescription*, *FIPSTokenDescription*, *FIPSSlotDescription*, *optimizeSpace*,
   *certPrefix*, *keyPrefix*, *flags*, and *minPWLen* are copied from the parameters above.

   Parameters:

   configDir 
      The location of the databases for this token. If ``configDir`` is not specified, the default
      ``configDir`` specified earlier will be used.
   certPrefix 
      Cert prefix for this token.
   keyPrefix 
      Prefix for the key database for this token.
   tokenDescription 
      The label value for this token returned in the ``CK_TOKEN_INFO`` structure with an
      internationalize string (UTF8). This value will be truncated at 32 bytes (no NULL, partial
      UTF8 characters dropped).
   slotDescription 
      The ``slotDescription`` value for this token returned in the ``CK_SLOT_INFO`` structure with
      an internationalize string (UTF8). This value will be truncated at 64 bytes (no NULL, partial
      UTF8 characters dropped).
   minPWLen 
      minimum password length for this token.
   flags 
      comma separated list of flag values, parsed case-insensitive.
      Valid flags are:

      readOnly 
         Databases should be opened read only.
      noCertDB 
         Don't try to open a certificate database.
      noKeyDB 
         Don't try to open a key database.
      forceOpen 
         Don't fail to initialize the token if the databases could not be opened.
      passwordRequired 
         Zero length passwords are not acceptable (valid only if there is a ``keyDB``).
      optimizeSpace 
         allocate smaller hash tables and lock tables. When this flag is not specified, Softoken
         will allocate large tables to prevent lock contention.