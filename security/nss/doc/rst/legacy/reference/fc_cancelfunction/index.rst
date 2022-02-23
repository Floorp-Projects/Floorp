.. _mozilla_projects_nss_reference_fc_cancelfunction:

FC_CancelFunction
=================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_CancelFunction - cancel a function running in parallel

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_CancelFunction(
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

   Parallel functions are not implemented. ``FC_CancelFunction`` is a legacy function that simply
   returns ``CKR_FUNCTION_NOT_PARALLEL``.

.. _return_value:

`Return value <#return_value>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_CancelFunction`` always returns ``CKR_FUNCTION_NOT_PARALLEL``.

`Examples <#examples>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

.. _see_also:

`See also <#see_also>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `NSC_CancelFunction </en-US/NSC_CancelFunction>`__