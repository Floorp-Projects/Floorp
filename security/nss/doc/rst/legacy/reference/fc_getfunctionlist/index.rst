.. _mozilla_projects_nss_reference_fc_getfunctionlist:

FC_GetFunctionList
==================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_GetFunctionList - get a pointer to the list of function pointers in the FIPS mode of
   operation.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_GetFunctionList(CK_FUNCTION_LIST_PTR *ppFunctionList);

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_GetFunctionList`` has one parameter:

   ``ppFunctionList``
      [Output] The address of a variable that will receive a pointer to the list of function
      pointers.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_GetFunctionList`` stores in ``*ppFunctionList`` a pointer to the
   :ref:`mozilla_projects_nss_reference_nss_cryptographic_module`'s list of function pointers in the
   :ref:`mozilla_projects_nss_reference_nss_cryptographic_module_fips_mode_of_operation`.

   A user may call ``FC_GetFunctionList`` without logging into the token (to assume the NSS User
   role).

.. _return_value:

`Return value <#return_value>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_GetFunctionList`` always returns ``CKR_OK``.

`Examples <#examples>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      #include <assert.h>

      CK_FUNCTION_LIST_PTR pFunctionList;
      CK_RV crv;

      crv = FC_GetFunctionList(&pFunctionList);
      assert(crv == CKR_OK);

      /* invoke the FC_XXX function as pFunctionList->C_XXX */

.. _see_also:

`See also <#see_also>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  `NSC_GetFunctionList </en-US/NSC_GetFunctionList>`__