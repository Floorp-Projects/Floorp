.. _mozilla_projects_nss_ssl_functions_pkfnc:

pkfnc
=====

.. container::

   .. note::

      -  This page is part of the :ref:`mozilla_projects_nss_ssl_functions_old_ssl_reference` that
         we are migrating into the format described in the `MDN Style
         Guide <https://developer.mozilla.org/en-US/docs/MDN/Guidelines>`__. If you are inclined to
         help with this migration, your help would be very much appreciated.

      -  Upgraded documentation may be found in the :ref:`mozilla_projects_nss_reference`

   .. rubric:: PKCS #11 Functions
      :name: PKCS_11_Functions

   --------------

.. _chapter_7_pkcs_11_functions:

`Chapter 7
PKCS #11 Functions <#chapter_7_pkcs_11_functions>`__
----------------------------------------------------

.. container::

   This chapter describes the core PKCS #11 functions that an application needs for communicating
   with cryptographic modules. In particular, these functions are used for obtaining certificates,
   keys, and passwords.

   |  ```PK11_FindCertFromNickname`` <#1035673>`__
   | ```PK11_FindKeyByAnyCert`` <#1026891>`__
   | ```PK11_GetSlotName`` <#1030779>`__
   | ```PK11_GetTokenName`` <#1026964>`__
   | ```PK11_IsHW`` <#1026762>`__
   | ```PK11_IsPresent`` <#1022948>`__
   | ```PK11_IsReadOnly`` <#1022991>`__
   | ```PK11_SetPasswordFunc`` <#1023128>`__

   .. rubric:: PK11_FindCertFromNickname
      :name: pk11_findcertfromnickname

   Finds a certificate from its nickname.

   .. rubric:: Syntax
      :name: syntax

   .. code:: notranslate

      #include <pk11func.h>
      #include <certt.h>

   .. code:: notranslate

      CERTCertificate *PK11_FindCertFromNickname(
         char *nickname,
         void *wincx);

   .. rubric:: Parameters
      :name: parameters

   This function has the following parameters:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to the nickname in the certificate    |
   |                                                 | database or to the nickname in the token.       |
   |    nickname                                     |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to application data for the password  |
   |                                                 | callback function. This pointer is set with     |
   |    wincx                                        | :ref:`moz                                       |
   |                                                 | illa_projects_nss_ssl_functions_sslfnc#1088040` |
   |                                                 | during SSL configuration. To retrieve its       |
   |                                                 | current value, use                              |
   |                                                 | :ref:`mozi                                      |
   |                                                 | lla_projects_nss_ssl_functions_sslfnc#1123385`. |
   +-------------------------------------------------+-------------------------------------------------+

   .. rubric:: Returns
      :name: returns

   The function returns one of these values:

   -  If successful, a pointer to a certificate structure.
   -  If unsuccessful, ``NULL``.

   .. rubric:: Description
      :name: description

   A nickname is an alias for a certificate subject. There may be multiple certificates with the
   same subject, and hence the same nickname. This function will return the newest certificate that
   matches the subject, based on the NotBefore / NotAfter fields of the certificate. When you are
   finished with the certificate structure returned by ``PK11_FindCertFromNickname``, you must free
   it by calling ```CERT_DestroyCertificate`` <sslcrt.html#1050532>`__.

   The ``PK11_FindCertFromNickname`` function calls the password callback function set with
   ```PK11_SetPasswordFunc`` <#1023128>`__ and passes it the pointer specified by the ``wincx``
   parameter.

   .. rubric:: PK11_FindKeyByAnyCert
      :name: pk11_findkeybyanycert

   Finds the private key associated with a specified certificate in any available slot.

   .. rubric:: Syntax
      :name: syntax_2

   .. code:: notranslate

      #include <pk11func.h>
      #include <certt.h>
      #include <keyt.h>

   .. code:: notranslate

      SECKEYPrivateKey *PK11_FindKeyByAnyCert(
         CERTCertificate *cert,
         void *wincx);

   .. rubric:: Parameters
      :name: parameters_2

   This function has the following parameters:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to a certificate structure in the     |
   |                                                 | certificate database.                           |
   |    cert                                         |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to application data for the password  |
   |                                                 | callback function. This pointer is set with     |
   |    wincx                                        | :ref:`moz                                       |
   |                                                 | illa_projects_nss_ssl_functions_sslfnc#1088040` |
   |                                                 | during SSL configuration. To retrieve its       |
   |                                                 | current value, use                              |
   |                                                 | :ref:`mozi                                      |
   |                                                 | lla_projects_nss_ssl_functions_sslfnc#1123385`. |
   +-------------------------------------------------+-------------------------------------------------+

   .. rubric:: Returns
      :name: returns_2

   The function returns one of these values:

   -  If successful, a pointer to a private key structure.
   -  If unsuccessful, ``NULL``.

   .. rubric:: Description
      :name: description_2

   When you are finished with the private key structure returned by ``PK11_FindKeyByAnyCert``, you
   must free it by calling ```SECKEY_DestroyPrivateKey`` <sslkey.html#1051017>`__.

   The ``PK11_FindKeyByAnyCert`` function calls the password callback function set with
   ```PK11_SetPasswordFunc`` <#1023128>`__ and passes it the pointer specified by the ``wincx``
   parameter.

   .. rubric:: PK11_GetSlotName
      :name: pk11_getslotname

   Gets the name of a slot.

   .. rubric:: Syntax
      :name: syntax_3

   .. code:: notranslate

      #include <pk11func.h>

   .. code:: notranslate

      char *PK11_GetSlotName(PK11SlotInfo *slot);

   .. rubric:: Parameters
      :name: parameters_3

   This function has the following parameter:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to a slot info structure.             |
   |                                                 |                                                 |
   |    slot                                         |                                                 |
   +-------------------------------------------------+-------------------------------------------------+

   .. rubric:: Returns
      :name: returns_3

   The function returns one of these values:

   -  If successful, a pointer to the name of the slot (a string).
   -  If unsuccessful, ``NULL``.

   .. rubric:: Description
      :name: description_3

   If the slot is freed, the string with the slot name may also be freed. If you want to preserve
   it, copy the string before freeing the slot. Do not try to free the string yourself.

   .. rubric:: PK11_GetTokenName
      :name: pk11_gettokenname

   Gets the name of the token.

   .. rubric:: Syntax
      :name: syntax_4

   .. code:: notranslate

      #include <pk11func.h>

   .. code:: notranslate

      char *PK11_GetTokenName(PK11SlotInfo *slot);

   .. rubric:: Parameters
      :name: parameters_4

   This function has the following parameter:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to a slot info structure.             |
   |                                                 |                                                 |
   |    slot                                         |                                                 |
   +-------------------------------------------------+-------------------------------------------------+

   .. rubric:: Returns
      :name: returns_4

   The function returns one of these values:

   -  If successful, a pointer to the name of the token (a string).
   -  If unsuccessful, ``NULL``.

   .. rubric:: Description
      :name: description_4

   If the slot is freed, the string with the token name may also be freed. If you want to preserve
   it, copy the string before freeing the slot. Do not try to free the string yourself.

   .. rubric:: PK11_IsHW
      :name: pk11_ishw

   Finds out whether a slot is implemented in hardware or software.

   .. rubric:: Syntax
      :name: syntax_5

   .. code:: notranslate

      #include <pk11func.h>
      #include <prtypes.h>

   .. code:: notranslate

      PRBool PK11_IsHW(PK11SlotInfo *slot);

   .. rubric:: Parameters
      :name: parameters_5

   This function has the following parameter:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to a slot info structure.             |
   |                                                 |                                                 |
   |    slot                                         |                                                 |
   +-------------------------------------------------+-------------------------------------------------+

   .. rubric:: Returns
      :name: returns_5

   The function returns one of these values:

   -  If the slot is implemented in hardware, ``PR_TRUE``.
   -  If the slot is implemented in software, ``PR_FALSE``.

   .. rubric:: PK11_IsPresent
      :name: pk11_ispresent

   Finds out whether the token for a slot is available.

   .. rubric:: Syntax
      :name: syntax_6

   .. code:: notranslate

      #include <pk11func.h>
      #include <prtypes.h>

   .. code:: notranslate

      PRBool PK11_IsPresent(PK11SlotInfo *slot);

   .. rubric:: Parameters
      :name: parameters_6

   This function has the following parameter:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to a slot info structure.             |
   |                                                 |                                                 |
   |    slot                                         |                                                 |
   +-------------------------------------------------+-------------------------------------------------+

   .. rubric:: Returns
      :name: returns_6

   The function returns one of these values:

   -  If token is available, ``PR_TRUE``.
   -  If token is disabled or missing, ``PR_FALSE``.

   .. rubric:: PK11_IsReadOnly
      :name: pk11_isreadonly

   Finds out whether a slot is read-only.

   .. rubric:: Syntax
      :name: syntax_7

   .. code:: notranslate

      #include <pk11func.h>
      #include <prtypes.h>

   .. code:: notranslate

      PRBool PK11_IsReadOnly(PK11SlotInfo *slot);

   .. rubric:: Parameters
      :name: parameters_7

   This function has the following parameter:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to a slot info structure.             |
   |                                                 |                                                 |
   |    slot                                         |                                                 |
   +-------------------------------------------------+-------------------------------------------------+

   .. rubric:: Returns
      :name: returns_7

   The function returns one of these values:

   -  If slot is read-only, ``PR_TRUE``.
   -  Otherwise, ``PR_FALSE``.

   .. rubric:: PK11_SetPasswordFunc
      :name: pk11_setpasswordfunc

   Defines a callback function used by the NSS libraries whenever information protected by a
   password needs to be retrieved from the key or certificate databases.

   .. rubric:: Syntax
      :name: syntax_8

   .. code:: notranslate

      #include <pk11func.h>
      #include <prtypes.h>

   .. code:: notranslate

      void PK11_SetPasswordFunc(PK11PasswordFunc func);

   .. rubric:: Parameter
      :name: parameter

   This function has the following parameter:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to the callback function to set.      |
   |                                                 |                                                 |
   |    func                                         |                                                 |
   +-------------------------------------------------+-------------------------------------------------+

   .. rubric:: Description
      :name: description_5

   During the course of an SSL operation, it may be necessary for the user to log in to a PKCS #11
   token (either a smart card or soft token) to access protected information, such as a private key.
   Such information is protected with a password that can be retrieved by calling an
   application-supplied callback function. The callback function is identified in a call to
   ``PK11_SetPasswordFunc`` that takes place during NSS initialization.

   The callback function set up by ``PK11_SetPasswordFunc`` has the following prototype:

   .. code:: notranslate

      typedef char *(*PK11PasswordFunc)(
         PK11SlotInfo *slot,
         PRBool retry,
         void *arg);

   This callback function has the following parameters:

   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer to a slot info structure.             |
   |                                                 |                                                 |
   |    slot                                         |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | Set to ``PR_TRUE`` if this is a retry. This     |
   |                                                 | implies that the callback has previously        |
   |    retry                                        | returned the wrong password.                    |
   +-------------------------------------------------+-------------------------------------------------+
   | .. code:: notranslate                           | A pointer supplied by the application that can  |
   |                                                 | be used to pass state information. Can be       |
   |    arg                                          | ``NULL``.                                       |
   +-------------------------------------------------+-------------------------------------------------+

   This callback function returns one of these values:

   -  If successful, a pointer to the password. This memory must have been allocated with
      ```PR_Malloc`` <../../../../../nspr/reference/html/prmem2.html#21428>`__ or
      ```PL_strdup`` <../../../../../nspr/reference/html/plstr.html#21753>`__.
   -  If unsuccessful, returns ``NULL``.

   Many tokens keep track of the number of attempts to enter a password and do not allow further
   attempts after a certain point. Therefore, if the ``retry`` argument is ``PR_TRUE``, indicating
   that the password was tried and is wrong, the callback function should return ``NULL`` to
   indicate that it is unsuccessful, rather than attempting to return the same password again.
   Failing to terminate when the ``retry`` argument is ``PR_TRUE`` can result in an endless loop.

   Several functions in the NSS libraries use the password callback function to obtain the password
   before performing operations that involve the protected information. The third parameter to the
   password callback function is application-defined and can be used for any purpose. For example,
   Communicator uses the parameter to pass information about which window is associated with the
   modal dialog box requesting the password from the user. When NSS libraries call the password
   callback function, the value they pass in the third parameter is determined by
   :ref:`mozilla_projects_nss_ssl_functions_sslfnc#1088040`.

   .. rubric:: See Also
      :name: see_also

   For examples of password callback functions, see the samples in the
   :ref:`mozilla_projects_nss_nss_sample_code` directory.