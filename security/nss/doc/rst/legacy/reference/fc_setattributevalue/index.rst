.. _mozilla_projects_nss_reference_fc_setattributevalue:

FC_SetAttributeValue
====================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_SetAttributeValue - set the values of attributes of an object.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_SetAttributeValue(
        CK_SESSION_HANDLE hSession,
        CK_OBJECT_HANDLE hObject,
        CK_ATTRIBUTE_PTR pTemplate,
        CK_ULONG usCount
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``hObject``
      [in] object handle.
   ``pTemplate``
      [in, out] pointer to template.
   ``usCount``
      [in] number of attributes in the template.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_SetAttributeValue`` sets the value of one or more attributes of an object.

   A user must log into the token before setting the attribute values of a secret or private key
   object.

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

   -  `NSC_SetAttributeValue </en-US/NSC_SetAttributeValue>`__