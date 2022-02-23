.. _mozilla_projects_nss_reference_fc_setoperationstate:

FC_SetOperationState
====================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_SetOperationState - restore the cryptographic operation state of a session.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_SetOperationState(
        CK_SESSION_HANDLE hSession,
        CK_BYTE_PTR pOperationState,
        CK_ULONG ulOperationStateLen,
        CK_OBJECT_HANDLE hEncryptionKey,
        CK_OBJECT_HANDLE hAuthenticationKey
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] handle of the open session.
   ``pOperationState``
      [in] pointer to a byte array containing the operation state.
   ``ulOperationStateLen``
      [in] contains the total length (in bytes) of the operation state.
   ``hEncryptionKey``
      [in] handle of the encryption or decryption key to be used in a stored session or zero if no
      key is needed.
   ``hAuthenticationKey``
      [in] handle of the authentication key to be used in the stored session or zero if none is
      needed.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_SetOperationState`` restores the cryptographic operations state of a session from an array
   of bytes obtained with ``FC_GetOperationState``. This function only works for digest operations
   for now. Therefore, a user may call ``FC_SetOperationState`` without logging into the token (to
   assume the NSS User role).

.. _return_value:

`Return value <#return_value>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

`Examples <#examples>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

.. _see_also:

`See also <#see_also>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  :ref:`mozilla_projects_nss_reference_fc_getoperationstate`,
      `NSC_SetOperationState </en-US/NSC_SetOperationState>`__