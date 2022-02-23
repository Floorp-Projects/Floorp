.. _mozilla_projects_nss_reference_fc_getmechanismlist:

FC_GetMechanismList
===================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_GetMechanismList - get a list of mechanism types supported by a token.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_GetMechanismList(
        CK_SLOT_ID slotID,
        CK_MECHANISM_TYPE_PTR pMechanismList,
        CK_ULONG_PTR pusCount
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_GetMechanismList`` takes three parameters:

   ``slotID``
      [Input]
   ``pInfo``
      [Output] The address of a variable that will receive a pointer to the list of function
      pointers.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_GetMechanismList`` obtains a list of mechanism types supported by a token.

   A user may call ``FC_GetMechanismList`` without logging into the token (to assume the NSS User
   role).

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

   -  `NSC_GetMechanismList </en-US/NSC_GetMechanismList>`__