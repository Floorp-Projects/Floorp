.. _mozilla_projects_nss_reference_fc_initpin:

FC_InitPIN
==========

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   ``FC_InitPIN()`` - Initialize the user's PIN.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_InitPIN(
        CK_SESSION_HANDLE hSession,
        CK_CHAR_PTR pPin,
        CK_ULONG ulPinLen
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_InitPIN()`` takes three parameters:

   ``hSession``
      [Input] Session handle.
   ``pPin``
      [Input] Pointer to the PIN being set.
   ``ulPinLen``
      [Input] Length of the PIN.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_InitPIN()`` initializes the normal user's PIN.

   ``FC_InitPIN()`` must be called when the PKCS #11 Security Officer (SO) is logged into the token
   and the session is read/write, that is, the session must be in the "R/W SO Functions" state
   (``CKS_RW_SO_FUNCTIONS``). The role of the PKCS #11 SO is to initialize a token and to initialize
   the normal user's PIN. In the NSS cryptographic module, one uses the empty string password ("")
   to log in as the PKCS #11 SO. The module only allows the PKCS #11 SO to log in if the normal
   user's PIN has not yet been set or has been reset.

.. _return_value:

`Return value <#return_value>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_InitPIN()`` returns the following return codes.

   -  ``CKR_OK``: normal user's PIN initialization succeeded.
   -  ``CKR_SESSION_HANDLE_INVALID``: the session handle is invalid.
   -  ``CKR_USER_NOT_LOGGED_IN``: the session is not in the "R/W SO Functions" state.
   -  ``CKR_PIN_INVALID``: the PIN has an invalid UTF-8 character.
   -  ``CKR_PIN_LEN_RANGE``: the PIN is too short, too long, or too weak (doesn't have enough
      character types).
   -  ``CKR_DEVICE_ERROR``: normal user's PIN is already initialized.

.. _see_also:

`See also <#see_also>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `NSC_InitPIN </en-US/NSC_InitPIN>`__, :ref:`mozilla_projects_nss_reference_fc_setpin`