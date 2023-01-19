.. _mozilla_projects_nss_reference_fc_getoperationstate:

FC_GetOperationState
====================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_GetOperationState - get the cryptographic operation state of a session.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_GetOperationState(
        CK_SESSION_HANDLE hSession,
        CK_BYTE_PTR  pOperationState,
        CK_ULONG_PTR pulOperationStateLen
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] handle of the open session.
   ``pOperationState``
      [out] pointer to a byte array of a length sufficient for containing the operation state or
      NULL.
   ``pulOperationStateLen``
      [out] pointer to `CK_ULONG </en-US/CK_ULONG>`__ which receives the total length (in bytes) of
      the operation state.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_GetOperationState`` saves the state of the cryptographic operation in a session. This
   function only works for digest operations for now. Therefore, a user may call
   ``FC_GetOperationState`` without logging into the token (to assume the NSS User role).

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

   -  :ref:`mozilla_projects_nss_reference_fc_setoperationstate`,
      `NSC_GetOperationState </en-US/NSC_GetOperationState>`__