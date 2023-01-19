.. _mozilla_projects_nss_reference_fc_waitforslotevent:

FC_WaitForSlotEvent
===================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_WaitForSlotEvent - waits for a slot event, such as token insertion or token removal, to occur.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_WaitForSlotEvent(CK_FLAGS flags, CK_SLOT_ID_PTR pSlot CK_VOID_PTR pReserved);

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_WaitForSlotEvent`` takes three parameters:

   ``flags``
   ``pSlot``.
   ``pReserved``.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This function is not supported by the NSS cryptographic module.

.. _return_value:

`Return value <#return_value>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_WaitForSlotEvent`` always returns ``CKR_FUNCTION_NOT_SUPPORTED``.

`Examples <#examples>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

.. _see_also:

`See also <#see_also>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  :ref:`mozilla_projects_nss_reference_fc_waitforslotevent`