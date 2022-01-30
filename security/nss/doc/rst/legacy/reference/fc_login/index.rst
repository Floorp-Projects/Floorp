.. _mozilla_projects_nss_reference_fc_login:

FC_Login
========

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   ``FC_Login()`` - log a user into a token.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_Login(
        CK_SESSION_HANDLE hSession,
        CK_USER_TYPE userType,
        CK_CHAR_PTR pPin,
        CK_ULONG ulPinLen
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_Login()`` takes four parameters:

   ``hSession``
      [in] a session handle
   ``userType``
      [in] the user type (``CKU_SO`` or ``CKU_USER``)
   ``pPin``
      [in] a pointer that points to the user's PIN
   ``ulPinLen``
      [in] the length of the PIN

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_Login()`` logs a user into a token.

   The Security Officer (``CKU_SO``) only logs in to initialize the normal user's PIN. The SO PIN is
   the empty string. The NSS cryptographic module doesn't allow the SO to log in if the normal
   user's PIN is already initialized.

.. _return_value:

`Return value <#return_value>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_Login()`` returns the following return codes.

   -  ``CKR_OK``: the user logged in successfully.
   -  ``CKR_DEVICE_ERROR``: the token is in the Error state.
   -  ``CKR_HOST_MEMORY``: memory allocation failed.
   -  ``CKR_PIN_INCORRECT``: the PIN is incorrect.
   -  ``CKR_PIN_LEN_RANGE``: the PIN is too long (``ulPinLen`` is greater than 255).

      .. note::

         The function should return ``CKR_PIN_INCORRECT`` in this case.

   -  ``CKR_SESSION_HANDLE_INVALID``: the session handle is invalid.
   -  ``CKR_USER_ALREADY_LOGGED_IN``: the user is already logged in.
   -  ``CKR_USER_TYPE_INVALID``

      -  The token can't authenticate the user because there is no key database or the user's
         password isn't initialized.
      -  ``userType`` is ``CKU_SO`` and the normal user's PIN is already initialized.

.. _see_also:

`See also <#see_also>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `NSC_Login </en-US/NSC_Login>`__