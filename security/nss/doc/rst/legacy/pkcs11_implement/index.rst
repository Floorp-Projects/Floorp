.. _mozilla_projects_nss_pkcs11_implement:

PKCS11 Implement
================

.. _implementing_pkcs_.2311_for_nss:

`Implementing PKCS #11 for NSS <#implementing_pkcs_.2311_for_nss>`__
--------------------------------------------------------------------

.. container::

   **NOTE:** This document was originally for the Netscape Security Library that came with Netscape
   Communicator 4.0. This note will be removed once the document is updated for the current version
   of NSS.

   This document supplements the information in PKCS #11: Cryptographic Token Interface Standard,
   version 2.0 with guidelines for implementors of cryptographic modules who want their products to
   work with Mozilla client software:

   -  How NSS Calls PKCS #11 Functions. Function-specific information organized in the same
      categories as the PKCS #11 specification.
   -  Functions for Different Kinds of Tokens. Summarizes the support NSS expects from different
      kinds of tokens.
   -  Installation. Installing modules and informing the user of changes in the Cryptographic
      Modules settings.
   -  Semantics Unique to NSS. Miscellaneous NSS semantics that affect module implementation.

   Future versions of Netscape server products will also support of PKCS #11 version 2.0.

   How NSS Calls PKCS #11 Functions This section is organized according to the categories used in
   PKCS #11: Cryptographic Token Interface Standard, version 2.0. To understand this section, you
   should be familiar with the standard specification.

.. _general-purpose_functions:

`General-Purpose Functions <#general-purpose_functions>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. rubric:: C_Initialize
      :name: c_initialize

   The NSS calls C_Initialize on startup or when it loads a new module. The NSS always passes NULL,
   as required by the PKCS #11 specification, in the single C_Initialize parameter pReserved.

   .. rubric:: C_Finalize
      :name: c_finalize

   The NSS calls C_Finalize on shutdown and whenever it unloads a module.

   .. rubric:: C_GetFunctionList
      :name: c_getfunctionlist

   The NSS calls C_GetFunctionList on startup or when it loads a new module. The function list
   returned should include all the PKCS 2.0 function calls. If you don't implement a function, you
   should still provide a stub that returns CKR_FUNCTION_NOT_SUPPORTED.

   .. rubric:: C_GetInfo
      :name: c_getinfo

   The NSS calls C_GetInfo on startup or when it loads a new module. The version numbers,
   manufacturer IDs, and so on are displayed when the user views the information. The supplied
   library names are used as the default library names; currently, these names should not include
   any double quotation marks. (This is more restrictive than PKCS 2.0 and may change in future
   versions of NSS.).

.. _slot_and_token_management:

`Slot and Token Management <#slot_and_token_management>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. rubric:: C_GetSlotList
      :name: c_getslotlist

   The NSS calls C_GetSlotList on startup or when it loads a new module, requests all the module's
   slots, and keeps track of the list from that point on. The slots are expected to remain static:
   that is, the module never has more slots or fewer slots than the number on the original list.

   .. rubric:: C_GetSlotInfo
      :name: c_getslotinfo

   The NSS calls C_GetSlotInfo on startup or when it loads a new module and reads in the information
   that can be viewed on the slot information page. If the CKF_REMOVABLE_DEVICE flag is set, NSS
   also calls C_GetSlotInfo whenever it looks up slots to make sure the token is present. If the
   CKF_REMOVABLE_DEVICE flag is not set, NSS uses that token information without checking again.

   If the CKF_REMOVABLE_DEVICE flag is not set, the CKF_TOKEN_PRESENT flag must be set, or else NSS
   marks the slot as bad and will never use it.

   The NSS doesn't currently use the CKF_HW_SLOT flag.

   .. rubric:: C_GetTokenInfo
      :name: c_gettokeninfo

   If a token is a permanent device (that is, if the CKF_REMOVABLE_DEVICE flag is not set), NSS
   calls C_GetTokenInfo only on startup or when it loads a new module. If the token is a removable
   device, NSS may call C_GetTokenInfo anytime it's looking for a new token to check whether the
   token is write protected, whether it can generate random numbers, and so on.

   The NSS expects CK_TOKEN_INFO.label to contain the name of the token.

   If the CKF_WRITE_PROTECTED flag is set, NSS won't use the token to generate keys.

   The NSS interprets the combination of the CKF_LOGIN_REQUIRED and CKF_USER_PIN_INITIALIZED flags
   as shown in Table 1.1.

   +-----------------------------------+--------------------------+-----------------------------------+
   | NSS's interpretation of the       |                          |                                   |
   | CKF_LOGIN_REQUIRED and            |                          |                                   |
   | CKF_USER_PIN_INITIALIZED flags    |                          |                                   |
   +-----------------------------------+--------------------------+-----------------------------------+
   | CFK_LOGIN_REQUIRED                | CFK_USER_PIN_INITIALIZED | NSS assumes that:                 |
   +-----------------------------------+--------------------------+-----------------------------------+
   | FALSE                             | FALSE                    | This is a general access device.  |
   |                                   |                          | The NSS will use it without       |
   |                                   |                          | prompting the user for a PIN.     |
   +-----------------------------------+--------------------------+-----------------------------------+
   | TRUE                              | FALSE                    | The device is uninitialized. The  |
   |                                   |                          | NSS attempts to initialize the    |
   |                                   |                          | device only if it needs to        |
   |                                   |                          | generate a key or needs to set    |
   |                                   |                          | the user PIN. The NSS calls       |
   |                                   |                          | C_InitPIN to initialize the       |
   |                                   |                          | device and set the user PIN; if   |
   |                                   |                          | these calls are successful, the   |
   |                                   |                          | key is generated and at that      |
   |                                   |                          | point the                         |
   |                                   |                          | CFK_USER_PIN_INITIALIZED flag     |
   |                                   |                          | should change from FALSE to TRUE. |
   +-----------------------------------+--------------------------+-----------------------------------+
   | FALSE                             | TRUE                     | This is a general access device   |
   |                                   |                          | that can have a PIN set on it.    |
   |                                   |                          | Because it's a general access     |
   |                                   |                          | device, NSS never prompts for the |
   |                                   |                          | PIN, even though it's possible to |
   |                                   |                          | set a PIN with C_SetPIN. If the   |
   |                                   |                          | PIN is set successfully, the      |
   |                                   |                          | CFK_LOGIN_REQUIRED flag should    |
   |                                   |                          | change to TRUE. The NSS uses this |
   |                                   |                          | combination of flags for its      |
   |                                   |                          | internal token when the key       |
   |                                   |                          | database password is NULL. These  |
   |                                   |                          | are not standard PKCS #11         |
   |                                   |                          | semantics; they are intended for  |
   |                                   |                          | NSS's internal use only.          |
   +-----------------------------------+--------------------------+-----------------------------------+
   | TRUE                              | TRUE                     | The device has been initialized   |
   |                                   |                          | and requires authentication. The  |
   |                                   |                          | NSS checks whether the user is    |
   |                                   |                          | logged on, and if not prompts the |
   |                                   |                          | user for a PIN.                   |
   +-----------------------------------+--------------------------+-----------------------------------+

   .. rubric:: C_GetMechanismList
      :name: c_getmechanismlist

   The NSS calls C_GetMechanismList fairly frequently to identify the mechanisms supported by a
   token.

   .. rubric:: C_GetMechanismInfo
      :name: c_getmechanisminfo

   The NSS currently doesn't call C_GetMechanismInfo. This function may be called in the future, so
   you should implement it anyway.

   .. rubric:: C_InitToken
      :name: c_inittoken

   The NSS never calls C_InitToken.

   .. rubric:: C_InitPIN
      :name: c_initpin

   The NSS calls C_InitPIN only in the key generation case, as noted in this document under
   C_GetTokenInfo, when CFK_LOGIN_REQUIRED = TRUE and CFK_USER_PIN_INITIALIZED = FALSE.

   .. rubric:: C_SetPIN
      :name: c_setpin

   Called only in the key generation case, as noted in this document under C_GetTokenInfo, when
   CFK_LOGIN_REQUIRED = TRUE and CFK_USER_PIN_INITIALIZED = FALSE.

.. _session_management:

`Session Management <#session_management>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. rubric:: C_OpenSession
      :name: c_opensession

   The NSS calls C_OpenSession whenever it initializes a token and keeps the session open as long as
   possible. The NSS almost never closes a session after it finishes doing something with a token.
   It uses a single session for all single-part RSA operations such as logging in, logging out,
   signing, verifying, generating keys, wrapping keys, and so on.

   The NSS opens a separate session for each part of a multipart encryption (bulk encryption). If it
   runs out of sessions, it uses the initial session for saves and restores.

   .. rubric:: C_CloseSession
      :name: c_closesession

   The NSS calls C_CloseSession to close sessions created for bulk encryption.

   .. rubric:: C_CloseAllSessions
      :name: c_closeallsessions

   The NSS may call C_CloseAllSessions when it closes down a slot.

   .. rubric:: C_GetSessionInfo
      :name: c_getsessioninfo

   The NSS calls C_GetSessionInfo frequently.

   If a token has been removed during a session, C_GetSessionInfo should return either
   CKR_SESSION_CLOSED or CKR_SESSION_HANDLE_INVALID. If a token has been removed and then the same
   or another token is inserted, C_GetSessionInfo should return CKR_SESSION_HANDLE_INVALID.

   .. rubric:: C_Login
      :name: c_login

   The NSS calls C_Login on a token's initial session whenever CKF_LOGIN_REQUIRED is TRUE and the
   user state indicates that the user isn't logged in.

   .. rubric:: C_Logout
      :name: c_logout

   The NSS calls C_Logout on a token's initial session

   -  when the password is timed out
   -  when performing any kind of private key operation if "ask always" is turned on
   -  when changing a password
   -  when the user logs out

.. _object_management:

`Object Management <#object_management>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. rubric:: C_CreateObject
      :name: c_createobject

   The NSS calls C_CreateObject when loading new private keys and new certificates into a token.
   Typically, NSS uses C_CreateObject for creating a new private key if PKCS #12 is operating or if
   your writable token doesn't support C_GenerateKeyPair. Currently PKCS #12 isn't allowed to import
   onto a token.

   The NSS also uses C_CreateObject to create new session keys. The NSS sometimes loads raw key data
   and builds a key from that.

   The NSS will be doing more and more session key generation on tokens in the future. It's also
   possible for NSS to load a key if the private key that decrypted the key is located on a
   different slot. For example, if a particular token can't do DES encryption, NSS decrypts the key,
   then copies it over to the token that can do DES encryption.

   The NSS creates certificates as token objects. It loads the token object only if the private key
   for that certificate exists on the token and was generated by NSS. All the fields defined by PKCS
   #11 for certificates are set.

   The NSS also sets the CKA_ID and CKA_LABEL attributes for the token. Currently, the CKA_ID
   attribute is set to the modulus for RSA or to the public value on DSA. The NSS may hash this
   value in the future. In either case, NSS does set the CKA_ID attribute and expects it to remain
   the same. If a certificate is loaded, the value of the certificate's CKA_ID attribute must match
   the value of the CKA_ID attribute for the corresponding private key, and the value of the
   certificate's CKA_LABEL attribute must also match the value of the CKA_LABEL attribute for the
   private key. For private keys that don't include certificates, NSS doesn't set the CKA_LABEL
   attribute, or sets it to NULL, until it receives the certificate.

   .. rubric:: C_CopyObject
      :name: c_copyobject

   The NSS rarely calls C_CopyObject but may sometimes do so for non-token private keys.

   .. rubric:: C_DestroyObject
      :name: c_destroyobject

   The NSS calls C_DestroyObject to destroy certificates and keys on tokens.

   .. rubric:: C_GetObjectSize
      :name: c_getobjectsize

   The NSS never calls C_GetObjectSize.

   .. rubric:: C_GetAttributeValue
      :name: c_getattributevalue

   The NSS calls C_GetAttributeValue to get the value of attributes for both single objects and
   multiple objects. This is useful for extracting public keys, nonsecret bulk keys, and so on.

   .. rubric:: C_SetAttributeValue
      :name: c_setattributevalue

   The NSS uses C_SetAttributeValue to change labels on private keys.

   .. rubric:: C_FindObjectsInit, C_FindObjects, C_FindFinal
      :name: c_findobjectsinit.2c_c_findobjects.2c_c_findfinal

   The NSS calls these functions frequently to look up objects by CKA_ID or CKA_LABEL. These values
   must match the equivalent values for related keys and certificates and must be unique among key
   pairs on a given token.

   The NSS also looks up certificates by CK_ISSUER and CK_SERIAL. If those fields aren't set on the
   token, S/MIME won't work.

   Functions for Different Kinds of Tokens The NSS expects different kinds of PKCS #11 support from
   four different kinds of tokens:

   -  External key distribution tokens are used with corresponding plug-ins to distribute private
      keys.
   -  Signing tokens include a signing certificate and are used to sign objects or messages or to
      perform SSL authentication. They cannot be used for encrypted S/MIME, because they can't
      decrypt messages.
   -  Signing and decryption tokens can be used for S/MIME and for encrypted transactions over
      unsecured networks such as the Internet.
   -  Multipurpose tokens provide the full range of cryptographic services. They can be thought of
      as cryptographic accelerator cards. Future releases of NSS will also support multipurpose
      tokens that are FIPS-140 compliant.

   Table 1.2 summarizes the PKCS #11 functions (in addition to the other functions described in this
   document) that NSS expects each type of token to support.

   +------------------------+------------------------+------------------------+------------------------+
   | PKCS #11 functions     |                        |                        |                        |
   | required for different |                        |                        |                        |
   | kinds of tokens        |                        |                        |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | External key           | Signing tokens         | Signing and decryption | Multipurpose tokens    |
   | distribution tokens    |                        | tokens                 |                        |
   +------------------------+------------------------+------------------------+------------------------+
   |                        |                        |                        | C_Encrypt              |
   +------------------------+------------------------+------------------------+------------------------+
   | C_Decrypt              |                        | C_Decrypt              | C_Decrypt              |
   |                        |                        |                        |                        |
   | -  CKM_RSA_PKCS        |                        | -  CKM_RSA_PKCS        |                        |
   | -  CKM_RSA_X_509 (SSL  |                        | -  CKM_RSA_X_509 (SSL  |                        |
   |    2.0 server only)    |                        |    2.0 server only)    |                        |
   +------------------------+------------------------+------------------------+------------------------+
   | C_Sign                 | C_Sign                 | C_Sign                 | C_Sign                 |
   |                        |                        |                        |                        |
   | -  CKM_RSA_PKCS        | -  CKM_RSA_PKCS        | -  CKM_RSA_PKCS        | -  CKM_RSA_PKCS        |
   | -  CKM_DSA             | -  CKM_DSA             | -  CKM_DSA             | -  CKM_DSA             |
   +------------------------+------------------------+------------------------+------------------------+
   |                        |                        |                        | C_Verify               |
   |                        |                        |                        |                        |
   |                        |                        |                        | -  CKM_RSA_PKCS        |
   |                        |                        |                        | -  CKM_DSA             |
   +------------------------+------------------------+------------------------+------------------------+
   |                        |                        |                        | C_VerifyRecover        |
   |                        |                        |                        |                        |
   |                        |                        |                        | -  CKM_RSA_PKCS        |
   +------------------------+------------------------+------------------------+------------------------+
   |                        |                        |                        | C_GenerateKey          |
   +------------------------+------------------------+------------------------+------------------------+
   | C_GenerateKeyPair (if  | C_GenerateKeyPair (if  | C_GenerateKeyPair (if  | C_GenerateKeyPair (if  |
   | token is read/write)   | token is read/write)   | token is read/write)   | token is read/write)   |
   +------------------------+------------------------+------------------------+------------------------+
   |                        |                        |                        | C_WrapKey              |
   +------------------------+------------------------+------------------------+------------------------+
   | C_UnwrapKey            | C_UnwrapKey            | C_UnwrapKey            | C_UnwrapKey            |
   |                        |                        |                        |                        |
   | -  CKM_RSA_PKCS        | -  CKM_RSA_PKCS        | -  CKM_RSA_PKCS        | -  CKM_RSA_PKCS        |
   +------------------------+------------------------+------------------------+------------------------+
   |                        |                        |                        | C_GenerateRandom       |
   +------------------------+------------------------+------------------------+------------------------+
   |                        |                        |                        | C_Save (when token     |
   |                        |                        |                        | runs out of sessions)  |
   +------------------------+------------------------+------------------------+------------------------+
   |                        |                        |                        | C_Restore (when token  |
   |                        |                        |                        | runs out of sessions)  |
   +------------------------+------------------------+------------------------+------------------------+

   External key tokens need to support C_Decrypt and C_Sign. If they have a read/write value and
   can't generate a key pair, NSS uses its own C_GenerateKeyPair and loads the key with
   C_CreateObject.

   Signing tokens just need to support C_Sign and possibly C_GenerateKeyPair.

   In addition to C_Sign and C_GenerateKeyPair, signing and decryption tokens should also support
   C_Decrypt and, optionally, C_UnwrapKey.

   Multipurpose tokens should support all the functions listed in Table 1.2, except that C_WrapKey
   and C_UnwrapKey are optional. The NSS always attempts to use these two functions but uses
   C_Encrypt and C_Decrypt instead if C_WrapKey and C_UnwrapKey aren't implemented.

`Installation <#installation>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   You can install your module in any convenient location on the user's hard disk, but you must tell
   the user to type the module name and location in the Cryptographic Modules portion of the
   Communicator Security Info window. To do so, the user should follow these steps:

   #. Click the Security icon near the top of any Communicator window.
   #. In the Security Info window, click Cryptographic Modules.
   #. In the Cryptographic Modules frame, click Add.
   #. In the Create a New Security Module dialog box, add the Security Module Name for your module
      and the full pathname for the Security Module File.

   To avoid requiring the user to type long pathnames, make sure your module is not buried too
   deeply.

.. _semantics_unique_to_nss:

`Semantics Unique to NSS <#semantics_unique_to_nss>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   These sections describe semantics required by NSS but not specified by PKCS #11.

   .. rubric:: Supporting Multiple Sessions
      :name: supporting_multiple_sessions

   If you support multiple sessions simultaneously and if you wish to support C_InitPIN, C_SetPIN,
   or C_GenerateKeyPair, you must support simultaneous read-only and read/write sessions.

   .. rubric:: Random-Number Generation and Simple Digesting
      :name: random-number_generation_and_simple_digesting

   The NSS requires that the following functions operate without authenticating to the token:
   C_SeedRandom, C_GenerateRandom, and C_Digest (for SHA, MD5, and MD2). If your token requires
   authentication before executing these functions, your token cannot provide the default
   implementation for them. (You can still use your token for other default functions.) NSS does not
   support replacement of default functions. Later versions will provide such support.

   .. rubric:: Read/Write and Read-Only Requirements
      :name: read.2fwrite_and_read-only_requirements

   The NSS assumes that the following operations always require a read/write session:

   -  creating a token object, such as with C_CreateObject (token) or C_DestroyObject (token)
   -  changing a password
   -  initializing a token

   Creating session objects must work with a read-only session.

   .. rubric:: Creating an RSA Private Key
      :name: creating_an_rsa_private_key

   When NSS creates an RSA private key with C_CreateObject, it writes the entire set of RSA
   components. It expects to be able to read back the modulus and the value of the CKA_ID attribute.
   It also expects to be able to set the label and the subject on the key after creating it.

   .. rubric:: Encrypting Email
      :name: encrypting_email

   If you wish to support encrypted email, your token must be able to look up a certificate by the
   issuer and serial number attributes. When NSS loads a certificate, it sets these attributes
   correctly. Token initialization software that you supply should also set these fields.

   .. rubric:: Use of Key IDs
      :name: use_of_key_ids

   The NSS associates a key with its certificates by its key ID (CKA-ID). It doesn't matter how the
   key ID is generated, as long as it is unique for the token and maps to a certificate to it
   associated private key. More than one certificate can point to the same private key.

   The only exception to this requirement involves key generation for a new certificate, during
   which an orphan key waits for a brief time for a matching certificate. The NSS uses part of the
   public key (modulus for RSA, value for DSA) as the key ID during this time.

   NSS doesn't require token public keys, but if they exist, NSS expects the value of the CKA_ID
   attribute to be associated with private key and any related certificates.

   .. rubric:: Sessions and Session Objects
      :name: sessions_and_session_objects

   The NSS depends on a PKCS #11 v. 2.0 semantic requiring all session objects to be visible in all
   of a token's sessions.