.. _mozilla_projects_nss_reference_fc_getslotinfo:

FC_GetSlotInfo
==============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_GetSlotInfo - get information about a particular slot in the system.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_GetSlotInfo(
        CK_SLOT_ID slotID,
        CK_SLOT_INFO_PTR pInfo
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_GetSlotInfo`` takes two parameters:

   ``slotID``
      [in]
   ``pInfo``
      [out] The address of a ``CK_SLOT_INFO`` structure.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_GetSlotInfo`` stores the information about the slot in the ``CK_SLOT_INFO`` structure that
   ``pInfo`` points to.

   A user may call ``FC_GetSlotInfo`` without logging into the token (to assume the NSS User role).

.. _return_value:

`Return value <#return_value>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``CKR_OK``
      Slot information was successfully copied.
   ``CKR_SLOT_ID_INVALID``
      The specified slot number is out of the defined range of values.

`Examples <#examples>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

.. _see_also:

`See also <#see_also>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `NSC_GetSlotInfo </en-US/NSC_GetSlotInfo>`__