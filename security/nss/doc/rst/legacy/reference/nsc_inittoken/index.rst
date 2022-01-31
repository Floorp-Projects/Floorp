.. _mozilla_projects_nss_reference_nsc_inittoken:

NSC_InitToken
=============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   ``NSC_InitToken()`` - initialize or re-initialize a token.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV NSC_InitToken(
        CK_SLOT_ID slotID,
        CK_CHAR_PTR pPin,
        CK_ULONG ulPinLen,
        CK_CHAR_PTR pLabel
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``NSC_InitToken()`` has the following parameters:

   ``slotID``
      the ID of the token's slot
   ``pPin``
      the password of the security officer (SO)
   ``ulPinLen``
      the length in bytes of the SO password
   ``pLabel``
      points to the label of the token, which must be padded with spaces to 32 bytes and not be
      null-terminated

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``NSC_InitToken()`` initializes a brand new token or re-initializes a token that was initialized
   before.

   Specifically, ``NSC_InitToken()`` initializes or clears the key database, removes the password,
   and then marks all the *user certs* in the certificate database as *non-user certs*. (User certs
   are the certificates that have their associated private keys in the key database.)

   .. note::

      **Note:** The SO password should be the empty string, i.e., ``ulPinLen`` argument should be 0.
      ``NSC_InitToken()`` ignores the ``pLabel`` argument.

.. _return_value:

`Return value <#return_value>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``NSC_InitToken()`` returns the following return codes.

   -  ``CKR_OK``: token initialization succeeded.
   -  ``CKR_SLOT_ID_INVALID``: slot ID is invalid.
   -  ``CKR_TOKEN_WRITE_PROTECTED``

      -  slot ID is 1. (The non-FIPS mode has two slots: 1 and 2. The key database is in slot 2.
         Slot 1 doesn't have a key database.)
      -  we don't have a reference to the key database (we failed to open the key database or we
         have released our reference).

   -  ``CKR_DEVICE_ERROR``: failed to reset the key database.

`Examples <#examples>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

.. _application_usage:

`Application usage <#application_usage>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``NSC_InitToken()`` is used to reset the password for the key database when the user forgets the
   password.

   -  The "Reset Password" button of the Mozilla Application Suite and SeaMonkey (in
      **Preferences->Privacy & Security->Master Passwords**) calls ``NSC_InitToken()``.
   -  The "-T" (token reset) command of ``certutil`` calls ``NSC_InitToken()``.

   .. note::

      **Note:** Resetting the password clears all permanent secret and private keys. You won't be
      able to decrypt the data, such as Mozilla's stored passwords, that were encrypted using any of
      those keys.

.. _see_also:

`See also <#see_also>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `NSC_InitPIN </en-US/NSC_InitPIN>`__, :ref:`mozilla_projects_nss_reference_fc_inittoken`