.. _mozilla_projects_nss_reference_fc_destroyobject:

FC_DestroyObject
================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_DestroyObject - destroy an object.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_DestroyObject(
        CK_SESSION_HANDLE hSession,
        CK_OBJECT_HANDLE hObject
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``hObject``
      [in] object handle.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_DestroyObject`` destroys an object.

   A user must log into the token (to assume the NSS User role) before destroying a secret or
   private key object.

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

   -  `NSC_DestroyObject </en-US/NSC_DestroyObject>`__