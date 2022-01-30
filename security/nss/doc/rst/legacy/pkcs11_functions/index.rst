.. _mozilla_projects_nss_pkcs11_functions:

NSS PKCS11 Functions
====================

.. _pkcs_.2311_functions:

`PKCS #11 Functions <#pkcs_.2311_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This chapter describes the core PKCS #11 functions that an application needs for communicating
   with cryptographic modules. In particular, these functions are used for obtaining certificates,
   keys, and passwords. This was converted from `"Chapter 7: PKCS #11
   Functions" <https://www.mozilla.org/projects/security/pki/nss/ref/ssl/pkfnc.html>`__.

   -  :ref:`mozilla_projects_nss_reference`
   -  `SECMOD_LoadUserModule <#secmod_loadusermodule>`__
   -  `SECMOD_UnloadUserModule <#secmod_unloadusermodule>`__
   -  `SECMOD_OpenUserDB <#secmod_openuserdb>`__
   -  `SECMOD_CloseUserDB <#secmod_closeuserdb>`__
   -  `PK11_FindCertFromNickname <#pk11_findcertfromnickname>`__
   -  `PK11_FindKeyByAnyCert <#pk11_findkeybyanycert>`__
   -  `PK11_GetSlotName <#pk11_getslotname>`__
   -  `PK11_GetTokenName <#pk11_gettokenname>`__
   -  `PK11_IsHW <#pk11_ishw>`__
   -  `PK11_IsPresent <#pk11_ispresent>`__
   -  `PK11_IsReadOnly <#pk11_isreadonly>`__
   -  `PK11_SetPasswordFunc <#pk11_setpasswordfunc>`__

   .. rubric:: SECMOD_LoadUserModule
      :name: secmod_loadusermodule

   Load a new PKCS #11 module based on a moduleSpec.

   .. rubric:: Syntax
      :name: syntax

   .. code:: notranslate

       #include "secmod.h"

       extern SECMODModule *SECMOD_LoadUserModule(char *moduleSpec, SECMODModule *parent, PRBool recurse);

   .. rubric:: Parameters
      :name: parameters

   This function has the following parameters:

   *moduleSpec* is a pkcs #11 moduleSpec. *parent* is the moduleDB that presented this module spec.
   For applications this value should be NULL. *recurse* is a boolean indicates whether or not the
   module should also launch additional pkcs #11 modules. This is only applicable if the loaded
   module is actually a moduleDB rather than a PKCS #11 module (see
   :ref:`mozilla_projects_nss_pkcs11_module_specs`).

   .. rubric:: Returns
      :name: returns

   The function returns one of these values:

   -  If successful, a pointer to a SECMODModule. Caller owns the reference
   -  If unsuccessful, NULL.

   .. rubric:: Description
      :name: description

   SECMOD_LoadUserModule loads a new PKCS #11 module into NSS and connects it to the current NSS
   trust infrastructure. Once the module has been successfully loaded, other NSS calls will use it
   in the normal course of searching.

   *modulespec* specifies how the module should be loaded. More information about module spec is
   available at :ref:`mozilla_projects_nss_pkcs11_module_specs`. NSS parameters may be specified in
   module specs used by SECMOD_LoadUserModule.

   Module will continue to function in the NSS infrastructure until unloaded with
   SECMOD_UnloadUserModule.

   .. rubric:: SECMOD_UnloadUserModule
      :name: secmod_unloadusermodule

   Unload a PKCS #11 module.

   .. rubric:: Syntax
      :name: syntax_2

   .. code:: notranslate

       #include "secmod.h"

       extern SECStatus SECMOD_UnloadUserModule(SECMODModule *module);

   .. rubric:: Parameters
      :name: parameters_2

   This function has the following parameters:

   *module* is the module to be unloaded.

   .. rubric:: Returns
      :name: returns_2

   The function returns one of these values:

   -  If successful, SECSuccess.
   -  If unsuccessful, SECFailure.

   .. rubric:: Description
      :name: description_2

   SECMOD_UnloadUserModule detaches a module from the nss trust domain and unloads it. The module
   should have previously been loaded by SECMOD_LoadUserModule.

   .. rubric:: SECMOD_CloseUserDB
      :name: secmod_closeuserdb

   Close an already opened user database. NOTE: the database must be in the internal token, and must
   be one created with SECMOD_OpenUserDB(). Once the database is closed, the slot will remain as an
   empty slot until it's used again with SECMOD_OpenUserDB().

   .. rubric:: Syntax
      :name: syntax_3

   .. code:: notranslate

       #include <pk11pub.h>

       SECStatus SECMOD_CloseUserDB(PK11SlotInfo *slot)

   .. rubric:: Parameters
      :name: parameters_3

   This function has the following parameter:

   *slot* A pointer to a slot info structure. This slot must a slot created by SECMOD_OpenUserDB()
   at some point in the past.

   .. rubric:: Returns
      :name: returns_3

   The function returns one of these values:

   -  If successful, SECSuccess).
   -  If unsuccessful, SECFailure.

   .. rubric:: SECMOD_OpenUserDB
      :name: secmod_openuserdb

   Open a new database using the softoken.

   .. rubric:: Syntax
      :name: syntax_4

   .. code:: notranslate

       #include "pk11pub.h"

       PK11SlotInfo *SECMOD_OpenUserDB(const char *moduleSpec)

   .. rubric:: Parameters
      :name: parameters_4

   This function has the following parameters:

   *moduleSpec* is the same data that you would pass to softoken at initialization time under the
   'tokens' options.

   .. rubric:: Returns
      :name: returns_4

   The function returns one of these values:

   -  If successful, a pointer to a slot.
   -  If unsuccessful, NULL.

   .. rubric:: Description
      :name: description_3

   Open a new database using the softoken. The caller is responsible for making sure the module spec
   is correct and usable. The caller should ask for one new database per call if the caller wants to
   get meaningful information about the new database.

   moduleSpec is the same data that you would pass to softoken at initialization time under the
   'tokens' options. For example, if you would normally specify *tokens=<0x4=[configdir='./mybackup'
   tokenDescription='Backup']>* to softoken if you at init time, then you could specify
   "*configdir='./mybackup' tokenDescription='Backup'*" as your module spec here to open the
   database ./mybackup on the fly. The slot ID will be calculated for you by SECMOD_OpenUserDB().

   Typical parameters here are configdir, tokenDescription and flags. a Full list is below:

   *configDir* The location of the databases for this token. If configDir is not specified, and
   noCertDB and noKeyDB is not specified, the load will fail.

   *certPrefix* Cert prefix for this token.

   *keyPrefix* Prefix for the key database for this token. (if not specified, certPrefix will be
   used).

   *tokenDescription* The label value for this token returned in the CK_TOKEN_INFO structure with an
   internationalize string (UTF8). This value will be truncated at 32 bytes (no NULL, partial UTF8
   characters dropped). You should specify a user friendly name here as this is the value the token
   will be referred to in most application UI's. You should make sure tokenDescription is unique.

   *slotDescription* The slotDescription value for this token returned in the CK_SLOT_INFO structure
   with an internationalize string (UTF8). This value will be truncated at 64 bytes (no NULL,
   partialUTF8 characters dropped). This name will not change after thedatabase is closed. It should
   have some number to make this unique.

   *minPWLen* Then minimum password length for this token.

   | *flags* A comma separated list of flag values, parsed case-insensitive.
   | Valid flags are:

   -  *readOnly* - Databases should be opened read only.
   -  *noCertDB* - Don't try to open a certificate database.
   -  *noKeyDB* - Don't try to open a key database.
   -  *forceOpen* - Don't fail to initialize the token if thedatabases could not be opened.
   -  *passwordRequired* - zero length passwords are not acceptable(valid only if there is a keyDB).
   -  *optimizeSpace* - allocate smaller hash tables and lock tables.When this flag is not
      specified, Softoken will allocatelarge tables to prevent lock contention.

   For more info on module strings see :ref:`mozilla_projects_nss_pkcs11_module_specs`.

   This function will return a reference to a slot. The caller is responsible for freeing the slot
   reference when it is through. Freeing the slot reference will not unload the slot. That happens
   with the corresponding SECMOD_CloseUserDB() function. Until the SECMOD_CloseUserDB function is
   called, the newly opened database will be visible to any NSS calls search for keys or certs.

   .. rubric:: PK11_FindCertFromNickname
      :name: pk11_findcertfromnickname

   Finds a certificate from its nickname.

   .. rubric:: Syntax
      :name: syntax_5

   .. code:: notranslate

       #include <pk11pub.h>
       #include <certt.h>

       CERTCertificate *PK11_FindCertFromNickname(
         char *nickname,
         void *passwordArg);

   .. rubric:: Parameters
      :name: parameters_5

   This function has the following parameters:

   *nickname* A pointer to the nickname in the certificate database or to the nickname in the token.

   *passwordArg* A pointer to application data for the password callback function. This pointer is
   set with SSL_SetPKCS11PinArg during SSL configuration. To retrieve its current value, use
   SSL_RevealPinArg.

   .. rubric:: Returns
      :name: returns_5

   The function returns one of these values:

   -  If successful, a pointer to a certificate structure.
   -  If unsuccessful, NULL.

   .. rubric:: Description
      :name: description_4

   When you are finished with the certificate structure returned by PK11_FindCertFromNickname, you
   must free it by calling CERT_DestroyCertificate.

   The PK11_FindCertFromNickname function calls the password callback function set with
   PK11_SetPasswordFunc and passes it the pointer specified by the wincx parameter.

   .. rubric:: PK11_FindKeyByAnyCert
      :name: pk11_findkeybyanycert

   Finds the private key associated with a specified certificate in any available slot.

   .. rubric:: Syntax
      :name: syntax_6

   .. code:: notranslate

       #include <pk11pub.h>
       #include <certt.h>
       #include <keyt.h>

       SECKEYPrivateKey *PK11_FindKeyByAnyCert(
         CERTCertificate *cert,
         void *passwordArg);

   .. rubric:: Parameters
      :name: parameters_6

   This function has the following parameters:

   *cert* A pointer to a certificate structure in the certificate database.

   *passwordArg* A pointer to application data for the password callback function. This pointer is
   set with SSL_SetPKCS11PinArg during SSL configuration. To retrieve its current value, use
   SSL_RevealPinArg.

   .. rubric:: Returns
      :name: returns_6

   The function returns one of these values:

   -  If successful, a pointer to a private key structure.
   -  If unsuccessful, NULL.

   .. rubric:: Description
      :name: description_5

   When you are finished with the private key structure returned by PK11_FindKeyByAnyCert, you must
   free it by calling SECKEY_DestroyPrivateKey.

   The PK11_FindKeyByAnyCert function calls the password callback function set with
   PK11_SetPasswordFunc and passes it the pointer specified by the wincx parameter.

   .. rubric:: PK11_GetSlotName
      :name: pk11_getslotname

   Gets the name of a slot.

   .. rubric:: Syntax
      :name: syntax_7

   .. code:: notranslate

       #include <pk11pub.h>

       char *PK11_GetSlotName(PK11SlotInfo *slot);

   .. rubric:: Parameters
      :name: parameters_7

   This function has the following parameter:

   *slot* A pointer to a slot info structure.

   .. rubric:: Returns
      :name: returns_7

   The function returns one of these values:

   -  If successful, a pointer to the name of the slot (a string).
   -  If unsuccessful, NULL.

   .. rubric:: Description
      :name: description_6

   If the slot is freed, the string with the slot name may also be freed. If you want to preserve
   it, copy the string before freeing the slot. Do not try to free the string yourself.

   .. rubric:: PK11_GetTokenName
      :name: pk11_gettokenname

   Gets the name of the token.

   .. rubric:: Syntax
      :name: syntax_8

   .. code:: notranslate

       #include <pk11pub.h>

       char *PK11_GetTokenName(PK11SlotInfo *slot);

   .. rubric:: Parameters
      :name: parameters_8

   This function has the following parameter:

   *slot* A pointer to a slot info structure.

   .. rubric:: Returns
      :name: returns_8

   The function returns one of these values:

   -  If successful, a pointer to the name of the token (a string).
   -  If unsuccessful, NULL.

   .. rubric:: Description
      :name: description_7

   If the slot is freed, the string with the token name may also be freed. If you want to preserve
   it, copy the string before freeing the slot. Do not try to free the string yourself.

   .. rubric:: PK11_IsHW
      :name: pk11_ishw

   Finds out whether a slot is implemented in hardware or software.

   .. rubric:: Syntax
      :name: syntax_9

   .. code:: notranslate

       #include <pk11pub.h>
       #include <prtypes.h>

       PRBool PK11_IsHW(PK11SlotInfo *slot);

   .. rubric:: Parameters
      :name: parameters_9

   This function has the following parameter:

   *slot* A pointer to a slot info structure.

   .. rubric:: Returns
      :name: returns_9

   The function returns one of these values:

   -  If the slot is implemented in hardware, PR_TRUE.
   -  If the slot is implemented in software, PR_FALSE.

   .. rubric:: PK11_IsPresent
      :name: pk11_ispresent

   Finds out whether the token for a slot is available.

   .. rubric:: Syntax
      :name: syntax_10

   .. code:: notranslate

       #include <pk11pub.h>
       #include <prtypes.h>

       PRBool PK11_IsPresent(PK11SlotInfo *slot);

   .. rubric:: Parameters
      :name: parameters_10

   This function has the following parameter:

   *slot* A pointer to a slot info structure.

   .. rubric:: Returns
      :name: returns_10

   The function returns one of these values:

   -  If token is available, PR_TRUE.
   -  If token is disabled or missing, PR_FALSE.

   .. rubric:: PK11_IsReadOnly
      :name: pk11_isreadonly

   Finds out whether a slot is read-only.

   .. rubric:: Syntax
      :name: syntax_11

   .. code:: notranslate

       #include <pk11pub.h>
       #include <prtypes.h>

       PRBool PK11_IsReadOnly(PK11SlotInfo *slot);

   .. rubric:: Parameters
      :name: parameters_11

   This function has the following parameter:

   *slot* A pointer to a slot info structure.

   .. rubric:: Returns
      :name: returns_11

   The function returns one of these values:

   -  If slot is read-only, PR_TRUE.
   -  Otherwise, PR_FALSE.

   .. rubric:: PK11_SetPasswordFunc
      :name: pk11_setpasswordfunc

   Defines a callback function used by the NSS libraries whenever information protected by a
   password needs to be retrieved from the key or certificate databases.

   .. rubric:: Syntax
      :name: syntax_12

   .. code:: notranslate

       #include <pk11pub.h>
       #include <prtypes.h>

       void PK11_SetPasswordFunc(PK11PasswordFunc func);

   .. rubric:: Parameter
      :name: parameter

   This function has the following parameter:

   *func* A pointer to the callback function to set.

   .. rubric:: Description
      :name: description_8

   During the course of an SSL operation, it may be necessary for the user to log in to a PKCS #11
   token (either a smart card or soft token) to access protected information, such as a private key.
   Such information is protected with a password that can be retrieved by calling an
   application-supplied callback function. The callback function is identified in a call to
   PK11_SetPasswordFunc that takes place during NSS initialization.

   The callback function set up by PK11_SetPasswordFunc has the following prototype:

   .. code:: eval

      typedef char *(*PK11PasswordFunc)(
        PK11SlotInfo *slot,
        PRBool retry,
        void *arg);

   This callback function has the following parameters:

   *slot* A pointer to a slot info structure.

   *retry* Set to PR_TRUE if this is a retry. This implies that the callback has previously returned
   the wrong password.

   *arg* A pointer supplied by the application that can be used to pass state information. Can be
   NULL.

   This callback function returns one of these values:

   -  If successful, a pointer to the password. This memory must have been allocated with PR_Malloc
      or PL_strdup.
   -  If unsuccessful, returns NULL.

   Many tokens keep track of the number of attempts to enter a password and do not allow further
   attempts after a certain point. Therefore, if the retry argument is PR_TRUE, indicating that the
   password was tried and is wrong, the callback function should return NULL to indicate that it is
   unsuccessful, rather than attempting to return the same password again. Failing to terminate when
   the retry argument is PR_TRUE can result in an endless loop.

   Several functions in the NSS libraries use the password callback function to obtain the password
   before performing operations that involve the protected information. The third parameter to the
   password callback function is application-defined and can be used for any purpose. For example,
   Mozilla uses the parameter to pass information about which window is associated with the modal
   dialog box requesting the password from the user. When NSS SSL libraries call the password
   callback function, the value they pass in the third parameter is determined by
   SSL_SetPKCS11PinArg.

   .. rubric:: See Also
      :name: see_also

   For examples of password callback functions, see the samples in the Samples directory.