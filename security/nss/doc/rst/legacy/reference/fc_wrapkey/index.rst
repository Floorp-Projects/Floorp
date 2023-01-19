.. _mozilla_projects_nss_reference_fc_wrapkey:

FC_WrapKey
==========

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_WrapKey - wrap a key

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code::

      CK_RV FC_WrapKey(
        CK_SESSION_HANDLE hSession,
        CK_MECHANISM_PTR pMechanism,
        CK_OBJECT_HANDLE hWrappingKey,
        CK_OBJECT_HANDLE hKey,
        CK_BYTE_PTR pWrappedKey,
        CK_ULONG_PTR pusWrappedKeyLen
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pMechanism``
      [in] pointer to the mechanism to use.
   ``hWrappingKey``
      [in] pointer to the public key template.
   ``hKey``
      [in] number of attributes in the public key template.
   ``pWrappedKey``
      [out] pointer to the location to receive the wrapped key or NULL.
   ``pusWrappedKeyLen``
      [in, out] pointer to length of wrapped key buffer.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_WrapKey`` wraps (encrypts) a key. If ``pWrappedKey`` is NULL the length of the wrapped key
   is returned in ``pusWrappedKeyLen`` and FC_WrapKey may be called again with ``pWrappedKey`` set
   to retrieve the wrapped key.

   A user must log into the token (to assume the NSS User role) before calling ``FC_WrapKey``.

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

   -  `NSC_WrapKey </en-US/NSC_WrapKey>`__