.. _mozilla_projects_nss_reference_fc_getobjectsize:

FC_GetObjectSize
================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_GetObjectSize - create a copy of an object.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_GetObjectSize(
        CK_SESSION_HANDLE hSession,
        CK_OBJECT_HANDLE hObject,
        CK_ULONG_PTR pusSize
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``hObject``
      [in] object handle.
   ``pusSize``
      [out] pointer to location to receive the object's size.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_GetObjectSize`` gets the size of an object in bytes.

   A user must log into the token (to assume the NSS User role) before getting the size of a secret
   or private key object.

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

   -  `NSC_GetObjectSize </en-US/NSC_GetObjectSize>`__