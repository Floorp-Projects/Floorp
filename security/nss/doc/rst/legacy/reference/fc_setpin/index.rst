.. _mozilla_projects_nss_reference_fc_setpin:

FC_SetPIN
=========

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_SetPIN - Modify the user's PIN.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_SetPIN(
        CK_SESSION_HANDLE hSession,
        CK_CHAR_PTR pOldPin,
        CK_ULONG ulOldLen,
        CK_CHAR_PTR pNewPin,
        CK_ULONG ulNewLen
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_SetPIN`` takes five parameters:

   ``hSession``
      [Input] the session's handle
   ``pOldPin``
      [Input] points to the old PIN.
   ``ulOldLen``
      [Input] the length in bytes of the old PIN.
   ``pNewPin``
      [Input] points to the new PIN.
   ``ulNewLen``
      [Input] the length in bytes of the new PIN.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_SetPIN`` modifies the PIN of the user. The user must log into the token (to assume the NSS
   User role) before calling ``FC_SetPIN``.

.. _return_value:

`Return value <#return_value>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``CKR_OK``

`Examples <#examples>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

.. _see_also:

`See also <#see_also>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `NSC_SetPIN </en-US/NSC_SetPIN>`__