.. _mozilla_projects_nss_reference_fc_verifyrecover:

FC_VerifyRecover
================

`Name <#name>`__
~~~~~~~~~~~~~~~~

.. container::

   FC_VerifyRecover - Verify data in a single recoverable operation.

`Syntax <#syntax>`__
~~~~~~~~~~~~~~~~~~~~

.. container::

   .. code:: eval

      CK_RV FC_VerifyRecover(
        CK_SESSION_HANDLE hSession,
        CK_BYTE_PTR pSignature,
        CK_ULONG usSignatureLen,
        CK_BYTE_PTR pData,
        CK_ULONG_PTR pusDataLen
      );

`Parameters <#parameters>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``hSession``
      [in] session handle.
   ``pSignature``
      [in] mechanism to be used for the signing operation.
   ``usSignatureLen``
      [in] handle of the key to be usedn.
   ``pData``
      [out] pointer to the buffer or NULL.
   ``pusDataLen``
      [in, out] pointer to the size of the output buffer, replaced by the length of the signature if
      the operation is successful.

`Description <#description>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   ``FC_VerifyRecover`` verifies data in a single operation where the (digest) data can be recovered
   from the signature. If ``pSignature`` is NULL only the length of the signature is returned in
   ``*pusSignatureLen``.

   A user must log into the token (to assume the NSS User role) before calling ``FC_VerifyRecover``.

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

   -  `NSC_VerifyRecover </en-US/NSC_VerifyRecover>`__