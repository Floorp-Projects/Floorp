.. _mozilla_projects_nss_reference_fc_getfunctionstatus:

FC_GetFunctionStatus
====================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_GetFunctionStatus - get the status of a function running in parallel

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_GetFunctionStatus(
        CK_SESSION_HANDLE hSession
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_GetFunctionStatus`` is a legacy function that simply returns ``CKR_FUNCTION_NOT_PARALLEL``.

.. _return_value:

`Return value <#return_value>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_GetFunctionStatus`` always returns ``CKR_FUNCTION_NOT_PARALLEL``.

`Examples <#examples>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

.. _see_also:

`See also <#see_also>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `NSC_GetFunctionStatus </en-US/NSC_GetFunctionStatus>`__