.. _mozilla_projects_nss_reference_fc_createobject:

FC_CreateObject
===============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_CreateObject - create a new object.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_CreateObject(
        CK_SESSION_HANDLE hSession,
        CK_ATTRIBUTE_PTR pTemplate,
        CK_ULONG ulCount,
        CK_OBJECT_HANDLE_PTR phObject
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pTemplate``
      [in] object template.
   ``ulCount``
      [in] number of attributes in the template.
   ``phObject``
      [out] pointer to location to receive the new objects handle.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_CreateObject`` creates an object using the attributes specified in the template.

   A user must log into the token (to assume the NSS User role) before calling ``FC_CreateObject``.

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

   -  :ref:`mozilla_projects_nss_reference_fc_destroyobject`,
      `NSC_CreateObject </en-US/NSC_CreateObject>`__