.. _mozilla_projects_nss_reference_fc_derivekey:

FC_DeriveKey
============

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_DeriveKey - derive a key from a base key

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_DeriveKey(
        CK_SESSION_HANDLE hSession,
        CK_MECHANISM_PTR pMechanism,
        CK_OBJECT_HANDLE hBaseKey,
        CK_ATTRIBUTE_PTR pTemplate,
        CK_ULONG usAttributeCount,
        CK_OBJECT_HANDLE_PTR phKey
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pMechanism``
      [in] pointer to the mechanism to use.
   ``hBaseKey``
      [in] handle of the base key.
   ``pWrappedKey``
      [in] pointer to the wrapped key.
   ``pTemplate``
      [in] pointer to the list of attributes for the new key.
   ``usAttributeCount``
      [in] number of attributes in the template.
   ``phKey``
      [out] pointer to the location to receive the handle of the new key.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_DeriveKey`` derives (decrypts) a key and creates a new key object.

   A user must log into the token (to assume the NSS User role) before calling ``FC_DeriveKey``.

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

   -  `NSC_DeriveKey </en-US/NSC_DeriveKey>`__