.. _mozilla_projects_nss_reference_fc_finalize:

FC_Finalize
===========

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_Finalize - indicate that an application is done with the PKCS #11 library.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_Finalize (CK_VOID_PTR pReserved);

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_Finalize`` has one parameter:

   ``pReserved``
      must be ``NULL``

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_Finalize`` shuts down the :ref:`mozilla_projects_nss_reference_nss_cryptographic_module` in
   the :ref:`mozilla_projects_nss_reference_nss_cryptographic_module_fips_mode_of_operation`. If the
   library is not initialized, it does nothing.

   The ``pReserved`` argument is not used and must be ``NULL``.

.. _return_value:

`Return value <#return_value>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_Finalize`` always returns ``CKR_OK``.

   .. note::

      ``FC_Finalize`` should check the ``pReserved`` argument and return ``CKR_ARGUMENTS_BAD`` if
      ``pReserved`` is not ``NULL``.

      ``FC_Finalize`` should return ``CKR_CRYPTOKI_NOT_INITIALIZED`` if the library is not
      initialized.

`Examples <#examples>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      #include <assert.h>

      CK_FUNCTION_LIST_PTR pFunctionList;
      CK_RV crv;

      crv = FC_GetFunctionList(&pFunctionList);
      assert(crv == CKR_OK);

      ...

      /* invoke FC_Finalize as pFunctionList->C_Finalize */
      crv = pFunctionList->C_Finalize(NULL);

.. _see_also:

`See also <#see_also>`__
~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  :ref:`mozilla_projects_nss_reference_fc_initialize`,
      `NSC_Initialize </en-US/NSC_Initialize>`__, `NSC_Finalize </en-US/NSC_Finalize>`__