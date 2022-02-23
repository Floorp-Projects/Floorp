.. _mozilla_projects_nss_reference_fc_copyobject:

FC_CopyObject
=============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_CopyObject - create a copy of an object.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_CopyObject(
        CK_SESSION_HANDLE hSession,
        CK_OBJECT_HANDLE hObject,
        CK_ATTRIBUTE_PTR pTemplate,
        CK_ULONG usCount,
        CK_OBJECT_HANDLE_PTR phNewObject
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``hObject``
      [in] object handle.
   ``pTemplate``
      [in] object template.
   ``usCount``
      [in] number of attributes in the template.
   ``phnewObject``
      [out] pointer to location to receive the new object's handle.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_CopyObject`` creates a copy of an object using the attributes specified in the template.

   A user must log into the token (to assume the NSS User role) before copying a secret or private
   key object.

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
      `NSC_CopyObject </en-US/NSC_CopyObject>`__